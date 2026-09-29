#pragma once

#include "dp/Policy.hpp"
#include "simulation/MarketEvent.hpp"

#include <vector>

namespace mm {

void drawPriceChart(const std::vector<MarketEvent>& events);
void drawPnLChart(const std::vector<MarketEvent>& events);
void drawInventoryChart(const std::vector<MarketEvent>& events);
void drawPolicyChart(const Policy& policy, double time);
void drawHistogram(const char* title, const std::vector<double>& values, int bins = 40);

}  // namespace mm
