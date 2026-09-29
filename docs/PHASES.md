# Phase 0

Specifications only. See MODEL.md and docs/model_mapping.md.

# Build verification checklist

- [x] Project builds on macOS with CMake
- [x] Unit tests pass (`ctest`)
- [x] Headless simulation emits Poisson events and CSV
- [x] Trades update inventory and cash
- [x] DP policies change with inventory
- [x] UI consumes live MarketEvent stream
- [x] Pause / resume / reset / replay
- [x] Monte Carlo tab runs on a worker thread

