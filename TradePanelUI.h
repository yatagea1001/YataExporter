#pragma once
#include "imgui.h"
#include "TradeModule.h"
#include "TradeSettingsUI.h"
#include <string>
#include <iomanip>
#include <sstream>
#include <cmath>

class TradePanelUI {
public:
    // --- DEPOSIT SETTINGS ---
    static inline double liveDeposit   = 10000.0;  // deposit mode live (default)
    static inline double replayDeposit  = 10000.0;  // deposit mode replay (dari Setup Replay popup)

    // =============================================================
    // RENDER UTAMA
    // Pass isReplayMode = true jika saat ini di mode replay
    // =============================================================
    static void Render(TradeManager& tm, double currentPrice, bool isReplayMode = false) {
        ImGui::SetNextWindowSize(ImVec2(0, 220), ImGuiCond_FirstUseEver);

        if (ImGui::Begin("Trade", nullptr)) {

            // --- HEADER: Balance + Equity + CLOSE ALL ---
            ImGui::BeginGroup();
            {
                // Mode badge — hanya tampil saat mode replay
                if (isReplayMode) {
                    ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.2f, 1.0f), "[REPLAY]");
                    ImGui::SameLine(0, 8);
                }

                // --- BALANCE ---
                ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Balance:");
                ImGui::SameLine();

                if (isReplayMode) {
                    // Mode replay: balance dari deposit popup (setup replay)
                    ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f),
                                      "$%s", FormatUSD(tm.balance).c_str());
                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip("Deposit: $%.2f (ubah di Setup Replay)",
                                          replayDeposit);
                    }
                } else {
                    // Mode live/demo: balance statis
                    ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f),
                                      "$%s", FormatUSD(tm.balance).c_str());
                }

                ImGui::SameLine(0, 20);
                ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Equity:");
                ImGui::SameLine();
                ImVec4 colEq = (tm.equity >= tm.balance)
                    ? ImVec4(0.2f, 1.0f, 0.3f, 1.0f)
                    : ImVec4(1.0f, 0.35f, 0.35f, 1.0f);
                ImGui::TextColored(colEq, "$%s", FormatUSD(tm.equity).c_str());

                ImGui::SameLine(0, 20);

                // Floating P/L
                double floatPL = tm.equity - tm.balance;
                ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "P/L:");
                ImGui::SameLine();
                ImVec4 plCol = (floatPL >= 0) ? ImVec4(0.2f, 1.0f, 0.3f, 1.0f)
                                             : ImVec4(1.0f, 0.35f, 0.35f, 1.0f);
                ImGui::TextColored(plCol, "%s$%s",
                    (floatPL >= 0 ? "+" : ""),
                    FormatUSD(fabs(floatPL)).c_str());

                ImGui::SameLine(0, 30);

                // Panic Button: CLOSE ALL
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.1f, 0.1f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.1f, 0.1f, 1.0f));
                if (ImGui::Button("CLOSE ALL", ImVec2(100, 28))) {
                    tm.CloseAllSafe(currentPrice);
                }
                ImGui::PopStyleColor(2);

                // Settings Icon (PNG asli) — di kanan CLOSE ALL
                ImGui::SameLine(0, 6);
                bool settingsActive = g_tradeSettingsUI.showPopup;
                if (TradeSettingsUI::settingIconTex) {
                    // PNG icon: invisible button + overlay image
                    ImVec4 tint = settingsActive
                        ? ImVec4(0.5f, 0.8f, 1.0f, 1.0f)
                        : ImVec4(0.6f, 0.6f, 0.65f, 0.85f);
                    ImU32 tintU32 = ImGui::ColorConvertFloat4ToU32(tint);
                    ImGui::InvisibleButton("##settings_icon", ImVec2(20, 20));
                    if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                    if (ImGui::IsItemActive()) {
                        g_tradeSettingsUI.Toggle();
                    }
                    ImVec2 iconMin = ImGui::GetItemRectMin();
                    ImVec2 iconMax = ImGui::GetItemRectMax();
                    ImGui::GetWindowDrawList()->AddImage(
                        TradeSettingsUI::settingIconTex, iconMin, iconMax,
                        ImVec2(0, 0), ImVec2(1, 1), tintU32);
                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip("Trade Visual Settings");
                    }
                } else {
                    // Fallback: text button jika texture belum loaded
                    ImGui::PushStyleColor(ImGuiCol_Button, settingsActive
                        ? ImVec4(0.15f, 0.45f, 0.75f, 1.0f)
                        : ImVec4(0.22f, 0.22f, 0.25f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.55f, 0.85f, 1.0f));
                    if (ImGui::Button("⚙##settings_fallback", ImVec2(24, 24))) {
                        g_tradeSettingsUI.Toggle();
                    }
                    ImGui::PopStyleColor(2);
                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip("Trade Visual Settings");
                    }
                }
            }
            ImGui::EndGroup();

            ImGui::Separator();

            // --- TABEL OPEN TRADES ---
            static ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg
                                         | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY;

            if (ImGui::BeginTable("TradeTable", 11, flags)) {

                ImGui::TableSetupColumn("ID",     ImGuiTableColumnFlags_WidthFixed, 40);
                ImGui::TableSetupColumn("Time");
                ImGui::TableSetupColumn("Type");
                ImGui::TableSetupColumn("Size");
                ImGui::TableSetupColumn("Symbol");
                ImGui::TableSetupColumn("Entry");
                ImGui::TableSetupColumn("S/L");
                ImGui::TableSetupColumn("T/P");
                ImGui::TableSetupColumn("Market");
                ImGui::TableSetupColumn("Profit");
                ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthFixed, 80);

                ImGui::TableHeadersRow();

                for (auto& t : tm.trades) {
                    if (!t.isOpen) continue;

                    ImGui::PushID(t.id);

                    // Warna Row — set BOTH RowBg0 & RowBg1 agar tidak bentrok dgn alternating
                    ImU32 rowBgColor = (t.profit >= 0) ? IM_COL32(0, 255, 0, 35) : IM_COL32(255, 0, 0, 35);
                    ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, rowBgColor);
                    ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg1, rowBgColor);
                    ImGui::TableNextRow();

                    ImGui::TableNextColumn(); ImGui::Text("%d", t.id);

                    ImGui::TableNextColumn();
                    ImGui::Text("%s", FormatTime(t.openTime).c_str());

                    ImGui::TableNextColumn();
                    if (t.type == TRADE_BUY)
                        ImGui::TextColored(ImVec4(0.2f, 0.6f, 1.0f, 1.0f), "BUY");
                    else
                        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "SELL");

                    ImGui::TableNextColumn(); ImGui::Text("%.2f", t.volume);
                    ImGui::TableNextColumn(); ImGui::Text("%s", t.symbol.c_str());
                    ImGui::TableNextColumn(); ImGui::Text("%.2f", t.entryPrice);

                    ImGui::TableNextColumn();
                    if (t.sl == 0) ImGui::Text("-"); else ImGui::Text("%.2f", t.sl);

                    ImGui::TableNextColumn();
                    if (t.tp == 0) ImGui::Text("-"); else ImGui::Text("%.2f", t.tp);

                    ImGui::TableNextColumn(); ImGui::Text("%.2f", currentPrice);

                    ImGui::TableNextColumn();
                    ImVec4 colP = (t.profit >= 0) ? ImVec4(0.2f, 1.0f, 0.2f, 1.0f) : ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
                    ImGui::TextColored(colP, "%.2f", t.profit);

                    // --- ACTION BUTTONS ---
                    ImGui::TableNextColumn();
                    if (t.isLocked) {
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.0f, 1.0f));
                        if (ImGui::Button("[ L ]")) { t.isLocked = false; }
                        ImGui::PopStyleColor();
                        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Posisi Terkunci (Aman). Klik untuk Buka.");
                    } else {
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
                        if (ImGui::Button("[   ]")) { t.isLocked = true; }
                        ImGui::PopStyleColor();
                        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Klik untuk Mengunci (Lock)");
                    }
                    ImGui::SameLine();
                    if (!t.isLocked) {
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 0.6f));
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
                        if (ImGui::Button(" X ")) {
                            tm.CloseTrade(t.id, currentPrice);
                        }
                        ImGui::PopStyleColor(2);
                        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Close Manual");
                    } else {
                        ImGui::TextDisabled(" Safe");
                    }

                    ImGui::PopID();
                }
                ImGui::EndTable();
            }
        }
        ImGui::End();
    }
};

