#include "ui/OrderBookView.hpp"

#include <imgui.h>

#include <cstdio>

namespace mm {

void drawOrderBook(const MarketMakerState& state, float askFlash, float bidFlash,
                   float height) {
    ImGui::BeginChild("OrderBook", ImVec2(0, height), true);
    ImGui::TextUnformatted("ORDER BOOK / QUOTES");
    ImGui::Separator();

    const ImVec4 askColor = ImVec4(0.2f + 0.6f * askFlash, 0.85f, 0.45f, 1.0f);
    const ImVec4 bidColor = ImVec4(0.95f, 0.35f + 0.4f * bidFlash, 0.35f, 1.0f);
    const ImVec4 midColor = ImVec4(0.45f, 0.70f, 0.95f, 1.0f);

    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Text, askColor);
    ImGui::SetWindowFontScale(1.35f);
    ImGui::Text("ASK   %.4f", state.askPrice);
    ImGui::Text("##############");
    ImGui::PopStyleColor();

    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Text, midColor);
    ImGui::Text("MID   %.4f", state.midPrice);
    ImGui::PopStyleColor();

    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Text, bidColor);
    ImGui::Text("##############");
    ImGui::Text("BID   %.4f", state.bidPrice);
    ImGui::SetWindowFontScale(1.0f);
    ImGui::PopStyleColor();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("Spread: %.4f", state.askPrice - state.bidPrice);
    ImGui::Text("la=%.3f   lb=%.3f", state.lambdaAsk, state.lambdaBid);
    ImGui::Text("a*=%.4f   b*=%.4f", state.askOffset, state.bidOffset);
    ImGui::EndChild();
}

}  // namespace mm
