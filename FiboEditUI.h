#pragma once
#include "imgui.h"
#include "imgui_internal.h"
#include "GlobalShapeManager.h"

// =========================================================
// 1. EXTERNAL VARIABLES (Jembatan ke Main & Texture)
// =========================================================

// 🔥 SOLUSI ERROR: Kasih tahu file ini kalau g_shapes itu ada di luar
extern GlobalShapeManager g_shapes; 

// Texture Khusus Fibo (Seri 1 & 2 agar tidak bentrok dengan ShapeEditUI)
extern ImTextureID texPopupCopy1;
extern ImTextureID texPopupColor1;
extern ImTextureID texPopupThick1;
extern ImTextureID texPopupLock1;
extern ImTextureID texTrash2;       // Sampah Khusus Fibo
extern ImTextureID texPopupSetting; // Ikon Gerigi

class FiboEditUI {
private:
    // ---------------------------------------------------------
    // HELPER: ICON BUTTON (Visual Tombol Kecil)
    // ---------------------------------------------------------
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

        // Visual Feedback (Background Biru Transparan)
        if (hovered || isActive) 
            win->DrawList->AddRectFilled(bb.Min, bb.Max, IM_COL32(255, 255, 255, 40), 6.0f);
        
        // Border Hijau kalau sedang Aktif (misal Lock ON)
        if (isActive) 
            win->DrawList->AddRect(bb.Min, bb.Max, IM_COL32(0, 255, 0, 200), 6.0f);

        // Render Gambar Ikon
        if (user_texture_id != 0) {
            float padding = 8.0f; 
            ImVec2 pMin = ImVec2(bb.Min.x + padding, bb.Min.y + padding);
            ImVec2 pMax = ImVec2(bb.Max.x - padding, bb.Max.y - padding);
            win->DrawList->AddImage(user_texture_id, pMin, pMax, ImVec2(0,0), ImVec2(1,1), color);
        }

        return pressed;
    }

public:
    // =========================================================
    // 🔥 FUNGSI RENDER UTAMA
    // =========================================================
    static bool Render(GlobalShape& s, bool& isSettingsOpen) {
        bool requestDelete = false;
        
        // Warna Standar
        ImU32 white = IM_COL32(240, 240, 240, 255);
        ImU32 gold  = IM_COL32(255, 215, 0, 255);

        // Atur Jarak Antar Ikon
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(5, 5));

        // -----------------------------------------------------
        // A. MENU BUBBLE KECIL (POPUP BARIS)
        // -----------------------------------------------------

        // 1. LOCK 🔒 (Pakai texPopupLock1)
        if(IconButton("##L", texPopupLock1, s.locked ? gold : white, s.locked)) s.locked = !s.locked;
        ImGui::SameLine();
        
        // 2. COPY 📄 (Pakai texPopupCopy1)
        if(IconButton("##C", texPopupCopy1, white)) {
             g_shapes.DuplicateShape(s.id); 
        }
        ImGui::SameLine();
        
        // 3. COLOR 🎨 (Pakai texPopupColor1)
        if(IconButton("##Col", texPopupColor1, ImGui::ColorConvertFloat4ToU32(s.color))) ImGui::OpenPopup("CP_FIB");
        if(ImGui::BeginPopup("CP_FIB")) { 
            ImGui::ColorPicker4("##p",(float*)&s.color, ImGuiColorEditFlags_NoSidePreview|ImGuiColorEditFlags_NoSmallPreview); 
            ImGui::EndPopup(); 
        }
        ImGui::SameLine();
        
        // 4. THICKNESS ✏️ (Pakai texPopupThick1)
        if(IconButton("##T", texPopupThick1, white)) ImGui::OpenPopup("TS_FIB");
        if(ImGui::BeginPopup("TS_FIB")) { 
            ImGui::SetNextItemWidth(150);
            ImGui::SliderFloat("##w",&s.thickness, 1.0f, 10.0f, "Tebal: %.1f"); 
            ImGui::EndPopup(); 
        }
        ImGui::SameLine();

        // 5. SETTINGS ⚙️ (Pakai texPopupSetting) - Pemicu Window Besar
        ImU32 settingCol = isSettingsOpen ? IM_COL32(100, 255, 100, 255) : white;
        if(IconButton("##Set", texPopupSetting, settingCol, isSettingsOpen)) {
            isSettingsOpen = !isSettingsOpen; // Toggle Buka/Tutup
        }
        ImGui::SameLine();

        // 6. DELETE 🗑️ (Pakai texTrash2)
        if(IconButton("##D", texTrash2, IM_COL32(255, 100, 100, 255))) {
            if(!s.locked) requestDelete = true;
        }

        ImGui::PopStyleVar(); // Restore Spacing

        // -----------------------------------------------------
        // B. WINDOW BESAR (CONFIG FIBONACCI PRO)
        // -----------------------------------------------------
        if (isSettingsOpen) {
            // Ukuran default window saat pertama kali buka
            ImGui::SetNextWindowSize(ImVec2(340, 480), ImGuiCond_FirstUseEver);
            
            char title[64];
            sprintf(title, "Fibo Settings ##%s", s.id.c_str());
            
            // Render Window
            if (ImGui::Begin(title, &isSettingsOpen)) { 
                
                // --- SECTION 1: LINE STYLES (Trendline & Horizontal) ---
                if (ImGui::CollapsingHeader("Line Styles", ImGuiTreeNodeFlags_DefaultOpen)) {
                    
                    // a. Trendline (Diagonal)
                    ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.9f, 1.0f), "Trendline (Diagonal)");
                    ImGui::Checkbox("Show##Trend", &s.fibConfig.showTrendline);
                    
                    if (s.fibConfig.showTrendline) {
                        ImGui::SameLine();
                        ImGui::ColorEdit4("##tcol", (float*)&s.fibConfig.trendlineColor, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);
                        ImGui::SameLine();
                        ImGui::SetNextItemWidth(110);
                        const char* styles[] = { "Solid", "Dashed", "Dotted" };
                        ImGui::Combo("##tstyle", &s.fibConfig.trendlineStyle, styles, IM_ARRAYSIZE(styles));
                    }

                    ImGui::Spacing();
                    
                    // b. Levels (Horizontal)
                    ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.9f, 1.0f), "Levels (Horizontal)");
                    ImGui::SetNextItemWidth(180);
                    const char* hStyles[] = { "Solid Line", "Dashed Line", "Dotted Line" };
                    ImGui::Combo("##hstyle", &s.fibConfig.horizStyle, hStyles, IM_ARRAYSIZE(hStyles));
                }     

                // --- SECTION 2: GENERAL SETTINGS ---
                if (ImGui::CollapsingHeader("General", ImGuiTreeNodeFlags_DefaultOpen)) {
                    ImGui::Checkbox("Extend Left", &s.fibConfig.extendLeft);
                    ImGui::SameLine();
                    ImGui::Checkbox("Extend Right", &s.fibConfig.extendRight);
                    
                    ImGui::Checkbox("Reverse Levels", &s.fibConfig.reversed);
                    
                    ImGui::Checkbox("Show Labels", &s.fibConfig.showLabels);
                    if(s.fibConfig.showLabels) {
                        ImGui::SameLine();
                        ImGui::Checkbox("Right Side", &s.fibConfig.labelRight);
                    }
                    
                    ImGui::Checkbox("Background Fill", &s.fibConfig.showBackground);
                }

                // --- SECTION 3: LEVELS & DESCRIPTION (MT5 Style) ---
                if (ImGui::CollapsingHeader("Levels", ImGuiTreeNodeFlags_DefaultOpen)) {
                    
                    ImGui::TextDisabled("Vis  Color    Level    Description");
                    
                    ImGui::BeginChild("ScrollLevels", ImVec2(0, 180), true);
                    int i = 0;
                    for (auto& lvl : s.fibConfig.levels) {
                        ImGui::PushID(i++);
                        
                        // 1. Visible
                        ImGui::Checkbox("##v", &lvl.visible);
                        ImGui::SameLine();
                        
                        // 2. Color
                        ImGui::ColorEdit4("##c", (float*)&lvl.color, ImGuiColorEditFlags_NoInputs|ImGuiColorEditFlags_NoLabel);
                        ImGui::SameLine();
                        
                        // 3. Coeff (Angka)
                        ImGui::SetNextItemWidth(55);
                        ImGui::InputDouble("##val", &lvl.coeff, 0.0, 0.0, "%.3f");
                        ImGui::SameLine();
                        
                        // 4. 🔥 DESCRIPTION (Input Teks Baru)
                        ImGui::SetNextItemWidth(90);
                        ImGui::InputText("##lbl", lvl.label, sizeof(lvl.label));
                        
                        ImGui::PopID();
                    }
                    ImGui::EndChild();

                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::Spacing();

                    // --- SECTION 4: SAVE DEFAULT BUTTON ---
                    // Tombol untuk menyimpan settingan jadi default
                    if (ImGui::Button("💾 SAVE AS DEFAULT", ImVec2(-1, 35))) {
                        g_shapes.SaveAsDefault(s.fibConfig);
                    }
                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip("Simpan settingan ini (Style, Warna, Deskripsi)\nagar otomatis dipakai untuk Fibo berikutnya.");
                    }
                }
            }
            ImGui::End();
        }

        return requestDelete;
    }
};