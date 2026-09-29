#pragma once

#include "simulation/ExternalOrder.hpp"

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace mm {

class ExternalOrderManager {
public:
    // Trader BUY hits dealer ask; trader SELL hits dealer bid.
    using FillHandler =
        std::function<void(const ExternalFill& fill, bool hitAsk /*trader buy*/)>;

    void setFillHandler(FillHandler handler) { fillHandler_ = std::move(handler); }

    // Returns reject reason empty on success.
    std::string place(ExternalOrder order);
    std::string cancel(const std::string& id);

    // Match resting limits / trigger stops against current quotes.
    std::vector<ExternalFill> match(double mid, double bid, double ask);

    const std::unordered_map<std::string, ExternalOrder>& orders() const { return orders_; }

    void clear() { orders_.clear(); }

    int liveCount() const;

private:
    void fillOrder(ExternalOrder& order, double price, std::vector<ExternalFill>& out);

    std::unordered_map<std::string, ExternalOrder> orders_;
    FillHandler fillHandler_;
};

}  // namespace mm
