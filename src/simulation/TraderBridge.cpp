#include "simulation/TraderBridge.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace mm {
namespace {

std::string extractString(const std::string& json, const char* key) {
    const std::string pat = std::string("\"") + key + "\"";
    auto pos = json.find(pat);
    if (pos == std::string::npos) {
        return {};
    }
    pos = json.find(':', pos);
    if (pos == std::string::npos) {
        return {};
    }
    pos = json.find('"', pos + 1);
    if (pos == std::string::npos) {
        return {};
    }
    const auto end = json.find('"', pos + 1);
    if (end == std::string::npos) {
        return {};
    }
    return json.substr(pos + 1, end - pos - 1);
}

double extractNumber(const std::string& json, const char* key, double fallback = 0.0) {
    const std::string pat = std::string("\"") + key + "\"";
    auto pos = json.find(pat);
    if (pos == std::string::npos) {
        return fallback;
    }
    pos = json.find(':', pos);
    if (pos == std::string::npos) {
        return fallback;
    }
    ++pos;
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) {
        ++pos;
    }
    try {
        return std::stod(json.substr(pos));
    } catch (...) {
        return fallback;
    }
}

}  // namespace

TraderBridge::TraderBridge(TraderBridgeConfig config) : config_(config) {
    oms_.setFillHandler([this](const ExternalFill& fill, bool hitAsk) {
        onFill(fill, hitAsk);
    });
}

bool TraderBridge::start() {
    resetTraderPnL();
    oms_.clear();
    seq_ = 0;
    return server_.start(config_.port);
}

void TraderBridge::stop() { server_.stop(); }

std::string TraderBridge::jsonEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        if (c == '"' || c == '\\') {
            out.push_back('\\');
        }
        out.push_back(c);
    }
    return out;
}

void TraderBridge::onFill(const ExternalFill& fill, bool /*hitAsk*/) {
    // Trader accounting (opposite of dealer).
    if (fill.side == ExtSide::Buy) {
        traderCash_ -= fill.size * fill.price;
        traderInventory_ += fill.size;
    } else {
        traderCash_ += fill.size * fill.price;
        traderInventory_ -= fill.size;
    }
    std::ostringstream oss;
    oss.setf(std::ios::fixed);
    oss.precision(8);
    oss << "{\"type\":\"fill\",\"id\":\"" << jsonEscape(fill.id) << "\",\"side\":\""
        << (fill.side == ExtSide::Buy ? "BUY" : "SELL") << "\",\"price\":" << fill.price
        << ",\"size\":" << fill.size << "}";
    server_.sendLine(oss.str());
}

void TraderBridge::publishBook(const MarketMakerState& state,
                               const std::vector<MarketEvent>& recentEvents) {
    if (!server_.clientConnected()) {
        return;
    }
    ++seq_;
    std::ostringstream oss;
    oss.setf(std::ios::fixed);
    oss.precision(8);
    oss << "{\"type\":\"book\",\"seq\":" << seq_ << ",\"t\":" << state.time
        << ",\"mid\":" << state.midPrice << ",\"bid\":" << state.bidPrice
        << ",\"ask\":" << state.askPrice << ",\"spread\":" << (state.askPrice - state.bidPrice)
        << ",\"bid_size\":" << config_.defaultSize << ",\"ask_size\":" << config_.defaultSize
        << ",\"inventory\":" << state.inventory << ",\"lambda_ask\":" << state.lambdaAsk
        << ",\"lambda_bid\":" << state.lambdaBid << ",\"trader_cash\":" << traderCash_
        << ",\"trader_inventory\":" << traderInventory_
        << ",\"trader_pnl\":" << traderPnL(state.midPrice);

    oss << ",\"bids\":[";
    for (int i = 0; i < config_.depthLevels; ++i) {
        if (i) {
            oss << ',';
        }
        const double px = state.bidPrice - i * config_.tick;
        oss << '[' << px << ',' << config_.defaultSize << ']';
    }
    oss << "],\"asks\":[";
    for (int i = 0; i < config_.depthLevels; ++i) {
        if (i) {
            oss << ',';
        }
        const double px = state.askPrice + i * config_.tick;
        oss << '[' << px << ',' << config_.defaultSize << ']';
    }
    oss << "],\"tape\":[";
    const int n = static_cast<int>(recentEvents.size());
    const int start = std::max(0, n - 20);
    bool first = true;
    for (int i = start; i < n; ++i) {
        const auto& e = recentEvents[static_cast<std::size_t>(i)];
        if (e.type != EventType::AskHit && e.type != EventType::BidHit) {
            continue;
        }
        if (!first) {
            oss << ',';
        }
        first = false;
        oss << "{\"t\":" << e.timestamp << ",\"side\":\""
            << (e.type == EventType::AskHit ? "BUY" : "SELL") << "\",\"price\":" << e.tradePrice
            << ",\"size\":" << e.tradeSize << ",\"source\":\""
            << (e.external ? "external" : "poisson") << "\"}";
    }
    oss << "]}";
    server_.sendLine(oss.str());
}

void TraderBridge::handleLine(const std::string& line, const MarketMakerState& /*state*/) {
    const std::string type = extractString(line, "type");
    if (type == "place") {
        ExternalOrder order;
        order.id = extractString(line, "id");
        const std::string ot = extractString(line, "order_type");
        const std::string side = extractString(line, "side");
        order.size = extractNumber(line, "size", config_.defaultSize);
        order.price = extractNumber(line, "price", 0.0);
        order.stop = extractNumber(line, "stop", 0.0);
        if (ot == "MARKET") {
            order.type = ExtOrderType::Market;
        } else if (ot == "LIMIT") {
            order.type = ExtOrderType::Limit;
        } else if (ot == "STOP") {
            order.type = ExtOrderType::Stop;
        } else {
            server_.sendLine(R"({"type":"reject","reason":"bad order_type"})");
            return;
        }
        if (side == "BUY") {
            order.side = ExtSide::Buy;
        } else if (side == "SELL") {
            order.side = ExtSide::Sell;
        } else {
            server_.sendLine(R"({"type":"reject","reason":"bad side"})");
            return;
        }
        const std::string err = oms_.place(order);
        if (!err.empty()) {
            std::ostringstream oss;
            oss << "{\"type\":\"reject\",\"id\":\"" << jsonEscape(order.id)
                << "\",\"reason\":\"" << jsonEscape(err) << "\"}";
            server_.sendLine(oss.str());
            return;
        }
        std::ostringstream ack;
        ack << "{\"type\":\"ack\",\"id\":\"" << jsonEscape(order.id) << "\",\"status\":\"live\"}";
        server_.sendLine(ack.str());
        return;
    }
    if (type == "cancel") {
        const std::string id = extractString(line, "id");
        const std::string err = oms_.cancel(id);
        if (!err.empty()) {
            std::ostringstream oss;
            oss << "{\"type\":\"reject\",\"id\":\"" << jsonEscape(id) << "\",\"reason\":\""
                << jsonEscape(err) << "\"}";
            server_.sendLine(oss.str());
        } else {
            std::ostringstream oss;
            oss << "{\"type\":\"ack\",\"id\":\"" << jsonEscape(id)
                << "\",\"status\":\"cancelled\"}";
            server_.sendLine(oss.str());
        }
        return;
    }
    if (type == "ping") {
        server_.sendLine(R"({"type":"pong"})");
    }
}

std::vector<ExternalFill> TraderBridge::pollAndMatch(
    const MarketMakerState& state, const std::vector<MarketEvent>& recentEvents) {
    server_.poll();
    for (const auto& line : server_.drainIncoming()) {
        handleLine(line, state);
    }
    auto fills = oms_.match(state.midPrice, state.bidPrice, state.askPrice);
    publishBook(state, recentEvents);
    return fills;
}

}  // namespace mm
