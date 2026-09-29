#pragma once

#include "Policy.hpp"
#include "model/Model.hpp"
#include "model/Utility.hpp"

#include <utility>
#include <vector>

namespace mm {

enum class DPResolution {
    Fast,
    Normal,
    High
};

struct DPConfig {
    DPResolution resolution = DPResolution::Normal;
    int gaussHermiteOrder = 5;
};

class DPSolver {
public:
    DPSolver(Model model, IdentityUtility utility, DPConfig config = {})
        : model_(std::move(model)), utility_(std::move(utility)), config_(config) {}

    void setModel(Model model) { model_ = std::move(model); }
    void setConfig(DPConfig config) { config_ = config; }

    const Policy& policy() const { return policy_; }
    bool solved() const { return !policy_.empty(); }

    void solve();

    double getOptimalAskOffset(double time, double inventory) const {
        return policy_.optimalAskOffset(time, inventory);
    }

    double getOptimalBidOffset(double time, double inventory) const {
        return policy_.optimalBidOffset(time, inventory);
    }

    double getValue(double time, double inventory) const {
        return policy_.value(time, inventory);
    }

    double terminalValue(double inventory) const {
        return utility_.evaluate(-model_.params().psi * inventory * inventory);
    }

private:
    int controlPoints() const;
    void buildShockGrid();
    double interpolateValue(const std::vector<double>& values,
                            const std::vector<double>& inventories,
                            double inventory) const;
    double expectedObjective(double inventory, double a, double b,
                             const std::vector<double>& nextValues,
                             const std::vector<double>& inventories) const;

    Model model_;
    IdentityUtility utility_;
    DPConfig config_;
    Policy policy_;

    std::vector<double> omegaNodes_;
    std::vector<double> omegaWeights_;
    std::vector<double> epsilonNodes_;
    std::vector<double> epsilonWeights_;
};

}  // namespace mm
