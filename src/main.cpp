#include "analysis/CsvExport.hpp"
#include "dp/DPSolver.hpp"
#include "model/Model.hpp"
#include "simulation/MarketSimulator.hpp"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

namespace {

void printUsage(const char* argv0) {
    std::cerr << "Usage: " << argv0 << " [--headless] [--csv PATH] [--seed N]\n"
              << "  --headless   Run DP + simulation without UI and exit\n"
              << "  --csv PATH   Write event log CSV (default: simulation.csv)\n"
              << "  --seed N     RNG seed (default: from model params)\n"
              << "  --ui         Launch Dear ImGui trading terminal (default if built)\n";
}

}  // namespace

#if defined(MM_HAS_UI)
#include "ui/TradingTerminal.hpp"
#endif

int main(int argc, char** argv) {
    bool headless = false;
    bool forceUi = false;
    std::string csvPath = "simulation.csv";
    std::uint64_t seedOverride = 0;
    bool hasSeedOverride = false;

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--headless") == 0) {
            headless = true;
        } else if (std::strcmp(argv[i], "--ui") == 0) {
            forceUi = true;
        } else if (std::strcmp(argv[i], "--csv") == 0 && i + 1 < argc) {
            csvPath = argv[++i];
        } else if (std::strcmp(argv[i], "--seed") == 0 && i + 1 < argc) {
            seedOverride = static_cast<std::uint64_t>(std::strtoull(argv[++i], nullptr, 10));
            hasSeedOverride = true;
        } else if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0) {
            printUsage(argv[0]);
            return 0;
        } else {
            printUsage(argv[0]);
            return 1;
        }
    }

#if defined(MM_HAS_UI)
    if (!headless || forceUi) {
        if (forceUi || !headless) {
            return mm::runTradingTerminal(argc, argv);
        }
    }
#else
    (void)forceUi;
    if (!headless) {
        std::cout << "UI not built; running headless. Pass --headless explicitly.\n";
        headless = true;
    }
#endif

    mm::ModelParams params;
    if (hasSeedOverride) {
        params.seed = seedOverride;
    }

    mm::Model model(params);
    mm::DPSolver solver(model, mm::IdentityUtility{},
                        mm::DPConfig{mm::DPResolution::Normal, 5});
    std::cout << "Solving DP policy...\n";
    solver.solve();
    std::cout << "DP solved. Running event-driven simulation...\n";

    mm::MarketSimulator sim(model, solver.policy(), params.seed);
    sim.runToEnd();

    const auto& st = sim.state();
    std::cout << "MARKET CLOSED\n"
              << "  final mid:        " << st.midPrice << '\n'
              << "  final inventory:  " << st.inventory << '\n'
              << "  trades:           " << st.tradeCount << '\n'
              << "  ask hits:         " << st.askHits << '\n'
              << "  bid hits:         " << st.bidHits << '\n'
              << "  realized PnL:     " << st.realizedPnL << '\n'
              << "  total PnL:        " << st.totalPnL << '\n'
              << "  events:           " << sim.events().size() << '\n';

    if (!mm::exportSimulationCsv(csvPath, sim.events())) {
        std::cerr << "Failed to write " << csvPath << '\n';
        return 1;
    }
    std::cout << "Wrote " << csvPath << '\n';
    return 0;
}
