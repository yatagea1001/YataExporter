#pragma once
#include "imgui.h"
#include <string>
#include <vector>
#include <map>
#include <cmath>
#include <algorithm>
#include "TextureHelper.h"

// Struktur Data per Simbol
struct MarketRow {
    std::string symbol;
    ImTextureID iconTex    = 0;
    double currentPrice    = 0.0;
    double dailyOpenPrice  = 0.0;
    double prevDayClose    = 0.0;  // 🆕 Untuk hitung % change yang akurat
    double dailyHigh       = -1e9;
    double dailyLow        =  1e9;
    double volume          = 0.0;
    bool   isUp            = true;
    bool   isReady         = false; // 🆕 false = "Waiting...", true = ada data

    std::vector<float> tickHistory;
    int maxHistory = 50;
};

class MarketWatchPanel {
private:
    std::map<std::string, MarketRow> marketData;
    std::vector<std::string> symbolList;

public:
    bool isOpen = true; // show/hide dari RenderRightBar()
    // =========================================================================
    // CONSTRUCTOR: Daftar semua symbol + icon
    // =========================================================================
    MarketWatchPanel() {
        AddSymbol("XAUUSD", texIconGold);
        AddSymbol("EURUSD", texIconEuro);
        AddSymbol("GBPUSD", texIconPound);
        AddSymbol("BTCUSDT", texIconBTC);
        AddSymbol("ETHUSDT", texIconETH);
    }

    void AddSymbol(const std::string& sym, ImTextureID ico) {
        symbolList.push_back(sym);
        marketData[sym].symbol  = sym;
        marketData[sym].iconTex = ico;
    }

    // =========================================================================
    // UPDATE TICK: Dipanggil dari wasm_push_tick() untuk SEMUA symbol
    // Tidak perlu fungsi terpisah, cukup ini saja.
    // =========================================================================
    // Ambil harga live terkini untuk symbol apapun (dipakai oleh price overlay non-primary tab)
    double GetLivePrice(const std::string& sym) const {
        auto it = marketData.find(sym);
        if (it == marketData.end()) return 0.0;
        return it->second.currentPrice;
    }

    void UpdateTick(const std::string& sym, double price, double vol) {
        auto it = marketData.find(sym);
        if (it == marketData.end()) return;

        MarketRow& row = it->second;

        // Direction arrow
        if      (price > row.currentPrice) row.isUp = true;
        else if (price < row.currentPrice) row.isUp = false;

        row.currentPrice = price;
        row.volume       = vol;
        row.isReady      = true; // 🆕 Ada data masuk, berarti siap ditampilkan

        // Daily stats
        if (row.dailyOpenPrice == 0.0) row.dailyOpenPrice = price;
        if (price > row.dailyHigh)     row.dailyHigh = price;
        if (price < row.dailyLow)      row.dailyLow  = price;

        // Sparkline history
        row.tickHistory.push_back((float)price);
        if ((int)row.tickHistory.size() > row.maxHistory)
            row.tickHistory.erase(row.tickHistory.begin());
    }

    // =========================================================================
    // RENDER UI
    // =========================================================================
    void Render(bool* p_open = nullptr, const std::string& activeSymbol = "") {
        // Kalau p_open tidak di-pass dari luar, pakai isOpen internal
        bool* openPtr = p_open ? p_open : &isOpen;
        if (!(*openPtr)) return; // tidak render kalau ditutup

        ImGui::SetNextWindowSize(ImVec2(380, 300), ImGuiCond_FirstUseEver);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.07f, 0.07f, 0.09f, 0.97f));

        if (!ImGui::Begin("Market Watch", openPtr,
                ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
            ImGui::End();
            ImGui::PopStyleColor();
            return;
        }

        // === TITLE BAR MINI ===
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.8f, 1.0f, 1.0f));
        ImGui::Text("WATCH LIST");
        ImGui::PopStyleColor();
        ImGui::Separator();

        // === TABLE ===
        ImGuiTableFlags tflags =
            ImGuiTableFlags_RowBg       |
            ImGuiTableFlags_BordersInnerV |
            ImGuiTableFlags_NoBordersInBody |
            ImGuiTableFlags_SizingStretchProp;

        // 4 Kolom: PAIR | PRICE | CHANGE | SPARKLINE
        if (!ImGui::BeginTable("MWTable", 4, tflags)) {
            ImGui::End();
            ImGui::PopStyleColor();
            return;
        }

        ImGui::TableSetupColumn("SIMBOL",    ImGuiTableColumnFlags_WidthFixed,   100.0f);
        ImGui::TableSetupColumn("PRICE",   ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("CHG%",    ImGuiTableColumnFlags_WidthFixed,    55.0f);
        ImGui::TableSetupColumn("VOLUME",   ImGuiTableColumnFlags_WidthFixed,    60.0f);

        // Header Row
        ImGui::PushStyleColor(ImGuiCol_TableHeaderBg, ImVec4(0.15f, 0.15f, 0.2f, 1.0f));
        ImGui::TableHeadersRow();
        ImGui::PopStyleColor();

        for (const auto& sym : symbolList) {
            MarketRow& row = marketData[sym];

            ImGui::TableNextRow(ImGuiTableRowFlags_None, 28.0f);

            // -----------------------------------------------------------------
            // KOLOM 0: ICON + NAMA (clickable)
            // -----------------------------------------------------------------
            ImGui::TableSetColumnIndex(0);

            // Highlight row jika ini symbol aktif di chart
            bool isActive = (sym == activeSymbol);
            if (isActive) {
                ImVec2 rowMin = ImGui::GetItemRectMin();
                // Tint baris aktif dengan warna biru transparan
                ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0,
                    IM_COL32(30, 80, 140, 80));
            }

            // Icon
            if (row.iconTex) {
                float iconY = ImGui::GetCursorPosY() + 4.0f;
                ImGui::SetCursorPosY(iconY);
                ImGui::Image(row.iconTex, ImVec2(18, 18));
                ImGui::SameLine(0, 6);
                ImGui::SetCursorPosY(iconY + 1.0f);
            }

            // Nama symbol (clickable → switch chart)
            ImGuiSelectableFlags sel_flags =
                ImGuiSelectableFlags_SpanAllColumns |
                ImGuiSelectableFlags_AllowOverlap;

            if (ImGui::Selectable(row.symbol.c_str(), isActive, sel_flags, ImVec2(0, 22))) {
                // 🔥 SATU-SATUNYA TEMPAT SWITCH SYMBOL
                // SwitchSymbol di main.cpp akan:
                //   1. Clear g_allCandles
                //   2. Set g_symbol = newSym
                //   3. Call JS: SetActiveSymbol('newSym')
                extern void SwitchSymbol(const std::string&);
                SwitchSymbol(row.symbol);
            }

            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Click to chart: %s", row.symbol.c_str());
            }

            // -----------------------------------------------------------------
            // KOLOM 1: HARGA
            // -----------------------------------------------------------------
            ImGui::TableSetColumnIndex(1);

            if (!row.isReady) {
                // Belum ada data sama sekali
                ImGui::TextDisabled("---");
            } else {
                // Warna: hijau naik, merah turun
                ImVec4 col = row.isUp
                    ? ImVec4(0.20f, 0.95f, 0.55f, 1.0f)  // hijau mint
                    : ImVec4(0.95f, 0.30f, 0.30f, 1.0f);  // merah

                ImGui::PushStyleColor(ImGuiCol_Text, col);

                // Format: > 500 pakai .2f (XAUUSD, BTC), sisanya .5f (forex)
                if (row.currentPrice > 500.0)
                    ImGui::Text("%.2f", row.currentPrice);
                else
                    ImGui::Text("%.5f", row.currentPrice);

                ImGui::PopStyleColor();
            }

            // -----------------------------------------------------------------
            // KOLOM 2: % CHANGE (dari daily open)
            // -----------------------------------------------------------------
            ImGui::TableSetColumnIndex(2);

            if (row.isReady && row.dailyOpenPrice > 0.0) {
                double pct = ((row.currentPrice - row.dailyOpenPrice)
                              / row.dailyOpenPrice) * 100.0;

                ImVec4 pctCol = (pct >= 0)
                    ? ImVec4(0.20f, 0.95f, 0.55f, 1.0f)
                    : ImVec4(0.95f, 0.30f, 0.30f, 1.0f);

                ImGui::PushStyleColor(ImGuiCol_Text, pctCol);
                ImGui::Text("%+.2f%%", pct);
                ImGui::PopStyleColor();
            } else {
                ImGui::TextDisabled("--.--%");
            }

            // -----------------------------------------------------------------
            // KOLOM 3: SPARKLINE
            // -----------------------------------------------------------------
            ImGui::TableSetColumnIndex(3);

            if (row.tickHistory.size() >= 2) {
                ImVec4 sparkCol = row.isUp
                    ? ImVec4(0.20f, 0.95f, 0.55f, 0.9f)
                    : ImVec4(0.95f, 0.30f, 0.30f, 0.9f);

                ImGui::PushStyleColor(ImGuiCol_PlotLines,    sparkCol);
                ImGui::PushStyleColor(ImGuiCol_FrameBg,      ImVec4(0, 0, 0, 0));
                ImGui::PlotLines("##sp",
                    row.tickHistory.data(),
                    (int)row.tickHistory.size(),
                    0, nullptr,
                    FLT_MAX, FLT_MAX,
                    ImVec2(55.0f, 24.0f));
                ImGui::PopStyleColor(2);
            } else if (row.isReady) {
                ImGui::TextDisabled("...");
            }
        }

        ImGui::EndTable();
        ImGui::End();
        ImGui::PopStyleColor();
    }
};