#pragma once
#include "imgui.h"
#include "imgui_internal.h"
#include "GlobalShapeManager.h"
#include <vector>
#include <algorithm>

// =========================================================
// 🎯 FIBONACCI SETTINGS UI (PROFESSIONAL STYLE)
// =========================================================
// Popup khusus untuk edit Fibonacci dengan custom levels

class FibSettingsUI {
public:
    // State Internal (untuk animasi dan tracking)
    static bool isOpen;
    static float animProgress;
    static std::string editingShapeId;
    
    // Temporary Buffer untuk Edit Levels
    static std::vector<double> tempLevels;
    static char newLevelInput[16];
    
    // =========================================================
    // 📌 FUNGSI UTAMA: BUKA POPUP
    // =========================================================
    static void Open(const std::string& shapeId, const std::vector<double>& currentLevels) {
        isOpen = true;
        animProgress = 0.0f;
        editingShapeId = shapeId;
        
        // Copy levels yang ada ke buffer temporary
        tempLevels = currentLevels;
        
        // Sort supaya urut dari kecil ke besar
        std::sort(tempLevels.begin(), tempLevels.end());
        
        newLevelInput[0] = '\0'; // Reset input field
    }
    
    // =========================================================
    // 🎨 RENDER POPUP (Dipanggil di Main Loop)
    // =========================================================
    static bool Render(GlobalShape* shape) {
        if (!isOpen || !shape) return false;
        
        // --- ANIMASI SMOOTH (Elastic Easing) ---
        float animSpeed = 8.0f;
        animProgress += ImGui::GetIO().DeltaTime * animSpeed;
        if (animProgress > 1.0f) animProgress = 1.0f;
        
        float t = animProgress;
        float scale = 1.0f - std::pow(1.0f - t, 3.0f);
        
        // --- UKURAN & POSISI ---
        float targetW = 320.0f;
        float targetH = 450.0f;
        float currentW = targetW * scale;
        float currentH = targetH * scale;
        
        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImVec2 popupPos(
            center.x - currentW * 0.5f, 
            center.y - currentH * 0.5f
        );
        
        ImGui::SetNextWindowPos(popupPos, ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(currentW, currentH));
        ImGui::SetNextWindowBgAlpha(0.95f * scale);
        
        // --- STYLING ---
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 12.0f * scale);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(15, 15));
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.12f, 0.12f, 0.12f, 0.95f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
        
        ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | 
                                 ImGuiWindowFlags_NoMove | 
                                 ImGuiWindowFlags_NoResize |
                                 ImGuiWindowFlags_NoSavedSettings;
        
        bool shouldClose = false;
        
        if (ImGui::Begin("##FibSettings", nullptr, flags)) {
            ImGui::SetWindowFontScale(scale);
            
            // =============================================
            // 📋 HEADER
            // =============================================
            ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]); // Font besar jika ada
            ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.0f, 1.0f), "⚙️ Fibonacci Settings");
            ImGui::PopFont();
            
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            
            // =============================================
            // 🎨 SECTION 1: COLOR & THICKNESS
            // =============================================
            ImGui::Text("Visual Style");
            ImGui::Spacing();
            
            // Color Picker
            ImGui::Text("Color:");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(200);
            ImGui::ColorEdit4("##FibColor", (float*)&shape->color, 
                ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar);
            
            // Thickness Slider
            ImGui::Text("Line Thickness:");
            ImGui::SetNextItemWidth(250);
            ImGui::SliderFloat("##FibThick", &shape->thickness, 0.5f, 5.0f, "%.1f px");
            
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            
            // =============================================
            // 📊 SECTION 2: FIBONACCI LEVELS (CUSTOM)
            // =============================================
            ImGui::Text("Fibonacci Levels (Custom)");
            ImGui::Spacing();
            
            // Scrollable Child Window untuk list levels
            ImGui::BeginChild("LevelsList", ImVec2(0, 180), true, ImGuiWindowFlags_AlwaysVerticalScrollbar);
            
            // Loop render semua levels
            for (int i = 0; i < (int)tempLevels.size(); i++) {
                ImGui::PushID(i);
                
                // Display Level Value
                char label[32];
                sprintf(label, "%.3f (%.1f%%)", tempLevels[i], tempLevels[i] * 100.0f);
                
                ImGui::Bullet();
                ImGui::SameLine();
                ImGui::Text("%s", label);
                
                // Delete Button (Red X)
                ImGui::SameLine(ImGui::GetContentRegionAvail().x - 30);
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 0.6f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 0.3f, 0.3f, 0.8f));
                
                if (ImGui::Button("✖", ImVec2(25, 20))) {
                    tempLevels.erase(tempLevels.begin() + i);
                    i--; // Adjust loop karena size berubah
                }
                
                ImGui::PopStyleColor(2);
                ImGui::PopID();
            }
            
            ImGui::EndChild();
            
            ImGui::Spacing();
            
            // =============================================
            // ➕ ADD NEW LEVEL
            // =============================================
            ImGui::Text("Add New Level:");
            ImGui::SetNextItemWidth(150);
            ImGui::InputText("##NewLevel", newLevelInput, sizeof(newLevelInput), 
                ImGuiInputTextFlags_CharsDecimal);
            
            ImGui::SameLine();
            
            if (ImGui::Button("Add +", ImVec2(80, 0))) {
                // Parse input
                double newLevel = 0.0;
                if (sscanf(newLevelInput, "%lf", &newLevel) == 1) {
                    // Validasi range (0.0 - 2.0 biasanya cukup)
                    if (newLevel >= -1.0 && newLevel <= 3.0) {
                        tempLevels.push_back(newLevel);
                        std::sort(tempLevels.begin(), tempLevels.end());
                        newLevelInput[0] = '\0'; // Clear input
                    }
                }
            }
            
            ImGui::Spacing();
            
            // Quick Presets
            ImGui::Text("Quick Presets:");
            if (ImGui::Button("Standard Fib", ImVec2(110, 0))) {
                tempLevels = {0.0, 0.236, 0.382, 0.5, 0.618, 0.786, 1.0};
            }
            ImGui::SameLine();
            if (ImGui::Button("Extended", ImVec2(110, 0))) {
                tempLevels = {0.0, 0.236, 0.382, 0.5, 0.618, 0.786, 1.0, 1.272, 1.414, 1.618};
            }
            
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            
            // =============================================
            // 💾 BOTTOM BUTTONS (Save / Cancel)
            // =============================================
            float btnWidth = (ImGui::GetContentRegionAvail().x - 10) * 0.5f;
            
            // SAVE Button (Green)
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.3f, 0.8f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.8f, 0.4f, 1.0f));
            
            if (ImGui::Button("💾 SAVE", ImVec2(btnWidth, 35))) {
                // Apply changes ke shape asli
                if (!tempLevels.empty()) {
                    // Simpan ke field custom di GlobalShape
                    // NOTE: Anda perlu tambah field ini di GlobalShape struct
                    // Contoh: shape->fibLevels = tempLevels;
                    
                    // Untuk sementara kita print aja (implementasi full nanti)
                    printf("✅ Fib Levels Updated: ");
                    for (double lvl : tempLevels) printf("%.3f ", lvl);
                    printf("\n");
                }
                shouldClose = true;
            }
            
            ImGui::PopStyleColor(2);
            ImGui::SameLine();
            
            // CANCEL Button (Red)
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.2f, 0.2f, 0.8f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.3f, 0.3f, 1.0f));
            
            if (ImGui::Button("✖ CANCEL", ImVec2(btnWidth, 35))) {
                shouldClose = true;
            }
            
            ImGui::PopStyleColor(2);
            
            ImGui::SetWindowFontScale(1.0f);
        }
        ImGui::End();
        
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(3);
        
        // Close Logic
        if (shouldClose) {
            isOpen = false;
            editingShapeId = "";
        }
        
        return !shouldClose;
    }
    
    // =========================================================
    // 🔒 CLOSE MANUAL (Dari luar jika perlu)
    // =========================================================
    static void Close() {
        isOpen = false;
        editingShapeId = "";
    }
};

// Static Member Initialization (Taruh di .cpp atau di main sebelum main())
bool FibSettingsUI::isOpen = false;
float FibSettingsUI::animProgress = 0.0f;
std::string FibSettingsUI::editingShapeId = "";
std::vector<double> FibSettingsUI::tempLevels;
char FibSettingsUI::newLevelInput[16] = "";