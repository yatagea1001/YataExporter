// ==================================================================================
// CROSSHAIR RENDERER — Per-Tab Reusable
// ==================================================================================
// File ini berisi semua logika crosshair yang bisa dipakai oleh SETIAP tab chart:
//
//   DrawAxisLabel()              → utilitas label harga/waktu
//   CrosshairState               → struct state crosshair
//   CrosshairReset(state)        → reset active flag tiap frame
//   CrosshairDrawMain(state, …)  → crosshair di chart utama (candle)
//   CrosshairDrawPanel(state, …) → crosshair di panel indicator
//   CrosshairDrawPost(state)     → garis V di chart utama saat cursor di panel
//
// CARA PAKAI (di RenderSingleChartWindow, per-tab):
//   1. Di awal BeginSubplots:
//        CrosshairReset(tab->crosshair);
//   2. Di dalam chart utama (BeginPlot candle):
//        CrosshairDrawMain(tab->crosshair, tab->timeframe, hasPanels, globalOffset);
//   3. Di dalam setiap panel indicator (BeginPlot panel):
//        CrosshairDrawPanel(tab->crosshair, tab->timeframe, globalOffset, isLast, isDrawing);
//   4. Setelah EndSubplots:
//        if (hasPanels) CrosshairDrawPost(tab->crosshair);
//
// DEPENDENSI:
//   - imgui.h, implot.h
//   - Candle.h
//   - g_allCandles, g_isTouchActive (extern dari main.cpp)
// ==================================================================================

#pragma once

#include "imgui.h"
#include "implot.h"
#include "Candle.h"
#include <cmath>
#include <cstring>
#include <ctime>
#include <string>
#include <map>
#include <vector>
#include <mutex>

// ---- Forward declarations dari main.cpp ----
extern std::map<std::string, std::vector<Candle>> g_allCandles;
extern bool g_isTouchActive;

// =========================================================
// 1. UTILITAS: DrawAxisLabel
// =========================================================
inline void DrawAxisLabel(ImDrawList* draw, ImVec2 pos, const char* text,
                           ImU32 bgColor, ImU32 textColor, bool isYAxis) {
    ImVec2 textSize = ImGui::CalcTextSize(text);
    float padding = 4.0f;
    ImVec2 rectMin, rectMax;

    if (isYAxis) {
        rectMin = ImVec2(pos.x, pos.y - textSize.y / 2 - padding);
        rectMax = ImVec2(pos.x + textSize.x + padding * 2, pos.y + textSize.y / 2 + padding);
    } else {
        rectMin = ImVec2(pos.x - textSize.x / 2 - padding, pos.y);
        rectMax = ImVec2(pos.x + textSize.x / 2 + padding, pos.y + textSize.y + padding * 2);
    }

    draw->AddRectFilled(rectMin, rectMax, bgColor, 4.0f);
    ImVec2 textPos = ImVec2(rectMin.x + padding, rectMin.y + padding);
    draw->AddText(textPos, textColor, text);
}

// =========================================================
// 2. CROSSHAIR STATE (per-tab)
// =========================================================
struct CrosshairState {
    bool active = false;
    double x_index = 0;
    double y_price = 0;
    std::string timeLabel = "";
    ImVec2 mousePosPixels;
    float  xPixel = 0.0f;
    // Batas pixel chart utama — disimpan saat BeginPlot chart utama
    ImVec2 mainPlotMin = {0, 0};
    ImVec2 mainPlotMax = {0, 0};

    void Reset() { active = false; }
};

// =========================================================
// 3. RESET tiap frame (panggil di awal render tab)
// =========================================================
inline void CrosshairReset(CrosshairState& cs) {
    cs.Reset();
}

// =========================================================
// 4. CROSSHAIR CHART UTAMA
//    Dipanggil di dalam BeginPlot() chart candle
//    cs            → state crosshair tab ini
//    activeTF      → timeframe (misal "M5")
//    hasPanels     → true kalau ada panel indicator di bawah
//    globalOffset  → offset index buffer render ke global index
//    symbolKey     → key candle (misal "XAUUSD_M5" atau "M5")
// =========================================================
inline void CrosshairDrawMain(CrosshairState& cs,
                               const std::string& symbolKey,
                               bool hasPanels,
                               int globalOffset) {
    bool plotHovered = ImPlot::IsPlotHovered();

    #ifdef __EMSCRIPTEN__
    if (!plotHovered && g_isTouchActive) {
        ImVec2 pPos  = ImPlot::GetPlotPos();
        ImVec2 pSz   = ImPlot::GetPlotSize();
        ImGuiIO& tio = ImGui::GetIO();
        plotHovered = (tio.MousePos.x >= pPos.x && tio.MousePos.x <= pPos.x + pSz.x &&
                       tio.MousePos.y >= pPos.y && tio.MousePos.y <= pPos.y + pSz.y);
    }
    #endif

    if (plotHovered) {
        cs.active = true;
        ImPlotPoint mouse = ImPlot::GetPlotMousePos();
        ImDrawList* draw = ImPlot::GetPlotDrawList();

        cs.x_index = mouse.x;
        cs.y_price = mouse.y;
        cs.mousePosPixels = ImPlot::PlotToPixels(mouse);

        ImVec2 plotMin = ImPlot::GetPlotPos();
        ImVec2 plotSize = ImPlot::GetPlotSize();
        ImVec2 plotMax = ImVec2(plotMin.x + plotSize.x, plotMin.y + plotSize.y);
        ImU32 lineColor = IM_COL32(150, 150, 150, 180);

        cs.mainPlotMin = plotMin;
        cs.mainPlotMax = plotMax;

        // 1. GAMBAR GARIS
        cs.xPixel = cs.mousePosPixels.x;
        draw->AddLine(ImVec2(plotMin.x, cs.mousePosPixels.y),
                      ImVec2(plotMax.x, cs.mousePosPixels.y), lineColor, 1.0f);
        draw->AddLine(ImVec2(cs.mousePosPixels.x, plotMin.y),
                      ImVec2(cs.mousePosPixels.x, plotMax.y), lineColor, 1.0f);

        // 2. LABEL HARGA (Y)
        ImU32 labelBg = IM_COL32(40, 40, 40, 255);
        ImU32 labelTxt = IM_COL32(255, 255, 255, 255);
        char priceBuf[32];
        snprintf(priceBuf, 32, "%.5f", mouse.y);
        DrawAxisLabel(draw, ImVec2(plotMax.x, cs.mousePosPixels.y),
                      priceBuf, labelBg, labelTxt, true);

        // 3. LABEL WAKTU (X)
        time_t rawTime = 0;
        int localIdx = (int)round(mouse.x);
        int globalIdx = localIdx + globalOffset;

        if (g_allCandles.count(symbolKey) && !g_allCandles[symbolKey].empty()) {
            const auto& fullData = g_allCandles[symbolKey];

            if (globalIdx >= 0 && globalIdx < (int)fullData.size()) {
                rawTime = (time_t)fullData[globalIdx].time;
            } else {
                double lastTime = fullData.back().time;
                int lastIndex = (int)fullData.size() - 1;

                double timePerCandle = 60.0;
                if (symbolKey.find("M5") != std::string::npos || symbolKey == "M5") timePerCandle = 300.0;
                else if (symbolKey.find("M15") != std::string::npos || symbolKey == "M15") timePerCandle = 900.0;
                else if (symbolKey.find("M30") != std::string::npos || symbolKey == "M30") timePerCandle = 1800.0;
                else if (symbolKey.find("H1") != std::string::npos || symbolKey == "H1") timePerCandle = 3600.0;
                else if (symbolKey.find("H4") != std::string::npos || symbolKey == "H4") timePerCandle = 14400.0;
                else if (symbolKey.find("D1") != std::string::npos || symbolKey == "D1") timePerCandle = 86400.0;

                double diffSeconds = (double)(globalIdx - lastIndex) * timePerCandle;
                rawTime = (time_t)(lastTime + diffSeconds);
            }
        }

        struct tm* timeInfo = localtime(&rawTime);
        char timeBuf[32];
        strftime(timeBuf, 32, "%d %b %H:%M", timeInfo);
        cs.timeLabel = std::string(timeBuf);

        if (!hasPanels) {
            DrawAxisLabel(draw, ImVec2(cs.mousePosPixels.x, plotMax.y),
                         cs.timeLabel.c_str(), labelBg, labelTxt, false);
        }

        ImGui::SetMouseCursor(ImGuiMouseCursor_None);
    }
}

// =========================================================
// 5. CROSSHAIR DI PANEL INDICATOR
//    Dipanggil di dalam BeginPlot() panel indicator.
// =========================================================
inline void CrosshairDrawPanel(CrosshairState& cs,
                                const std::string& symbolKey,
                                int global_idx_offset,
                                bool isLast,
                                bool isDrawingActive) {
    ImDrawList* pdl  = ImPlot::GetPlotDrawList();
    ImVec2 pPos      = ImPlot::GetPlotPos();
    ImVec2 pSz       = ImPlot::GetPlotSize();
    ImVec2 pMax      = ImVec2(pPos.x + pSz.x, pPos.y + pSz.y);
    ImU32  lineColor = IM_COL32(150, 150, 150, 150);
    ImU32  labelBg   = IM_COL32(40, 40, 40, 255);
    ImU32  labelTxt  = IM_COL32(255, 255, 255, 255);

    bool panelHovered = ImPlot::IsPlotHovered();
    #ifdef __EMSCRIPTEN__
    if (!panelHovered && g_isTouchActive) {
        ImGuiIO& tio = ImGui::GetIO();
        panelHovered = (tio.MousePos.x >= pPos.x && tio.MousePos.x <= pMax.x &&
                        tio.MousePos.y >= pPos.y && tio.MousePos.y <= pMax.y);
    }
    #endif

    if (panelHovered) {
        ImPlotPoint mouse = ImPlot::GetPlotMousePos();
        ImVec2 mp = ImPlot::PlotToPixels(mouse);

        cs.active    = true;
        cs.xPixel    = mp.x;
        cs.x_index   = mouse.x;

        // Update time label dari index
        int localIdx  = (int)round(mouse.x);
        int globalIdx = localIdx + global_idx_offset;
        if (g_allCandles.count(symbolKey) && !g_allCandles[symbolKey].empty()) {
            const auto& fd = g_allCandles[symbolKey];
            if (globalIdx >= 0 && globalIdx < (int)fd.size()) {
                time_t rt = (time_t)fd[globalIdx].time;
                struct tm* ti = localtime(&rt);
                char tb[32];
                strftime(tb, 32, "%d %b %H:%M", ti);
                cs.timeLabel = std::string(tb);
            }
        }

        // Garis horizontal (Y) — hanya di panel yang di-hover
        pdl->AddLine(ImVec2(pPos.x, mp.y), ImVec2(pMax.x, mp.y), lineColor, 1.0f);

        // Label nilai Y di kanan
        char yBuf[32];
        snprintf(yBuf, 32, "%.2f", mouse.y);
        DrawAxisLabel(pdl, ImVec2(pMax.x, mp.y), yBuf, labelBg, labelTxt, true);

        ImGui::SetMouseCursor(ImGuiMouseCursor_None);
    }

    // Garis vertikal — dari state (chart utama ATAU panel lain yang di-hover)
    if (!isDrawingActive && cs.active &&
        cs.xPixel >= pPos.x && cs.xPixel <= pMax.x) {
        pdl->AddLine(
            ImVec2(cs.xPixel, pPos.y),
            ImVec2(cs.xPixel, pMax.y),
            lineColor, 1.0f);

        // Label waktu hanya di panel paling bawah
        if (isLast && !cs.timeLabel.empty()) {
            DrawAxisLabel(pdl,
                ImVec2(cs.xPixel, pMax.y),
                cs.timeLabel.c_str(),
                labelBg, labelTxt, false);
        }
    }
}

// =========================================================
// 6. POST-RENDER: Garis V di chart utama jika cursor di panel
//    Dipanggil SETELAH ImPlot::EndSubplots()
// =========================================================
inline void CrosshairDrawPost(CrosshairState& cs) {
    if (cs.active &&
        cs.xPixel > 0.0f &&
        cs.mainPlotMax.x > cs.mainPlotMin.x) {
        float xp = cs.xPixel;
        if (xp >= cs.mainPlotMin.x && xp <= cs.mainPlotMax.x) {
            ImDrawList* wdl = ImGui::GetWindowDrawList();
            wdl->AddLine(
                ImVec2(xp, cs.mainPlotMin.y),
                ImVec2(xp, cs.mainPlotMax.y),
                IM_COL32(150, 150, 150, 150), 1.0f);
        }
    }
}

// =========================================================
// 7. HELPER: Build symbol key untuk g_allCandles lookup
//    Tab utama (usesGlobalData)  → key = "M5", "H1", dll
//    Tab non-primary             → key = "XAUUSD_M5", "BTCUSDT_H1", dll
// =========================================================
inline std::string BuildCandleKey(const std::string& symbol, 
                                    const std::string& tf, 
                                    bool usesGlobalData) {
    return usesGlobalData ? tf : (symbol + "_" + tf);
}
