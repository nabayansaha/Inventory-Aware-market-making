#pragma once

#include "simulation/MarketEvent.hpp"
#include "simulation/MarketMakerState.hpp"

#include <mutex>
#include <vector>

namespace mm {

void drawEventTape(const std::vector<MarketEvent>& events, int maxRows = 40);

}  // namespace mm
