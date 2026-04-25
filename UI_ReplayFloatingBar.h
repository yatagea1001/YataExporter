#pragma once

#include <cmath>
#include <algorithm>
#include <mutex>

// ==============================================================================
// UI_ReplayFloatingBar.h
// Dipisahkan dari MAIN.CPP agar kode lebih rapi. 
// 
// PENTING: Pastikan untuk melakukan #include file ini di MAIN.CPP 
// SETELAH semua variabel global (g_replay, g_candlesMutex, IconType, dll) 
// dan fungsi IconButton selesai dideklarasikan!
// ==============================================================================

inline void RenderReplayFloatingBar(bool& replayMode, bool& replayStarted) {
    // -----------------------------------------------------------
    // 1. LOGIKA RESET OTOMATIS (Berdasarkan Frame Count) 🔥 BARU 🔥
    // -----------------------------------------------------------
    // Kita cek: Apakah fungsi ini dipanggil berurutan?
    // Kalau selisih frame sekarang dan terakhir > 1, berarti panel ini BARU MUNCUL LAGI.
    
    static int lastFrameRendered = -1;
    static float animProgress = 0.0f;
    
    int currentFrame = ImGui::GetFrameCount();
    
    // JIKA BARU MUNCUL SETELAH HILANG (Gap Frame Besar) -> RESET ANIMASI
    if (currentFrame - lastFrameRendered > 1) {
        animProgress = 0.0f; 
    }
    
    // Simpan frame saat ini untuk pengecekan berikutnya
    lastFrameRendered = currentFrame;

    // --- Update Animasi ---
    float animSpeed = 6.0f; 
    animProgress += ImGui::GetIO().DeltaTime * animSpeed;
    if (animProgress > 1.0f) animProgress = 1.0f;

    // Easing Effect (Membal dikit)
    float t = animProgress;
    float scale = 1.0f - std::pow(1.0f - t, 3.0f); 

    // =========================================================
    // ... DARI SINI KE BAWAH SAMA PERSIS KAYA SEBELUMNYA ...
    // =========================================================

    ImGuiIO& io = ImGui::GetIO();

    float targetW = 320.0f;
    float targetH = 85.0f;
    float currentW = targetW * scale;
    float currentH = targetH * scale;

    static ImVec2 windowPos;
    float defaultX = (io.DisplaySize.x - targetW) * 0.5f;
    float defaultY = io.DisplaySize.y - targetH - 50.0f;

    float animX = defaultX + (targetW - currentW) * 0.5f;
    float animY = defaultY + (targetH - currentH) * 0.5f;

    ImGui::SetNextWindowPos(ImVec2(animX, animY), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(currentW, currentH));
    ImGui::SetNextWindowBgAlpha(0.85f * scale); 
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 15.0f * scale);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 8));

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | 
                             ImGuiWindowFlags_NoDocking | 
                             ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoScrollbar |
                             ImGuiWindowFlags_NoFocusOnAppearing; 

    if (ImGui::Begin("##ReplayFloatingBar", nullptr, flags)) {
        
        ImGui::SetWindowFontScale(scale);

        // --- LOGIKA DRAG ---
        static bool isDragging = false;
        static ImVec2 dragOffset = ImVec2(0, 0);

        if (animProgress > 0.8f) {
            if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                if (!ImGui::IsAnyItemActive()) {
                    isDragging = true;
                    ImVec2 mousePos = ImGui::GetIO().MousePos;
                    ImVec2 curWinPos = ImGui::GetWindowPos();
                    dragOffset = ImVec2(mousePos.x - curWinPos.x, mousePos.y - curWinPos.y);
                    ImGui::SetWindowFocus(); 
                }
            }

            if (isDragging && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                ImVec2 mousePos = ImGui::GetIO().MousePos;
                ImGui::SetWindowPos(ImVec2(mousePos.x - dragOffset.x, mousePos.y - dragOffset.y));
            }

            if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) isDragging = false;
        }

        // --- ISI KONTEN ---
        float contentW = ImGui::GetContentRegionAvail().x;
        ImGui::SetCursorPosY(10.0f * scale); 
        float buttonsW = 120.0f * scale; 
        float cursorX = (contentW - buttonsW) * 0.5f;
        if (cursorX < 0) cursorX = 0;
        ImGui::SetCursorPosX(cursorX);

        // [PREV]
        if (IconButton("##BtnJump", ICON_PREV, false)) {
            int targetIndex = (int)std::round(g_replayCutoff.lineIndex);
            int m1_index_target = 0;
            {
                std::lock_guard<std::mutex> lock(g_candlesMutex);
                if (g_allCandles.count(g_activeTF) && !g_allCandles[g_activeTF].empty()) {
                    double t = g_allCandles[g_activeTF][std::clamp(targetIndex, 0, (int)g_allCandles[g_activeTF].size()-1)].time;
                    auto& m1 = g_allCandles["M1"];
                    if (!m1.empty()) {
                        auto it = std::lower_bound(m1.begin(), m1.end(), t, [](const Candle& c, double time){ return c.time < time; });
                        if(it != m1.end()) m1_index_target = (int)std::distance(m1.begin(), it);
                    }
                }
            }
            g_replay.SetIndex(m1_index_target);
            g_replay.Pause();
        }
        ImGui::SameLine();

        // [PLAY/PAUSE]
        IconType playIcon = g_replay.running ? ICON_PAUSE : ICON_PLAY;
        if (IconButton("##BtnPlay", playIcon, g_replay.running)) {
            g_replay.Toggle();
            // Set replayStarted=true saat Play pertama kali ditekan
            if (g_replay.running) replayStarted = true;
        }
        ImGui::SameLine();

        // [NEXT]
        if (IconButton("##BtnStep", ICON_NEXT, false)) {
            int stepSize = 1;
            if (g_activeTF == "M5") stepSize = 5;
            else if (g_activeTF == "M15") stepSize = 15;
            else if (g_activeTF == "M30") stepSize = 30;
            else if (g_activeTF == "H1") stepSize = 60;
            else if (g_activeTF == "H4") stepSize = 240;
            
            g_replay.StepForward(stepSize);
        }

        // --- SLIDER SPEED ---
        ImGui::SetCursorPosY(50.0f * scale); 
        ImGui::Text("Speed"); 
        ImGui::SameLine();
        
        float sliderWidth = contentW - (55.0f * scale);
        if (sliderWidth < 10) sliderWidth = 10;
        ImGui::SetNextItemWidth(sliderWidth); 
        
        static int speedLevel = 20;
        ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(40, 40, 40, 255));
        ImGui::PushStyleColor(ImGuiCol_SliderGrab, IM_COL32(0, 140, 255, 255));
        
        if (ImGui::SliderInt("##SpeedSlider", &speedLevel, 1, 100, "")) {
            float finalSpeed = 0.0f;
            if (speedLevel <= 30) {
                finalSpeed = (float)speedLevel * 0.33f;
                if (finalSpeed < 0.1f) finalSpeed = 0.1f;
            } else {
                finalSpeed = 10.0f * std::pow(1.12f, (float)(speedLevel - 30));
            }
            g_replay.speed = finalSpeed;
        }
        ImGui::PopStyleColor(2);

        ImGui::SetWindowFontScale(1.0f);
    }
    ImGui::End();
    ImGui::PopStyleVar(2);
}