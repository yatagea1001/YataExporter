// =========================================================
// CREATOR ENGINE HEADER - THE BRIDGE & AI CONTEXT
// =========================================================
#pragma once

#include "imgui.h"
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <map>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

// ---------------------------------------------------------
// 1. STRUKTUR INJECTION (SUNTIKAN DARI JS)
// ---------------------------------------------------------
struct CInjectedFeature {
    std::string name;
    std::string category;
    std::string jsActionCode; 
};

// Database Fitur Suntikan
inline std::vector<CInjectedFeature> g_injectedFeatures;

// FUNGSI EXPORT: Supaya JS bisa kirim perintah ke sini
extern "C" {
#ifdef __EMSCRIPTEN__
    EMSCRIPTEN_KEEPALIVE
#endif
    void InjectFeature(const char* name, const char* category, const char* jsCode) {
        CInjectedFeature item;
        item.name = name;
        item.category = category;
        item.jsActionCode = jsCode;
        
        g_injectedFeatures.push_back(item);
        printf("💉 [CreatorEngine] Fitur Masuk: %s [%s]\n", name, category);
    }
}

// ---------------------------------------------------------
// 2. SOURCE INSPECTOR (AI CONTEXT HELPER)
// ---------------------------------------------------------
// Membaca file header yang di-embed ke dalam virtual file system
inline std::string ReadEmbeddedFile(const char* filepath) {
    std::ifstream file(filepath);
    if (file) {
        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }
    return "// Error: File ini tidak di-embed di build_web.bat!\n// Pastikan --embed-file sudah dipasang.";
}

inline void RenderSourceInspector() {
    static std::string currentCode = "// Pilih file di atas untuk melihat isinya...";
    static const char* currentFileLabel = "Pilih File Header...";

    ImGui::SeparatorText("🧠 AI Context Helper");
    ImGui::TextWrapped("Gunakan ini untuk menyalin struktur kode C++ ke AI (ChatGPT/Gemini) agar dia bisa bantu buatkan logika JS.");

    // DAFTAR FILE PENTING (Sesuaikan dengan yang di-embed di .bat)
    if (ImGui::Button("Candle.h")) { 
        currentFileLabel = "Candle.h"; currentCode = ReadEmbeddedFile("Candle.h"); 
    } ImGui::SameLine();
    
    if (ImGui::Button("Indicators.h")) { 
        currentFileLabel = "Indicators.h"; currentCode = ReadEmbeddedFile("Indicators.h"); 
    } ImGui::SameLine();

    if (ImGui::Button("TradeModule.h")) { 
        currentFileLabel = "TradeModule.h"; currentCode = ReadEmbeddedFile("TradeModule.h"); 
    }

    if (ImGui::Button("GlobalShapeManager.h")) { 
        currentFileLabel = "GlobalShapeManager.h"; currentCode = ReadEmbeddedFile("GlobalShapeManager.h"); 
    } ImGui::SameLine();

    if (ImGui::Button("GPUCandleRenderer.h")) { 
        currentFileLabel = "GPUCandleRenderer.h"; currentCode = ReadEmbeddedFile("GPUCandleRenderer.h"); 
    }

    ImGui::Separator();
    
    // Header File & Copy Button
    ImGui::Text("📂 File: %s", currentFileLabel);
    ImGui::SameLine();
    if (ImGui::Button("📋 COPY CODE")) {
        ImGui::SetClipboardText(currentCode.c_str());
        ImGui::OpenPopup("Copied!");
    }

    // Popup kecil notifikasi copy
    if (ImGui::BeginPopup("Copied!")) {
        ImGui::Text("Tersalin ke Clipboard!");
        ImGui::EndPopup();
    }

    // Tampilan Kode
    ImGui::BeginChild("CodeView", ImVec2(0, 300), true, ImGuiWindowFlags_HorizontalScrollbar);
    ImGui::TextUnformatted(currentCode.c_str());
    ImGui::EndChild();
}

// ---------------------------------------------------------
// 3. RENDERER UTAMA (PANEL DEVELOPER)
// ---------------------------------------------------------
inline void ShowCreatorPanel(bool* p_open) {
    if (!ImGui::Begin("🛠️ YATA CREATOR MODE (Dev Only)", p_open)) {
        ImGui::End();
        return;
    }

    if (ImGui::BeginTabBar("CreatorTabs")) {
        
        // TAB 1: LIVE FEATURES (Fitur Suntikan JS)
        if (ImGui::BeginTabItem("🚀 Live Features")) {
            if (g_injectedFeatures.empty()) {
                ImGui::TextDisabled("Belum ada fitur disuntikkan dari JS.");
                ImGui::TextDisabled("Edit 'my_dev_logic.js' lalu reload browser.");
            } else {
                // Grouping by Category
                std::map<std::string, std::vector<CInjectedFeature*>> groups;
                for (auto& item : g_injectedFeatures) groups[item.category].push_back(&item);

                for (auto const& [catName, items] : groups) {
                    if (ImGui::CollapsingHeader(catName.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
                        for (auto* item : items) {
                            if (ImGui::Button(item->name.c_str())) {
                                #ifdef __EMSCRIPTEN__
                                    emscripten_run_script(item->jsActionCode.c_str());
                                #else
                                    printf("EXEC JS: %s\n", item->jsActionCode.c_str());
                                #endif
                            }
                            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Klik untuk jalankan logika JS");
                        }
                    }
                }
            }
            ImGui::EndTabItem();
        }

        // TAB 2: AI CONTEXT HELPER (Source Viewer)
        if (ImGui::BeginTabItem("🧠 Source Inspector")) {
            RenderSourceInspector();
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::End();
}