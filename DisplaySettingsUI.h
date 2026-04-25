// =================================================================================
// DisplaySettingsUI.h  —  Popup Pengaturan Tampilan (Font, Tema, Warna, Icon Size)
// =================================================================================
// Dipanggil dari tombol Settings di navigation bar.
// Semua setting disimpan via SaveSettings() di main.cpp.
// =================================================================================
#pragma once
#include "imgui.h"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <algorithm>

// =================================================================================
// Forward declarations — semua variable global yang didefinisikan di main.cpp
// =================================================================================
extern ImVec4 g_colorBg;
extern ImVec4 g_colorText;
extern ImVec4 g_colorPanel;
extern ImVec4 g_colorHeader;
extern ImVec4 g_colBuy;
extern ImVec4 g_colSell;
extern bool   g_isLightMode;
extern float  g_iconSize;
extern int    g_selectedFontIdx;
extern ImFont* g_fonts[3];
extern const char* g_fontNames[];

// Forward declarations dari main.cpp
extern void SaveSettings();
extern void ApplyThemePreset(bool light);

// =================================================================================
// CLASS: DisplaySettingsUI
// =================================================================================
class DisplaySettingsUI {
public:
    bool showPopup = false;

    // Panggil sekali per frame
    void Render() {
        if (!showPopup) return;
        RenderPopup();
    }

    void Toggle() { showPopup = !showPopup; }
    void Open()   { showPopup = true; }
    void Close()  { showPopup = false; }

private:
    // Layout: lebar kolom label (kiri) dan widget (kanan)
    static constexpr float COL_LABEL  = 155.0f;
    static constexpr float COL_WIDGET = 180.0f;

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
    // HELPER: Color Picker Row (label kiri, color picker kanan)
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
    // HELPER: Toggle Row (label kiri, checkbox kanan)
    // =========================================================
    void ToggleRow(const char* label, bool* v) {
        ImGui::PushID(label);
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(label);
        ImGui::SameLine(COL_LABEL);
        if (*v) {
            ImGui::PushStyleColor(ImGuiCol_FrameBg,   ImVec4(0.15f, 0.55f, 0.25f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_CheckMark, ImVec4(0.2f,  1.0f,  0.4f,  1.0f));
        } else {
            ImGui::PushStyleColor(ImGuiCol_FrameBg,   ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
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
    // HELPER: Theme Preset Buttons (2 tombol Dark/Light)
    // =========================================================
    void ThemeButtons() {
        float btnW = (COL_WIDGET + COL_LABEL - 10.0f) * 0.5f;
        const char* names[] = { "Dark", "Light" };
        for (int i = 0; i < 2; i++) {
            if (i > 0) ImGui::SameLine();
            bool active = (i == 0) ? !g_isLightMode : g_isLightMode;
            if (active) {
                ImGui::PushStyleColor(ImGuiCol_Button,       ImVec4(0.18f, 0.52f, 0.82f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.62f, 0.92f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.22f, 0.58f, 0.88f, 1.0f));
            } else {
                ImGui::PushStyleColor(ImGuiCol_Button,       ImVec4(0.22f, 0.22f, 0.22f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.35f, 0.35f, 0.35f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.28f, 0.28f, 0.28f, 1.0f));
            }
            if (ImGui::Button(names[i], ImVec2(btnW, 0))) {
                g_isLightMode = (i == 1);
                ApplyThemePreset(g_isLightMode);
                SaveSettings();
            }
            ImGui::PopStyleColor(3);
        }
    }

    // =========================================================
    // HELPER: Font Preview
    // =========================================================
    void FontPreview(const char* sample) {
        ImGui::Spacing();
        ImGui::Indent(COL_LABEL);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0.35f));
        ImGui::BeginChild("##fontPreview", ImVec2(COL_WIDGET, 32), ImGuiChildFlags_Borders);
        ImGui::TextWrapped("%s", sample);
        ImGui::EndChild();
        ImGui::PopStyleColor();
        ImGui::Unindent(COL_LABEL);
    }

    // =========================================================
    // MAIN POPUP
    // =========================================================
    void RenderPopup() {
        // Window style push SEBELUM Begin
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14, 10));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 6.0f);

        ImGui::SetNextWindowSize(ImVec2(400, 0), ImGuiCond_FirstUseEver);

        if (!ImGui::Begin("Pengaturan Tampilan", &showPopup, ImGuiWindowFlags_NoCollapse)) {
            ImGui::End();
            ImGui::PopStyleVar(2);
            return;
        }

        // Content style
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6, 5));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);

        // =============================================
        // SECTION: FONT
        // =============================================
        SectionHeader("FONT");

        ImGui::TextUnformatted("Jenis Font");
        ImGui::SameLine(COL_LABEL);
        ImGui::SetNextItemWidth(COL_WIDGET);
        // Combo pilihan font
        if (ImGui::BeginCombo("##fontCombo", g_fontNames[g_selectedFontIdx])) {
            for (int i = 0; i < 3; i++) {
                if (g_fonts[i] == nullptr) continue; // skip jika font tidak tersedia
                bool isSelected = (g_selectedFontIdx == i);
                if (ImGui::Selectable(g_fontNames[i], isSelected)) {
                    g_selectedFontIdx = i;
                    ImGui::GetIO().FontDefault = g_fonts[g_selectedFontIdx];
                    SaveSettings();
                }
                if (isSelected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        // Preview font
        FontPreview("XAUUSD 2,048.50  +12.30 (+0.60%)");

        // =============================================
        // SECTION: TEMA (Theme)
        // =============================================
        SectionHeader("TEMA (Theme)");

        ImGui::TextUnformatted("Mode Tampilan");
        ImGui::SameLine(COL_LABEL);
        ThemeButtons();

        // =============================================
        // SECTION: WARNA UI (Interface Colors)
        // =============================================
        SectionHeader("WARNA UI (Interface)");

        ColorRow("Background",     (float*)&g_colorBg);
        ColorRow("Text",           (float*)&g_colorText);
        ColorRow("Panel",          (float*)&g_colorPanel);
        ColorRow("Header / Tab",   (float*)&g_colorHeader);

        // =============================================
        // SECTION: WARNA TRADING
        // =============================================
        SectionHeader("WARNA TRADING");

        ColorRow("Buy (Bull)",     (float*)&g_colBuy);
        ColorRow("Sell (Bear)",    (float*)&g_colSell);

        // =============================================
        // SECTION: UKURAN
        // =============================================
        SectionHeader("UKURAN");

        SliderRow("Icon Size",     &g_iconSize, 24.0f, 72.0f, "%.0f px");

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

        // Cleanup
        ImGui::PopStyleVar(2);  // content styles
        ImGui::End();
        ImGui::PopStyleVar(2);  // window styles
    }

    // =========================================================
    // RESET KE DEFAULT
    // =========================================================
    void ResetDefaults() {
        g_isLightMode    = false;
        g_selectedFontIdx = 0;
        g_iconSize       = 40.0f;

        ApplyThemePreset(false);
        ImGui::GetIO().FontDefault = g_fonts[0];

        SaveSettings();
    }
};

// --- Global instance ---
inline DisplaySettingsUI g_displaySettingsUI;
