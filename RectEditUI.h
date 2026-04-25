#pragma once
#include "imgui.h"
#include "imgui_internal.h"
#include "GlobalShapeManager.h"

// =========================================================
// RectEditUI.h — Rectangle Property Popup (Own File, Own State)
// =========================================================
// Dipisah dari ShapeEditUI.h agar RECT punya state sendiri,
// seperti pola FiboEditUI.h.
// Return: 0=nothing, 1=delete, 2=copy
// =========================================================

extern GlobalShapeManager g_shapes;

// Texture (sama dengan ShapeEditUI)
extern ImTextureID texPopupCopy;
extern ImTextureID texPopupColor;
extern ImTextureID texPopupThick;
extern ImTextureID texPopupLock;
extern ImTextureID texTrash1;
extern ImTextureID texPopupSetting;

class RectEditUI {
private:
    // =========================================================
    // HELPER: ICON BUTTON (Sama dengan ShapeEditUI/FiboEditUI)
    // =========================================================
    static bool IconButton(const char* idStr, ImTextureID user_texture_id, ImU32 color, bool isActive = false) {
        ImGuiWindow* win = ImGui::GetCurrentWindow();
        if (win->SkipItems) return false;

        const ImGuiID id = win->GetID(idStr);
        ImVec2 size(43, 43);

        const ImRect bb(win->DC.CursorPos, ImVec2(win->DC.CursorPos.x + size.x, win->DC.CursorPos.y + size.y));
        ImGui::ItemSize(bb);
        if (!ImGui::ItemAdd(bb, id)) return false;

        bool hovered, held;
        bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);

        if (hovered || isActive)
            win->DrawList->AddRectFilled(bb.Min, bb.Max, IM_COL32(255, 255, 255, 40), 6.0f);

        if (isActive)
            win->DrawList->AddRect(bb.Min, bb.Max, IM_COL32(0, 255, 0, 200), 6.0f);

        if (user_texture_id != 0) {
            float padding = 8.0f;
            ImVec2 pMin = ImVec2(bb.Min.x + padding, bb.Min.y + padding);
            ImVec2 pMax = ImVec2(bb.Max.x - padding, bb.Max.y - padding);
            win->DrawList->AddImage(user_texture_id, pMin, pMax, ImVec2(0,0), ImVec2(1,1), color);
        }

        return pressed;
    }

    // =========================================================
    // STATIC STATE (Own state, like FiboEditUI pattern)
    // inline static = C++17, avoids ODR violation across TUs
    // =========================================================
    inline static std::string lastRectPopupId;
    inline static char rectLabelBuf[128] = "";
    inline static bool settingsOpen = false;

public:
    // =========================================================
    // RENDER UTAMA
    // Return: 0=nothing, 1=delete, 2=copy
    // =========================================================
    static int Render(GlobalShape& s) {
        int result = 0;
        ImU32 white = IM_COL32(240, 240, 240, 255);
        ImU32 gold  = IM_COL32(255, 215, 0, 255);

        // --- Sync state saat ganti shape ---
        if (lastRectPopupId != s.id) {
            lastRectPopupId = s.id;
            settingsOpen = false;
            // Copy label dari shape ke buffer
            memset(rectLabelBuf, 0, sizeof(rectLabelBuf));
            strncpy(rectLabelBuf, s.rectLabel.c_str(), sizeof(rectLabelBuf) - 1);
        }

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(5, 5));

        // =====================================================
        // ROW 1: BUBBLE BAR (7 Ikon)
        // [Lock][Copy][BorderColor][FillColor][TextColor][Thick][Del]
        // =====================================================

        // 1. LOCK
        if (IconButton("##RL", texPopupLock, s.locked ? gold : white, s.locked))
            s.locked = !s.locked;
        ImGui::SameLine();

        // 2. COPY
        if (IconButton("##RC", texPopupCopy, white))
            result = 2; // copy
        ImGui::SameLine();

        // 3. BORDER COLOR (warna garis tepi)
        if (IconButton("##RBC", texPopupColor, ImGui::ColorConvertFloat4ToU32(s.color)))
            ImGui::OpenPopup("RCP_Border");
        if (ImGui::BeginPopup("RCP_Border")) {
            ImGui::Text("Border Color");
            ImGui::ColorPicker4("##rbordercol", (float*)&s.color,
                ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_NoSmallPreview);
            ImGui::EndPopup();
        }
        ImGui::SameLine();

        // 4. FILL COLOR (warna isi)
        if (IconButton("##RFC", texPopupColor, IM_COL32(100, 180, 255, 255)))
            ImGui::OpenPopup("RCP_Fill");
        if (ImGui::BeginPopup("RCP_Fill")) {
            ImGui::Text("Fill Color");
            static ImVec4 fillColor = ImVec4(0.2f, 0.4f, 0.8f, 1.0f);
            ImGui::ColorPicker4("##rfillcol", (float*)&fillColor,
                ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_NoSmallPreview);
            ImGui::EndPopup();
        }
        ImGui::SameLine();

        // 5. TEXT COLOR (warna label teks)
        if (IconButton("##RTC", texPopupColor, ImGui::ColorConvertFloat4ToU32(s.textColor)))
            ImGui::OpenPopup("RCP_Text");
        if (ImGui::BeginPopup("RCP_Text")) {
            ImGui::Text("Text Color");
            ImGui::ColorPicker4("##rtxtcol", (float*)&s.textColor,
                ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_NoSmallPreview);
            ImGui::EndPopup();
        }
        ImGui::SameLine();

        // 6. THICKNESS
        if (IconButton("##RT", texPopupThick, white))
            ImGui::OpenPopup("RCP_Thick");
        if (ImGui::BeginPopup("RCP_Thick")) {
            ImGui::SetNextItemWidth(150);
            ImGui::SliderFloat("##rw", &s.thickness, 0.5f, 10.0f, "Tebal: %.1f");
            ImGui::EndPopup();
        }
        ImGui::SameLine();

        // 7. DELETE
        if (IconButton("##RD", texTrash1, IM_COL32(255, 100, 100, 255))) {
            if (!s.locked) result = 1; // delete
        }

        ImGui::PopStyleVar();

        // =====================================================
        // ROW 2: SETTINGS WINDOW (terbuka saat Settings ON)
        // =====================================================
        ImU32 settingCol = settingsOpen ? IM_COL32(100, 255, 100, 255) : white;
        // Settings toggle via bubble bar extension (pakai tombol setting di atas)

        if (settingsOpen) {
            char title[64];
            sprintf(title, "Rect Settings##%s", s.id.c_str());
            ImGui::SetNextWindowSize(ImVec2(340, 300), ImGuiCond_FirstUseEver);

            if (ImGui::Begin(title, &settingsOpen)) {

                // --- Label Teks Custom ---
                if (ImGui::CollapsingHeader("Label", ImGuiTreeNodeFlags_DefaultOpen)) {
                    ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.9f, 1.0f), "Custom Text");
                    ImGui::SameLine();
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Teks custom di dalam rectangle.\nKosongkan = tampilkan ukuran (W x H).");
                    ImGui::SetNextItemWidth(200);
                    if (ImGui::InputText("##rectlabel", rectLabelBuf, sizeof(rectLabelBuf))) {
                        s.rectLabel = std::string(rectLabelBuf);
                    }

                    ImGui::Spacing();
                    ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.9f, 1.0f), "Text Color");
                    ImGui::SameLine();
                    ImGui::ColorEdit4("##rtcol2", (float*)&s.textColor,
                        ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);
                }

                // --- Fill ---
                if (ImGui::CollapsingHeader("Fill", ImGuiTreeNodeFlags_DefaultOpen)) {
                    ImGui::Checkbox("Filled", &s.filled);

                    if (s.filled) {
                        ImGui::Spacing();
                        ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.9f, 1.0f), "Fill Color");
                        ImGui::SameLine();
                        ImGui::ColorEdit4("##rfillcol2", (float*)&s.color,
                            ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);

                        ImGui::Spacing();
                        ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.9f, 1.0f), "Opacity");
                        ImGui::SetNextItemWidth(200);
                        ImGui::SliderFloat("##rectopacity", &s.fillOpacity, 0.0f, 1.0f, "%.2f");
                    }
                }

                // --- Border ---
                if (ImGui::CollapsingHeader("Border", ImGuiTreeNodeFlags_DefaultOpen)) {
                    ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.9f, 1.0f), "Line Style");
                    ImGui::SetNextItemWidth(180);
                    const char* styles[] = { "Solid", "Dashed", "Dotted" };
                    ImGui::Combo("##rectstyle", &s.lineStyle, styles, IM_ARRAYSIZE(styles));

                    ImGui::Spacing();
                    ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.9f, 1.0f), "Thickness");
                    ImGui::SetNextItemWidth(200);
                    ImGui::SliderFloat("##rectthick2", &s.thickness, 0.5f, 10.0f, "%.1f px");
                }

                // --- Display ---
                if (ImGui::CollapsingHeader("Display")) {
                    ImGui::Checkbox("Show Dimensions (W x H)", &s.showDimensions);
                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip("Tampilkan ukuran lebar x tinggi.\nHanya aktif jika Label kosong.");
                    }
                }
            }
            ImGui::End();
        }

        return result;
    }
};

// No out-of-class definitions needed — all static members are inline (C++17)
