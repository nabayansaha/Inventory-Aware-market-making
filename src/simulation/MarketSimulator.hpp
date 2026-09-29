#pragma once

#include "dp/DPSolver.hpp"
#include "model/Model.hpp"
#include "model/PriceProcess.hpp"
#include "simulation/MarketEvent.hpp"
#include "simulation/MarketMakerState.hpp"

#include <cstdint>
#include <cstddef>
#include <random>
#include <vector>

namespace mm {

class MarketSimulator {
public:
    MarketSimulator(Model model, Policy policy, std::uint64_t seed);

    void setPolicy(Policy policy) { policy_ = std::move(policy); }
    void setModel(Model model);
    void reset();
    void reset(std::uint64_t seed);

    // Advance until next trade event or horizon. Returns false if finished.
    bool step();

    // Run to horizon, collecting events.
    void runToEnd();

    bool finished() const { return state_.time >= model_.params().horizon - 1e-15; }

    const MarketMakerState& state() const { return state_; }
    const std::vector<MarketEvent>& events() const { return events_; }
    std::vector<MarketEvent>& eventsMutable() { return events_; }

    double initialWealth() const { return initialWealth_; }
    std::uint64_t seed() const { return seed_; }

    // Exposed for tests.
    double sampleWaitingTime(double lambdaAsk, double lambdaBid);
    bool sampleAskHit(double lambdaAsk, double lambdaBid);

    // External trader: BUY hits ask, SELL hits bid.
    void applyExternalBuy(double qty);
    void applyExternalSell(double qty);

private:
    void refreshQuotes(double omega, double epsilon);
    void executeAskHit(double qty, bool external);
    void executeBidHit(double qty, bool external);
    void pushEvent(EventType type, double tradePrice, double tradeSize, bool external);

    Model model_;
    Policy policy_;
    ArithmeticBrownianMotion priceProcess_;
    std::uint64_t seed_ = 0;
    std::mt19937_64 rng_;
    MarketMakerState state_;
    std::vector<MarketEvent> events_;
    double initialWealth_ = 0.0;
    double avgEntryMid_ = 0.0;  // for realized attribution helper
};

}  // namespace mm
