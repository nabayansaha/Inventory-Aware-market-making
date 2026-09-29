#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <numeric>
#include <vector>

#include "dp/DPSolver.hpp"
#include "model/Model.hpp"
#include "simulation/MarketSimulator.hpp"

using Catch::Approx;

namespace {

mm::Policy makeFlatPolicy(const mm::Model& model) {
    mm::ModelParams p = model.params();
    p.dpSteps = 4;
    p.inventoryMin = -5.0;
    p.inventoryMax = 5.0;
    p.inventoryStep = 1.0;
    p.horizon = 2.0;
    p.controlResolution = 9;
    p.psi = 0.2;
    mm::DPSolver solver(mm::Model(p), mm::IdentityUtility{},
                        mm::DPConfig{mm::DPResolution::Fast, 3});
    solver.solve();
    return solver.policy();
}

}  // namespace

TEST_CASE("Trade accounting ASK and BID hits", "[accounting]") {
    mm::ModelParams params;
    params.initialMid = 100.0;
    params.initialCash = 0.0;
    params.initialInventory = 0.0;
    params.orderSize = 2.0;
    params.horizon = 5.0;
    params.sigma = 0.0;  // freeze mid for accounting clarity
    params.sigmaOmega = 0.0;
    params.sigmaEpsilon = 0.0;
    params.alpha = 5.0;
    params.beta = 5.0;
    params.gamma = 2.0;
    params.phi = 2.0;

    mm::Model model(params);
    auto policy = makeFlatPolicy(model);
    mm::MarketSimulator sim(model, policy, 7);

    // Force a step
    REQUIRE(sim.step());
    const auto& st = sim.state();
    REQUIRE(st.tradeCount == 1);

    if (st.askHits == 1) {
        // inventory -= Q, cash += Q * Ask
        REQUIRE(st.inventory == Approx(-params.orderSize));
        REQUIRE(st.cash == Approx(params.orderSize * st.askPrice));
    } else {
        REQUIRE(st.bidHits == 1);
        REQUIRE(st.inventory == Approx(params.orderSize));
        REQUIRE(st.cash == Approx(-params.orderSize * st.bidPrice));
    }
}

TEST_CASE("Wealth identity", "[accounting]") {
    mm::ModelParams params;
    params.horizon = 3.0;
    params.sigma = 0.3;
    mm::Model model(params);
    auto policy = makeFlatPolicy(model);
    mm::MarketSimulator sim(model, policy, 99);
    sim.runToEnd();
    const auto& st = sim.state();
    REQUIRE(st.wealth() == Approx(st.cash + st.inventory * st.midPrice));
}

TEST_CASE("Poisson mean waiting time approximates 1/lambda", "[poisson]") {
    mm::ModelParams params;
    params.alpha = 4.0;
    params.beta = 0.0;
    params.gamma = 1.0;
    params.phi = 1.0;
    params.sigmaOmega = 0.0;
    params.sigmaEpsilon = 0.0;
    params.horizon = 100.0;
    mm::Model model(params);
    mm::Policy empty;
    mm::MarketSimulator sim(model, empty, 12345);

    const double a = 1.0;
    const double b = 0.0;
    const double la = model.askIntensity(a, 0.0);
    const double lb = model.bidIntensity(b, 0.0);
    const double lam = la + lb;
    REQUIRE(lam > 0.0);

    constexpr int N = 5000;
    double sum = 0.0;
    for (int i = 0; i < N; ++i) {
        sum += sim.sampleWaitingTime(la, lb);
    }
    const double mean = sum / N;
    REQUIRE(mean == Approx(1.0 / lam).margin(0.05));
}

TEST_CASE("Same seed yields identical simulation path", "[seed]") {
    mm::ModelParams params;
    params.horizon = 2.0;
    params.sigma = 0.4;
    mm::Model model(params);
    auto policy = makeFlatPolicy(model);

    mm::MarketSimulator a(model, policy, 4242);
    mm::MarketSimulator b(model, policy, 4242);
    a.runToEnd();
    b.runToEnd();

    REQUIRE(a.events().size() == b.events().size());
    REQUIRE(a.state().cash == Approx(b.state().cash));
    REQUIRE(a.state().inventory == Approx(b.state().inventory));
    REQUIRE(a.state().midPrice == Approx(b.state().midPrice));
    REQUIRE(a.state().totalPnL == Approx(b.state().totalPnL));
}
