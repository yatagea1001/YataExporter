#pragma once
#include "imgui.h"
#include "implot.h"
#include <chrono>
#include <string>
#include <vector>
#include <iostream>
#include "TradeModule.h"   // pastikan ini ada untuk TradeLine & logic drag

// Forward declaration agar tipe Candle dikenali
struct Candle;

class CTradeModule {
public:
    std::vector<TradeLine> trades;
    std::string lastMsg = "";
    std::chrono::steady_clock::time_point lastMsgTime;

    // 🟢 Buka posisi
    void OpenTrade(bool isBuy, double price) {
        TradeLine t;
        t.Open(isBuy, price);
        trades.push_back(t);
        SetLastMsg(std::string(isBuy ? "Opened BUY trade #" : "Opened SELL trade #") + std::to_string(trades.size()));
    }

    // 🧮 Periksa apakah TP/SL kena (aman & multi-TF aware)
    void CheckTradeProgress(const std::vector<Candle>& candles, int currentIndex) {
        if (candles.empty() || currentIndex < 0 || currentIndex >= (int)candles.size())
            return;

        for (auto &t : trades) {
            if (t.active && !t.closed) {
                t.CheckHit(candles[currentIndex]);
                if (!t.lastMsg.empty())
                    SetLastMsg(t.lastMsg);
            }
        }
    }

    // Backward-compat wrapper — agar main.cpp yang lama tetap jalan
    void CheckTradeProgressOn(const std::vector<Candle>& candles, int currentIndex) {
        CheckTradeProgress(candles, currentIndex);
    }

    void DrawAndHandle(int start, int end) {
        if (trades.empty()) return;
        for (auto &t : trades) {
            if (!t.active || t.closed) continue;
            t.HandleMouseInteraction();
        }
    }

    void DrawTradeMessage() {
        if (lastMsg.empty()) return;

        using namespace std::chrono;
        float age = duration_cast<duration<float>>(steady_clock::now() - lastMsgTime).count();
        if (age > 5.0f) return;

        ImGui::SetNextWindowPos(ImVec2(20, 60), ImGuiCond_Always);
        ImGui::Begin("##TradeMsg", nullptr,
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
            ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoInputs);

        ImGui::TextColored(ImVec4(1, 1, 0.3f, 1), "%s", lastMsg.c_str());
        ImGui::End();
    }

    bool IsDragging() const {
        for (const auto &t : trades)
            if (t.draggingEntry || t.draggingSL || t.draggingTP)
                return true;
        return false;
    }

    void SetLastMsg(const std::string& s) {
        lastMsg = s;
        lastMsgTime = std::chrono::steady_clock::now();
    }

    void ShowTradingButtons(int) { /* intentionally empty */ }
};
