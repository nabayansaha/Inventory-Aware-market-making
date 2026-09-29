#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "analysis/MonteCarlo.hpp"
#include "dp/DPSolver.hpp"
#include "model/Model.hpp"

TEST_CASE("Monte Carlo reuses one policy across seeds", "[montecarlo]") {
    mm::ModelParams params;
    params.horizon = 1.0;
    params.dpSteps = 4;
    params.inventoryMin = -5.0;
    params.inventoryMax = 5.0;
    params.inventoryStep = 1.0;
    params.controlResolution = 7;
    params.sigma = 0.2;
    params.psi = 0.2;

    mm::DPSolver solver(mm::Model(params), mm::IdentityUtility{},
                        mm::DPConfig{mm::DPResolution::Fast, 3});
    solver.solve();

    const auto result = mm::runMonteCarlo(params, solver.policy(), 25, 100);
    REQUIRE(result.pnl.size() == 25);
    REQUIRE(result.finalInventory.size() == 25);
    REQUIRE(std::isfinite(result.pnlStats.mean));
    REQUIRE(result.pnlStats.p05 <= result.pnlStats.median);
    REQUIRE(result.pnlStats.median <= result.pnlStats.p95);
}
