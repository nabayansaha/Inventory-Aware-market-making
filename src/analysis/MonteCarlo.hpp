#pragma once

#include "analysis/Statistics.hpp"
#include "dp/DPSolver.hpp"
#include "dp/Policy.hpp"
#include "model/Model.hpp"
#include "simulation/MarketSimulator.hpp"

#include <cmath>
#include <cstdint>
#include <functional>
#include <vector>

namespace mm {

struct MonteCarloResult {
    std::vector<double> pnl;
    std::vector<double> finalInventory;
    std::vector<double> maxAbsInventory;
    SummaryStats pnlStats{};
    SummaryStats inventoryStats{};
    double meanMaxAbsInventory = 0.0;
};

inline MonteCarloResult runMonteCarlo(const ModelParams& params, const Policy& policy,
                                      int runs, std::uint64_t baseSeed,
                                      const std::function<void(int, int)>& onProgress = {}) {
    MonteCarloResult result;
    result.pnl.reserve(static_cast<std::size_t>(runs));
    result.finalInventory.reserve(static_cast<std::size_t>(runs));
    result.maxAbsInventory.reserve(static_cast<std::size_t>(runs));

    Policy usePolicy = policy;
    if (usePolicy.empty()) {
        DPSolver solver(Model(params), IdentityUtility{},
                        DPConfig{DPResolution::Fast, 3});
        solver.solve();
        usePolicy = solver.policy();
    }

    for (int i = 0; i < runs; ++i) {
        MarketSimulator sim(Model(params), usePolicy,
                            baseSeed + static_cast<std::uint64_t>(i));
        sim.runToEnd();
        result.pnl.push_back(sim.state().totalPnL);
        result.finalInventory.push_back(sim.state().inventory);
        result.maxAbsInventory.push_back(
            std::max(std::abs(sim.state().maxInventory), std::abs(sim.state().minInventory)));
        if (onProgress) {
            onProgress(i + 1, runs);
        }
    }

    result.pnlStats = summarize(result.pnl);
    result.inventoryStats = summarize(result.finalInventory);
    double sum = 0.0;
    for (double v : result.maxAbsInventory) {
        sum += v;
    }
    result.meanMaxAbsInventory =
        result.maxAbsInventory.empty()
            ? 0.0
            : sum / static_cast<double>(result.maxAbsInventory.size());
    return result;
}

}  // namespace mm
