#include "ui/TradingTerminal.hpp"

#include "analysis/CsvExport.hpp"
#include "analysis/MonteCarlo.hpp"
#include "analysis/Statistics.hpp"
#include "dp/DPSolver.hpp"
#include "model/Model.hpp"
#include "simulation/MarketSimulator.hpp"
#include "simulation/TraderBridge.hpp"
#include "ui/Charts.hpp"
#include "ui/EventTape.hpp"
#include "ui/OrderBookView.hpp"

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <implot.h>

#if defined(__APPLE__)
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace mm {
namespace {

struct AppState {
    ModelParams draftParams{};
    ModelParams appliedParams{};
    Model model{appliedParams};
    Policy policy;
    std::unique_ptr<MarketSimulator> sim;

    bool running = false;
    bool paused = true;
    bool finished = false;
    float speed = 1.0f;
    int dpResolution = 1;  // 0 fast, 1 normal, 2 high

    float askFlash = 0.0f;
    float bidFlash = 0.0f;

    std::mutex mu;
    std::vector<MarketEvent> uiEvents;
    MarketMakerState uiState{};

    // Worker for DP / Monte Carlo
    std::atomic<bool> workerBusy{false};
    std::atomic<bool> workerDone{false};
    std::string workerStatus;
    std::mutex workerMu;

    // Monte Carlo results
    std::vector<double> mcPnL;
    std::vector<double> mcInventory;
    std::vector<double> mcMaxAbsInventory;
    SummaryStats mcPnLStats{};
    SummaryStats mcInvStats{};
    int mcRuns = 200;
    double mcMeanMaxAbsInv = 0.0;

    std::string csvPath = "simulation.csv";
    std::string statusLine = "MARKET MAKER ONLINE";

    // External Python trader bridge
    bool traderPortEnabled = false;
    int traderPort = 8765;
    std::unique_ptr<TraderBridge> traderBridge;
    int traderLiveOrders = 0;
    double traderCash = 0.0;
    double traderInventory = 0.0;
    double traderPnL = 0.0;
};

void applyDarkTheme() {
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 4.0f;
    style.FrameRounding = 3.0f;
    style.ScrollbarRounding = 3.0f;
    style.WindowBorderSize = 1.0f;
    ImVec4* c = style.Colors;
    c[ImGuiCol_WindowBg] = ImVec4(0.07f, 0.08f, 0.10f, 1.0f);
    c[ImGuiCol_ChildBg] = ImVec4(0.09f, 0.10f, 0.13f, 1.0f);
    c[ImGuiCol_FrameBg] = ImVec4(0.12f, 0.14f, 0.18f, 1.0f);
    c[ImGuiCol_Button] = ImVec4(0.16f, 0.35f, 0.28f, 1.0f);
    c[ImGuiCol_ButtonHovered] = ImVec4(0.20f, 0.45f, 0.35f, 1.0f);
    c[ImGuiCol_Header] = ImVec4(0.14f, 0.22f, 0.30f, 1.0f);
    c[ImGuiCol_TitleBg] = ImVec4(0.06f, 0.07f, 0.09f, 1.0f);
    c[ImGuiCol_TitleBgActive] = ImVec4(0.08f, 0.10f, 0.14f, 1.0f);
    c[ImGuiCol_Text] = ImVec4(0.90f, 0.92f, 0.95f, 1.0f);
}

DPResolution toResolution(int idx) {
    if (idx <= 0) {
        return DPResolution::Fast;
    }
    if (idx >= 2) {
        return DPResolution::High;
    }
    return DPResolution::Normal;
}

void solveDpAsync(AppState& app) {
    if (app.workerBusy.exchange(true)) {
        return;
    }
    {
        std::lock_guard<std::mutex> lock(app.workerMu);
        app.workerStatus = "Solving DP...";
    }
    app.workerDone = false;
    std::thread([&app]() {
        ModelParams params;
        int resIdx = 0;
        {
            std::lock_guard<std::mutex> lock(app.mu);
            params = app.appliedParams;
            resIdx = app.dpResolution;
        }
        Model model(params);
        DPSolver solver(model, IdentityUtility{},
                        DPConfig{toResolution(resIdx), resIdx == 0 ? 3 : 5});
        solver.solve();
        {
            std::lock_guard<std::mutex> lock(app.mu);
            app.model = model;
            app.policy = solver.policy();
            app.sim = std::make_unique<MarketSimulator>(app.model, app.policy, params.seed);
            app.uiEvents.clear();
            app.uiState = app.sim->state();
            app.finished = false;
            app.paused = true;
            app.running = false;
            app.statusLine = "DP READY - press START";
        }
        {
            std::lock_guard<std::mutex> lock(app.workerMu);
            app.workerStatus = "DP solved";
        }
        app.workerDone = true;
        app.workerBusy = false;
    }).detach();
}

void runMonteCarloAsync(AppState& app) {
    if (app.workerBusy.exchange(true)) {
        return;
    }
    {
        std::lock_guard<std::mutex> lock(app.workerMu);
        app.workerStatus = "Running Monte Carlo...";
    }
    std::thread([&app]() {
        Policy policy;
        ModelParams params;
        int runs = 0;
        {
            std::lock_guard<std::mutex> lock(app.mu);
            policy = app.policy;
            params = app.appliedParams;
            runs = app.mcRuns;
        }

        auto result = runMonteCarlo(params, policy, runs, params.seed, [&](int done, int total) {
            if (done % 50 == 0 || done == total) {
                std::lock_guard<std::mutex> lock(app.workerMu);
                app.workerStatus =
                    "Monte Carlo " + std::to_string(done) + "/" + std::to_string(total);
            }
        });

        {
            std::lock_guard<std::mutex> lock(app.mu);
            app.mcPnL = std::move(result.pnl);
            app.mcInventory = std::move(result.finalInventory);
            app.mcMaxAbsInventory = std::move(result.maxAbsInventory);
            app.mcPnLStats = result.pnlStats;
            app.mcInvStats = result.inventoryStats;
            app.mcMeanMaxAbsInv = result.meanMaxAbsInventory;
        }
        {
            std::lock_guard<std::mutex> lock(app.workerMu);
            app.workerStatus = "Monte Carlo complete";
        }
        app.workerBusy = false;
    }).detach();
}

void syncUiFromSim(AppState& app) {
    if (!app.sim) {
        return;
    }
    app.uiState = app.sim->state();
    // Append-only sync: avoid full vector copy each frame when possible.
    const auto& src = app.sim->events();
    if (app.uiEvents.size() > src.size()) {
        app.uiEvents = src;
    } else if (app.uiEvents.size() < src.size()) {
        app.uiEvents.insert(app.uiEvents.end(), src.begin() + static_cast<std::ptrdiff_t>(app.uiEvents.size()),
                            src.end());
    }
}

void advanceSimulation(AppState& app, double wallDt) {
    if (!app.sim || app.paused || app.finished || app.workerBusy) {
        return;
    }
    const double simBudget = wallDt * static_cast<double>(app.speed);
    double advanced = 0.0;
    const double t0 = app.sim->state().time;
    int guard = 0;
    while (advanced < simBudget && guard++ < 500) {
        const auto prevHitsAsk = app.sim->state().askHits;
        const auto prevHitsBid = app.sim->state().bidHits;
        if (!app.sim->step()) {
            app.finished = true;
            app.paused = true;
            app.statusLine = "MARKET CLOSED";
            break;
        }
        if (app.sim->state().askHits > prevHitsAsk) {
            app.askFlash = 1.0f;
        }
        if (app.sim->state().bidHits > prevHitsBid) {
            app.bidFlash = 1.0f;
        }
        advanced = app.sim->state().time - t0;
    }
    syncUiFromSim(app);
}

void drawParams(AppState& app) {
    ImGui::BeginChild("ParametersPane", ImVec2(0, 420), true);
    ImGui::TextUnformatted("PARAMETERS");
    ImGui::Separator();
    auto& p = app.draftParams;
    ImGui::InputDouble("Initial Mid", &p.initialMid);
    ImGui::InputDouble("Initial Inventory", &p.initialInventory);
    ImGui::InputDouble("Initial Cash", &p.initialCash);
    ImGui::Separator();
    ImGui::InputDouble("alpha", &p.alpha);
    ImGui::InputDouble("beta", &p.beta);
    ImGui::InputDouble("gamma", &p.gamma);
    ImGui::InputDouble("phi", &p.phi);
    ImGui::InputDouble("sigma (mid)", &p.sigma);
    ImGui::InputDouble("sigma omega", &p.sigmaOmega);
    ImGui::InputDouble("sigma epsilon", &p.sigmaEpsilon);
    ImGui::InputDouble("psi (terminal)", &p.psi);
    ImGui::InputDouble("order size Q", &p.orderSize);
    ImGui::InputDouble("horizon T", &p.horizon);
    ImGui::Separator();
    ImGui::InputDouble("I min", &p.inventoryMin);
    ImGui::InputDouble("I max", &p.inventoryMax);
    ImGui::InputDouble("dI", &p.inventoryStep);
    ImGui::InputInt("DP steps", &p.dpSteps);
    ImGui::InputInt("Control resolution", &p.controlResolution);
    ImGui::InputScalar("seed", ImGuiDataType_U64, &p.seed);
    ImGui::Combo("DP resolution", &app.dpResolution, "Fast\0Normal\0High\0");

    const bool busy = app.workerBusy.load();
    if (busy) {
        ImGui::BeginDisabled();
    }
    if (ImGui::Button("APPLY / RESET", ImVec2(-1, 0))) {
        {
            std::lock_guard<std::mutex> lock(app.mu);
            app.appliedParams = app.draftParams;
        }
        solveDpAsync(app);
    }
    if (busy) {
        ImGui::EndDisabled();
    }
    ImGui::EndChild();
}

void drawControls(AppState& app) {
    ImGui::BeginChild("ControlsPane", ImVec2(0, 0), true);
    ImGui::TextUnformatted("CONTROLS");
    ImGui::Separator();
    if (ImGui::Button("START")) {
        if (app.sim) {
            app.paused = false;
            app.running = true;
            app.statusLine = "MARKET MAKER ONLINE";
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("PAUSE")) {
        app.paused = true;
        app.statusLine = "PAUSED";
    }
    ImGui::SameLine();
    if (ImGui::Button("RESET")) {
        if (app.sim) {
            app.sim->reset(app.appliedParams.seed);
            syncUiFromSim(app);
            app.finished = false;
            app.paused = true;
            app.askFlash = 0.0f;
            app.bidFlash = 0.0f;
            app.statusLine = "RESET - press START";
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("REPLAY")) {
        if (app.sim) {
            app.sim->reset(app.appliedParams.seed);
            syncUiFromSim(app);
            app.finished = false;
            app.paused = false;
            app.running = true;
            app.statusLine = "REPLAY";
        }
    }

    ImGui::TextUnformatted("Simulation Speed");
    const float speeds[] = {0.25f, 1.0f, 5.0f, 20.0f, 100.0f};
    const char* labels[] = {"0.25x", "1x", "5x", "20x", "100x"};
    for (int i = 0; i < 5; ++i) {
        if (i > 0) {
            ImGui::SameLine();
        }
        const bool selected = std::abs(app.speed - speeds[i]) < 1e-6f;
        if (selected) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f, 0.55f, 0.40f, 1.0f));
        }
        if (ImGui::Button(labels[i])) {
            app.speed = speeds[i];
        }
        if (selected) {
            ImGui::PopStyleColor();
        }
    }

    if (ImGui::Button("Export CSV")) {
        if (app.sim) {
            exportSimulationCsv(app.csvPath, app.sim->events());
            app.statusLine = "Wrote " + app.csvPath;
        }
    }

    ImGui::Separator();
    ImGui::TextUnformatted("EXTERNAL TRADER");
    ImGui::InputInt("Port", &app.traderPort);
    if (app.traderPort < 1) {
        app.traderPort = 8765;
    }
    if (!app.traderPortEnabled) {
        if (ImGui::Button("Enable Trader Port")) {
            TraderBridgeConfig cfg;
            cfg.port = static_cast<std::uint16_t>(app.traderPort);
            app.traderBridge = std::make_unique<TraderBridge>(cfg);
            if (app.traderBridge->start()) {
                app.traderPortEnabled = true;
                app.statusLine = "Trader port " + std::to_string(app.traderPort) + " listening";
            } else {
                app.traderBridge.reset();
                app.statusLine = "Failed to bind trader port";
            }
        }
    } else {
        if (ImGui::Button("Disable Trader Port")) {
            if (app.traderBridge) {
                app.traderBridge->stop();
            }
            app.traderBridge.reset();
            app.traderPortEnabled = false;
            app.statusLine = "Trader port closed";
        }
        ImGui::Text("Listening :%d  client=%s", app.traderPort,
                    (app.traderBridge && app.traderBridge->clientConnected()) ? "yes" : "no");
        ImGui::Text("Open orders: %d", app.traderLiveOrders);
        ImGui::Text("Trader cash: %.2f", app.traderCash);
        ImGui::Text("Trader inv:  %.2f", app.traderInventory);
        ImGui::Text("Trader PnL:  %.2f", app.traderPnL);
    }

    std::string workerStatus;
    {
        std::lock_guard<std::mutex> lock(app.workerMu);
        workerStatus = app.workerStatus;
    }
    if (!workerStatus.empty()) {
        ImGui::TextWrapped("%s", workerStatus.c_str());
    }
    ImGui::EndChild();
}

void drawHeader(const MarketMakerState& st, const std::string& status) {
    ImGui::TextUnformatted("MARKET MAKER TERMINAL");
    ImGui::SameLine(ImGui::GetWindowWidth() - 280);
    ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.5f, 1.0f), "%s", status.c_str());
    ImGui::Separator();
    ImGui::Columns(4, nullptr, false);
    ImGui::Text("MID PRICE");
    ImGui::Text("$%.4f", st.midPrice);
    ImGui::NextColumn();
    ImGui::Text("BID");
    ImGui::TextColored(ImVec4(0.95f, 0.35f, 0.35f, 1.0f), "$%.4f", st.bidPrice);
    ImGui::NextColumn();
    ImGui::Text("ASK");
    ImGui::TextColored(ImVec4(0.2f, 0.85f, 0.45f, 1.0f), "$%.4f", st.askPrice);
    ImGui::NextColumn();
    ImGui::Text("SPREAD");
    ImGui::Text("$%.4f", st.askPrice - st.bidPrice);
    ImGui::Columns(1);
}

void drawInventoryPnL(const MarketMakerState& st) {
    ImGui::BeginChild("InvPnL", ImVec2(0, 140), true);
    ImGui::Columns(4, nullptr, false);
    ImGui::Text("INVENTORY");
    if (st.inventory >= 0) {
        ImGui::TextColored(ImVec4(0.2f, 0.85f, 0.45f, 1.0f), "%+.2f", st.inventory);
    } else {
        ImGui::TextColored(ImVec4(0.95f, 0.35f, 0.35f, 1.0f), "%+.2f", st.inventory);
    }
    ImGui::Text("Max %.2f  Min %.2f", st.maxInventory, st.minInventory);
    ImGui::NextColumn();
    ImGui::Text("CASH");
    ImGui::Text("$%.2f", st.cash);
    ImGui::NextColumn();
    ImGui::Text("REALIZED / INV / TOTAL");
    ImGui::Text("$%.2f", st.realizedPnL);
    ImGui::Text("$%.2f", st.totalPnL - st.realizedPnL);
    ImGui::TextColored(st.totalPnL >= 0 ? ImVec4(0.2f, 0.85f, 0.45f, 1.0f)
                                        : ImVec4(0.95f, 0.35f, 0.35f, 1.0f),
                       "$%.2f", st.totalPnL);
    ImGui::NextColumn();
    ImGui::Text("ACTIVITY");
    const double lamTot = st.lambdaAsk + st.lambdaBid;
    const char* level = lamTot < 2.0 ? "LOW" : (lamTot < 6.0 ? "NORMAL" : "HIGH");
    ImGui::Text("la %.2f  lb %.2f", st.lambdaAsk, st.lambdaBid);
    ImGui::Text("ltot %.2f  %s", lamTot, level);
    ImGui::Text("Trades %d", st.tradeCount);
    ImGui::Columns(1);

    // Inventory bar relative to +/- 20 default scale
    float frac = static_cast<float>(st.inventory / 20.0);
    frac = std::clamp(frac, -1.0f, 1.0f);
    ImGui::ProgressBar(0.5f + 0.5f * frac, ImVec2(-1, 0), "inventory");
    ImGui::EndChild();
}

void drawStrategy(const AppState& app) {
    ImGui::BeginChild("Strategy", ImVec2(0, 0), true);
    ImGui::TextUnformatted("DEALER STRATEGY");
    ImGui::Separator();
    const auto& st = app.uiState;
    ImGui::Text("Inventory: %+.2f", st.inventory);
    ImGui::Text("Optimal ask offset a*: %.4f", st.askOffset);
    ImGui::Text("Optimal bid offset b*: %.4f", st.bidOffset);
    ImGui::Text("Ask %.4f   Bid %.4f", st.askPrice, st.bidPrice);
    ImGui::Text("ask intensity %.4f   bid intensity %.4f", st.lambdaAsk, st.lambdaBid);
    ImGui::Spacing();
    if (st.inventory > 0.5) {
        ImGui::TextWrapped(
            "INVENTORY: %+.2f\nDealer is currently long.\n"
            "The optimal policy is adjusting quotes to manage inventory risk.\n"
            "Ask offset: $%.4f\nBid offset: $%.4f",
            st.inventory, st.askOffset, std::abs(st.bidOffset));
    } else if (st.inventory < -0.5) {
        ImGui::TextWrapped(
            "INVENTORY: %+.2f\nDealer is currently short.\n"
            "The optimal policy is adjusting quotes to manage inventory risk.\n"
            "Ask offset: $%.4f\nBid offset: $%.4f",
            st.inventory, st.askOffset, std::abs(st.bidOffset));
    } else {
        ImGui::TextWrapped(
            "INVENTORY: %+.2f\nDealer inventory is near flat.\n"
            "Quotes reflect balanced arrival intensities.\n"
            "Ask offset: $%.4f\nBid offset: $%.4f",
            st.inventory, st.askOffset, std::abs(st.bidOffset));
    }
    drawPolicyChart(app.policy, st.time);
    ImGui::EndChild();
}

void drawSummary(const AppState& app) {
    if (!app.finished && !app.sim) {
        ImGui::TextUnformatted("Run a simulation to see the close summary.");
        return;
    }
    const auto& st = app.uiState;
    ImGui::TextUnformatted("MARKET CLOSED");
    ImGui::Separator();
    ImGui::Text("Final Mid Price:     %.4f", st.midPrice);
    ImGui::Text("Final Inventory:     %.4f", st.inventory);
    ImGui::Text("Total Trades:        %d", st.tradeCount);
    ImGui::Text("Ask Hits:            %d", st.askHits);
    ImGui::Text("Bid Hits:            %d", st.bidHits);
    const double avgSpread =
        st.quoteUpdates > 0 ? st.spreadSum / static_cast<double>(st.quoteUpdates) : 0.0;
    ImGui::Text("Average Spread:      %.4f", avgSpread);
    ImGui::Text("Maximum Inventory:   %.4f", st.maxInventory);
    ImGui::Text("Minimum Inventory:   %.4f", st.minInventory);
    ImGui::Text("Realized P&L:        %.4f", st.realizedPnL);
    ImGui::Text("Inventory P&L:       %.4f", st.totalPnL - st.realizedPnL);
    ImGui::Text("Total P&L:           %.4f", st.totalPnL);
    drawPnLChart(app.uiEvents);
    drawInventoryChart(app.uiEvents);
}

void drawMonteCarlo(AppState& app) {
    ImGui::InputInt("Number of simulations", &app.mcRuns);
    app.mcRuns = std::max(10, app.mcRuns);
    if (ImGui::Button("Run Monte Carlo") && !app.workerBusy) {
        runMonteCarloAsync(app);
    }
    ImGui::Separator();
    ImGui::Text("Mean P&L:     %.4f", app.mcPnLStats.mean);
    ImGui::Text("Std P&L:      %.4f", app.mcPnLStats.stddev);
    ImGui::Text("Median P&L:   %.4f", app.mcPnLStats.median);
    ImGui::Text("5th / 25th:   %.4f / %.4f", app.mcPnLStats.p05, app.mcPnLStats.p25);
    ImGui::Text("75th / 95th:  %.4f / %.4f", app.mcPnLStats.p75, app.mcPnLStats.p95);
    ImGui::Text("Mean final inventory: %.4f", app.mcInvStats.mean);
    ImGui::Text("Mean max |inventory|: %.4f", app.mcMeanMaxAbsInv);
    drawHistogram("P&L distribution", app.mcPnL);
    drawHistogram("Inventory distribution", app.mcInventory);
}

}  // namespace

int runTradingTerminal(int argc, char** argv) {
    (void)argc;
    (void)argv;

    if (!glfwInit()) {
        return 1;
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#if defined(__APPLE__)
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow* window =
        glfwCreateWindow(1440, 900, "Market Maker Terminal", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();
    applyDarkTheme();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 150");

    AppState app;
    app.draftParams = ModelParams{};
    app.appliedParams = app.draftParams;
    solveDpAsync(app);

    auto last = std::chrono::steady_clock::now();

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        const auto now = std::chrono::steady_clock::now();
        const double wallDt =
            std::chrono::duration<double>(now - last).count();
        last = now;

        app.askFlash = std::max(0.0f, app.askFlash - static_cast<float>(wallDt) * 2.0f);
        app.bidFlash = std::max(0.0f, app.bidFlash - static_cast<float>(wallDt) * 2.0f);

        {
            std::lock_guard<std::mutex> lock(app.mu);
            advanceSimulation(app, wallDt);

            if (app.traderBridge && app.traderPortEnabled && app.sim) {
                auto fills = app.traderBridge->pollAndMatch(app.sim->state(), app.sim->events());
                for (const auto& f : fills) {
                    if (f.side == ExtSide::Buy) {
                        app.sim->applyExternalBuy(f.size);
                        app.askFlash = 1.0f;
                    } else {
                        app.sim->applyExternalSell(f.size);
                        app.bidFlash = 1.0f;
                    }
                    // Refresh quotes after external inventory shock.
                    // Quotes already on state; DP offsets update on next sim step.
                }
                if (!fills.empty()) {
                    // Nudge policy offsets after inventory change without advancing time.
                    // Re-query via a zero-dt quote refresh by taking a no-op: state already
                    // updated; next Poisson step will recompute a*/b*.
                    syncUiFromSim(app);
                } else {
                    app.uiState = app.sim->state();
                }
                app.traderLiveOrders = app.traderBridge->oms().liveCount();
                app.traderCash = app.traderBridge->traderCash();
                app.traderInventory = app.traderBridge->traderInventory();
                app.traderPnL = app.traderBridge->traderPnL(app.uiState.midPrice);
            }
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        const ImGuiViewport* vp = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(vp->WorkPos);
        ImGui::SetNextWindowSize(vp->WorkSize);
        ImGui::Begin("MarketMakerRoot", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                         ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus);

        drawHeader(app.uiState, app.statusLine);
        ImGui::Separator();

        ImGui::BeginChild("Left", ImVec2(320, 0), false);
        drawParams(app);
        drawControls(app);
        ImGui::EndChild();

        ImGui::SameLine();
        ImGui::BeginChild("Center", ImVec2(0, 0), false);
        if (ImGui::BeginTabBar("MainTabs")) {
            if (ImGui::BeginTabItem("Live")) {
                drawOrderBook(app.uiState, app.askFlash, app.bidFlash);
                drawInventoryPnL(app.uiState);
                drawPriceChart(app.uiEvents);
                drawPnLChart(app.uiEvents);
                drawInventoryChart(app.uiEvents);

                ImGui::Columns(2, nullptr, false);
                drawEventTape(app.uiEvents);
                ImGui::NextColumn();
                drawStrategy(app);
                ImGui::Columns(1);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Monte Carlo")) {
                drawMonteCarlo(app);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Summary")) {
                drawSummary(app);
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
        ImGui::EndChild();

        ImGui::End();
        ImGui::Render();
        int display_w = 0;
        int display_h = 0;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.05f, 0.06f, 0.08f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}

}  // namespace mm
