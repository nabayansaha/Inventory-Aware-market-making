#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>

namespace mm {

struct ModelParams {
    double alpha = 5.0;
    double beta = 5.0;
    double gamma = 2.0;
    double phi = 2.0;
    double sigmaOmega = 0.2;
    double sigmaEpsilon = 0.2;
    double psi = 0.05;
    double sigma = 0.5;
    double orderSize = 1.0;
    double initialMid = 100.0;
    double initialInventory = 0.0;
    double initialCash = 0.0;
    double horizon = 10.0;
    double inventoryMin = -20.0;
    double inventoryMax = 20.0;
    double inventoryStep = 1.0;
    int dpSteps = 20;
    int controlResolution = 21;
    std::uint64_t seed = 42;
};

class Model {
public:
    explicit Model(ModelParams params = {}) : params_(std::move(params)) {}

    const ModelParams& params() const { return params_; }
    void setParams(ModelParams params) { params_ = std::move(params); }

    double askIntensity(double a, double omega) const {
        return std::max(params_.alpha - params_.gamma * a + omega, 0.0);
    }

    double bidIntensity(double b, double epsilon) const {
        return std::max(params_.beta + params_.phi * b + epsilon, 0.0);
    }

    bool feasible(double a, double b) const {
        if (a < 0.0 || b > 0.0) {
            return false;
        }
        if (params_.alpha - params_.gamma * a < -1e-12) {
            return false;
        }
        if (params_.beta + params_.phi * b < -1e-12) {
            return false;
        }
        return true;
    }

    double maxAskOffset() const {
        if (params_.gamma <= 0.0) {
            return 0.0;
        }
        return params_.alpha / params_.gamma;
    }

    double minBidOffset() const {
        if (params_.phi <= 0.0) {
            return 0.0;
        }
        return -params_.beta / params_.phi;
    }

    double askPrice(double mid, double a) const { return mid + a; }
    double bidPrice(double mid, double b) const { return mid + b; }
    double spread(double a, double b) const { return a - b; }

    double periodPayoff(double a, double b, double omega, double epsilon) const {
        const double la = askIntensity(a, omega);
        const double lb = bidIntensity(b, epsilon);
        return a * la - b * lb;
    }

    double nextInventory(double inventory, double a, double b, double omega,
                         double epsilon) const {
        const double la = askIntensity(a, omega);
        const double lb = bidIntensity(b, epsilon);
        return inventory + lb - la;
    }

private:
    ModelParams params_;
};

}  // namespace mm
