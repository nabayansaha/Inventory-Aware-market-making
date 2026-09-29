#include "ui/OrderBookView.hpp"

#include <imgui.h>

#include <cstdio>

namespace mm {

void drawOrderBook(const MarketMakerState& state, float askFlash, float bidFlash) {
    ImGui::BeginChild("OrderBook", ImVec2(0, 260), true);
    ImGui::TextUnformatted("MARKET / QUOTES");
    ImGui::Separator();

    const ImVec4 askColor = ImVec4(0.2f + 0.6f * askFlash, 0.85f, 0.45f, 1.0f);
    const ImVec4 bidColor = ImVec4(0.95f, 0.35f + 0.4f * bidFlash, 0.35f, 1.0f);
    const ImVec4 midColor = ImVec4(0.45f, 0.70f, 0.95f, 1.0f);

    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Text, askColor);
    ImGui::Text("           ASK");
    ImGui::Text("         %8.4f", state.askPrice);
    ImGui::Text("      #############");
    ImGui::PopStyleColor();

    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Text, midColor);
    ImGui::Text("         %8.4f", state.midPrice);
    ImGui::Text("           MID");
    ImGui::PopStyleColor();

    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Text, bidColor);
    ImGui::Text("         %8.4f", state.bidPrice);
    ImGui::Text("      #############");
    ImGui::Text("           BID");
    ImGui::PopStyleColor();

    ImGui::Spacing();
    ImGui::Text("Spread: %.4f   la=%.3f   lb=%.3f", state.askPrice - state.bidPrice,
                state.lambdaAsk, state.lambdaBid);
    ImGui::EndChild();
}

}  // namespace mm
