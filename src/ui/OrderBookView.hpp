#pragma once

#include "simulation/MarketMakerState.hpp"

namespace mm {

void drawOrderBook(const MarketMakerState& state, float askFlash, float bidFlash,
                   float height = 320.0f);

}  // namespace mm
