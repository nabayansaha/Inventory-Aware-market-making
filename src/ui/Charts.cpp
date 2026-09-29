#include "ui/Charts.hpp"

#include <imgui.h>
#include <implot.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace mm {
namespace {

void collectSeries(const std::vector<MarketEvent>& events, std::vector<double>& t,
                   std::vector<double>& mid, std::vector<double>& bid,
                   std::vector<double>& ask, std::vector<double>& pnl,
                   std::vector<double>& inv) {
    t.clear();
    mid.clear();
    bid.clear();
    ask.clear();
    pnl.clear();
    inv.clear();
    t.reserve(events.size());
    mid.reserve(events.size());
    bid.reserve(events.size());
    ask.reserve(events.size());
    pnl.reserve(events.size());
    inv.reserve(events.size());
    for (const auto& e : events) {
        t.push_back(e.timestamp);
        mid.push_back(e.midPrice);
        bid.push_back(e.bidPrice);
        ask.push_back(e.askPrice);
        pnl.push_back(e.totalPnL);
        inv.push_back(e.inventoryAfter);
    }
}

}  // namespace

void drawPriceChart(const std::vector<MarketEvent>& events) {
    std::vector<double> t, mid, bid, ask, pnl, inv;
    collectSeries(events, t, mid, bid, ask, pnl, inv);
    if (ImPlot::BeginPlot("Price", ImVec2(-1, 220))) {
        ImPlot::SetupAxes("time", "price");
        if (!t.empty()) {
            ImPlot::PlotLine("Mid", t.data(), mid.data(), static_cast<int>(t.size()));
            ImPlot::PlotLine("Bid", t.data(), bid.data(), static_cast<int>(t.size()));
            ImPlot::PlotLine("Ask", t.data(), ask.data(), static_cast<int>(t.size()));
        }
        ImPlot::EndPlot();
    }
}

void drawPnLChart(const std::vector<MarketEvent>& events) {
    std::vector<double> t, mid, bid, ask, pnl, inv;
    collectSeries(events, t, mid, bid, ask, pnl, inv);
    if (ImPlot::BeginPlot("Total P&L", ImVec2(-1, 180))) {
        ImPlot::SetupAxes("time", "pnl");
        if (!t.empty()) {
            ImPlot::PlotLine("PnL", t.data(), pnl.data(), static_cast<int>(t.size()));
        }
        ImPlot::EndPlot();
    }
}

void drawInventoryChart(const std::vector<MarketEvent>& events) {
    std::vector<double> t, mid, bid, ask, pnl, inv;
    collectSeries(events, t, mid, bid, ask, pnl, inv);
    if (ImPlot::BeginPlot("Inventory", ImVec2(-1, 160))) {
        ImPlot::SetupAxes("time", "inventory");
        if (!t.empty()) {
            ImPlot::PlotLine("I(t)", t.data(), inv.data(), static_cast<int>(t.size()));
        }
        ImPlot::EndPlot();
    }
}

void drawPolicyChart(const Policy& policy, double time) {
    if (policy.empty()) {
        ImGui::TextUnformatted("No DP policy loaded.");
        return;
    }
    std::vector<double> ask;
    std::vector<double> bid;
    policy.policySliceAtTime(time, ask, bid);
    const auto& inv = policy.inventories();
    if (ImPlot::BeginPlot("Dealer Strategy a*(I), b*(I)", ImVec2(-1, 200))) {
        ImPlot::SetupAxes("inventory", "offset");
        ImPlot::PlotLine("a*(I)", inv.data(), ask.data(), static_cast<int>(inv.size()));
        ImPlot::PlotLine("b*(I)", inv.data(), bid.data(), static_cast<int>(inv.size()));
        ImPlot::EndPlot();
    }
}

void drawHistogram(const char* title, const std::vector<double>& values, int bins) {
    if (values.empty()) {
        ImGui::Text("No data for %s", title);
        return;
    }
    double lo = *std::min_element(values.begin(), values.end());
    double hi = *std::max_element(values.begin(), values.end());
    if (hi <= lo) {
        hi = lo + 1.0;
    }
    std::vector<double> counts(static_cast<std::size_t>(bins), 0.0);
    std::vector<double> centers(static_cast<std::size_t>(bins), 0.0);
    const double width = (hi - lo) / bins;
    for (int i = 0; i < bins; ++i) {
        centers[static_cast<std::size_t>(i)] = lo + (i + 0.5) * width;
    }
    for (double v : values) {
        int idx = static_cast<int>((v - lo) / width);
        idx = std::clamp(idx, 0, bins - 1);
        counts[static_cast<std::size_t>(idx)] += 1.0;
    }
    if (ImPlot::BeginPlot(title, ImVec2(-1, 180))) {
        ImPlot::SetupAxes("value", "count");
        ImPlot::PlotBars("hist", centers.data(), counts.data(), bins, width * 0.9);
        ImPlot::EndPlot();
    }
}

}  // namespace mm
