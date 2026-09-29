#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "simulation/ExternalOrderManager.hpp"

using Catch::Approx;

TEST_CASE("External market order fills immediately", "[oms]") {
    mm::ExternalOrderManager oms;
    mm::ExternalOrder o;
    o.id = "m1";
    o.type = mm::ExtOrderType::Market;
    o.side = mm::ExtSide::Buy;
    o.size = 2.0;
    REQUIRE(oms.place(o).empty());
    auto fills = oms.match(100.0, 99.5, 100.5);
    REQUIRE(fills.size() == 1);
    REQUIRE(fills[0].price == Approx(100.5));
    REQUIRE(fills[0].size == Approx(2.0));
}

TEST_CASE("External limit waits until ask crosses", "[oms]") {
    mm::ExternalOrderManager oms;
    mm::ExternalOrder o;
    o.id = "l1";
    o.type = mm::ExtOrderType::Limit;
    o.side = mm::ExtSide::Buy;
    o.price = 100.2;
    o.size = 1.0;
    REQUIRE(oms.place(o).empty());
    REQUIRE(oms.match(100.0, 99.5, 100.5).empty());
    auto fills = oms.match(100.0, 99.5, 100.1);
    REQUIRE(fills.size() == 1);
    REQUIRE(fills[0].price == Approx(100.1));
}

TEST_CASE("External sell stop triggers on mid drop", "[oms]") {
    mm::ExternalOrderManager oms;
    mm::ExternalOrder o;
    o.id = "s1";
    o.type = mm::ExtOrderType::Stop;
    o.side = mm::ExtSide::Sell;
    o.stop = 99.0;
    o.size = 1.0;
    REQUIRE(oms.place(o).empty());
    REQUIRE(oms.match(100.0, 99.5, 100.5).empty());
    auto fills = oms.match(98.5, 98.0, 99.0);
    REQUIRE(fills.size() == 1);
    REQUIRE(fills[0].side == mm::ExtSide::Sell);
    REQUIRE(fills[0].price == Approx(98.0));
}
