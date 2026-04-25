#pragma once
#include "imgui.h"
#include "implot.h"
#include "OrderFlowRenderer.h"
#include "MultiChart.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

// =============================================================================
// ORDER FLOW SETTINGS UI — POPUP EDITION v2
//
// Setiap overlay punya popup settings sendiri:
//   -> Volume Profile (VP) — simple ON/OFF toggles
//   -> Footprint (FP) — lengkap: mode, colors, font, detection
//
// Popup TIDAK auto-close saat click outside.
// Hanya bisa ditutup lewat tombol:
//   [Cancel] = revert semua perubahan ke snapshot awal + tutup
//   [Done]   = terapkan perubahan (sudah live) + tutup
//
// Pola: ImGui::BeginPopup() — auto-clamp, responsive, scrollable.
// =============================================================================

// State panel — static, persist selama session
static bool  s_vpSettingsOpen   = false;
static bool  s_fpSettingsOpen   = false;

// State untuk expandable sections (FP only)
static bool s_fpColorsOpen      = false;
static bool s_fpAdvancedOpen    = false;

// =============================================================================
// SNAPSHOT — simpan state saat popup dibuka, buat Cancel bisa revert
// =============================================================================

struct VPSnapshot {
    bool showVolumeProfile;
    bool showVPLabels;
    bool showVAShading;
    bool showVPOCLine;
};

struct FPSnapshot {
    int   fpDisplayMode;
    bool  showVolumeNumbers;
    bool  showDeltaLabel;
    float heatmapBaseOpacity;
    float fpFontScale;
    bool  showMiniDeltaBar;
    bool  showImbalance;
    float imbalanceRatio;
    bool  showStackedImbalance;
    bool  showAbsorption;
    bool  showSinglePrints;
    bool  showPOCHighlight;
    ImVec4 fpColorBuy;
    ImVec4 fpColorSell;
    ImVec4 fpColorDeltaPos;
    ImVec4 fpColorDeltaNeg;
    ImVec4 fpColorPOC;
    ImVec4 fpColorImbalBuy;
    ImVec4 fpColorImbalSell;
    ImVec4 fpColorAbsorbBuy;
    ImVec4 fpColorAbsorbSell;
};

static VPSnapshot s_vpSnapshot;
static FPSnapshot s_fpSnapshot;

static void SaveVPSnapshot(ChartTab* tab) {
    if (!tab) return;
    s_vpSnapshot.showVolumeProfile = tab->showVolumeProfile;
    s_vpSnapshot.showVPLabels      = tab->orderFlowRenderer.showVPLabels;
    s_vpSnapshot.showVAShading     = tab->orderFlowRenderer.showVAShading;
    s_vpSnapshot.showVPOCLine      = tab->orderFlowRenderer.showVPOCLine;
}

static void RestoreVPSnapshot(ChartTab* tab) {
    if (!tab) return;
    tab->showVolumeProfile              = s_vpSnapshot.showVolumeProfile;
    tab->orderFlowRenderer.showVPLabels = s_vpSnapshot.showVPLabels;
    tab->orderFlowRenderer.showVAShading= s_vpSnapshot.showVAShading;
    tab->orderFlowRenderer.showVPOCLine = s_vpSnapshot.showVPOCLine;
}

static void SaveFPSnapshot(ChartTab* tab) {
    if (!tab) return;
    OrderFlowRenderer& of = tab->orderFlowRenderer;
    s_fpSnapshot.fpDisplayMode     = of.fpDisplayMode;
    s_fpSnapshot.showVolumeNumbers = of.showVolumeNumbers;
    s_fpSnapshot.showDeltaLabel    = of.showDeltaLabel;
    s_fpSnapshot.heatmapBaseOpacity= of.heatmapBaseOpacity;
    s_fpSnapshot.fpFontScale       = of.fpFontScale;
    s_fpSnapshot.showMiniDeltaBar  = of.showMiniDeltaBar;
    s_fpSnapshot.showImbalance     = of.showImbalance;
    s_fpSnapshot.imbalanceRatio    = of.imbalanceRatio;
    s_fpSnapshot.showStackedImbalance = of.showStackedImbalance;
    s_fpSnapshot.showAbsorption    = of.showAbsorption;
    s_fpSnapshot.showSinglePrints  = of.showSinglePrints;
    s_fpSnapshot.showPOCHighlight  = of.showPOCHighlight;
    s_fpSnapshot.fpColorBuy        = of.fpColorBuy;
    s_fpSnapshot.fpColorSell       = of.fpColorSell;
    s_fpSnapshot.fpColorDeltaPos   = of.fpColorDeltaPos;
    s_fpSnapshot.fpColorDeltaNeg   = of.fpColorDeltaNeg;
    s_fpSnapshot.fpColorPOC        = of.fpColorPOC;
    s_fpSnapshot.fpColorImbalBuy   = of.fpColorImbalBuy;
    s_fpSnapshot.fpColorImbalSell  = of.fpColorImbalSell;
    s_fpSnapshot.fpColorAbsorbBuy  = of.fpColorAbsorbBuy;
    s_fpSnapshot.fpColorAbsorbSell = of.fpColorAbsorbSell;
}

static void RestoreFPSnapshot(ChartTab* tab) {
    if (!tab) return;
    OrderFlowRenderer& of = tab->orderFlowRenderer;
    of.fpDisplayMode       = s_fpSnapshot.fpDisplayMode;
    of.showVolumeNumbers   = s_fpSnapshot.showVolumeNumbers;
    of.showDeltaLabel      = s_fpSnapshot.showDeltaLabel;
    of.heatmapBaseOpacity  = s_fpSnapshot.heatmapBaseOpacity;
    of.fpFontScale         = s_fpSnapshot.fpFontScale;
    of.showMiniDeltaBar    = s_fpSnapshot.showMiniDeltaBar;
    of.showImbalance       = s_fpSnapshot.showImbalance;
    of.imbalanceRatio      = s_fpSnapshot.imbalanceRatio;
    of.showStackedImbalance = s_fpSnapshot.showStackedImbalance;
    of.showAbsorption      = s_fpSnapshot.showAbsorption;
    of.showSinglePrints    = s_fpSnapshot.showSinglePrints;
    of.showPOCHighlight    = s_fpSnapshot.showPOCHighlight;
    of.fpColorBuy          = s_fpSnapshot.fpColorBuy;
    of.fpColorSell         = s_fpSnapshot.fpColorSell;
    of.fpColorDeltaPos     = s_fpSnapshot.fpColorDeltaPos;
    of.fpColorDeltaNeg     = s_fpSnapshot.fpColorDeltaNeg;
    of.fpColorPOC          = s_fpSnapshot.fpColorPOC;
    of.fpColorImbalBuy     = s_fpSnapshot.fpColorImbalBuy;
    of.fpColorImbalSell    = s_fpSnapshot.fpColorImbalSell;
    of.fpColorAbsorbBuy    = s_fpSnapshot.fpColorAbsorbBuy;
    of.fpColorAbsorbSell   = s_fpSnapshot.fpColorAbsorbSell;
}

// =============================================================================
// HELPER: CancelDoneButtons — dua tombol di bawah popup
// Cancel (kiri, abu) = revert + tutup.  Done (kanan, accent) = tutup.
// Return: 1 = Done diklik, -1 = Cancel diklik, 0 = belum
// =============================================================================
static int CancelDoneButtons(ImU32 accentColor) {
    int result = 0;
    float w = ImGui::GetContentRegionAvail().x;
    float btnW = (w - 8.0f) * 0.5f;
    float btnH = 28.0f;

    ImVec4 colAccent = ImVec4(
        ((accentColor >>  0) & 0xFF) / 255.0f,
        ((accentColor >>  8) & 0xFF) / 255.0f,
        ((accentColor >> 16) & 0xFF) / 255.0f, 1.0f);

    // Cancel — kiri
    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.20f, 0.20f, 0.25f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.30f, 0.30f, 0.38f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.38f, 0.38f, 0.48f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(0.70f, 0.70f, 0.75f, 1.0f));
    if (ImGui::Button("Cancel", ImVec2(btnW, btnH))) {
        result = -1;
    }
    ImGui::PopStyleColor(4);

    // Done — kanan
    ImGui::SameLine(0, 8.0f);
    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(colAccent.x * 0.30f, colAccent.y * 0.30f, colAccent.z * 0.30f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(colAccent.x * 0.45f, colAccent.y * 0.45f, colAccent.z * 0.45f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(colAccent.x * 0.60f, colAccent.y * 0.60f, colAccent.z * 0.60f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Text,          colAccent);
    if (ImGui::Button("Done", ImVec2(btnW, btnH))) {
        result = 1;
    }
    ImGui::PopStyleColor(4);

    return result;
}

// =============================================================================
// HELPER: SettingRow — label kiri, toggle/slider kanan, 1 baris rapi
// =============================================================================

// Toggle row: label + ON/OFF toggle di kanan
static void SettingToggleRow(const char* label, bool& value, ImU32 accentColor) {
    ImVec4 colOn  = ImVec4(
        ((accentColor >>  0) & 0xFF) / 255.0f,
        ((accentColor >>  8) & 0xFF) / 255.0f,
        ((accentColor >> 16) & 0xFF) / 255.0f, 1.0f);

    // Label kiri
    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(ImVec4(0.75f, 0.75f, 0.78f, 1.0f), "%s", label);

    // Toggle kanan
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 48.0f);
    ImGui::PushID(label);
    if (value) {
        ImGui::PushStyleColor(ImGuiCol_Button,        colOn);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, colOn);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  colOn);
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.22f, 0.22f, 0.28f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.30f, 0.30f, 0.38f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.35f, 0.35f, 0.45f, 1.0f));
    }
    if (ImGui::Button(value ? "ON" : "OFF", ImVec2(48, 0))) {
        value = !value;
    }
    ImGui::PopStyleColor(3);
    ImGui::PopID();
}

// Slider row: label + slider full width di bawah
static void SettingSliderRow(const char* label, float& value, float v_min, float v_max, const char* fmt) {
    ImGui::TextColored(ImVec4(0.75f, 0.75f, 0.78f, 1.0f), "%s", label);
    float w = ImGui::GetContentRegionAvail().x;
    ImGui::PushItemWidth(w);
    ImGui::SliderFloat(
        std::string("##").append(label).c_str(),
        &value, v_min, v_max, fmt);
    ImGui::PopItemWidth();
}

// Color swatch row: label + swatch kecil + klik buka color picker
static void SettingColorRow(const char* label, ImVec4& color) {
    ImGui::TextColored(ImVec4(0.70f, 0.70f, 0.73f, 1.0f), "%s", label);
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 100.0f);
    ImGui::PushID(label);
    ImGui::ColorButton("##swatch", color, ImGuiColorEditFlags_None, ImVec2(90, ImGui::GetTextLineHeight()));
    if (ImGui::BeginPopupContextItem("##cpick")) {
        ImGui::ColorEdit4("##cedit", &color.x,
            ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel
            | ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_DisplayHex);
        ImGui::EndPopup();
    }
    if (ImGui::IsItemActive())
        ImGui::OpenPopup("##cpick");
    ImGui::PopID();
}

// Expandable section header
static bool SettingSectionHeader(const char* label, bool& isOpen, ImU32 accentColor) {
    ImVec4 col = ImVec4(
        ((accentColor >>  0) & 0xFF) / 255.0f,
        ((accentColor >>  8) & 0xFF) / 255.0f,
        ((accentColor >> 16) & 0xFF) / 255.0f, 1.0f);

    ImGui::PushID(label);
    bool clicked = false;

    ImVec2 p = ImGui::GetCursorScreenPos();
    float w = std::max(ImGui::GetContentRegionAvail().x, 50.0f); // clamp minimum
    float btnH = ImGui::GetTextLineHeight() + 6.0f;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    bool hov = ImGui::IsMouseHoveringRect(p, ImVec2(p.x + w, p.y + btnH));

    ImGui::InvisibleButton("##section", ImVec2(w, btnH));
    if (ImGui::IsItemClicked()) { isOpen = !isOpen; clicked = true; }

    if (hov) {
        dl->AddRectFilled(p, ImVec2(p.x + w, p.y + btnH),
            IM_COL32(255, 255, 255, 8), 4.0f);
    }

    const char* arrow = isOpen ? "v " : "> ";
    ImGui::TextColored(ImVec4(0.55f, 0.55f, 0.65f, 1.0f), "%s", arrow);
    ImGui::SameLine(0, 0);
    ImGui::TextColored(hov ? col : ImVec4(0.80f, 0.80f, 0.85f, 1.0f), "%s", label);

    if (isOpen) {
        float lineY = p.y + btnH;
        dl->AddLine(ImVec2(p.x, lineY), ImVec2(p.x + w, lineY), accentColor, 1.0f);
    }

    ImGui::PopID();
    return clicked;
}

// Separator tipis
static void SettingSeparator() {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    float w = ImGui::GetContentRegionAvail().x;
    dl->AddLine(ImVec2(p.x, p.y), ImVec2(p.x + w, p.y), IM_COL32(255, 255, 255, 15), 1.0f);
    ImGui::Dummy(ImVec2(0, 2));
}

// Segmented 3 control (FP display mode)
static void SettingSegmented3(const char* label, int& activeIdx,
                               const char* labels[3], ImU32 colors[3]) {
    ImGui::TextColored(ImVec4(0.75f, 0.75f, 0.78f, 1.0f), "%s", label);
    ImGui::Spacing();

    float avail = ImGui::GetContentRegionAvail().x;
    float segW = avail / 3.0f;

    for (int i = 0; i < 3; i++) {
        ImGui::PushID(i);
        bool isActive = (activeIdx == i);
        ImVec4 col = ImVec4(
            ((colors[i] >>  0) & 0xFF) / 255.0f,
            ((colors[i] >>  8) & 0xFF) / 255.0f,
            ((colors[i] >> 16) & 0xFF) / 255.0f, 1.0f);

        if (isActive) {
            ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(col.x * 0.25f, col.y * 0.25f, col.z * 0.25f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(col.x * 0.30f, col.y * 0.30f, col.z * 0.30f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(col.x * 0.35f, col.y * 0.35f, col.z * 0.35f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Text, col);
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.15f, 0.15f, 0.20f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.22f, 0.22f, 0.30f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.28f, 0.28f, 0.38f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.50f, 0.50f, 0.55f, 1.0f));
        }

        if (i > 0) ImGui::SameLine(0, 0);
        if (ImGui::Button(labels[i], ImVec2(segW, 0))) {
            activeIdx = i;
        }
        ImGui::PopStyleColor(4);
        ImGui::PopID();
    }
}

// =============================================================================
// VOLUME PROFILE SETTINGS POPUP
// =============================================================================
inline void RenderVolumeProfileSettings(ChartTab* tab) {
    if (!tab) return;

    ImU32 accentAmber = IM_COL32(239, 159, 39, 255);
    OrderFlowRenderer& of = tab->orderFlowRenderer;

    // OpenPopup setiap frame kalau state open
    if (s_vpSettingsOpen) ImGui::OpenPopup("##VPSettingsPopup");

    ImGui::PushStyleColor(ImGuiCol_PopupBg,  ImVec4(0.09f, 0.10f, 0.14f, 0.97f));
    ImGui::PushStyleColor(ImGuiCol_Border,   ImVec4(1.0f,  1.0f,  1.0f,  0.10f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 12.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,   ImVec2(6.0f, 8.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 10.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, ImVec2(220.0f, 0.0f));

    if (ImGui::BeginPopup("##VPSettingsPopup")) {

        // ── Header ──
        ImGui::TextColored(ImVec4(0.94f, 0.62f, 0.15f, 1.0f), "VOLUME PROFILE");
        ImGui::Spacing();

        SettingSeparator();

        // ── Show VP ──
        SettingToggleRow("Show VP", tab->showVolumeProfile, accentAmber);

        // ── Labels ──
        SettingToggleRow("Labels (VAH/VAL/VPOC)", of.showVPLabels, accentAmber);

        // ── VA Shading ──
        SettingToggleRow("VA Shading", of.showVAShading, accentAmber);

        // ── VPOC Line ──
        SettingToggleRow("VPOC Line", of.showVPOCLine, accentAmber);

        // ── Info tip ──
        ImGui::Spacing();
        SettingSeparator();
        ImGui::TextColored(ImVec4(0.45f, 0.45f, 0.50f, 1.0f), "VPOC = Price of Control");
        ImGui::TextColored(ImVec4(0.45f, 0.45f, 0.50f, 1.0f), "VA = Value Area 70%%");

        // ── Cancel / Done ──
        ImGui::Spacing();
        ImGui::Spacing();
        int action = CancelDoneButtons(accentAmber);

        if (action == -1) {
            // Cancel → revert ke snapshot + tutup
            RestoreVPSnapshot(tab);
            s_vpSettingsOpen = false;
            ImGui::CloseCurrentPopup();
        } else if (action == 1) {
            // Done → terapkan (sudah live) + tutup
            // Jika VP baru di-ON dari snapshot, request data
            if (tab->showVolumeProfile && !s_vpSnapshot.showVolumeProfile) {
                #ifdef __EMSCRIPTEN__
                std::string sym = tab->symbol;
                EM_ASM({
                    var s = UTF8ToString($0);
                    if (window.requestFootprint) window.requestFootprint(s, 500);
                }, sym.c_str());
                #endif
            }
            s_vpSettingsOpen = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();

    } else {
        // Popup gagal buka / sudah ditutup ImGui
        // JANGAN reset state — biar frame depan coba buka lagi
        // Hanya reset kalau kita sendiri yang set false (di Cancel/Done)
        if (!s_vpSettingsOpen) {
            // Sudah ditutup secara eksplisit — OK
        } else {
            // ImGui menutup popup (misal click outside) — REOPEN
            ImGui::OpenPopup("##VPSettingsPopup");
        }
    }

    ImGui::PopStyleVar(4);
    ImGui::PopStyleColor(2);
}

// =============================================================================
// FOOTPRINT SETTINGS POPUP
// =============================================================================
inline void RenderFootprintSettings(ChartTab* tab) {
    if (!tab) return;

    ImU32 accentBlue = IM_COL32(100, 180, 255, 255);
    OrderFlowRenderer& of = tab->orderFlowRenderer;

    // OpenPopup setiap frame kalau state open
    if (s_fpSettingsOpen) ImGui::OpenPopup("##FPSettingsPopup");

    ImGui::PushStyleColor(ImGuiCol_PopupBg,  ImVec4(0.09f, 0.10f, 0.14f, 0.97f));
    ImGui::PushStyleColor(ImGuiCol_Border,   ImVec4(1.0f,  1.0f,  1.0f,  0.10f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 12.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,   ImVec2(6.0f, 8.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 10.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, ImVec2(240.0f, 0.0f));

    if (ImGui::BeginPopup("##FPSettingsPopup")) {

        // ── Header ──
        ImGui::TextColored(ImVec4(0.39f, 0.71f, 1.0f, 1.0f), "FOOTPRINT SETTINGS");
        ImGui::Spacing();

        SettingSeparator();

        // ════════════════════════════════════
        // DISPLAY MODE
        // ════════════════════════════════════
        static const char* fpModeLabels[3] = {"Ask|Bid", "Delta", "Volume"};
        static ImU32     fpModeColors[3]  = {
            IM_COL32(100, 180, 255, 255),
            IM_COL32(80, 255, 130, 255),
            IM_COL32(255, 200, 80, 255)
        };
        SettingSegmented3("Display mode", of.fpDisplayMode, fpModeLabels, fpModeColors);

        ImGui::Spacing();
        SettingSeparator();

        // ════════════════════════════════════
        // TOGGLES
        // ════════════════════════════════════
        SettingToggleRow("Volume numbers", of.showVolumeNumbers, accentBlue);
        SettingToggleRow("Delta label", of.showDeltaLabel, accentBlue);

        ImGui::Spacing();
        SettingSeparator();

        // ════════════════════════════════════
        // SLIDERS
        // ════════════════════════════════════
        SettingSliderRow("Heatmap opacity", of.heatmapBaseOpacity, 0.10f, 1.0f, "%.2f");
        SettingSliderRow("Font size", of.fpFontScale, 0.70f, 1.50f, "%.2fx");

        ImGui::Spacing();

        // ════════════════════════════════════
        // COLORS (expandable — 9 color swatches)
        // ════════════════════════════════════
        SettingSectionHeader("Colors", s_fpColorsOpen, IM_COL32(255, 180, 80, 255));

        if (s_fpColorsOpen) {
            ImGui::Indent(8.0f);
            SettingColorRow("Buy",           of.fpColorBuy);
            SettingColorRow("Sell",          of.fpColorSell);
            SettingColorRow("Delta +",       of.fpColorDeltaPos);
            SettingColorRow("Delta -",       of.fpColorDeltaNeg);
            SettingColorRow("POC",           of.fpColorPOC);
            SettingColorRow("Imbal. Buy",    of.fpColorImbalBuy);
            SettingColorRow("Imbal. Sell",   of.fpColorImbalSell);
            SettingColorRow("Absorb. Buy",   of.fpColorAbsorbBuy);
            SettingColorRow("Absorb. Sell",  of.fpColorAbsorbSell);
            ImGui::Unindent(8.0f);
        }

        ImGui::Spacing();

        // ════════════════════════════════════
        // DETECTION (expandable — toggles + slider)
        // ════════════════════════════════════
        SettingSectionHeader("Detection", s_fpAdvancedOpen, IM_COL32(180, 100, 255, 255));

        if (s_fpAdvancedOpen) {
            ImGui::Indent(8.0f);
            SettingToggleRow("Imbalance",          of.showImbalance,          IM_COL32(100,255,100,255));
            SettingSliderRow("Imbalance ratio",    of.imbalanceRatio,         1.5f, 8.0f, "%.1fx");
            SettingToggleRow("Stacked imbal.",     of.showStackedImbalance,   IM_COL32(50,220,255,255));
            SettingToggleRow("Absorption",         of.showAbsorption,         IM_COL32(200,80,255,255));
            SettingToggleRow("Single prints",      of.showSinglePrints,       IM_COL32(255,215,0,255));
            SettingToggleRow("POC highlight",      of.showPOCHighlight,       IM_COL32(255,215,0,255));
            ImGui::Unindent(8.0f);
        }

        ImGui::Spacing();
        SettingSeparator();

        // ════════════════════════════════════
        // MINI DELTA BAR
        // ════════════════════════════════════
        SettingToggleRow("Mini delta bar", of.showMiniDeltaBar, IM_COL32(80,200,120,255));

        // ── Cancel / Done ──
        ImGui::Spacing();
        ImGui::Spacing();
        int action = CancelDoneButtons(accentBlue);

        if (action == -1) {
            // Cancel → revert ke snapshot + tutup
            RestoreFPSnapshot(tab);
            s_fpSettingsOpen = false;
            ImGui::CloseCurrentPopup();
        } else if (action == 1) {
            // Done → terapkan (sudah live) + tutup
            s_fpSettingsOpen = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();

    } else {
        // ImGui menutup popup (misal click outside) — REOPEN biar gak hilang
        if (s_fpSettingsOpen) {
            ImGui::OpenPopup("##FPSettingsPopup");
        }
    }

    ImGui::PopStyleVar(4);
    ImGui::PopStyleColor(2);
}

// =============================================================================
// RENDER ORDER FLOW SETTINGS — dipanggil setiap frame
// =============================================================================
inline void RenderOrderFlowSettings(ChartTab* tab) {
    if (!tab) return;
    RenderVolumeProfileSettings(tab);
    RenderFootprintSettings(tab);
}

// =============================================================================
// TOGGLE PANEL + GETTERS
// =============================================================================

// Toggle VP — buka popup + simpan snapshot
inline void ToggleVPSettings() {
    ChartTab* tab = g_chartManager.GetActiveTab();
    if (!tab) return;
    s_vpSettingsOpen = !s_vpSettingsOpen;
    if (s_vpSettingsOpen) {
        s_fpSettingsOpen = false; // mutual exclusion
        SaveVPSnapshot(tab);      // snapshot buat Cancel
    }
}

// Toggle FP — buka popup + simpan snapshot
inline void ToggleFPSettings() {
    ChartTab* tab = g_chartManager.GetActiveTab();
    if (!tab) return;
    s_fpSettingsOpen = !s_fpSettingsOpen;
    if (s_fpSettingsOpen) {
        s_vpSettingsOpen = false; // mutual exclusion
        SaveFPSnapshot(tab);      // snapshot buat Cancel
    }
}

// Legacy — buka settings sesuai style aktif
inline void ToggleOrderFlowSettings() {
    ChartTab* tab = g_chartManager.GetActiveTab();
    if (tab && IsFootprintStyle(tab->renderStyle)) {
        ToggleFPSettings();
    } else {
        ToggleVPSettings();
    }
}

inline bool IsOrderFlowSettingsOpen() { return s_vpSettingsOpen || s_fpSettingsOpen; }
inline bool IsVPSettingsOpen()        { return s_vpSettingsOpen; }
inline bool IsFPSettingsOpen()        { return s_fpSettingsOpen; }
