#include "dp/DPSolver.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace mm {
namespace {

// Physicists' Gauss-Hermite nodes/weights for integral e^{-x^2} f(x) dx.
// For E[f(Z)], Z~N(0,1): (1/sqrt(pi)) sum w_i f(sqrt(2) x_i)
void gaussHermite(int order, std::vector<double>& nodes, std::vector<double>& weights) {
    nodes.clear();
    weights.clear();
    if (order == 3) {
        nodes = {-1.224744871391589, 0.0, 1.224744871391589};
        weights = {0.295408975150919, 1.181635900603677, 0.295408975150919};
    } else if (order == 5) {
        nodes = {-2.020182870456086, -0.9585724646138185, 0.0, 0.9585724646138185,
                 2.020182870456086};
        weights = {0.01995324205904591, 0.3936193231522411, 0.9453087204829419,
                   0.3936193231522411, 0.01995324205904591};
    } else if (order == 7) {
        nodes = {-2.651961356835233, -1.673551628767471, -0.8162878828589647, 0.0,
                 0.8162878828589647, 1.673551628767471, 2.651961356835233};
        weights = {0.0009717812450995192, 0.05451558281912703, 0.4256072526101276,
                   0.8102646175568073, 0.4256072526101276, 0.05451558281912703,
                   0.0009717812450995192};
    } else {
        throw std::invalid_argument("Unsupported Gauss-Hermite order");
    }
}

void scaleNormalShocks(double sigma, const std::vector<double>& ghNodes,
                       const std::vector<double>& ghWeights, std::vector<double>& outNodes,
                       std::vector<double>& outWeights) {
    outNodes.resize(ghNodes.size());
    outWeights.resize(ghWeights.size());
    const double invSqrtPi = 1.0 / std::sqrt(std::acos(-1.0));
    for (std::size_t i = 0; i < ghNodes.size(); ++i) {
        outNodes[i] = sigma * std::sqrt(2.0) * ghNodes[i];
        outWeights[i] = invSqrtPi * ghWeights[i];
    }
}

}  // namespace

int DPSolver::controlPoints() const {
    switch (config_.resolution) {
        case DPResolution::Fast:
            return 11;
        case DPResolution::High:
            return 41;
        case DPResolution::Normal:
        default:
            return std::max(5, model_.params().controlResolution);
    }
}

void DPSolver::buildShockGrid() {
    std::vector<double> ghNodes;
    std::vector<double> ghWeights;
    gaussHermite(config_.gaussHermiteOrder, ghNodes, ghWeights);
    scaleNormalShocks(model_.params().sigmaOmega, ghNodes, ghWeights, omegaNodes_,
                      omegaWeights_);
    scaleNormalShocks(model_.params().sigmaEpsilon, ghNodes, ghWeights, epsilonNodes_,
                      epsilonWeights_);
}

double DPSolver::interpolateValue(const std::vector<double>& values,
                                  const std::vector<double>& inventories,
                                  double inventory) const {
    if (inventories.empty()) {
        return 0.0;
    }
    if (inventory <= inventories.front()) {
        return values.front();
    }
    if (inventory >= inventories.back()) {
        return values.back();
    }
    std::size_t lo = 0;
    std::size_t hi = inventories.size() - 1;
    while (hi - lo > 1) {
        const std::size_t mid = (lo + hi) / 2;
        if (inventories[mid] <= inventory) {
            lo = mid;
        } else {
            hi = mid;
        }
    }
    const double span = inventories[hi] - inventories[lo];
    const double w = (span > 0.0) ? (inventory - inventories[lo]) / span : 0.0;
    return values[lo] * (1.0 - w) + values[hi] * w;
}

double DPSolver::expectedObjective(double inventory, double a, double b,
                                   const std::vector<double>& nextValues,
                                   const std::vector<double>& inventories) const {
    double expectation = 0.0;
    for (std::size_t i = 0; i < omegaNodes_.size(); ++i) {
        for (std::size_t j = 0; j < epsilonNodes_.size(); ++j) {
            const double omega = omegaNodes_[i];
            const double epsilon = epsilonNodes_[j];
            const double weight = omegaWeights_[i] * epsilonWeights_[j];
            const double payoff = model_.periodPayoff(a, b, omega, epsilon);
            const double nextI = model_.nextInventory(inventory, a, b, omega, epsilon);
            const double cont = interpolateValue(nextValues, inventories, nextI);
            expectation += weight * utility_.evaluate(payoff + cont);
        }
    }
    return expectation;
}

void DPSolver::solve() {
    const auto& p = model_.params();
    if (p.dpSteps < 1) {
        throw std::invalid_argument("dpSteps must be >= 1");
    }
    if (p.inventoryStep <= 0.0 || p.inventoryMax < p.inventoryMin) {
        throw std::invalid_argument("invalid inventory grid");
    }

    buildShockGrid();

    std::vector<double> inventories;
    for (double i = p.inventoryMin; i <= p.inventoryMax + 1e-12; i += p.inventoryStep) {
        inventories.push_back(i);
    }
    const std::size_t nI = inventories.size();
    const int N = p.dpSteps;

    std::vector<double> times(static_cast<std::size_t>(N));
    const double dt = p.horizon / static_cast<double>(N);
    for (int n = 0; n < N; ++n) {
        times[static_cast<std::size_t>(n)] = n * dt;
    }

    std::vector<double> values(times.size() * nI, 0.0);
    std::vector<double> askOffsets(times.size() * nI, 0.0);
    std::vector<double> bidOffsets(times.size() * nI, 0.0);

    std::vector<double> nextValues(nI, 0.0);
    for (std::size_t i = 0; i < nI; ++i) {
        nextValues[i] = terminalValue(inventories[i]);
    }

    const int points = controlPoints();
    const double aMax = model_.maxAskOffset();
    const double bMin = model_.minBidOffset();

    std::vector<double> aGrid(static_cast<std::size_t>(points));
    std::vector<double> bGrid(static_cast<std::size_t>(points));
    for (int k = 0; k < points; ++k) {
        const double t = (points == 1) ? 0.0 : static_cast<double>(k) / (points - 1);
        aGrid[static_cast<std::size_t>(k)] = t * aMax;
        bGrid[static_cast<std::size_t>(k)] = t * bMin;  // 0 down to bMin
    }

    std::vector<double> currentValues(nI, 0.0);

    for (int n = N - 1; n >= 0; --n) {
        for (std::size_t ii = 0; ii < nI; ++ii) {
            const double I = inventories[ii];
            double best = -std::numeric_limits<double>::infinity();
            double bestA = 0.0;
            double bestB = 0.0;

            for (double a : aGrid) {
                for (double b : bGrid) {
                    if (!model_.feasible(a, b)) {
                        continue;
                    }
                    const double obj =
                        expectedObjective(I, a, b, nextValues, inventories);
                    if (obj > best) {
                        best = obj;
                        bestA = a;
                        bestB = b;
                    }
                }
            }

            if (!std::isfinite(best)) {
                best = expectedObjective(I, 0.0, 0.0, nextValues, inventories);
                bestA = 0.0;
                bestB = 0.0;
            }

            currentValues[ii] = best;
            const std::size_t idx =
                static_cast<std::size_t>(n) * nI + ii;
            values[idx] = best;
            askOffsets[idx] = bestA;
            bidOffsets[idx] = bestB;
        }
        nextValues = currentValues;
    }

    policy_ = Policy(std::move(times), std::move(inventories), std::move(values),
                     std::move(askOffsets), std::move(bidOffsets));
}

}  // namespace mm
