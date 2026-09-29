#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <random>

#include "model/Model.hpp"
#include "model/Utility.hpp"
#include "model/PriceProcess.hpp"

using Catch::Approx;

TEST_CASE("Quote construction", "[model]") {
    mm::Model model;
    const double mid = 100.0;
    const double a = 0.5;
    const double b = -0.4;

    REQUIRE(model.askPrice(mid, a) > mid);
    REQUIRE(model.bidPrice(mid, b) < mid);
    REQUIRE(model.bidPrice(mid, b) < model.askPrice(mid, a));
    REQUIRE(model.spread(a, b) == Approx(0.9));
}

TEST_CASE("Arrival intensity monotonicity", "[model]") {
    mm::Model model;
    const double omega = 0.0;
    const double epsilon = 0.0;

    const double la_near = model.askIntensity(0.1, omega);
    const double la_far = model.askIntensity(1.0, omega);
    REQUIRE(la_far <= la_near);

    const double lb_near = model.bidIntensity(-0.1, epsilon);
    const double lb_far = model.bidIntensity(-1.0, epsilon);
    REQUIRE(lb_far <= lb_near);
}

TEST_CASE("Feasible set respects constraints", "[model]") {
    mm::Model model;
    REQUIRE(model.feasible(0.0, 0.0));
    REQUIRE(model.feasible(model.maxAskOffset(), model.minBidOffset()));
    REQUIRE_FALSE(model.feasible(-0.1, 0.0));
    REQUIRE_FALSE(model.feasible(0.0, 0.1));
    REQUIRE_FALSE(model.feasible(model.maxAskOffset() + 1.0, 0.0));
    REQUIRE_FALSE(model.feasible(0.0, model.minBidOffset() - 1.0));
}

TEST_CASE("Period payoff and inventory transition", "[model]") {
    mm::Model model;
    const double a = 0.5;
    const double b = -0.5;
    const double omega = 0.0;
    const double epsilon = 0.0;

    const double la = model.askIntensity(a, omega);
    const double lb = model.bidIntensity(b, epsilon);
    REQUIRE(model.periodPayoff(a, b, omega, epsilon) == Approx(a * la - b * lb));
    REQUIRE(model.nextInventory(3.0, a, b, omega, epsilon) == Approx(3.0 + lb - la));
}

TEST_CASE("Identity utility", "[utility]") {
    mm::IdentityUtility u;
    REQUIRE(u.evaluate(12.5) == Approx(12.5));
}

TEST_CASE("ABM price process is deterministic for fixed seed", "[price]") {
    mm::ArithmeticBrownianMotion abm(0.5);
    std::mt19937_64 rng1(123);
    std::mt19937_64 rng2(123);
    const double p1 = abm.evolve(100.0, 0.25, rng1);
    const double p2 = abm.evolve(100.0, 0.25, rng2);
    REQUIRE(p1 == Approx(p2));
}
