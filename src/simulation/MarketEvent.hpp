#pragma once

#include <string>

namespace mm {

enum class EventType {
    MidMove,
    AskHit,
    BidHit,
    QuoteUpdate
};

struct MarketEvent {
    double timestamp = 0.0;
    EventType type = EventType::MidMove;
    double midPrice = 0.0;
    double bidPrice = 0.0;
    double askPrice = 0.0;
    double tradePrice = 0.0;
    double tradeSize = 0.0;
    double inventoryAfter = 0.0;
    double cashAfter = 0.0;
    double lambdaAsk = 0.0;
    double lambdaBid = 0.0;
    double realizedPnL = 0.0;
    double inventoryPnL = 0.0;
    double totalPnL = 0.0;
    double askOffset = 0.0;
    double bidOffset = 0.0;
    bool external = false;

    static const char* typeName(EventType t) {
        switch (t) {
            case EventType::MidMove:
                return "MID_MOVE";
            case EventType::AskHit:
                return "ASK_HIT";
            case EventType::BidHit:
                return "BID_HIT";
            case EventType::QuoteUpdate:
                return "QUOTE_UPDATE";
        }
        return "UNKNOWN";
    }
};

}  // namespace mm
