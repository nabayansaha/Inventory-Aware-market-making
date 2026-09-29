#pragma once

namespace mm {

struct MarketMakerState {
    double time = 0.0;
    double midPrice = 100.0;
    double bidPrice = 100.0;
    double askPrice = 100.0;
    double askOffset = 0.0;
    double bidOffset = 0.0;
    double inventory = 0.0;
    double cash = 0.0;
    double realizedPnL = 0.0;
    double inventoryPnL = 0.0;
    double totalPnL = 0.0;
    double lambdaAsk = 0.0;
    double lambdaBid = 0.0;
    int tradeCount = 0;
    int askHits = 0;
    int bidHits = 0;
    double maxInventory = 0.0;
    double minInventory = 0.0;
    double spreadSum = 0.0;
    int quoteUpdates = 0;

    double wealth() const { return cash + inventory * midPrice; }

    void updatePnL(double initialWealth) {
        inventoryPnL = inventory * midPrice;
        totalPnL = wealth() - initialWealth;
        // realizedPnL tracked incrementally on trades relative to mid at fill
    }

    void noteInventoryExtremes() {
        maxInventory = inventory > maxInventory ? inventory : maxInventory;
        minInventory = inventory < minInventory ? inventory : minInventory;
    }
};

}  // namespace mm
