#include "simulation/ExternalOrderManager.hpp"

namespace mm {

int ExternalOrderManager::liveCount() const {
    int n = 0;
    for (const auto& [_, o] : orders_) {
        if (o.status == ExtOrderStatus::Live) {
            ++n;
        }
    }
    return n;
}

std::string ExternalOrderManager::place(ExternalOrder order) {
    if (order.id.empty()) {
        return "missing id";
    }
    if (order.size <= 0.0) {
        return "size must be > 0";
    }
    if (order.type == ExtOrderType::Limit && order.price <= 0.0) {
        return "limit requires price > 0";
    }
    if (order.type == ExtOrderType::Stop && order.stop <= 0.0) {
        return "stop requires stop > 0";
    }
    if (orders_.count(order.id) && orders_[order.id].status == ExtOrderStatus::Live) {
        return "duplicate live id";
    }
    order.status = ExtOrderStatus::Live;
    order.stopTriggered = false;
    orders_[order.id] = order;
    return {};
}

std::string ExternalOrderManager::cancel(const std::string& id) {
    auto it = orders_.find(id);
    if (it == orders_.end()) {
        return "unknown id";
    }
    if (it->second.status != ExtOrderStatus::Live) {
        return "not live";
    }
    it->second.status = ExtOrderStatus::Cancelled;
    return {};
}

void ExternalOrderManager::fillOrder(ExternalOrder& order, double price,
                                     std::vector<ExternalFill>& out) {
    ExternalFill fill;
    fill.id = order.id;
    fill.side = order.side;
    fill.price = price;
    fill.size = order.size;
    order.status = ExtOrderStatus::Filled;
    out.push_back(fill);
    if (fillHandler_) {
        const bool hitAsk = (order.side == ExtSide::Buy);
        fillHandler_(fill, hitAsk);
    }
}

std::vector<ExternalFill> ExternalOrderManager::match(double mid, double bid, double ask) {
    std::vector<ExternalFill> fills;

    // Copy ids to allow map mutation during iteration.
    std::vector<std::string> ids;
    ids.reserve(orders_.size());
    for (const auto& [id, _] : orders_) {
        ids.push_back(id);
    }

    for (const auto& id : ids) {
        auto& order = orders_[id];
        if (order.status != ExtOrderStatus::Live) {
            continue;
        }

        if (order.type == ExtOrderType::Market) {
            const double px = (order.side == ExtSide::Buy) ? ask : bid;
            fillOrder(order, px, fills);
            continue;
        }

        if (order.type == ExtOrderType::Stop && !order.stopTriggered) {
            const bool trigger = (order.side == ExtSide::Buy) ? (mid >= order.stop)
                                                             : (mid <= order.stop);
            if (trigger) {
                order.stopTriggered = true;
                // Become market immediately.
                const double px = (order.side == ExtSide::Buy) ? ask : bid;
                fillOrder(order, px, fills);
            }
            continue;
        }

        if (order.type == ExtOrderType::Limit) {
            if (order.side == ExtSide::Buy && ask <= order.price + 1e-12) {
                fillOrder(order, ask, fills);
            } else if (order.side == ExtSide::Sell && bid + 1e-12 >= order.price) {
                fillOrder(order, bid, fills);
            }
        }
    }
    return fills;
}

}  // namespace mm
