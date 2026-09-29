#pragma once

#include "net/TcpJsonServer.hpp"
#include "simulation/ExternalOrderManager.hpp"
#include "simulation/MarketEvent.hpp"
#include "simulation/MarketMakerState.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace mm {

struct TraderBridgeConfig {
    std::uint16_t port = 8765;
    double tick = 0.05;
    int depthLevels = 5;
    double defaultSize = 1.0;
};

// Owns TCP server + OMS. Call poll() each UI frame.
class TraderBridge {
public:
    explicit TraderBridge(TraderBridgeConfig config = {});

    bool start();
    void stop();
    bool running() const { return server_.running(); }
    bool clientConnected() const { return server_.clientConnected(); }
    std::uint16_t port() const { return config_.port; }

    ExternalOrderManager& oms() { return oms_; }
    const ExternalOrderManager& oms() const { return oms_; }

    // Process inbound place/cancel; match vs quotes; return fills for MM accounting.
    std::vector<ExternalFill> pollAndMatch(const MarketMakerState& state,
                                           const std::vector<MarketEvent>& recentEvents);

    // Broadcast book snapshot (also called from pollAndMatch).
    void publishBook(const MarketMakerState& state,
                     const std::vector<MarketEvent>& recentEvents);

    double traderCash() const { return traderCash_; }
    double traderInventory() const { return traderInventory_; }
    double traderPnL(double mid) const {
        return traderCash_ + traderInventory_ * mid;
    }

    void resetTraderPnL() {
        traderCash_ = 0.0;
        traderInventory_ = 0.0;
    }

    // Push strategy source to the connected Python bot (and cache for reconnect).
    void setStrategySource(std::string source);
    const std::string& strategySource() const { return strategySource_; }
    void pushStrategyToClient();

private:
    void handleLine(const std::string& line, const MarketMakerState& state);
    void onFill(const ExternalFill& fill, bool /*hitAsk*/);
    static std::string jsonEscape(const std::string& s);

    TraderBridgeConfig config_;
    TcpJsonServer server_;
    ExternalOrderManager oms_;
    std::uint64_t seq_ = 0;
    double traderCash_ = 0.0;
    double traderInventory_ = 0.0;
    std::string strategySource_;
    bool clientWasConnected_ = false;
};

}  // namespace mm
