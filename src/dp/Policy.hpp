#pragma once

#include <cmath>
#include <cstddef>
#include <vector>

namespace mm {

class Policy {
public:
    Policy() = default;

    Policy(std::vector<double> times, std::vector<double> inventories,
           std::vector<double> values, std::vector<double> askOffsets,
           std::vector<double> bidOffsets)
        : times_(std::move(times)),
          inventories_(std::move(inventories)),
          values_(std::move(values)),
          askOffsets_(std::move(askOffsets)),
          bidOffsets_(std::move(bidOffsets)) {}

    bool empty() const { return times_.empty() || inventories_.empty(); }

    std::size_t numTimes() const { return times_.size(); }
    std::size_t numInventories() const { return inventories_.size(); }

    const std::vector<double>& times() const { return times_; }
    const std::vector<double>& inventories() const { return inventories_; }

    double optimalAskOffset(double time, double inventory) const {
        return interpolate(askOffsets_, time, inventory);
    }

    double optimalBidOffset(double time, double inventory) const {
        return interpolate(bidOffsets_, time, inventory);
    }

    double value(double time, double inventory) const {
        return interpolate(values_, time, inventory);
    }

    // Slice of a*(I) and b*(I) at nearest time index (for strategy chart).
    void policySliceAtTime(double time, std::vector<double>& askOut,
                           std::vector<double>& bidOut) const {
        askOut.clear();
        bidOut.clear();
        if (empty()) {
            return;
        }
        const std::size_t ti = nearestTimeIndex(time);
        const std::size_t nI = inventories_.size();
        askOut.resize(nI);
        bidOut.resize(nI);
        for (std::size_t i = 0; i < nI; ++i) {
            askOut[i] = askOffsets_[index(ti, i)];
            bidOut[i] = bidOffsets_[index(ti, i)];
        }
    }

private:
    std::size_t index(std::size_t t, std::size_t i) const {
        return t * inventories_.size() + i;
    }

    std::size_t nearestTimeIndex(double time) const {
        if (times_.size() == 1) {
            return 0;
        }
        if (time <= times_.front()) {
            return 0;
        }
        if (time >= times_.back()) {
            return times_.size() - 1;
        }
        std::size_t lo = 0;
        std::size_t hi = times_.size() - 1;
        while (hi - lo > 1) {
            const std::size_t mid = (lo + hi) / 2;
            if (times_[mid] <= time) {
                lo = mid;
            } else {
                hi = mid;
            }
        }
        return (time - times_[lo] <= times_[hi] - time) ? lo : hi;
    }

    static void bracket(const std::vector<double>& grid, double x, std::size_t& i0,
                        std::size_t& i1, double& w) {
        if (grid.size() == 1) {
            i0 = 0;
            i1 = 0;
            w = 0.0;
            return;
        }
        if (x <= grid.front()) {
            i0 = 0;
            i1 = 0;
            w = 0.0;
            return;
        }
        if (x >= grid.back()) {
            i0 = grid.size() - 1;
            i1 = grid.size() - 1;
            w = 0.0;
            return;
        }
        std::size_t lo = 0;
        std::size_t hi = grid.size() - 1;
        while (hi - lo > 1) {
            const std::size_t mid = (lo + hi) / 2;
            if (grid[mid] <= x) {
                lo = mid;
            } else {
                hi = mid;
            }
        }
        i0 = lo;
        i1 = hi;
        const double span = grid[hi] - grid[lo];
        w = (span > 0.0) ? (x - grid[lo]) / span : 0.0;
    }

    double interpolate(const std::vector<double>& field, double time,
                       double inventory) const {
        if (empty()) {
            return 0.0;
        }
        std::size_t t0 = 0;
        std::size_t t1 = 0;
        std::size_t i0 = 0;
        std::size_t i1 = 0;
        double wt = 0.0;
        double wi = 0.0;
        bracket(times_, time, t0, t1, wt);
        bracket(inventories_, inventory, i0, i1, wi);

        const double v00 = field[index(t0, i0)];
        const double v01 = field[index(t0, i1)];
        const double v10 = field[index(t1, i0)];
        const double v11 = field[index(t1, i1)];
        const double v0 = v00 * (1.0 - wi) + v01 * wi;
        const double v1 = v10 * (1.0 - wi) + v11 * wi;
        return v0 * (1.0 - wt) + v1 * wt;
    }

    std::vector<double> times_;
    std::vector<double> inventories_;
    std::vector<double> values_;
    std::vector<double> askOffsets_;
    std::vector<double> bidOffsets_;
};

}  // namespace mm
