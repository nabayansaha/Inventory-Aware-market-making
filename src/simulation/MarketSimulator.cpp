#include "simulation/MarketSimulator.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace mm {

MarketSimulator::MarketSimulator(Model model, Policy policy, std::uint64_t seed)
    : model_(std::move(model)),
      policy_(std::move(policy)),
      priceProcess_(model_.params().sigma),
      seed_(seed),
      rng_(seed) {
    events_.reserve(4096);
    reset(seed);
}

void MarketSimulator::setModel(Model model) {
    model_ = std::move(model);
    priceProcess_.setSigma(model_.params().sigma);
}

void MarketSimulator::reset() { reset(seed_); }

void MarketSimulator::reset(std::uint64_t seed) {
    seed_ = seed;
    rng_.seed(seed_);
    const auto& p = model_.params();
    state_ = MarketMakerState{};
    state_.time = 0.0;
    state_.midPrice = p.initialMid;
    state_.inventory = p.initialInventory;
    state_.cash = p.initialCash;
    state_.maxInventory = state_.inventory;
    state_.minInventory = state_.inventory;
    initialWealth_ = state_.wealth();
    avgEntryMid_ = p.initialMid;
    events_.clear();
    if (events_.capacity() < 4096) {
        events_.reserve(4096);
    }
    refreshQuotes(0.0, 0.0);
    state_.updatePnL(initialWealth_);
}

double MarketSimulator::sampleWaitingTime(double lambdaAsk, double lambdaBid) {
    const double lam = lambdaAsk + lambdaBid;
    if (lam <= 1e-12) {
        return model_.params().horizon;  // no activity
    }
    std::exponential_distribution<double> expDist(lam);
    return expDist(rng_);
}

bool MarketSimulator::sampleAskHit(double lambdaAsk, double lambdaBid) {
    const double lam = lambdaAsk + lambdaBid;
    if (lam <= 1e-12) {
        return true;
    }
    std::uniform_real_distribution<double> uni(0.0, 1.0);
    return uni(rng_) < (lambdaAsk / lam);
}

void MarketSimulator::refreshQuotes(double omega, double epsilon) {
    const double t = state_.time;
    const double I = state_.inventory;
    state_.askOffset = policy_.empty() ? 0.0 : policy_.optimalAskOffset(t, I);
    state_.bidOffset = policy_.empty() ? 0.0 : policy_.optimalBidOffset(t, I);

    // Clamp to feasible set if interpolation drifts slightly.
    if (!model_.feasible(state_.askOffset, state_.bidOffset)) {
        state_.askOffset = std::clamp(state_.askOffset, 0.0, model_.maxAskOffset());
        state_.bidOffset = std::clamp(state_.bidOffset, model_.minBidOffset(), 0.0);
    }

    state_.askPrice = model_.askPrice(state_.midPrice, state_.askOffset);
    state_.bidPrice = model_.bidPrice(state_.midPrice, state_.bidOffset);
    state_.lambdaAsk = model_.askIntensity(state_.askOffset, omega);
    state_.lambdaBid = model_.bidIntensity(state_.bidOffset, epsilon);
    state_.spreadSum += state_.askPrice - state_.bidPrice;
    ++state_.quoteUpdates;
}

void MarketSimulator::pushEvent(EventType type, double tradePrice, double tradeSize,
                                bool external) {
    MarketEvent e;
    e.timestamp = state_.time;
    e.type = type;
    e.midPrice = state_.midPrice;
    e.bidPrice = state_.bidPrice;
    e.askPrice = state_.askPrice;
    e.tradePrice = tradePrice;
    e.tradeSize = tradeSize;
    e.inventoryAfter = state_.inventory;
    e.cashAfter = state_.cash;
    e.lambdaAsk = state_.lambdaAsk;
    e.lambdaBid = state_.lambdaBid;
    e.realizedPnL = state_.realizedPnL;
    e.inventoryPnL = state_.inventoryPnL;
    e.totalPnL = state_.totalPnL;
    e.askOffset = state_.askOffset;
    e.bidOffset = state_.bidOffset;
    e.external = external;
    events_.push_back(e);
}

void MarketSimulator::executeAskHit(double qty, bool external) {
    const double tradePrice = state_.askPrice;
    state_.realizedPnL += qty * (tradePrice - state_.midPrice);
    state_.inventory -= qty;
    state_.cash += qty * tradePrice;
    ++state_.tradeCount;
    ++state_.askHits;
    state_.noteInventoryExtremes();
    state_.updatePnL(initialWealth_);
    pushEvent(EventType::AskHit, tradePrice, qty, external);
}

void MarketSimulator::executeBidHit(double qty, bool external) {
    const double tradePrice = state_.bidPrice;
    state_.realizedPnL += qty * (state_.midPrice - tradePrice);
    state_.inventory += qty;
    state_.cash -= qty * tradePrice;
    ++state_.tradeCount;
    ++state_.bidHits;
    state_.noteInventoryExtremes();
    state_.updatePnL(initialWealth_);
    pushEvent(EventType::BidHit, tradePrice, qty, external);
}

void MarketSimulator::applyExternalBuy(double qty) { executeAskHit(qty, true); }

void MarketSimulator::applyExternalSell(double qty) { executeBidHit(qty, true); }

bool MarketSimulator::step() {
    if (finished()) {
        return false;
    }

    std::normal_distribution<double> shockOmega(0.0, model_.params().sigmaOmega);
    std::normal_distribution<double> shockEpsilon(0.0, model_.params().sigmaEpsilon);
    const double omega = shockOmega(rng_);
    const double epsilon = shockEpsilon(rng_);
    refreshQuotes(omega, epsilon);

    double dt = sampleWaitingTime(state_.lambdaAsk, state_.lambdaBid);
    if (state_.time + dt > model_.params().horizon) {
        dt = model_.params().horizon - state_.time;
        state_.midPrice = priceProcess_.evolve(state_.midPrice, dt, rng_);
        state_.time = model_.params().horizon;
        state_.askPrice = model_.askPrice(state_.midPrice, state_.askOffset);
        state_.bidPrice = model_.bidPrice(state_.midPrice, state_.bidOffset);
        state_.updatePnL(initialWealth_);
        pushEvent(EventType::MidMove, 0.0, 0.0, false);
        return false;
    }

    state_.midPrice = priceProcess_.evolve(state_.midPrice, dt, rng_);
    state_.time += dt;
    state_.askPrice = model_.askPrice(state_.midPrice, state_.askOffset);
    state_.bidPrice = model_.bidPrice(state_.midPrice, state_.bidOffset);

    const double qty = model_.params().orderSize;
    if (sampleAskHit(state_.lambdaAsk, state_.lambdaBid)) {
        executeAskHit(qty, false);
    } else {
        executeBidHit(qty, false);
    }

    // Recompute quotes after inventory change for next interval.
    refreshQuotes(0.0, 0.0);
    state_.updatePnL(initialWealth_);
    return !finished();
}

void MarketSimulator::runToEnd() {
    while (step()) {
    }
}

}  // namespace mm
