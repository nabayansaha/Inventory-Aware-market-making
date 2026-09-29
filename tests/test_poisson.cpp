#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "model/Model.hpp"
#include "simulation/MarketSimulator.hpp"

using Catch::Approx;

TEST_CASE("Poisson waiting time distribution mean", "[poisson]") {
    mm::Model model;
    mm::MarketSimulator sim(model, mm::Policy{}, 1);
    const double lambda = 3.5;
    double sum = 0.0;
    constexpr int N = 8000;
    for (int i = 0; i < N; ++i) {
        sum += sim.sampleWaitingTime(lambda, 0.0);
    }
    REQUIRE(sum / N == Approx(1.0 / lambda).margin(0.04));
}
