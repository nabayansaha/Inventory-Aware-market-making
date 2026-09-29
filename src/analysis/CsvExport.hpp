#pragma once

#include "simulation/MarketEvent.hpp"
#include "simulation/MarketMakerState.hpp"

#include <fstream>
#include <string>
#include <vector>

namespace mm {

inline bool exportSimulationCsv(const std::string& path,
                                const std::vector<MarketEvent>& events) {
    std::ofstream out(path);
    if (!out) {
        return false;
    }
    out << "timestamp,mid,bid,ask,spread,ask_offset,bid_offset,"
           "lambda_ask,lambda_bid,event_type,trade_price,trade_size,"
           "inventory,cash,realized_pnl,inventory_pnl,total_pnl\n";
    out.setf(std::ios::fixed);
    out.precision(8);
    for (const auto& e : events) {
        out << e.timestamp << ',' << e.midPrice << ',' << e.bidPrice << ','
            << e.askPrice << ',' << (e.askPrice - e.bidPrice) << ','
            << e.askOffset << ',' << e.bidOffset << ',' << e.lambdaAsk << ','
            << e.lambdaBid << ',' << MarketEvent::typeName(e.type) << ','
            << e.tradePrice << ',' << e.tradeSize << ',' << e.inventoryAfter
            << ',' << e.cashAfter << ',' << e.realizedPnL << ','
            << e.inventoryPnL << ',' << e.totalPnL << '\n';
    }
    return true;
}

}  // namespace mm
