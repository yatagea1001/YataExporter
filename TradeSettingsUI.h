// =================================================================================
// TradeSettingsUI.h  popup pengaturan visual trade (garis, warna, zona, dll)
// =================================================================================
#pragma once
#include "imgui.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <cmath>
#include <algorithm>

// =================================================================================
// TradeZoneRow — data untuk tabel trade aktif (show/hide zona per trade)
// Diisi dari TradeModule.h, tidak ada dependency ke LiveTrade/TradeManager
// =================================================================================
struct TradeZoneRow {
    int id;
    std::string symbol;
    std::string typeStr;   // "BUY" / "SELL"
    double entryPrice;
    double profit;
    bool* pShowZones;       // pointer ke LiveTrade::showZones
};

// =================================================================================
// TradeVisualSettings — Global pengaturan visual trade
// Disimpan di sini agar bisa diakses TradeModule.h & main.cpp
// =================================================================================
struct TradeVisualSettings {

    // --- GARIS (LINES) ---
    // Style & ketebalan GABUNG untuk semua garis (Entry, SL, TP)
    int lineStyle = 1;           // 0 = Solid, 1 = Dashed, 2 = Dotted
    float lineThickness = 1.5f;  // ketebalan semua garis
    bool showLineLabels = false;  // tampilkan harga di garis SL/TP

    // --- WARNA (COLORS) — PISAH per garis ---
    float entryColorBuy[4]  = { 0.0f, 0.6f, 1.0f, 1.0f };   // biru
    float entryColorSell[4] = { 1.0f, 0.4f, 0.4f, 1.0f };   // merah
    float slColor[4] = { 1.0f, 0.3f, 0.3f, 1.0f };          // merah
    float tpColor[4] = { 0.0f, 0.8f, 0.4f, 1.0f };          // hijau

    // --- ZONA (ZONES) ---
    float lossZoneColor[4] = { 1.0f, 0.2f, 0.2f, 1.0f };    // merah
    float winZoneColor[4]  = { 0.0f, 0.85f, 0.6f, 1.0f };   // hijau
    bool showZones          = true;   // default tampilkan zona untuk trade BARU
    float zoneAlphaSelected = 0.22f;
    float zoneAlphaNormal   = 0.15f;
    int zoneLabelMode = 0;          // 0 = Harga + RR, 1 = Harga Saja, 2 = RR Saja
    float zoneFontScale = 1.0f;

    // --- GHOST ZONE (RIWAYAT) ---
    bool showGhostZones     = true;   // tampilkan zona trade yang sudah close
    float ghostAlphaMult    = 1.0f;   // pengali transparansi ghost
    bool showGhostBadge     = true;   // badge profit pada ghost zone
    bool showGhostLabels    = false;  // label harga/RR pada ghost zone

    // --- GENERAL ---
    bool showRRBadge        = true;   // badge RR di kanan
    bool showConnector      = true;   // garis konektor SL-TP
    float defaultSLOffset   = 0.3f;   // default SL offset (% dari entry)
    float defaultRR         = 2.0f;   // default Risk:Reward ratio

    // --- DEFAULT SL/TP SAAT OPEN TRADE ---
    void CalcDefaultSLTP(TradeType type, double entryPrice, double& outSL, double& outTP) {
        double offset = entryPrice * (defaultSLOffset / 100.0);
        if (type == TRADE_BUY) {
            outSL = entryPrice - offset;
            outTP = entryPrice + offset * defaultRR;
        } else {
            outSL = entryPrice + offset;
            outTP = entryPrice - offset * defaultRR;
        }
    }
};

// --- Global instance ---
inline TradeVisualSettings g_TradeSettings;

// =================================================================================
// CLASS: TradeSettingsUI — Popup pengaturan visual trade
// =================================================================================
class TradeSettingsUI {
public:
    bool showPopup = false;

    // Texture ikon gear — set dari main.cpp setelah InitIcons()
    static ImTextureID settingIconTex;

    // Set trade data sebelum Render() — diisi dari TradeModule.h
    void SetTradeData(const std::vector<TradeZoneRow>* data) { m_tradeData = data; }

    // Panggil sekali per frame
    void Render() {
        if (!showPopup) return;
        RenderPopup();
    }

    void Toggle() { showPopup = !showPopup; }
    void Open() { showPopup = true; }
    void Close() { showPopup = false; }

private:
    const std::vector<TradeZoneRow>* m_tradeData = nullptr;

    // Layout: lebar kolom label (kiri) dan widget (kanan)
    static constexpr float COL_LABEL  = 158.0f;
    static constexpr float COL_WIDGET = 175.0f;

    // =========================================================
    // HELPER: Section Header
    // =========================================================
    void SectionHeader(const char* title) {
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.45f, 0.78f, 1.0f, 1.0f));
        ImGui::Text("%s", title);
        ImGui::PopStyleColor();
        ImGui::Separator();
        ImGui::Spacing();
    }

    // =========================================================
    // HELPER: Styled Toggle (FIX: unique ID per label, no SameLine overflow)
    // =========================================================
    void ToggleRow(const char* label, bool* v) {
        ImGui::PushID(label);  // ← FIX: unique ID per label!
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(label);
        ImGui::SameLine(COL_LABEL);  // ← FIX: fixed position, bukan kalkulasi dinamis
        if (*v) {
            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.15f, 0.55f, 0.25f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_CheckMark, ImVec4(0.2f, 1.0f, 0.4f, 1.0f));
        } else {
            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_CheckMark, ImVec4(0.55f, 0.55f, 0.55f, 1.0f));
        }
        ImGui::Checkbox("##tgl", v);
        ImGui::PopStyleColor(2);
        ImGui::PopID();
    }

    // =========================================================
    // HELPER: Slider Row (label kiri, slider kanan)
    // =========================================================
    void SliderRow(const char* label, float* v, float mn, float mx, const char* fmt = "%.2f") {
        ImGui::PushID(label);
        ImGui::TextUnformatted(label);
        ImGui::SameLine(COL_LABEL);
        ImGui::SetNextItemWidth(COL_WIDGET);
        ImGui::SliderFloat("##sl", v, mn, mx, fmt);
        ImGui::PopID();
    }

    // =========================================================
    // HELPER: Color Picker Row
    // =========================================================
    void ColorRow(const char* label, float col[4]) {
        ImGui::PushID(label);
        ImGui::TextUnformatted(label);
        ImGui::SameLine(COL_LABEL);
        ImGui::SetNextItemWidth(COL_WIDGET);
        ImGui::ColorEdit4("##clr", col,
            ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar
            | ImGuiColorEditFlags_AlphaPreviewHalf);
        ImGui::PopID();
    }

    // =========================================================
    // HELPER: Combo Row
    // =========================================================
    void ComboRow(const char* label, int* v, const char* items) {
        ImGui::PushID(label);
        ImGui::TextUnformatted(label);
        ImGui::SameLine(COL_LABEL);
        ImGui::SetNextItemWidth(COL_WIDGET);
        ImGui::Combo("##cmb", v, items);
        ImGui::PopID();
    }

    // =========================================================
    // LINE STYLE BUTTONS: 3 tombol dalam satu baris
    // =========================================================
    void LineStyleButtons() {
        float btnW = 75.0f;
        const char* names[] = { "Solid", "Dashed", "Dotted" };
        for (int i = 0; i < 3; i++) {
            if (i > 0) ImGui::SameLine();
            bool active = (g_TradeSettings.lineStyle == i);
            if (active) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.52f, 0.82f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.62f, 0.92f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.22f, 0.58f, 0.88f, 1.0f));
            } else {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.22f, 0.22f, 0.22f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.35f, 0.35f, 0.35f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.28f, 0.28f, 0.28f, 1.0f));
            }
            if (ImGui::Button(names[i], ImVec2(btnW, 0))) {
                g_TradeSettings.lineStyle = i;
            }
            ImGui::PopStyleColor(3);
        }
    }

    // =========================================================
    // TRADE ZONE TABLE — daftar trade aktif dengan toggle zona
    // =========================================================
    void RenderTradeTable() {
        if (!m_tradeData || m_tradeData->empty()) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
            ImGui::Text("  Tidak ada trade aktif");
            ImGui::PopStyleColor();
            return;
        }

        ImGui::Spacing();
        // Tinggi tabel: maks 5 baris, minimal 2 baris
        float rowH = ImGui::GetTextLineHeightWithSpacing();
        float tableH = std::clamp((float)m_tradeData->size() * rowH + rowH, rowH * 2.5f, rowH * 5.5f);
        ImGui::BeginChild("##trade_zone_list", ImVec2(0, tableH), ImGuiChildFlags_Borders);

        if (ImGui::BeginTable("##tz", 5,
            ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingFixedFit))
        {
            ImGui::TableSetupColumn("ID",    ImGuiTableColumnFlags_WidthFixed, 38.0f);
            ImGui::TableSetupColumn("",      ImGuiTableColumnFlags_WidthFixed, 38.0f);
            ImGui::TableSetupColumn("Entry", ImGuiTableColumnFlags_WidthStretch, 0.0f);
            ImGui::TableSetupColumn("PnL",   ImGuiTableColumnFlags_WidthFixed, 68.0f);
            ImGui::TableSetupColumn("Zone",  ImGuiTableColumnFlags_WidthFixed, 42.0f);
            ImGui::TableHeadersRow();

            for (int i = 0; i < (int)m_tradeData->size(); i++) {
                const auto& row = (*m_tradeData)[i];
                ImGui::PushID(i);
                ImGui::TableNextRow();

                // Kolom ID
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("#%d", row.id);

                // Kolom Type
                ImGui::TableSetColumnIndex(1);
                if (row.typeStr == "BUY") {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.3f, 0.9f, 0.4f, 1.0f));
                } else {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
                }
                ImGui::TextUnformatted(row.typeStr.c_str());
                ImGui::PopStyleColor();

                // Kolom Entry Price
                ImGui::TableSetColumnIndex(2);
                ImGui::Text("%.2f", row.entryPrice);

                // Kolom Profit
                ImGui::TableSetColumnIndex(3);
                if (row.profit >= 0) {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.3f, 1.0f, 0.4f, 1.0f));
                } else {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.35f, 0.35f, 1.0f));
                }
                ImGui::Text("%+.2f", row.profit);
                ImGui::PopStyleColor();

                // Kolom Zone Toggle
                ImGui::TableSetColumnIndex(4);
                if (*row.pShowZones) {
                    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.15f, 0.5f, 0.25f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_CheckMark, ImVec4(0.2f, 1.0f, 0.4f, 1.0f));
                } else {
                    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.28f, 0.28f, 0.28f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_CheckMark, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
                }
                ImGui::Checkbox("##z", row.pShowZones);
                ImGui::PopStyleColor(2);

                ImGui::PopID();
            }
            ImGui::EndTable();
        }
        ImGui::EndChild();
    }

    // =========================================================
    // MAIN POPUP
    // =========================================================
    void RenderPopup() {
        // FIX: Window style push SEBELUM Begin
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14, 10));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 6.0f);

        ImGui::SetNextWindowSize(ImVec2(400, 0), ImGuiCond_FirstUseEver);

        // FIX: Hapus NoScrollbar — biar ImGui handle scroll otomatis
        if (!ImGui::Begin("Visual Trade Settings", &showPopup, ImGuiWindowFlags_NoCollapse)) {
            ImGui::End();
            ImGui::PopStyleVar(2);
            return;
        }

        // Content style (push SETELAH Begin)
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6, 5));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);

        // =============================================
        // SECTION: GARIS (Lines)
        // =============================================
        SectionHeader("GARIS (Lines)");

        // Style — 3 tombol gabungan
        ImGui::TextUnformatted("Style Garis");
        ImGui::SameLine(COL_LABEL);
        LineStyleButtons();

        // Ketebalan — satu slider untuk semua
        SliderRow("Ketebalan", &g_TradeSettings.lineThickness, 0.5f, 4.0f, "%.1f");

        // Label di garis SL/TP
        ToggleRow("Label SL/TP", &g_TradeSettings.showLineLabels);

        // Warna — PISAH per garis
        ImGui::Spacing();
        ImGui::TextUnformatted("Warna Garis");
        ImGui::Indent(8.0f);
        ColorRow("Entry BUY",    g_TradeSettings.entryColorBuy);
        ColorRow("Entry SELL",   g_TradeSettings.entryColorSell);
        ColorRow("Stop Loss",    g_TradeSettings.slColor);
        ColorRow("Take Profit",  g_TradeSettings.tpColor);
        ImGui::Unindent(8.0f);

        // =============================================
        // SECTION: ZONA (Zones)
        // =============================================
        SectionHeader("ZONA (Zones)");

        // FIX: "Tampilkan Zona" → "Default Zona Baru" (untuk trade mendatang)
        ToggleRow("Default Zona Baru", &g_TradeSettings.showZones);

        ImGui::Indent(8.0f);
        // Label Zona — combo isi label
        ComboRow("Isi Label", &g_TradeSettings.zoneLabelMode,
                  "Harga + RR\0Harga Saja\0RR Saja\0");
        SliderRow("Font Scale", &g_TradeSettings.zoneFontScale, 0.7f, 1.5f, "%.1f");
        SliderRow("Opacity Selected", &g_TradeSettings.zoneAlphaSelected, 0.05f, 0.5f, "%.02f");
        SliderRow("Opacity Normal",   &g_TradeSettings.zoneAlphaNormal,   0.05f, 0.4f, "%.02f");
        ColorRow("Warna Loss",    g_TradeSettings.lossZoneColor);
        ColorRow("Warna Win",     g_TradeSettings.winZoneColor);
        ImGui::Unindent(8.0f);

        // =============================================
        // SECTION: TRADE AKTIF — tabel show/hide zona per trade
        // =============================================
        SectionHeader("TRADE AKTIF (Zone Control)");
        RenderTradeTable();

        // =============================================
        // SECTION: RIWAYAT (Ghost Zone)
        // =============================================
        SectionHeader("RIWAYAT (Ghost Zone)");

        ToggleRow("Tampilkan Riwayat", &g_TradeSettings.showGhostZones);

        if (g_TradeSettings.showGhostZones) {
            ImGui::Indent(8.0f);
            ToggleRow("Label Harga/RR",  &g_TradeSettings.showGhostLabels);
            ToggleRow("Badge Profit",    &g_TradeSettings.showGhostBadge);
            SliderRow("Ghost Opacity",  &g_TradeSettings.ghostAlphaMult, 0.0f, 2.0f, "%.1f");
            ImGui::Unindent(8.0f);
        }

        // =============================================
        // SECTION: GENERAL
        // =============================================
        SectionHeader("GENERAL");

        ToggleRow("Badge RR",          &g_TradeSettings.showRRBadge);
        ToggleRow("Konektor SL-TP",   &g_TradeSettings.showConnector);

        ImGui::Spacing();
        ImGui::TextUnformatted("Default SL/TP Baru");
        ImGui::Indent(8.0f);
        SliderRow("SL Offset (%)", &g_TradeSettings.defaultSLOffset, 0.05f, 2.0f, "%.2f");
        SliderRow("RR Ratio",     &g_TradeSettings.defaultRR,      0.5f, 10.0f, "1:%.1f");
        ImGui::Unindent(8.0f);

        // =============================================
        // RESET BUTTON
        // =============================================
        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        float btnW = ImGui::GetContentRegionAvail().x;
        ImGui::PushStyleColor(ImGuiCol_Button,       ImVec4(0.4f, 0.15f, 0.15f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.6f, 0.25f, 0.25f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.5f, 0.2f, 0.2f, 1.0f));
        if (ImGui::Button("Reset ke Default", ImVec2(btnW, 28.0f))) {
            ResetDefaults();
        }
        ImGui::PopStyleColor(3);

        // Cleanup — FIX: pop content styles SEBELUM End, pop window styles SETELAH End
        ImGui::PopStyleVar(2);  // content styles
        ImGui::End();
        ImGui::PopStyleVar(2);  // window styles
    }

    // =========================================================
    // RESET KE DEFAULT
    // =========================================================
    void ResetDefaults() {
        TradeVisualSettings def;
        g_TradeSettings = def;
    }
};

// --- Global instance ---
inline TradeSettingsUI g_tradeSettingsUI;

// --- Static member initialization ---
inline ImTextureID TradeSettingsUI::settingIconTex = 0;
