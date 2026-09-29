#pragma once

#include "simulation/MarketEvent.hpp"

#include <vector>

namespace mm {

// size_x/size_y <= 0 means stretch to remaining region.
void drawEventTape(const std::vector<MarketEvent>& events, int maxRows = 120,
                   float size_x = 0.0f, float size_y = 0.0f);

}  // namespace mm
