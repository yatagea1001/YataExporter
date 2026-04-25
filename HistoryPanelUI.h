#pragma once
// ===========================================================================
// HistoryPanelUI.h — Panel ImGui untuk Trade History & Statistik
// ===========================================================================
// V3.2: Export buttons now trigger real browser downloads
//   - TriggerBrowserDownload() reads from virtual FS and creates Blob URL
//   - Download toast notification for user feedback
//   - All export buttons (Trade Log, Stats Popup, Compare, Statistics tab) wired
//
// V3.1: Visual fixes
//   - Card content clip rects prevent text overflow between cards
//   - Manual separator lines (card-width only) instead of full-width ImGui::Separator
//   - Step-chart equity curve (horizontal flat + vertical jump per trade)
//     instead of smooth diagonal lines — shows exact trade-by-trade impact
//
// V3: Phase 1 — UI Statistik Overhaul
//   - Chip toggle [LIVE] [REPLAY] dengan badge jumlah trade
//   - KPI Cards fixed-width + color indicator strip
//   - Equity Curve (cumulative P/L line chart via ImDrawList)
//   - Win/Loss horizontal stacked bar
//   - Detail stats grouped dalam section cards (2x2 grid)
//   - Export Statistics CSV + Export Performance CSV
//   - Live vs Replay compare popup
//   - Stats popup ringkasan per mode
//
// Dependensi:
//   - TradeHistory.h  (ClosedTrade, TradeStats, TradeHistory, TH_FormatUSD, etc.)
//   - imgui.h         (sudah ada di project)
//
// Cara Pakai:
//   HistoryPanelUI::Render(g_liveHistory, g_replayHistory);
//
// ===========================================================================

#include "TradeHistory.h"
#include "imgui.h"
#include <cstring>
#include <cstdio>
#include <algorithm>
#include <string>
#include <vector>

class HistoryPanelUI {
public:
    // --- STATE FILTER & SORT ---
    static inline char filterSymbol[64]   = "";
    static inline int  sortMode           = 0; // 0=Terbaru, 1=Terlama, 2=Profit High, 3=Profit Low
    static inline bool showOnlyWins       = false;
    static inline bool showOnlyLosses     = false;
    static inline int  selectedTab        = 0; // 0=History, 1=Statistik, 2=Per-Symbol
    static inline int  modeChip           = 0; // 0=LIVE, 1=REPLAY

    // --- DOWNLOAD STATUS TOAST ---
    static inline char  s_downloadStatus[256] = "";
    static inline float s_downloadTimer       = 0.0f;

    // =============================================================
    // RENDER UTAMA — menerima dua TradeHistory (live & replay)
    // =============================================================
    static void Render(TradeHistory& liveHistory, TradeHistory& replayHistory,
                       double liveDeposit = 10000.0, double replayDeposit = 10000.0) {
        ImGui::SetNextWindowSize(ImVec2(780, 520), ImGuiCond_FirstUseEver);

        if (!ImGui::Begin("Trade History & Stats", nullptr)) {
            ImGui::End();
            return;
        }

        // --- MODE CHIP: [LIVE] [REPLAY] + badges ---
        RenderModeChip(liveHistory, replayHistory);

        // Ambil history yang sedang aktif
        TradeHistory& activeHistory = (modeChip == 0) ? liveHistory : replayHistory;

        // --- POPUP STATS RINGKASAN ---
        RenderStatsPopup(activeHistory);

        // --- POPUP COMPARE: LIVE vs REPLAY ---
        RenderComparePopup(liveHistory, replayHistory);

        // --- TAB BAR ---
        ImGui::BeginTabBar("##HistoryTabs");

        if (ImGui::BeginTabItem("Trade Log")) {
            selectedTab = 0;
            RenderTradeLog(activeHistory);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Statistik")) {
            selectedTab = 1;
            double activeDeposit = (modeChip == 0) ? liveDeposit : replayDeposit;
            RenderStatistics(activeHistory, activeDeposit);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Per-Symbol")) {
            selectedTab = 2;
            RenderPerSymbol(activeHistory);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();

        ImGui::End();

        // Download status toast (rendered on foreground, outside window)
        ShowDownloadToast(ImGui::GetIO().DeltaTime);
    }

private:
    // =============================================================
    // MODE CHIP: [LIVE] [REPLAY] + badge + Compare button
    // =============================================================
    static void RenderModeChip(TradeHistory& live, TradeHistory& replay) {
        int liveCount  = (int)live.closedTrades.size();
        int replayCount = (int)replay.closedTrades.size();

        const char* labels[] = { "LIVE", "REPLAY" };
        const ImVec4 colors[] = {
            ImVec4(0.2f, 0.6f, 1.0f, 1.0f),
            ImVec4(1.0f, 0.75f, 0.2f, 1.0f),
        };
        const ImVec4 colorsActive[] = {
            ImVec4(0.15f, 0.45f, 0.85f, 1.0f),
            ImVec4(0.85f, 0.6f, 0.1f, 1.0f),
        };
        int counts[] = { liveCount, replayCount };

        float chipW = 90.0f;
        float chipH = 28.0f;

        for (int i = 0; i < 2; i++) {
            bool sel = (modeChip == i);
            ImGui::PushStyleColor(ImGuiCol_Button,
                sel ? colorsActive[i] : ImVec4(0.1f, 0.12f, 0.18f, 0.9f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, colors[i]);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, colorsActive[i]);
            ImGui::PushStyleColor(ImGuiCol_Text,
                sel ? ImVec4(1,1,1,1) : ImVec4(0.55f, 0.6f, 0.7f, 1));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));

            char id[32];
            snprintf(id, sizeof(id), "%s (%d)##mode%d", labels[i], counts[i], i);
            if (ImGui::Button(id, ImVec2(chipW, chipH))) {
                modeChip = i;
            }

            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor(4);

            if (i == 0) ImGui::SameLine(0, 8);
        }

        // Mode label
        ImGui::SameLine(0, 16);
        const char* modeName = (modeChip == 0) ? "Mode: LIVE / Demo" : "Mode: REPLAY";
        ImGui::TextColored(ImVec4(0.5f, 0.55f, 0.65f, 1.0f), "%s", modeName);

        // Tombol Stats Summary
        ImGui::SameLine(0, 12);
        if (ImGui::SmallButton("Stats Summary")) {
            ImGui::OpenPopup("##StatsPopup");
        }

        // Tombol Live vs Replay Compare
        ImGui::SameLine(0, 8);
        if (ImGui::SmallButton("Live vs Replay")) {
            ImGui::OpenPopup("##ComparePopup");
        }

        // Tombol Reset Replay
        if (modeChip == 1) {
            ImGui::SameLine(0, 8);
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.15f, 0.15f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.85f, 0.2f, 0.2f, 1.0f));
            if (ImGui::SmallButton("Reset Replay")) {
                ImGui::OpenPopup("##ResetConfirm");
            }
            ImGui::PopStyleColor(2);

            if (ImGui::BeginPopup("##ResetConfirm")) {
                ImGui::TextColored(ImVec4(1,0.5,0.3,1), "Hapus semua data replay?");
                ImGui::Text("Trade aktif, history, dan balance akan direset.");
                ImGui::Separator();
                if (ImGui::Button("Ya, Reset", ImVec2(120, 0))) {
                    s_resetReplayRequested = true;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button("Batal", ImVec2(80, 0))) {
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }
        }
    }

    // =============================================================
    // STATS POPUP — Ringkasan singkat per mode
    // =============================================================
    static void RenderStatsPopup(TradeHistory& history) {
        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(400, 340), ImGuiCond_Appearing);

        ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.08f, 0.09f, 0.14f, 0.97f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.25f, 0.35f, 0.55f, 0.9f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.0f);

        const char* title = (modeChip == 0) ? "Stats Ringkasan — LIVE" : "Stats Ringkasan — REPLAY";

        if (ImGui::BeginPopupModal(title, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            TradeStats s = history.CalculateStats();

            if (s.totalTrades == 0) {
                ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f),
                    "Belum ada trade yang ditutup di mode ini.");
            } else {
                const char* modeLabel = (modeChip == 0) ? "LIVE" : "REPLAY";
                ImVec4 modeCol = (modeChip == 0) ? ImVec4(0.3f, 0.7f, 1.0f, 1.0f) : ImVec4(1.0f, 0.8f, 0.2f, 1.0f);
                ImGui::TextColored(modeCol, "Mode: %s", modeLabel);
                ImGui::Separator();

                // KPI Grid (3 columns)
                ImGui::Columns(3, "##PopupStats", true);

                ImVec4 wrCol = (s.winRate >= 50) ? ImVec4(0.2f,0.9f,0.3f,1) : ImVec4(1.0f,0.6f,0.1f,1);
                ImGui::TextColored(ImVec4(0.5f,0.5f,0.5f,1), "Win Rate");
                ImGui::TextColored(wrCol, "%.1f%%", s.winRate);
                ImGui::TextDisabled("%dW / %dL", s.wins, s.losses);
                ImGui::NextColumn();

                ImVec4 pnlCol = (s.netProfit >= 0) ? ImVec4(0.2f,0.9f,0.3f,1) : ImVec4(1.0f,0.3f,0.3f,1);
                ImGui::TextColored(ImVec4(0.5f,0.5f,0.5f,1), "Net P&L");
                ImGui::TextColored(pnlCol, "$%s", TH_FormatUSD(s.netProfit).c_str());
                ImGui::TextDisabled("Avg: $%s/trade", TH_FormatUSD(s.avgTrade).c_str());
                ImGui::NextColumn();

                ImVec4 pfCol = (s.profitFactor >= 1.5) ? ImVec4(0.2f,0.9f,0.3f,1) : ImVec4(1.0f,0.6f,0.1f,1);
                ImGui::TextColored(ImVec4(0.5f,0.5f,0.5f,1), "Profit Factor");
                ImGui::TextColored(pfCol, "%.2f", s.profitFactor);
                ImGui::TextDisabled("RR: 1:%.2f", s.avgRR);
                ImGui::Columns(1);

                ImGui::Separator();

                DetailRow("Total Trades",    "%d",          s.totalTrades);
                DetailRow("Avg Win / Loss",   "$%s / $%s",  TH_FormatUSD(s.avgWin).c_str(), TH_FormatUSD(s.avgLoss).c_str());
                DetailRow("Largest Win",     "$%s",         TH_FormatUSD(s.largestWin).c_str());
                DetailRow("Largest Loss",    "$%s",         TH_FormatUSD(s.largestLoss).c_str());
                DetailRow("Max Consec",      "%dW / %dL",   s.maxConsecWin, s.maxConsecLoss);
            }

            ImGui::Spacing();
            ImGui::Separator();

            const char* exportLabel = (modeChip == 0) ? "Export CSV (Live)" : "Export CSV (Replay)";
            if (ImGui::Button(exportLabel, ImVec2(-1, 0))) {
                const char* path = (modeChip == 0) ? "/data/trade_history_live_export.csv"
                                                 : "/data/trade_history_replay_export.csv";
                const char* fname = (modeChip == 0) ? "trade_history_live.csv"
                                                   : "trade_history_replay.csv";
                history.ExportCSV(path);
                TriggerBrowserDownload(path, fname);
            }
            ImGui::Spacing();

            if (ImGui::Button("Tutup", ImVec2(80, 0))) {
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }

        ImGui::PopStyleVar();
        ImGui::PopStyleColor(2);
    }

    // =============================================================
    // COMPARE POPUP — Live vs Replay side-by-side
    // =============================================================
    static void RenderComparePopup(TradeHistory& live, TradeHistory& replay) {
        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

        ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.08f, 0.09f, 0.14f, 0.97f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.25f, 0.35f, 0.55f, 0.9f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.0f);

        if (ImGui::BeginPopup("##ComparePopup")) {
            TradeStats sL = live.CalculateStats();
            TradeStats sR = replay.CalculateStats();

            ImGui::TextColored(ImVec4(0.8f, 0.85f, 0.95f, 1.0f), "Live vs Replay — Trade Comparison");
            ImGui::Separator();

            // Tabel perbandingan
            if (ImGui::BeginTable("##CompareTable", 3,
                    ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit)) {

                ImGui::TableSetupColumn("Metric",      ImGuiTableColumnFlags_WidthFixed, 140);
                ImGui::TableSetupColumn("LIVE",        ImGuiTableColumnFlags_WidthFixed, 150);
                ImGui::TableSetupColumn("REPLAY",      ImGuiTableColumnFlags_WidthFixed, 150);
                ImGui::TableHeadersRow();

                CompareRow("Total Trades",   "%d",      sL.totalTrades, sR.totalTrades);
                CompareRow("Win Rate",       "%.1f%%",  sL.winRate,     sR.winRate);
                CompareRow("Wins",           "%d",      sL.wins,        sR.wins);
                CompareRow("Losses",         "%d",      sL.losses,      sR.losses);
                CompareRow("Breakevens",     "%d",      sL.breakevens,  sR.breakevens);
                CompareRow("Net P&L",        "$%s",     TH_FormatUSD(sL.netProfit).c_str(), TH_FormatUSD(sR.netProfit).c_str());
                CompareRow("Avg Win",        "$%s",     TH_FormatUSD(sL.avgWin).c_str(),   TH_FormatUSD(sR.avgWin).c_str());
                CompareRow("Avg Loss",       "$%s",     TH_FormatUSD(sL.avgLoss).c_str(),  TH_FormatUSD(sR.avgLoss).c_str());
                CompareRow("Profit Factor",  "%.2f",    sL.profitFactor, sR.profitFactor);
                CompareRow("Avg RR",         "1:%.2f",  sL.avgRR,        sR.avgRR);
                CompareRow("Max Consec Win",  "%d",     sL.maxConsecWin, sR.maxConsecWin);
                CompareRow("Max Consec Loss", "%d",     sL.maxConsecLoss,sR.maxConsecLoss);

                ImGui::EndTable();
            }

            ImGui::Spacing();
            ImGui::Separator();

            if (ImGui::Button("Export Comparison CSV", ImVec2(-1, 0))) {
                ExportCompareCSV(live, replay);
                TriggerBrowserDownload("/data/compare_live_replay.csv", "compare_live_replay.csv");
            }
            ImGui::Spacing();

            if (ImGui::Button("Tutup", ImVec2(80, 0))) {
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }

        ImGui::PopStyleVar();
        ImGui::PopStyleColor(2);
    }

    // Helper: satu baris di compare table (double values)
    static void CompareRow(const char* metric, const char* fmt, double valL, double valR) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextColored(ImVec4(0.7f, 0.72f, 0.8f, 1.0f), "%s", metric);

        ImGui::TableNextColumn();
        char bufL[128];
        snprintf(bufL, sizeof(bufL), fmt, valL);
        ImGui::Text("%s", bufL);

        ImGui::TableNextColumn();
        char bufR[128];
        snprintf(bufR, sizeof(bufR), fmt, valR);
        ImGui::Text("%s", bufR);
    }

    // Helper: satu baris di compare table (string values — overload)
    static void CompareRow(const char* metric, const char* fmt,
                           const char* valL, const char* valR) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextColored(ImVec4(0.7f, 0.72f, 0.8f, 1.0f), "%s", metric);

        ImGui::TableNextColumn();
        char bufL[128];
        snprintf(bufL, sizeof(bufL), fmt, valL);
        ImGui::Text("%s", bufL);

        ImGui::TableNextColumn();
        char bufR[128];
        snprintf(bufR, sizeof(bufR), fmt, valR);
        ImGui::Text("%s", bufR);
    }

    // =============================================================
    // TAB 1: TRADE LOG (Tabel Historis) — UNCHANGED
    // =============================================================
    static void RenderTradeLog(TradeHistory& history) {
        // --- HEADER: Filter & Sort Controls ---
        ImGui::BeginGroup();
        {
            TradeStats s = history.CalculateStats();
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Total Closed:");
            ImGui::SameLine();
            ImGui::Text("%d trades", s.totalTrades);
            ImGui::SameLine(0, 20);
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Net P&L:");
            ImGui::SameLine();
            ImVec4 colPnL = (s.netProfit >= 0) ? ImVec4(0.2f, 1.0f, 0.2f, 1.0f) : ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
            ImGui::TextColored(colPnL, "$%s", TH_FormatUSD(s.netProfit).c_str());

            ImGui::SameLine(0, 20);
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Win Rate:");
            ImGui::SameLine();
            ImVec4 colWR = (s.winRate >= 50) ? ImVec4(0.2f, 1.0f, 0.2f, 1.0f) : ImVec4(1.0f, 0.8f, 0.2f, 1.0f);
            ImGui::TextColored(colWR, "%.1f%%", s.winRate);
        }
        ImGui::EndGroup();

        ImGui::Separator();

        // --- FILTER BAR ---
        ImGui::PushItemWidth(140);
        ImGui::InputTextWithHint("##FilterSymbol", "Filter Symbol...", filterSymbol, sizeof(filterSymbol));
        ImGui::PopItemWidth();

        ImGui::SameLine();
        ImGui::PushItemWidth(130);
        const char* sortLabels[] = { "Terbaru", "Terlama", "Profit High", "Profit Low" };
        ImGui::Combo("##SortMode", &sortMode, sortLabels, IM_ARRAYSIZE(sortLabels));
        ImGui::PopItemWidth();

        ImGui::SameLine();
        ImGui::Checkbox("Win", &showOnlyWins);
        ImGui::SameLine();
        ImGui::Checkbox("Loss", &showOnlyLosses);

        ImGui::SameLine(0, 15);

        // Action buttons
        if (ImGui::Button("Save JSON")) {
            const char* path = (modeChip == 0) ? "/data/trade_history_live.json"
                                             : "/data/trade_history_replay.json";
            const char* fname = (modeChip == 0) ? "trade_history_live.json"
                                               : "trade_history_replay.json";
            history.SaveToFile(path);
            TriggerBrowserDownload(path, fname);
        }
        ImGui::SameLine();
        if (ImGui::Button("Export CSV")) {
            const char* path = (modeChip == 0) ? "/data/trade_history_live_export.csv"
                                             : "/data/trade_history_replay_export.csv";
            const char* fname = (modeChip == 0) ? "trade_history_live.csv"
                                               : "trade_history_replay.csv";
            history.ExportCSV(path);
            TriggerBrowserDownload(path, fname);
        }
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.15f, 0.15f, 1.0f));
        if (ImGui::Button("Clear All")) {
            history.Clear();
        }
        ImGui::PopStyleColor();

        ImGui::Separator();

        // --- FILTERED DATA ---
        std::vector<const ClosedTrade*> filtered;
        std::string filterStr(filterSymbol);

        for (const auto& t : history.closedTrades) {
            if (!filterStr.empty() && t.symbol.find(filterStr) == std::string::npos) continue;
            if (showOnlyWins && t.profit <= 0.0001) continue;
            if (showOnlyLosses && t.profit >= -0.0001) continue;
            filtered.push_back(&t);
        }

        // --- SORT ---
        switch (sortMode) {
            case 0: break;
            case 1:
                std::sort(filtered.begin(), filtered.end(),
                    [](const ClosedTrade* a, const ClosedTrade* b) {
                        return a->closeTime < b->closeTime;
                    });
                break;
            case 2:
                std::sort(filtered.begin(), filtered.end(),
                    [](const ClosedTrade* a, const ClosedTrade* b) {
                        return a->profit > b->profit;
                    });
                break;
            case 3:
                std::sort(filtered.begin(), filtered.end(),
                    [](const ClosedTrade* a, const ClosedTrade* b) {
                        return a->profit < b->profit;
                    });
                break;
        }

        // --- TABEL ---
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Showing %d / %d trades",
                           (int)filtered.size(), (int)history.closedTrades.size());

        static ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg
                                     | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY
                                     | ImGuiTableFlags_SortMulti;

        if (ImGui::BeginTable("HistoryTable", 12, flags)) {

            ImGui::TableSetupColumn("ID",      ImGuiTableColumnFlags_WidthFixed, 35);
            ImGui::TableSetupColumn("Symbol",  ImGuiTableColumnFlags_WidthFixed, 70);
            ImGui::TableSetupColumn("Type",    ImGuiTableColumnFlags_WidthFixed, 42);
            ImGui::TableSetupColumn("Open Time",  ImGuiTableColumnFlags_WidthFixed, 120);
            ImGui::TableSetupColumn("Entry",   ImGuiTableColumnFlags_WidthFixed, 65);
            ImGui::TableSetupColumn("Close",   ImGuiTableColumnFlags_WidthFixed, 65);
            ImGui::TableSetupColumn("S/L",     ImGuiTableColumnFlags_WidthFixed, 55);
            ImGui::TableSetupColumn("T/P",     ImGuiTableColumnFlags_WidthFixed, 55);
            ImGui::TableSetupColumn("Profit",  ImGuiTableColumnFlags_WidthFixed, 70);
            ImGui::TableSetupColumn("Close Reason", ImGuiTableColumnFlags_WidthStretch, 0);
            ImGui::TableSetupColumn("Close Time", ImGuiTableColumnFlags_WidthFixed, 120);
            ImGui::TableSetupColumn("Duration", ImGuiTableColumnFlags_WidthFixed, 65);

            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableHeadersRow();

            for (const ClosedTrade* t : filtered) {
                ImU32 rowBg = (t->profit >= 0)
                    ? IM_COL32(0, 200, 0, 25)
                    : IM_COL32(200, 0, 0, 25);
                ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, rowBg);
                ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg1, rowBg);
                ImGui::TableNextRow();

                ImGui::PushID(t->id);

                ImGui::TableNextColumn(); ImGui::Text("%d", t->id);
                ImGui::TableNextColumn(); ImGui::Text("%s", t->symbol.c_str());

                ImGui::TableNextColumn();
                if (t->type == TH_TRADE_BUY)
                    ImGui::TextColored(ImVec4(0.2f, 0.6f, 1.0f, 1.0f), "BUY");
                else
                    ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "SELL");

                ImGui::TableNextColumn();
                ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "%s", TH_FormatTime(t->openTime).c_str());
                ImGui::TableNextColumn(); ImGui::Text("%.2f", t->entryPrice);
                ImGui::TableNextColumn(); ImGui::Text("%.2f", t->closePrice);

                ImGui::TableNextColumn();
                if (t->sl > 0 && t->sl != t->entryPrice) ImGui::Text("%.2f", t->sl);
                else ImGui::TextDisabled("-");

                ImGui::TableNextColumn();
                if (t->tp > 0 && t->tp != t->entryPrice) ImGui::Text("%.2f", t->tp);
                else ImGui::TextDisabled("-");

                ImGui::TableNextColumn();
                ImVec4 pColor = (t->profit >= 0)
                    ? ImVec4(0.2f, 1.0f, 0.2f, 1.0f)
                    : ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
                ImGui::TextColored(pColor, "$%s", TH_FormatUSD(t->profit).c_str());

                ImGui::TableNextColumn();
                {
                    ImVec4 rColor;
                    const char* reasonStr = t->closeReason.c_str();
                    if (strstr(reasonStr, "TP")) {
                        rColor = ImVec4(0.2f, 0.9f, 0.3f, 1.0f);
                    } else if (strstr(reasonStr, "SL")) {
                        rColor = ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
                    } else if (strstr(reasonStr, "Manual")) {
                        rColor = ImVec4(0.7f, 0.7f, 0.7f, 1.0f);
                    } else if (strstr(reasonStr, "Close All")) {
                        rColor = ImVec4(0.9f, 0.6f, 0.1f, 1.0f);
                    } else {
                        rColor = ImVec4(0.5f, 0.7f, 1.0f, 1.0f);
                    }
                    ImGui::TextColored(rColor, "%s", reasonStr);
                }

                ImGui::TableNextColumn();
                ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.5f, 1.0f), "%s", TH_FormatTime(t->closeTime).c_str());

                ImGui::TableNextColumn();
                ImGui::Text("%s", FormatDuration(t->duration).c_str());

                // Tooltip saat hover
                if (ImGui::IsItemHovered()) {
                    ImGui::BeginTooltip();
                    ImGui::Text("ID: #%d", t->id);
                    ImGui::Text("Symbol: %s", t->symbol.c_str());
                    ImGui::Text("Entry: %.2f  ->  Close: %.2f", t->entryPrice, t->closePrice);
                    ImGui::Text("Volume: %.2f", t->volume);
                    ImGui::Text("Profit: $%s", TH_FormatUSD(t->profit).c_str());
                    ImGui::Text("Open: %s", TH_FormatTimeLong(t->openTime).c_str());
                    ImGui::Text("Close: %s", TH_FormatTimeLong(t->closeTime).c_str());
                    ImGui::Text("Duration: %.0f seconds (%s)", t->duration,
                                 FormatDuration(t->duration).c_str());
                    ImGui::Text("RR Executed: %.2f", t->GetRRRatio());
                    ImGui::EndTooltip();
                }

                ImGui::PopID();
            }
            ImGui::EndTable();
        }
    }

    // =============================================================
    // TAB 2: STATISTIK — PHASE 1 OVERHAUL
    // =============================================================
    static void RenderStatistics(TradeHistory& history, double deposit = 10000.0) {
        TradeStats s = history.CalculateStats();

        if (s.totalTrades == 0) {
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f),
                               "Belum ada trade yang ditutup.");
            return;
        }

        ImVec2 avail = ImGui::GetContentRegionAvail();
        ImDrawList* draw = ImGui::GetWindowDrawList();
        float gap = 8.0f;

        // =======================================================
        // 1. KPI CARDS (4 in a row) — fixed width, gap konsisten
        // =======================================================
        float cardW = (avail.x - gap * 3) / 4.0f;
        float cardH = 74.0f;
        ImVec2 cardP = ImGui::GetCursorScreenPos();

        // Card 1: Win Rate
        {
            char val[32]; snprintf(val, 32, "%.1f%%", s.winRate);
            char sub[64]; snprintf(sub, 64, "%dW / %dL / %dBE", s.wins, s.losses, s.breakevens);
            ImVec4 vc = (s.winRate >= 50) ? ImVec4(0.25f,0.9f,0.35f,1) : ImVec4(1.0f,0.65f,0.15f,1);
            StatCardFixed("Win Rate", val, vc, sub,
                          cardP.x, cardP.y, cardW, cardH);
        }

        // Card 2: Net P&L
        {
            char val[48]; snprintf(val, 48, "$%s", TH_FormatUSD(s.netProfit).c_str());
            char sub[64]; snprintf(sub, 64, "Avg: $%s/trade", TH_FormatUSD(s.avgTrade).c_str());
            ImVec4 vc = (s.netProfit >= 0) ? ImVec4(0.25f,0.9f,0.35f,1) : ImVec4(1.0f,0.35f,0.35f,1);
            StatCardFixed("Net P&L", val, vc, sub,
                          cardP.x + (cardW + gap), cardP.y, cardW, cardH);
        }

        // Card 3: Profit Factor
        {
            char val[32]; snprintf(val, 32, "%.2f", s.profitFactor);
            char sub[64]; snprintf(sub, 64, "G: $%s / L: $%s",
                TH_FormatUSD(s.grossProfit).c_str(), TH_FormatUSD(fabs(s.grossLoss)).c_str());
            ImVec4 vc = (s.profitFactor >= 1.5) ? ImVec4(0.25f,0.9f,0.35f,1) : ImVec4(1.0f,0.65f,0.15f,1);
            StatCardFixed("Profit Factor", val, vc, sub,
                          cardP.x + (cardW + gap) * 2, cardP.y, cardW, cardH);
        }

        // Card 4: Avg RR
        {
            char val[32]; snprintf(val, 32, "1:%.2f", s.avgRR);
            char sub[64]; snprintf(sub, 64, "Max: %dW / %dL", s.maxConsecWin, s.maxConsecLoss);
            ImVec4 vc = (s.avgRR >= 1.5) ? ImVec4(0.25f,0.9f,0.35f,1) : ImVec4(1.0f,0.65f,0.15f,1);
            StatCardFixed("Avg RR", val, vc, sub,
                          cardP.x + (cardW + gap) * 3, cardP.y, cardW, cardH);
        }

        // Advance cursor past cards
        ImGui::SetCursorScreenPos(ImVec2(cardP.x, cardP.y + cardH + 14));

        // =======================================================
        // 2. EQUITY CURVE — cumulative P/L line chart
        // =======================================================
        SectionTitle("Equity Curve");
        RenderEquityCurve(history, deposit);

        ImGui::Spacing();

        // =======================================================
        // 3. WIN / LOSS DISTRIBUTION BAR
        // =======================================================
        SectionTitle("Win / Loss Distribution");
        RenderWinLossBar(s);

        ImGui::Spacing();

        // =======================================================
        // 4. DETAIL CARDS — 1 row: Breakdown (left) + Financials (right)
        // =======================================================
        float colW = (avail.x - gap) / 2.0f;
        float rowH = 108.0f;

        ImVec2 rowP = ImGui::GetCursorScreenPos();

        // Background: Left card (Trade Breakdown)
        draw->AddRectFilled(rowP, ImVec2(rowP.x + colW, rowP.y + rowH),
                            IM_COL32(28, 31, 42, 255), 6.0f);
        draw->AddRect(rowP, ImVec2(rowP.x + colW, rowP.y + rowH),
                      IM_COL32(50, 55, 70, 150), 6.0f);

        // Background: Right card (Financials)
        ImVec2 rRight = ImVec2(rowP.x + colW + gap, rowP.y);
        draw->AddRectFilled(rRight, ImVec2(rRight.x + colW, rRight.y + rowH),
                            IM_COL32(28, 31, 42, 255), 6.0f);
        draw->AddRect(rRight, ImVec2(rRight.x + colW, rRight.y + rowH),
                      IM_COL32(50, 55, 70, 150), 6.0f);

        // Content: Left — Trade Breakdown
        {
            float cx = rowP.x + 12;
            float cardRight = rowP.x + colW - 4;
            ImGui::SetCursorScreenPos(ImVec2(cx, rowP.y + 8));
            ImGui::TextColored(ImVec4(0.72f, 0.76f, 0.88f, 1.0f), "Trade Breakdown");
            draw->AddLine(ImVec2(cx, rowP.y + 24), ImVec2(cardRight, rowP.y + 24),
                          IM_COL32(50, 55, 70, 120), 1.0f);
            ImGui::PushClipRect(ImVec2(rowP.x, rowP.y), ImVec2(rowP.x + colW, rowP.y + rowH), true);
            DetailRow("Total Trades", "%d",  s.totalTrades);
            DetailRow("Wins",         "%d",  s.wins);
            DetailRow("Losses",       "%d",  s.losses);
            DetailRow("Breakevens",   "%d",  s.breakevens);
            ImGui::PopClipRect();
        }

        // Content: Right — Financials
        {
            float cx = rRight.x + 12;
            float cardRight = rRight.x + colW - 4;
            ImGui::SetCursorScreenPos(ImVec2(cx, rRight.y + 8));
            ImGui::TextColored(ImVec4(0.72f, 0.76f, 0.88f, 1.0f), "Financials");
            draw->AddLine(ImVec2(cx, rRight.y + 24), ImVec2(cardRight, rRight.y + 24),
                          IM_COL32(50, 55, 70, 120), 1.0f);
            ImGui::PushClipRect(ImVec2(rRight.x, rRight.y), ImVec2(rRight.x + colW, rRight.y + rowH), true);
            DetailRow("Gross Profit",  "$%s", TH_FormatUSD(s.grossProfit).c_str());
            DetailRow("Gross Loss",    "$%s", TH_FormatUSD(fabs(s.grossLoss)).c_str());
            DetailRow("Net Profit",    "$%s", TH_FormatUSD(s.netProfit).c_str());
            DetailRow("Avg Trade",     "$%s", TH_FormatUSD(s.avgTrade).c_str());
            ImGui::PopClipRect();
        }

        // Advance past row
        ImGui::SetCursorScreenPos(ImVec2(rowP.x, rowP.y + rowH + 12));

        // =======================================================
        // 5. EXPORT BUTTONS
        // =======================================================
        ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.3f, 0.35f, 0.5f, 0.8f));
        ImGui::Separator();
        ImGui::PopStyleColor();
        ImGui::Spacing();

        if (ImGui::Button("Export Statistics CSV", ImVec2(200, 28))) {
            const char* fname = (modeChip == 0) ? "stats_live.csv" : "stats_replay.csv";
            ExportStatsCSV(history);
            const char* vpath = (modeChip == 0) ? "/data/stats_live.csv" : "/data/stats_replay.csv";
            TriggerBrowserDownload(vpath, fname);
        }
        ImGui::SameLine(0, 12);
        if (ImGui::Button("Export Performance CSV", ImVec2(220, 28))) {
            const char* fname = (modeChip == 0) ? "performance_live.csv" : "performance_replay.csv";
            ExportPerformanceCSV(history);
            const char* vpath = (modeChip == 0) ? "/data/performance_live.csv" : "/data/performance_replay.csv";
            TriggerBrowserDownload(vpath, fname);
        }
    }

    // =============================================================
    // TAB 3: PER-SYMBOL BREAKDOWN — UNCHANGED
    // =============================================================
    static void RenderPerSymbol(TradeHistory& history) {
        TradeStats s = history.CalculateStats();

        if (s.tradesPerSymbol.empty()) {
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f),
                               "Belum ada data per-symbol.");
            return;
        }

        ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "P&L per Symbol:");

        double maxPnL = 0;
        for (auto& [sym, pnl] : s.pnlPerSymbol) {
            if (fabs(pnl) > maxPnL) maxPnL = fabs(pnl);
        }

        ImGui::Separator();

        static ImGuiTableFlags tblFlags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable;

        if (ImGui::BeginTable("SymbolTable", 5, tblFlags)) {
            ImGui::TableSetupColumn("Symbol");
            ImGui::TableSetupColumn("Trades",  ImGuiTableColumnFlags_WidthFixed, 60);
            ImGui::TableSetupColumn("Win Rate", ImGuiTableColumnFlags_WidthFixed, 70);
            ImGui::TableSetupColumn("P&L");
            ImGui::TableSetupColumn("Visual",   ImGuiTableColumnFlags_WidthStretch, 0);
            ImGui::TableHeadersRow();

            std::vector<std::pair<std::string, double>> sortedSymbols(
                s.pnlPerSymbol.begin(), s.pnlPerSymbol.end());
            std::sort(sortedSymbols.begin(), sortedSymbols.end(),
                [](const auto& a, const auto& b) { return a.second > b.second; });

            for (const auto& [sym, pnl] : sortedSymbols) {
                int trades = s.tradesPerSymbol[sym];
                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::Text("%s", sym.c_str());
                ImGui::TableNextColumn(); ImGui::Text("%d", trades);

                int symWins = 0;
                for (const auto& t : history.closedTrades) {
                    if (t.symbol == sym && t.profit > 0.0001) symWins++;
                }
                double symWR = (trades > 0) ? (double)symWins / trades * 100.0 : 0;
                ImGui::TableNextColumn();
                ImGui::TextColored(
                    (symWR >= 50) ? ImVec4(0.2f,0.9f,0.3f,1) : ImVec4(1.0f,0.6f,0.1f,1),
                    "%.0f%%", symWR);

                ImGui::TableNextColumn();
                ImVec4 pCol = (pnl >= 0) ? ImVec4(0.2f,1.0f,0.2f,1) : ImVec4(1.0f,0.3f,0.3f,1);
                ImGui::TextColored(pCol, "$%s", TH_FormatUSD(pnl).c_str());

                ImGui::TableNextColumn();
                if (maxPnL > 0.0001) {
                    float barWidth = (float)(fabs(pnl) / maxPnL);
                    ImVec2 p = ImGui::GetCursorScreenPos();
                    ImDrawList* draw = ImGui::GetWindowDrawList();
                    float barH = ImGui::GetTextLineHeight() * 0.6f;
                    float barY = p.y + (ImGui::GetTextLineHeight() - barH) * 0.5f;
                    float maxBarW = ImGui::GetContentRegionAvail().x;

                    if (pnl >= 0) {
                        draw->AddRectFilled(
                            ImVec2(p.x, barY),
                            ImVec2(p.x + barWidth * maxBarW, barY + barH),
                            IM_COL32(40, 200, 80, 180), 2.0f);
                    } else {
                        draw->AddRectFilled(
                            ImVec2(p.x, barY),
                            ImVec2(p.x + barWidth * maxBarW, barY + barH),
                            IM_COL32(200, 60, 60, 180), 2.0f);
                    }
                }

                ImGui::Dummy(ImVec2(0, ImGui::GetTextLineHeight()));
            }
            ImGui::EndTable();
        }
    }

    // =============================================================
    // CHART RENDERERS
    // =============================================================

    // KPI Card dengan fixed position & width
    static void StatCardFixed(const char* label, const char* value, ImVec4 valColor,
                              const char* subText,
                              float x, float y, float w, float h) {
        ImDrawList* draw = ImGui::GetWindowDrawList();

        // Reserve layout space (invisible)
        // Background
        draw->AddRectFilled(ImVec2(x, y), ImVec2(x + w, y + h),
                            IM_COL32(28, 31, 42, 255), 6.0f);
        // Border
        draw->AddRect(ImVec2(x, y), ImVec2(x + w, y + h),
                      IM_COL32(55, 60, 78, 180), 6.0f);
        // Color indicator strip at top
        ImU32 stripColor = ImGui::ColorConvertFloat4ToU32(valColor);
        draw->AddRectFilled(ImVec2(x + 8, y + 2), ImVec2(x + w - 8, y + 4),
                            stripColor, 2.0f);

        // Label (muted)
        ImGui::SetCursorScreenPos(ImVec2(x + 12, y + 10));
        ImGui::TextColored(ImVec4(0.52f, 0.55f, 0.65f, 1.0f), "%s", label);

        // Value (bold color)
        ImGui::SetCursorScreenPos(ImVec2(x + 12, y + 28));
        ImGui::TextColored(valColor, "%s", value);

        // Sub text (dim)
        ImGui::SetCursorScreenPos(ImVec2(x + 12, y + 52));
        ImGui::TextColored(ImVec4(0.42f, 0.45f, 0.55f, 1.0f), "%s", subText);
    }

    // Equity Curve — cumulative P/L line chart starting from deposit
    static void RenderEquityCurve(TradeHistory& history, double deposit = 10000.0) {
        if (history.closedTrades.empty()) {
            ImGui::TextDisabled("No trade data for equity curve.");
            return;
        }

        ImVec2 avail = ImGui::GetContentRegionAvail();
        if (avail.x < 80) return;

        float chartH = 140.0f;
        ImVec2 chartP = ImGui::GetCursorScreenPos();
        float chartW = avail.x;

        // Reserve layout space
        ImGui::Dummy(ImVec2(chartW, chartH));

        ImDrawList* draw = ImGui::GetWindowDrawList();

        // Background
        draw->AddRectFilled(chartP, ImVec2(chartP.x + chartW, chartP.y + chartH),
                            IM_COL32(22, 25, 35, 255), 4.0f);

        // Calculate cumulative equity (starting from deposit)
        int n = (int)history.closedTrades.size();
        std::vector<float> cumPnL(n);
        float cumulative = (float)deposit;
        float minVal = (float)deposit, maxVal = (float)deposit;
        for (int i = 0; i < n; i++) {
            cumulative += (float)history.closedTrades[i].profit;
            cumPnL[i] = cumulative;
            if (cumulative < minVal) minVal = cumulative;
            if (cumulative > maxVal) maxVal = cumulative;
        }

        // Ensure valid range
        if (maxVal <= minVal) { maxVal = minVal + 1.0f; }
        float range = maxVal - minVal;

        // Padding 8% each side
        float pad = range * 0.08f;
        minVal -= pad;
        maxVal += pad;
        range = maxVal - minVal;

        // Plot area
        float plotL = chartP.x + 50;
        float plotR = chartP.x + chartW - 12;
        float plotT = chartP.y + 10;
        float plotB = chartP.y + chartH - 22;
        float plotW = plotR - plotL;
        float plotH = plotB - plotT;

        // Grid lines (5 horizontal)
        for (int i = 0; i <= 4; i++) {
            float y = plotT + (float)i / 4.0f * plotH;
            draw->AddLine(ImVec2(plotL, y), ImVec2(plotR, y),
                          IM_COL32(42, 47, 62, 200), 1.0f);

            // Y-axis label
            float val = maxVal - (float)i / 4.0f * range;
            char lbl[32];
            snprintf(lbl, 32, "$%.0f", val);
            draw->AddText(ImVec2(chartP.x + 4, y - 7), IM_COL32(85, 90, 108, 200), lbl);
        }

        // Deposit reference line (horizontal dashed line at deposit level)
        float depositY = plotT + plotH * (1.0f - ((float)deposit - minVal) / range);
        draw->AddLine(ImVec2(plotL, depositY), ImVec2(plotR, depositY),
                      IM_COL32(100, 100, 140, 140), 1.0f);
        char depLbl[32];
        snprintf(depLbl, 32, "Deposit: $%.0f", deposit);
        draw->AddText(ImVec2(plotL, depositY - 14), IM_COL32(100, 105, 130, 200), depLbl);

        // Determine line color based on final equity vs deposit
        ImU32 lineColor = (cumPnL.back() >= (float)deposit) ? IM_COL32(80, 220, 120, 255)
                                                            : IM_COL32(220, 80, 80, 255);

        // Plot STEP CHART (not smooth — each trade = horizontal step + vertical jump)
        if (n >= 2) {
            // Step width per trade (evenly spaced)
            float stepW = plotW / (float)n;
            // Half-step offset so steps are centered on trade markers
            float halfStep = stepW * 0.5f;

            for (int i = 0; i < n; i++) {
                float xLeft  = plotL + (float)i * stepW;
                float xRight = xLeft + stepW;
                float yVal   = plotT + plotH * (1.0f - (cumPnL[i] - minVal) / range);

                // Color segment: green if above deposit, red if below
                ImU32 segColor = (cumPnL[i] >= (float)deposit) ? IM_COL32(80, 210, 110, 240)
                                                               : IM_COL32(210, 75, 75, 240);

                // Horizontal step at this trade's equity level
                draw->AddLine(ImVec2(xLeft, yVal), ImVec2(xRight, yVal), segColor, 2.0f);

                // Vertical connector to next trade's level (except last)
                if (i < n - 1) {
                    float yNext = plotT + plotH * (1.0f - (cumPnL[i+1] - minVal) / range);
                    // Use the outgoing color (current trade's color) for the vertical line
                    draw->AddLine(ImVec2(xRight, yVal), ImVec2(xRight, yNext), segColor, 1.5f);
                }

                // Small dot at each trade step boundary
                draw->AddCircleFilled(ImVec2(xRight, yVal), 2.5f, segColor);
            }

            // End dot (pulsing effect — larger circle)
            float endY = plotT + plotH * (1.0f - (cumPnL.back() - minVal) / range);
            draw->AddCircleFilled(ImVec2(plotR, endY), 5.0f, lineColor);
            draw->AddCircle(ImVec2(plotR, endY), 5.0f, lineColor, 0, 1.5f);

            // Start dot
            float startY = plotT + plotH * (1.0f - (cumPnL[0] - minVal) / range);
            draw->AddCircleFilled(ImVec2(plotL, startY), 3.5f, IM_COL32(120, 130, 150, 220));
        } else if (n == 1) {
            float y = plotT + plotH * (1.0f - (cumPnL[0] - minVal) / range);
            draw->AddCircleFilled(ImVec2(plotL, y), 5.0f, lineColor);
        }

        // X-axis label (trade count)
        char xLabel[48];
        snprintf(xLabel, 48, "%d trades", n);
        draw->AddText(ImVec2(plotR - 55, plotB + 4), IM_COL32(85, 90, 108, 200), xLabel);

        // End value label: show equity + P/L from deposit
        char endLabel[64];
        float plFromDep = cumPnL.back() - (float)deposit;
        const char* plSign = (plFromDep >= 0) ? "+" : "";
        snprintf(endLabel, 64, "Equity: $%.2f (%s$%.2f)", cumPnL.back(), plSign, fabs(plFromDep));
        draw->AddText(ImVec2(plotL, plotB + 4), lineColor, endLabel);
    }

    // Win / Loss horizontal stacked bar
    static void RenderWinLossBar(const TradeStats& s) {
        ImVec2 avail = ImGui::GetContentRegionAvail();
        float barW = avail.x;
        float barH = 38.0f;
        ImVec2 barP = ImGui::GetCursorScreenPos();

        ImGui::Dummy(ImVec2(barW, barH));

        ImDrawList* draw = ImGui::GetWindowDrawList();

        // Background
        draw->AddRectFilled(barP, ImVec2(barP.x + barW, barP.y + barH),
                            IM_COL32(22, 25, 35, 255), 4.0f);

        if (s.totalTrades <= 0) return;

        float padX = 4.0f, padY = 4.0f;
        float innerW = barW - padX * 2;
        float innerH = barH - padY * 2;
        float x = barP.x + padX;
        float y = barP.y + padY;

        float winFrac  = (float)s.wins / s.totalTrades;
        float beFrac   = (float)s.breakevens / s.totalTrades;
        float lossFrac = 1.0f - winFrac - beFrac;
        if (lossFrac < 0) lossFrac = 0;

        // Wins (green)
        float winW = winFrac * innerW;
        if (s.wins > 0 && winW > 2) {
            draw->AddRectFilled(ImVec2(x, y), ImVec2(x + winW - 1, y + innerH),
                                IM_COL32(45, 175, 75, 220), 4.0f);
            if (winW > 55) {
                char buf[48];
                snprintf(buf, 48, "%dW (%.0f%%)", s.wins, winFrac * 100);
                draw->AddText(ImVec2(x + 8, y + innerH / 2 - 7),
                              IM_COL32(255, 255, 255, 230), buf);
            }
            x += winW;
        }

        // Breakevens (gray)
        float beW = beFrac * innerW;
        if (s.breakevens > 0 && beW > 2) {
            draw->AddRectFilled(ImVec2(x, y), ImVec2(x + beW - 1, y + innerH),
                                IM_COL32(85, 90, 108, 180));
            x += beW;
        }

        // Losses (red)
        float lossW = lossFrac * innerW;
        if (s.losses > 0 && lossW > 2) {
            draw->AddRectFilled(ImVec2(x, y), ImVec2(x + lossW - padX, y + innerH),
                                IM_COL32(185, 55, 55, 220), 4.0f);
            if (lossW > 55) {
                char buf[48];
                snprintf(buf, 48, "%dL (%.0f%%)", s.losses, lossFrac * 100);
                draw->AddText(ImVec2(x + 8, y + innerH / 2 - 7),
                              IM_COL32(255, 255, 255, 230), buf);
            }
        }
    }

    // Section title with separator
    static void SectionTitle(const char* title) {
        ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.3f, 0.35f, 0.5f, 0.8f));
        ImGui::Separator();
        ImGui::PopStyleColor();
        ImGui::TextColored(ImVec4(0.72f, 0.76f, 0.88f, 1.0f), "%s", title);
    }

    // =============================================================
    // EXPORT FUNCTIONS
    // =============================================================

    // Export ringkasan statistik ke CSV
    static void ExportStatsCSV(TradeHistory& history) {
        TradeStats s = history.CalculateStats();
        const char* mode = (modeChip == 0) ? "LIVE" : "REPLAY";

        const char* path = (modeChip == 0) ? "/data/stats_live.csv"
                                          : "/data/stats_replay.csv";

        std::ofstream o(path);
        if (!o.is_open()) {
            snprintf(s_downloadStatus, sizeof(s_downloadStatus),
                     "[ERROR] Failed to write %s", path);
            s_downloadTimer = 4.0f;
            return;
        }

        o << "Category,Metric,Value\n";
        o << "Mode,," << mode << "\n";
        o << "\n";
        o << "General,Total Trades," << s.totalTrades << "\n";
        o << "General,Wins," << s.wins << "\n";
        o << "General,Losses," << s.losses << "\n";
        o << "General,Breakevens," << s.breakevens << "\n";
        o << "General,Win Rate (%)," << std::fixed << std::setprecision(1) << s.winRate << "\n";
        o << "\n";
        o << "Performance,Avg Win," << std::setprecision(2) << s.avgWin << "\n";
        o << "Performance,Avg Loss," << s.avgLoss << "\n";
        o << "Performance,Avg Trade," << s.avgTrade << "\n";
        o << "Performance,Largest Win," << s.largestWin << "\n";
        o << "Performance,Largest Loss," << s.largestLoss << "\n";
        o << "\n";
        o << "Financial,Gross Profit," << s.grossProfit << "\n";
        o << "Financial,Gross Loss," << s.grossLoss << "\n";
        o << "Financial,Net Profit," << s.netProfit << "\n";
        o << "Financial,Profit Factor," << std::setprecision(2) << s.profitFactor << "\n";
        o << "\n";
        o << "Streaks,Max Consec Win," << s.maxConsecWin << "\n";
        o << "Streaks,Max Consec Loss," << s.maxConsecLoss << "\n";
        o << "Streaks,Avg RR," << std::setprecision(2) << s.avgRR << "\n";
        o.close();

        snprintf(s_downloadStatus, sizeof(s_downloadStatus),
                 "Statistics CSV saved (%s)", mode);
        s_downloadTimer = 3.0f;
        SyncToDisk();
    }

    // Export performa per-trade dengan cumulative P/L
    static void ExportPerformanceCSV(TradeHistory& history) {
        const char* mode = (modeChip == 0) ? "LIVE" : "REPLAY";
        const char* path = (modeChip == 0) ? "/data/performance_live.csv"
                                          : "/data/performance_replay.csv";

        std::ofstream o(path);
        if (!o.is_open()) {
            snprintf(s_downloadStatus, sizeof(s_downloadStatus),
                     "[ERROR] Failed to write %s", path);
            s_downloadTimer = 4.0f;
            return;
        }

        o << "# Mode: " << mode << "\n";
        o << "TradeID,Symbol,Type,Entry,Close,SL,TP,Profit,RR,CumulativePnL,Duration(s),CloseReason,OpenTime,CloseTime\n";

        double cumulative = 0;
        for (const auto& t : history.closedTrades) {
            cumulative += t.profit;
            double rr = t.GetRRRatio();
            o << t.id << ","
              << t.symbol << ","
              << t.typeStr << ","
              << std::fixed << std::setprecision(2)
              << t.entryPrice << ","
              << t.closePrice << ","
              << t.sl << ","
              << t.tp << ","
              << t.profit << ","
              << std::setprecision(2) << rr << ","
              << std::setprecision(2) << cumulative << ","
              << std::setprecision(0) << t.duration << ","
              << "\"" << t.closeReason << "\","
              << t.openTime << ","
              << t.closeTime << "\n";
        }
        o.close();

        snprintf(s_downloadStatus, sizeof(s_downloadStatus),
                 "Performance CSV saved (%s, %d trades)", mode, (int)history.closedTrades.size());
        s_downloadTimer = 3.0f;
        SyncToDisk();
    }

    // Export comparison Live vs Replay ke CSV
    static void ExportCompareCSV(TradeHistory& live, TradeHistory& replay) {
        TradeStats sL = live.CalculateStats();
        TradeStats sR = replay.CalculateStats();

        const char* path = "/data/compare_live_replay.csv";
        std::ofstream o(path);
        if (!o.is_open()) {
            snprintf(s_downloadStatus, sizeof(s_downloadStatus),
                     "[ERROR] Failed to write %s", path);
            s_downloadTimer = 4.0f;
            return;
        }

        o << "Metric,LIVE,REPLAY\n";
        o << "Total Trades," << sL.totalTrades << "," << sR.totalTrades << "\n";
        o << "Wins," << sL.wins << "," << sR.wins << "\n";
        o << "Losses," << sL.losses << "," << sR.losses << "\n";
        o << "Breakevens," << sL.breakevens << "," << sR.breakevens << "\n";
        o << std::fixed << std::setprecision(1);
        o << "Win Rate (%)," << sL.winRate << "," << sR.winRate << "\n";
        o << std::setprecision(2);
        o << "Net P&L," << sL.netProfit << "," << sR.netProfit << "\n";
        o << "Avg Win," << sL.avgWin << "," << sR.avgWin << "\n";
        o << "Avg Loss," << sL.avgLoss << "," << sR.avgLoss << "\n";
        o << "Gross Profit," << sL.grossProfit << "," << sR.grossProfit << "\n";
        o << "Gross Loss," << sL.grossLoss << "," << sR.grossLoss << "\n";
        o << "Profit Factor," << sL.profitFactor << "," << sR.profitFactor << "\n";
        o << "Avg RR," << sL.avgRR << "," << sR.avgRR << "\n";
        o << "Max Consec Win," << sL.maxConsecWin << "," << sR.maxConsecWin << "\n";
        o << "Max Consec Loss," << sL.maxConsecLoss << "," << sR.maxConsecLoss << "\n";
        o << "Largest Win," << sL.largestWin << "," << sR.largestWin << "\n";
        o << "Largest Loss," << sL.largestLoss << "," << sR.largestLoss << "\n";
        o.close();

        snprintf(s_downloadStatus, sizeof(s_downloadStatus),
                 "Comparison CSV saved (Live vs Replay)");
        s_downloadTimer = 3.0f;
        SyncToDisk();
    }

    // =============================================================
    // UTILITY HELPERS
    // =============================================================

    // Detail Row: Label + Value (same line)
    static void DetailRow(const char* label, const char* fmt, ...) {
        ImGui::TextColored(ImVec4(0.55f, 0.58f, 0.68f, 1.0f), "%s", label);
        ImGui::SameLine(0, 8);

        char buf[128];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);

        ImGui::Text("%s", buf);
    }

    // Format durasi detik -> "2h 15m" / "45m 30s" / "12s"
    static std::string FormatDuration(double seconds) {
        if (seconds < 0) return "0s";
        int totalSec = (int)seconds;
        int h = totalSec / 3600;
        int m = (totalSec % 3600) / 60;
        int s = totalSec % 60;

        std::string result;
        if (h > 0) result += std::to_string(h) + "h ";
        if (m > 0) result += std::to_string(m) + "m ";
        if (s > 0 || result.empty()) result += std::to_string(s) + "s";
        return result;
    }

    // =============================================================
    // BROWSER DOWNLOAD — Trigger file download from virtual FS
    // =============================================================
    static void TriggerBrowserDownload(const char* vpath, const char* downloadName) {
        #ifdef __EMSCRIPTEN__
            EM_ASM({
                try {
                    // Read file from Emscripten virtual FS
                    var data = FS.readFile(UTF8ToString($0), { encoding: 'binary' });
                    if (!data || data.length === 0) {
                        console.error('[Download] File empty or not found:', UTF8ToString($0));
                        return;
                    }

                    // Determine MIME type from extension
                    var fname = UTF8ToString($1);
                    var mime = 'application/octet-stream';
                    if (fname.endsWith('.csv')) mime = 'text/csv;charset=utf-8';
                    else if (fname.endsWith('.json')) mime = 'application/json;charset=utf-8';

                    // Create Blob and trigger download
                    var blob = new Blob([data], { type: mime });
                    var url = URL.createObjectURL(blob);
                    var a = document.createElement('a');
                    a.href = url;
                    a.download = fname;
                    a.style.display = 'none';
                    document.body.appendChild(a);
                    a.click();

                    // Cleanup after short delay
                    setTimeout(function() {
                        document.body.removeChild(a);
                        URL.revokeObjectURL(url);
                    }, 250);

                    console.log('[Download] Triggered download:', fname, '(' + data.length + ' bytes)');
                } catch(e) {
                    console.error('[Download] Error:', e);
                }
            }, vpath, downloadName);
        #else
            (void)vpath;
            (void)downloadName;
        #endif
    }

    // Download status toast — call at end of Render()
    static void ShowDownloadToast(float deltaTime) {
        if (s_downloadTimer <= 0.0f) return;
        s_downloadTimer -= deltaTime;

        float alpha = (s_downloadTimer < 0.5f) ? (s_downloadTimer / 0.5f) : 1.0f;

        ImVec2 displaySize = ImGui::GetIO().DisplaySize;
        float toastW = 320.0f;
        float toastH = 36.0f;
        float toastX = (displaySize.x - toastW) * 0.5f;
        float toastY = displaySize.y - 60.0f;

        ImDrawList* draw = ImGui::GetForegroundDrawList();
        ImU32 bgCol = IM_COL32(20, 80, 40, (int)(220 * alpha));
        ImU32 borderCol = IM_COL32(40, 200, 80, (int)(180 * alpha));
        ImU32 textCol = IM_COL32(180, 255, 200, (int)(255 * alpha));

        draw->AddRectFilled(
            ImVec2(toastX, toastY),
            ImVec2(toastX + toastW, toastY + toastH),
            bgCol, 8.0f);
        draw->AddRect(
            ImVec2(toastX, toastY),
            ImVec2(toastX + toastW, toastY + toastH),
            borderCol, 8.0f);

        const char* icon = (s_downloadStatus[0] == '[' && s_downloadStatus[1] == 'E') ? "[!]" : "[OK]";
        char displayText[280];
        snprintf(displayText, sizeof(displayText), " %s %s", icon, s_downloadStatus);
        draw->AddText(
            ImVec2(toastX + 12, toastY + 10),
            textCol, displayText);
    }

    // Sync ke disk (WebAssembly IDBFS)
    static void SyncToDisk() {
        #ifdef __EMSCRIPTEN__
            EM_ASM(
                FS.syncfs(false, function(err) {
                    if(err) console.log("Sync fail:", err);
                    else console.log("History synced to disk!");
                });
            );
        #endif
    }

    // Public: flag untuk reset replay data (dibaca oleh main loop)
    static inline bool s_resetReplayRequested = false;
};
