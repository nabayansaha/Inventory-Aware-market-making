#pragma once

#include <cmath>
#include <random>

namespace mm {

class PriceProcess {
public:
    virtual ~PriceProcess() = default;
    virtual double evolve(double currentPrice, double dt, std::mt19937_64& rng) = 0;
};

class ArithmeticBrownianMotion final : public PriceProcess {
public:
    explicit ArithmeticBrownianMotion(double sigma) : sigma_(sigma) {}

    double sigma() const { return sigma_; }
    void setSigma(double sigma) { sigma_ = sigma; }

    double evolve(double currentPrice, double dt, std::mt19937_64& rng) override {
        if (dt <= 0.0) {
            return currentPrice;
        }
        std::normal_distribution<double> normal(0.0, 1.0);
        const double z = normal(rng);
        return currentPrice + sigma_ * std::sqrt(dt) * z;
    }

private:
    double sigma_;
};

}  // namespace mm
