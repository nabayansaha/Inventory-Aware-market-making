#include "ui/EventTape.hpp"

#include <imgui.h>

#include <algorithm>
#include <cstdio>

namespace mm {

void drawEventTape(const std::vector<MarketEvent>& events, int maxRows, float size_x,
                   float size_y) {
    const ImVec2 size(size_x, size_y);
    ImGui::BeginChild("EventTape", size, true, ImGuiWindowFlags_AlwaysVerticalScrollbar);
    ImGui::TextUnformatted("EVENT TAPE");
    ImGui::Separator();

    const int n = static_cast<int>(events.size());
    const int start = std::max(0, n - maxRows);
    if (n == 0) {
        ImGui::TextDisabled("No events yet. Press START to run the market.");
    }
    for (int i = n - 1; i >= start; --i) {
        const auto& e = events[static_cast<std::size_t>(i)];
        ImVec4 color = ImVec4(0.75f, 0.78f, 0.85f, 1.0f);
        const char* side = "----";
        if (e.type == EventType::AskHit) {
            color = ImVec4(0.2f, 0.85f, 0.45f, 1.0f);
            side = "BUY ";
        } else if (e.type == EventType::BidHit) {
            color = ImVec4(0.95f, 0.35f, 0.35f, 1.0f);
            side = "SELL";
        } else if (e.type == EventType::MidMove) {
            color = ImVec4(0.45f, 0.65f, 0.95f, 1.0f);
            side = "MID ";
        }

        ImGui::PushStyleColor(ImGuiCol_Text, color);
        if (e.type == EventType::AskHit || e.type == EventType::BidHit) {
            ImGui::Text("%8.3f  %s  %.0f @ %.4f  %s%s", e.timestamp, side, e.tradeSize,
                        e.tradePrice, MarketEvent::typeName(e.type),
                        e.external ? " EXT" : "");
        } else {
            ImGui::Text("%8.3f  %s  mid=%.4f", e.timestamp, side, e.midPrice);
        }
        ImGui::PopStyleColor();
    }
    ImGui::EndChild();
}

}  // namespace mm
