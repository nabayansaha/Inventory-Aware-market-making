#pragma once

#include "simulation/MarketMakerState.hpp"

namespace mm {

void drawOrderBook(const MarketMakerState& state, float askFlash, float bidFlash);

}  // namespace mm
