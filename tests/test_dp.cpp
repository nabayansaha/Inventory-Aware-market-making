#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "dp/DPSolver.hpp"
#include "model/Model.hpp"

using Catch::Approx;

TEST_CASE("DP policies satisfy quote constraints", "[dp]") {
    mm::ModelParams params;
    params.alpha = 4.0;
    params.beta = 4.0;
    params.gamma = 2.0;
    params.phi = 2.0;
    params.psi = 0.1;
    params.sigmaOmega = 0.1;
    params.sigmaEpsilon = 0.1;
    params.horizon = 1.0;
    params.dpSteps = 5;
    params.inventoryMin = -5.0;
    params.inventoryMax = 5.0;
    params.inventoryStep = 1.0;
    params.controlResolution = 11;

    mm::Model model(params);
    mm::DPSolver solver(model, mm::IdentityUtility{},
                        mm::DPConfig{mm::DPResolution::Fast, 3});
    solver.solve();

    REQUIRE(solver.solved());

    for (double I = params.inventoryMin; I <= params.inventoryMax; I += 1.0) {
        for (double t = 0.0; t <= params.horizon; t += 0.25) {
            const double a = solver.getOptimalAskOffset(t, I);
            const double b = solver.getOptimalBidOffset(t, I);
            REQUIRE(model.feasible(a, b));
            REQUIRE(a >= 0.0);
            REQUIRE(b <= 0.0);
        }
    }
}

TEST_CASE("DP inventory skew: long inventory lowers ask offset", "[dp]") {
    mm::ModelParams params;
    params.alpha = 5.0;
    params.beta = 5.0;
    params.gamma = 2.0;
    params.phi = 2.0;
    params.psi = 0.5;
    params.sigmaOmega = 0.05;
    params.sigmaEpsilon = 0.05;
    params.horizon = 2.0;
    params.dpSteps = 8;
    params.inventoryMin = -10.0;
    params.inventoryMax = 10.0;
    params.inventoryStep = 1.0;
    params.controlResolution = 15;

    mm::DPSolver solver(mm::Model(params), mm::IdentityUtility{},
                        mm::DPConfig{mm::DPResolution::Normal, 5});
    solver.solve();

    const double a_long = solver.getOptimalAskOffset(0.0, 8.0);
    const double a_flat = solver.getOptimalAskOffset(0.0, 0.0);
    const double b_long = solver.getOptimalBidOffset(0.0, 8.0);
    const double b_flat = solver.getOptimalBidOffset(0.0, 0.0);

    // Long inventory: prefer selling (smaller/more aggressive ask) and discourage buys
    // (more negative / wider bid). Exact magnitudes depend on grid; check direction.
    REQUIRE(a_long <= a_flat + 1e-9);
    REQUIRE(b_long <= b_flat + 1e-9);
}

TEST_CASE("Terminal condition is quadratic inventory penalty", "[dp]") {
    mm::ModelParams params;
    params.psi = 0.25;
    mm::DPSolver solver(mm::Model(params), mm::IdentityUtility{});
    REQUIRE(solver.terminalValue(2.0) == Approx(-0.25 * 4.0));
    REQUIRE(solver.terminalValue(-3.0) == Approx(-0.25 * 9.0));
}
