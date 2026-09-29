#pragma once

#include <string>

namespace mm {

enum class ExtOrderType { Market, Limit, Stop };
enum class ExtSide { Buy, Sell };
enum class ExtOrderStatus { Live, Filled, Cancelled, Rejected };

struct ExternalOrder {
    std::string id;
    ExtOrderType type = ExtOrderType::Market;
    ExtSide side = ExtSide::Buy;
    double size = 1.0;
    double price = 0.0;   // limit price
    double stop = 0.0;    // stop trigger
    ExtOrderStatus status = ExtOrderStatus::Live;
    bool stopTriggered = false;
};

struct ExternalFill {
    std::string id;
    ExtSide side = ExtSide::Buy;
    double price = 0.0;
    double size = 0.0;
};

}  // namespace mm
