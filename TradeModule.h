// =================================================================================
// TradeModule.h  kelas LiveTrade untuk manajemen state dan rendering trade, termasuk logika hit SL/TP dan ghost zone setelah trade close
// =================================================================================
#pragma once
#include "imgui.h"
#include "implot.h"
#include <string>
#include <chrono>
#include <cmath>
#include <vector>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <ctime>   
#include "Candle.h" 
#include "TradeHistory.h"
#include "ChartCanvas.h"   // ← untuk FindCandleIndexByTime (cross-TF zone rendering)

// --- FORWARD ENUM (harus sebelum TradeSettingsUI.h) ---
enum TradeType { TRADE_BUY, TRADE_SELL };

#include "TradeSettingsUI.h"

// --- GLOBAL STATE VARIABLES ---
static int g_SelectedTradeID = -1; 
static bool g_IsDraggingGlobal = false;

// Helper Format Uang
static std::string FormatUSD(double val) {
    std::stringstream ss;
    ss << std::fixed << std::setprecision(2) << val;
    return ss.str();
}

// Helper Format Waktu
static std::string FormatTime(double timestamp) {
    time_t t = (time_t)timestamp;
    struct tm tm;
    #ifdef _WIN32
        localtime_s(&tm, &t);
    #else
        localtime_r(&t, &tm);
    #endif
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", &tm); 
    return std::string(buf);
}

// =================================================================================
// CLASS LIVETRADE
// =================================================================================
class LiveTrade {
public:
    int id;
    std::string symbol;
    TradeType type;
    
    // Status
    bool isOpen = true;       
    bool isPending = false;   
    bool isLocked = false; 
    
    // Config
    bool showZones = true; 

    // Harga & Size
    double entryPrice = 0;
    double sl = 0;
    double tp = 0;
    double volume = 0;
    double closePrice = 0;
    double profit = 0;        
    double openTime = 0;
    double openCandleIdx = 0; // ← FIX: index candle saat trade dibuka (bukan timestamp)
    std::string closeReason = "";   // ← FIX: untuk history
    double closeTime = 0;           // ← FIX: untuk history & duration
    double closeCandleIdx = 0;      // ← FIX: index candle saat trade ditutup (untuk ghost zone)

    // Visual State
    bool draggingEntry = false;
    bool draggingSL = false;
    bool draggingTP = false;
    bool slActive = false;
    bool tpActive = false;
    bool ghostPinned = false;  // true = button delete nempel (setelah klik zona)
    ImVec2 ghostPinPos = ImVec2(0,0);  // posisi button saat pinned (zone-top atau above-mouse)

    LiveTrade(int _id, std::string _sym, TradeType _type, double _price, double _vol, double _time, bool _pending)
        : id(_id), symbol(_sym), type(_type), entryPrice(_price), volume(_vol), openTime(_time), isPending(_pending) 
    {
        // Gunakan setting global sebagai default
        showZones = g_TradeSettings.showZones;
        // Auto-set default SL/TP dari TradeVisualSettings
        g_TradeSettings.CalcDefaultSLTP(type, _price, sl, tp);
        slActive = true;
        tpActive = true;
    }

    bool IsDragging() const { return draggingEntry || draggingSL || draggingTP; }

    // =========================================================
    // RENDER GHOST ZONE — dipanggil setelah trade close
    // Zona beku dari openCandleIdx → closeCandleIdx, redup
    // Label teks dihilangkan — hanya zona berwarna + badge profit
    // Return: 0 = nothing, 1 = hovering (prevent deselect), 2 = delete
    // =========================================================
    int RenderGhostZone(const std::vector<Candle>* tfCandles = nullptr) {
        int result = 0;
        if (isOpen) goto done;
        if (!slActive && !tpActive) goto done;
        if (!g_TradeSettings.showGhostZones) goto done;

        {
        ImPlotRect limits = ImPlot::GetPlotLimits();
        ImDrawList* draw  = ImPlot::GetPlotDrawList();
        bool isLong = (type == TRADE_BUY);

        // --- CROSS-TF: translate candle index ke TF target ---
        double savedOpenIdx  = openCandleIdx;
        double savedCloseIdx = closeCandleIdx;
        if (tfCandles && !tfCandles->empty()) {
            if (openTime > 0)
                openCandleIdx = (double)ChartCanvas::FindCandleIndexByTime(*tfCandles, openTime);
            if (closeTime > 0)
                closeCandleIdx = (double)ChartCanvas::FindCandleIndexByTime(*tfCandles, closeTime);
        }

        double openEdge  = openCandleIdx;
        // closeCandleIdx = 0 → fallback ke limits.X.Max (belum ter-set)
        double closeEdge = (closeCandleIdx > 0) ? closeCandleIdx : limits.X.Max;

        // Cull: kalau zona sepenuhnya di luar view, skip
        if (closeEdge < limits.X.Min || openEdge > limits.X.Max) {
            if (tfCandles) { openCandleIdx = savedOpenIdx; closeCandleIdx = savedCloseIdx; }
            goto done;
        }

        // === HOVER DETECTION: hit area = seluruh zona (loss + win) ===
        double yHi = entryPrice, yLo = entryPrice;
        if (slActive) { yHi = std::max(yHi, sl); yLo = std::min(yLo, sl); }
        if (tpActive) { yHi = std::max(yHi, tp); yLo = std::min(yLo, tp); }
        double xLeft  = std::max(openEdge, limits.X.Min);
        double xRight = std::min(closeEdge, limits.X.Max);
        ImVec2 pxZoneTL = ImPlot::PlotToPixels(ImPlotPoint(xLeft, yHi));
        ImVec2 pxZoneBR = ImPlot::PlotToPixels(ImPlotPoint(xRight, yLo));
        bool isHovering = ImGui::IsMouseHoveringRect(pxZoneTL, pxZoneBR);

        // Zona lebih terang saat di-hover, pakai ghost alpha setting
        float ghostAlpha = g_TradeSettings.ghostAlphaMult;
        float hoverBoost = isHovering ? 2.5f : 1.0f;

        // Siapkan label ghost zone jika showGhostLabels aktif
        const char* gL1loss = "", *gL2loss = "", *gL3loss = "";
        const char* gL1win  = "", *gL2win  = "", *gL3win  = "";
        if (g_TradeSettings.showGhostLabels) {
            double gRisk   = fabs(entryPrice - sl);
            double gReward = fabs(tp - entryPrice);
            char gbE[48], gbSL[48], gbR[48], gbTP[48], gbRW[48], gbRR[48];
            snprintf(gbE,  sizeof(gbE),  "Entry   %.2f", entryPrice);
            snprintf(gbSL, sizeof(gbSL), "Stop    %.2f", sl);
            snprintf(gbR,  sizeof(gbR),  "Risk    %.2f", gRisk);
            snprintf(gbTP, sizeof(gbTP), "Target  %.2f", tp);
            snprintf(gbRW, sizeof(gbRW), "Reward  %.2f", gReward);
            if (gRisk > 0.00001)
                snprintf(gbRR, sizeof(gbRR), "R:R     1:%.2f", gReward / gRisk);
            else
                snprintf(gbRR, sizeof(gbRR), "R:R     --");
            switch (g_TradeSettings.zoneLabelMode) {
                case 0: gL1loss=gbE; gL2loss=gbSL; gL3loss=gbR; gL1win=gbTP; gL2win=gbRW; gL3win=gbRR; break;
                case 1: gL1loss=gbE; gL2loss=gbSL; gL3loss=""; gL1win=gbTP; gL2win=""; gL3win=""; break;
                case 2: gL1loss=gbR; gL2loss=""; gL3loss=""; gL1win=gbRW; gL2win=gbRR; gL3win=""; break;
            }
        }

        // Zona Loss ghost — pakai warna dari settings + label jika aktif
        if (slActive) {
            ImVec4 colLoss = ImVec4(g_TradeSettings.lossZoneColor[0],
                                   g_TradeSettings.lossZoneColor[1],
                                   g_TradeSettings.lossZoneColor[2],
                                   0.08f * ghostAlpha * hoverBoost);
            DrawZoneWithLabel(entryPrice, sl, colLoss, draw,
                              openEdge, closeEdge, 0.35f,
                              gL1loss, gL2loss, gL3loss, isLong);
        }

        // Zona Win ghost — pakai warna dari settings + label jika aktif
        if (tpActive) {
            ImVec4 colWin = ImVec4(g_TradeSettings.winZoneColor[0],
                                  g_TradeSettings.winZoneColor[1],
                                  g_TradeSettings.winZoneColor[2],
                                  0.07f * ghostAlpha * hoverBoost);
            DrawZoneWithLabel(entryPrice, tp, colWin, draw,
                              openEdge, closeEdge, 0.35f,
                              gL1win, gL2win, gL3win, !isLong);
        }

        // --- Hover highlight: border putih tipis saat hover ---
        if (isHovering) {
            draw->AddRect(pxZoneTL, pxZoneBR, IM_COL32(255, 255, 255, 70), 3.0f, 0, 1.5f);
        }

        // --- Badge profit (pakai setting showGhostBadge) ---
        if (g_TradeSettings.showGhostBadge) {
            bool isProfit = (profit >= 0);
            double badgeY = entryPrice;
            if (isProfit && tpActive)       badgeY = tp;
            else if (!isProfit && slActive) badgeY = sl;
            badgeY = std::max(limits.Y.Min, std::min(limits.Y.Max, badgeY));

            ImVec2 pClose = ImPlot::PlotToPixels(ImPlotPoint(closeEdge, badgeY));
            char   bufProfit[48];
            snprintf(bufProfit, sizeof(bufProfit),
                     isProfit ? "+$%.2f" : "-$%.2f", fabs(profit));

            ImVec2 txtSz = ImGui::CalcTextSize(bufProfit);
            float  padX  = 8.0f, padY = 4.0f;
            ImVec2 bMin = ImVec2(pClose.x - txtSz.x - padX * 2,
                                 pClose.y - txtSz.y - padY * 2 - 4.0f);
            ImVec2 bMax = ImVec2(pClose.x, pClose.y - 4.0f);

            ImU32 bgCol = isProfit
                ? IM_COL32(0, 120, 80, 220)
                : IM_COL32(150, 20, 20, 220);
            ImU32 txCol = IM_COL32(255, 255, 255, 240);

            draw->AddRectFilled(bMin, bMax, bgCol, 4.0f);
            draw->AddRect(bMin, bMax, isProfit
                ? IM_COL32(0, 200, 120, 180)
                : IM_COL32(220, 60, 60, 180), 4.0f);
            draw->AddText(ImVec2(bMin.x + padX, bMin.y + padY), txCol, bufProfit);
        }

        // === SMART DELETE BUTTON ===
        // Hitung ukuran button sekali saja
        const char* delText = "X";
        ImVec2 delTxtSz = ImGui::CalcTextSize(delText);
        float delPadX = 14.0f, delPadY = 7.0f;
        float btnW = delTxtSz.x + delPadX * 2;
        float btnH = delTxtSz.y + delPadY * 2;

        ImVec2 popMin, popMax;
        bool hoverButton;

        if (ghostPinned) {
            popMin = ghostPinPos;
            popMax = ImVec2(popMin.x + btnW, popMin.y + btnH);
            hoverButton = ImGui::IsMouseHoveringRect(popMin, popMax);

            // Shadow
            draw->AddRectFilled(
                ImVec2(popMin.x + 3, popMin.y + 3),
                ImVec2(popMax.x + 3, popMax.y + 3),
                IM_COL32(0, 0, 0, 120), 8.0f);
            draw->AddRectFilled(popMin, popMax, IM_COL32(70, 15, 15, 250), 8.0f);
            draw->AddRect(popMin, popMax, IM_COL32(230, 60, 60, 240), 8.0f, 0, 2.0f);
            draw->AddText(ImVec2(popMin.x + delPadX, popMin.y + delPadY),
                          IM_COL32(255, 140, 140, 255), delText);

            if (hoverButton && ImGui::IsMouseClicked(0)) {
                result = 2; // DELETE
            } else {
                if ((ImGui::IsMouseClicked(0) || ImGui::IsMouseClicked(1)) && !isHovering && !hoverButton) {
                    ghostPinned = false;
                }
                result = 1; // selalu cegah deselect selama pinned
            }
        } else if (isHovering) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

            if (ImGui::IsMouseClicked(0)) {
                float centerX = (pxZoneTL.x + pxZoneBR.x) * 0.5f;
                ghostPinPos = ImVec2(centerX - btnW * 0.5f,
                                     pxZoneTL.y - btnH - 8.0f);
                ghostPinned = true;
                result = 1;
            } else if (ImGui::IsMouseClicked(1)) {
                ImVec2 mousePos = ImGui::GetMousePos();
                ghostPinPos = ImVec2(mousePos.x - btnW * 0.5f,
                                     mousePos.y - btnH - 15.0f);
                ghostPinned = true;
                result = 1;
            } else {
                result = 1; // HOVERING
            }
        }

        // --- CROSS-TF: restore index asli ---
        if (tfCandles) { openCandleIdx = savedOpenIdx; closeCandleIdx = savedCloseIdx; }
        } // end scope

    done:
        return result;
    }

    // =========================================================
    // VISUAL RENDERING
    // =========================================================
    bool RenderVisuals(bool readOnly = false, const std::vector<Candle>* tfCandles = nullptr) {
        if (!isOpen) return false;

        ImGui::PushID(id); 

        // 1. Cek Selection State
        bool isSelected = (g_SelectedTradeID == id);
        bool isHoveredAnyComponent = false; 

        ImPlotRect limits = ImPlot::GetPlotLimits();
        ImDrawList* draw = ImPlot::GetPlotDrawList();

        // --- CROSS-TF: translate candle index ke TF target ---
        double savedOpenIdx  = openCandleIdx;
        double savedCloseIdx = closeCandleIdx;
        if (tfCandles && !tfCandles->empty()) {
            if (openTime > 0)
                openCandleIdx = (double)ChartCanvas::FindCandleIndexByTime(*tfCandles, openTime);
            if (closeTime > 0)
                closeCandleIdx = (double)ChartCanvas::FindCandleIndexByTime(*tfCandles, closeTime);
        }
        
        // --- LOGIKA OPACITY (TRANSPARANSI) ---
        float baseAlpha = isSelected ? 1.0f : 0.9f; 

        // Warna Garis & Tombol — dari TradeVisualSettings
        ImVec4 colEntry = (type == TRADE_BUY)
            ? ImVec4(g_TradeSettings.entryColorBuy[0], g_TradeSettings.entryColorBuy[1], g_TradeSettings.entryColorBuy[2], g_TradeSettings.entryColorBuy[3])
            : ImVec4(g_TradeSettings.entryColorSell[0], g_TradeSettings.entryColorSell[1], g_TradeSettings.entryColorSell[2], g_TradeSettings.entryColorSell[3]);
        ImVec4 colSL = ImVec4(g_TradeSettings.slColor[0], g_TradeSettings.slColor[1], g_TradeSettings.slColor[2], g_TradeSettings.slColor[3]);
        ImVec4 colTP = ImVec4(g_TradeSettings.tpColor[0], g_TradeSettings.tpColor[1], g_TradeSettings.tpColor[2], g_TradeSettings.tpColor[3]); 

        // Terapkan Alpha
        colEntry.w = baseAlpha;
        colSL.w = baseAlpha;
        colTP.w = baseAlpha;

        // Update Posisi Drag (skip di read-only)
        if (!readOnly && IsDragging()) {
            UpdateDragPosition();
        }

        // --- 1. GAMBAR ZONE WITH LABEL ---
        if (g_TradeSettings.showZones && showZones) {
            bool isLong = (type == TRADE_BUY);

            // Estimasi lebar 1 candle dari visible range (asumsi ~80 candle di layar)
            double candleW   = (limits.X.Max - limits.X.Min) / 80.0;
            double openEdge  = openCandleIdx; // ← FIX: pakai candle index, bukan openTime (timestamp)
            double rightEdge = limits.X.Max + candleW * 3.0; // +3 candle offset ke kanan

            float zoneAlpha = isSelected ? g_TradeSettings.zoneAlphaSelected : g_TradeSettings.zoneAlphaNormal;

            // Siapkan string stats — berdasarkan zoneLabelMode dari settings
            char bufE[48], bufSL[48], bufRisk[48], bufTP[48], bufRew[48], bufRR[48];
            double risk   = fabs(entryPrice - sl);
            double reward = fabs(tp - entryPrice);

            snprintf(bufE,    sizeof(bufE),    "Entry   %.2f", entryPrice);
            snprintf(bufSL,   sizeof(bufSL),   "Stop    %.2f", sl);
            snprintf(bufRisk, sizeof(bufRisk), "Risk    %.2f", risk);
            snprintf(bufTP,   sizeof(bufTP),   "Target  %.2f", tp);
            snprintf(bufRew,  sizeof(bufRew),  "Reward  %.2f", reward);
            if (risk > 0.00001)
                snprintf(bufRR, sizeof(bufRR), "R:R     1:%.2f", reward / risk);
            else
                snprintf(bufRR, sizeof(bufRR), "R:R     --");

            // Tentukan label berdasarkan zoneLabelMode
            // 0 = Harga + RR, 1 = Harga Saja, 2 = RR Saja
            const char* l1loss = "", *l2loss = "", *l3loss = "";
            const char* l1win  = "", *l2win  = "", *l3win  = "";
            switch (g_TradeSettings.zoneLabelMode) {
                case 0: // Harga + RR
                    l1loss = bufE; l2loss = bufSL; l3loss = bufRisk;
                    l1win  = bufTP; l2win  = bufRew; l3win  = bufRR;
                    break;
                case 1: // Harga Saja
                    l1loss = bufE; l2loss = bufSL; l3loss = "";
                    l1win  = bufTP; l2win  = ""; l3win  = "";
                    break;
                case 2: // RR Saja
                    l1loss = bufRisk; l2loss = ""; l3loss = "";
                    l1win  = bufRew; l2win  = bufRR; l3win  = "";
                    break;
            }

            // Zona Loss — pakai warna dari settings
            if (slActive || draggingSL) {
                ImVec4 colLoss = ImVec4(g_TradeSettings.lossZoneColor[0],
                                       g_TradeSettings.lossZoneColor[1],
                                       g_TradeSettings.lossZoneColor[2],
                                       zoneAlpha);
                bool txtBot = isLong;
                DrawZoneWithLabel(entryPrice, sl, colLoss, draw,
                                  openEdge, rightEdge, baseAlpha,
                                  l1loss, l2loss, l3loss, txtBot);
            }

            // Zona Win — pakai warna dari settings
            if (tpActive || draggingTP) {
                ImVec4 colWin = ImVec4(g_TradeSettings.winZoneColor[0],
                                      g_TradeSettings.winZoneColor[1],
                                      g_TradeSettings.winZoneColor[2],
                                      zoneAlpha * 0.85f);
                bool txtBot = !isLong;
                DrawZoneWithLabel(entryPrice, tp, colWin, draw,
                                  openEdge, rightEdge, baseAlpha,
                                  l1win, l2win, l3win, txtBot);
            }
        }

        // --- 2. GAMBAR GARIS (sync ketebalan dari settings) ---
        DrawLine(entryPrice, colEntry, false, draggingEntry, "##EntryLine");
        if (slActive || draggingSL) DrawLine(sl, colSL, true, draggingSL, "##SLLine");
        if (tpActive || draggingTP) DrawLine(tp, colTP, true, draggingTP, "##TPLine");

        // --- 2b. LINE LABELS (harga SL, reward TP) di sisi kiri chart ---
        if (g_TradeSettings.showLineLabels) {
            if (slActive) {
                char lblSL[48]; snprintf(lblSL, sizeof(lblSL), "SL %.2f", sl);
                DrawLineLabel(draw, limits, sl, colSL, lblSL);
            }
            if (tpActive) {
                double reward = fabs(tp - entryPrice);
                char lblTP[48]; snprintf(lblTP, sizeof(lblTP), "TP +%.2f", reward);
                DrawLineLabel(draw, limits, tp, colTP, lblTP);
            }
        }

        // Konektor (hanya jika setting aktif)
        if (g_TradeSettings.showConnector && slActive && tpActive) {
            ImU32 colLine = ImGui::ColorConvertFloat4ToU32(colEntry);
            if (!isSelected) colLine = (colLine & 0x00FFFFFF) | ((ImU32)(baseAlpha * 255) << 24);

            ImVec2 pSL = ImPlot::PlotToPixels(ImPlotPoint(limits.X.Max, sl));
            ImVec2 pTP = ImPlot::PlotToPixels(ImPlotPoint(limits.X.Max, tp));
            draw->AddLine(pSL, pTP, colLine, 1.0f); 
        }

        // --- 3. RISK REWARD (RR) — hanya jika setting aktif ---
        if (g_TradeSettings.showRRBadge) DrawRiskReward(draw, limits, baseAlpha);

        // --- 4. DRAW TAGS (BUTTONS) ---
        auto HandleTag = [&](double& val, ImVec4 col, const char* label, bool& draggingFlag, bool isEntry, bool isSmall, float offX) {
            bool hovered = false;
            
            bool showChecklist = (isEntry && isSelected); 
            
            bool clicked = DrawTag(draw, val, col, label, draggingFlag, isSmall, offX, &hovered, showChecklist ? &showZones : nullptr, baseAlpha, readOnly);
            
            if (hovered) isHoveredAnyComponent = true;

            if (clicked && !readOnly) {
                if (g_SelectedTradeID != id) {
                    g_SelectedTradeID = id; 
                }
                draggingFlag = true;
                g_IsDraggingGlobal = true;
            }
        };

        std::string labelEntry = (type == TRADE_BUY) ? "BUY #" + std::to_string(id) : "SELL #" + std::to_string(id);
        std::string labelSL = "SL #" + std::to_string(id);
        std::string labelTP = "TP #" + std::to_string(id);

        // ENTRY
        bool visualEntryDrag = draggingEntry; if (!isPending) visualEntryDrag = false; 
        HandleTag(entryPrice, colEntry, labelEntry.c_str(), draggingEntry, true, false, 0);

        // SL
        if (slActive) HandleTag(sl, colSL, labelSL.c_str(), draggingSL, false, false, 0);
        else HandleTag(entryPrice, colSL, "SL", draggingSL, false, true, 120.0f); 

        // TP
        if (tpActive) HandleTag(tp, colTP, labelTP.c_str(), draggingTP, false, false, 0);
        else HandleTag(entryPrice, colTP, "TP", draggingTP, false, true, 160.0f); 

        // --- STOP DRAGGING (MOUSE UP) --- (skip di read-only)
        if (!readOnly && !ImGui::IsMouseDown(0) && IsDragging()) {
            draggingEntry = draggingSL = draggingTP = false;
            g_IsDraggingGlobal = false;
        }

        // --- CROSS-TF: restore index asli ---
        if (tfCandles) {
            openCandleIdx  = savedOpenIdx;
            closeCandleIdx = savedCloseIdx;
        }

        ImGui::PopID(); 
        return isHoveredAnyComponent;
    }

    // =========================================================
    // FIX: UpdateLogic terima currentTime + candleIdx untuk Close akurat
    // =========================================================
    void UpdateLogic(double currentPrice, double contractSize, double currentTime = 0, double candleIdx = 0) {
        if (!isOpen) return;
        double diff = (type == TRADE_BUY) ? (currentPrice - entryPrice) : (entryPrice - currentPrice);
        profit = diff * volume * contractSize;
        if (IsDragging()) return; 

        if (type == TRADE_BUY) {
            if (slActive && sl > 0 && currentPrice <= sl) Close(sl, "Hit SL", currentTime, candleIdx);
            else if (tpActive && tp > 0 && currentPrice >= tp) Close(tp, "Hit TP", currentTime, candleIdx);
        } else {
            if (slActive && sl > 0 && currentPrice >= sl) Close(sl, "Hit SL", currentTime, candleIdx);
            else if (tpActive && tp > 0 && currentPrice <= tp) Close(tp, "Hit TP", currentTime, candleIdx);
        }
    }

    // =========================================================
    // FIX: CheckHitCandle kirim cd.time + candleIdx ke Close()
    // =========================================================
    void CheckHitCandle(const Candle& cd, double contractSize, double candleIdx = 0) {
        if (!isOpen || IsDragging()) return;
        bool closedNow = false; double closeP = 0;
        if (type == TRADE_BUY) {
            if (slActive && cd.low <= sl) { closedNow = true; closeP = sl; }
            else if (tpActive && cd.high >= tp) { closedNow = true; closeP = tp; }
        } else {
            if (slActive && cd.high >= sl) { closedNow = true; closeP = sl; }
            else if (tpActive && cd.low <= tp) { closedNow = true; closeP = tp; }
        }
        if (closedNow) Close(closeP, "Candle Hit", cd.time, candleIdx);
    }

    // =========================================================
    // FIX: Close() terima candleCloseTime + candleCloseIdx supaya duration & ghost zone akurat
    // =========================================================
    void Close(double price, const char* reason, double candleCloseTime = 0, double candleCloseIdx = 0) {
        isOpen = false;
        closePrice = price;
        closeReason = reason ? reason : "Unknown";
        closeCandleIdx = candleCloseIdx; // ← simpan index candle penutup
        if (candleCloseTime > 0) {
            closeTime = candleCloseTime;
        } else {
            closeTime = std::chrono::system_clock::now().time_since_epoch().count() / 1000000000.0;
        }
    }

private:
    void UpdateDragPosition() {
        ImPlotPoint mousePlot = ImPlot::GetPlotMousePos();
        if (draggingEntry && isPending) entryPrice = mousePlot.y;
        if (draggingSL) { sl = mousePlot.y; slActive = true; }
        if (draggingTP) { tp = mousePlot.y; tpActive = true; }
    }

    // =========================================================
    // DRAW ZONE WITH LABEL — hybrid open/closed, index-based
    // openEdge  : candle index entry (zona mulai dari sini)
    // rightEdge : limits.X.Max+offset (open) atau closeCandleIdx (closed)
    // textAtBottom : true = teks di bawah zona (dekat SL), false = atas (dekat TP)
    // =========================================================
    void DrawZoneWithLabel(
        double price1, double price2,
        ImVec4 color, ImDrawList* draw,
        double openEdge, double rightEdge,
        float alpha,
        const char* line1, const char* line2, const char* line3,
        bool textAtBottom)
    {
        // 1. Fill zona via ImPlot (plot-space)
        double xs[2]  = { openEdge, rightEdge };
        double ys1[2] = { price1, price1 };
        double ys2[2] = { price2, price2 };
        ImPlot::SetNextFillStyle(color);
        ImPlot::PlotShaded(("##ZL" + std::to_string(price2)).c_str(), xs, ys1, ys2, 2);

        // 2. Hitung batas pixel zona
        double pHi = std::max(price1, price2);
        double pLo = std::min(price1, price2);
        ImVec2 pxTop    = ImPlot::PlotToPixels(ImPlotPoint(openEdge, pHi));
        ImVec2 pxBottom = ImPlot::PlotToPixels(ImPlotPoint(openEdge, pLo));
        float  zoneH    = pxBottom.y - pxTop.y;
        if (zoneH < 22.0f) return; // terlalu tipis, skip teks

        // 3. Render teks multi-baris di dalam zona
        const char* lines[3] = { line1, line2, line3 };
        // FIX: terapkan zoneFontScale dari settings
        ImFont* font    = ImGui::GetFont();
        float fontSize  = ImGui::GetFontSize() * g_TradeSettings.zoneFontScale;
        float lineH     = fontSize + 3.0f;
        float totalTxtH = lineH * 3.0f;
        float padV      = 6.0f;
        float padH      = 10.0f;

        // Pastikan teks tidak melewati batas zona
        float startY = textAtBottom
            ? std::max(pxTop.y + padV, pxBottom.y - totalTxtH - padV)
            : pxTop.y + padV;

        // Clamp supaya tidak keluar area zona
        if (startY + totalTxtH > pxBottom.y - 2.0f) return;

        float startX = pxTop.x + padH;

        ImU32 colText   = IM_COL32(255, 255, 255, (int)(210 * alpha));
        ImU32 colShadow = IM_COL32(0,   0,   0,   (int)(130 * alpha));

        for (int i = 0; i < 3; i++) {
            if (!lines[i] || lines[i][0] == '\0') continue;
            ImVec2 pos = ImVec2(startX, startY + i * lineH);
            // FIX: pakai font + fontSize eksplisit agar scale benar-benar terapply
            draw->AddText(font, fontSize, ImVec2(pos.x + 1, pos.y + 1), colShadow, lines[i]);
            draw->AddText(font, fontSize, pos, colText, lines[i]);
        }
    }

    void DrawLine(double y, ImVec4 color, bool isDashed, bool isThick, const char* labelId) {
        ImPlotRect limits = ImPlot::GetPlotLimits();
        float thick = g_TradeSettings.lineThickness;
        if (isThick) thick *= 1.5f; // lebih tebal saat drag

        int style = g_TradeSettings.lineStyle; // 0=Solid, 1=Dashed, 2=Dotted

        if (style == 0) {
            // SOLID — pakai ImPlot PlotLine biasa
            double xs[2] = { limits.X.Min, limits.X.Max };
            double ys[2] = { y, y };
            ImPlot::SetNextLineStyle(color, thick);
            ImPlot::PlotLine(labelId, xs, ys, 2);
        } else {
            // DASHED / DOTTED — render manual via DrawList pixel
            // ImPlot tidak punya native dash support, gambar segment sendiri
            ImDrawList* draw = ImPlot::GetPlotDrawList();
            ImVec2 p1 = ImPlot::PlotToPixels(ImPlotPoint(limits.X.Min, y));
            ImVec2 p2 = ImPlot::PlotToPixels(ImPlotPoint(limits.X.Max, y));
            ImU32 col = ImGui::ColorConvertFloat4ToU32(color);

            // Dashed: 8px on, 5px off | Dotted: 3px on, 3px off
            float onLen  = (style == 1) ? 8.0f : 3.0f;
            float offLen = (style == 1) ? 5.0f : 3.0f;

            float x = p1.x;
            while (x < p2.x) {
                float xEnd = std::min(x + onLen, p2.x);
                draw->AddLine(ImVec2(x, p1.y), ImVec2(xEnd, p1.y), col, thick);
                x += onLen + offLen;
            }
        }
    }

    // =========================================================
    // DRAW LINE LABEL — label harga/reward di sisi kiri garis
    // =========================================================
    void DrawLineLabel(ImDrawList* draw, const ImPlotRect& limits, double y, ImVec4 color, const char* text) {
        ImVec2 pLeft = ImPlot::PlotToPixels(ImPlotPoint(limits.X.Min, y));
        ImVec2 txtSz = ImGui::CalcTextSize(text);
        float padX = 5.0f, padY = 2.0f;
        ImVec2 bgMin = ImVec2(pLeft.x + 3.0f, pLeft.y - txtSz.y * 0.5f - padY);
        ImVec2 bgMax = ImVec2(pLeft.x + 3.0f + txtSz.x + padX * 2, pLeft.y + txtSz.y * 0.5f + padY);
        ImU32 bgCol = IM_COL32(15, 15, 15, 210);
        ImU32 txCol = ImGui::ColorConvertFloat4ToU32(color);
        draw->AddRectFilled(bgMin, bgMax, bgCol, 3.0f);
        draw->AddRect(bgMin, bgMax, IM_COL32(80, 80, 80, 180), 3.0f);
        draw->AddText(ImVec2(bgMin.x + padX, bgMin.y + padY), txCol, text);
    }

    // DRAW RISK REWARD — deprecated, info sudah masuk DrawZoneWithLabel
    // Dibiarkan exist agar tidak break call site lama, tapi tidak render apapun
    void DrawRiskReward(ImDrawList* draw, ImPlotRect limits, float alpha) {
        if (!slActive || !tpActive) return;
        double risk = fabs(entryPrice - sl); 
        double reward = fabs(tp - entryPrice);
        if (risk < 0.00001) return;
        double ratio = reward/risk;
        char buf[64]; sprintf(buf, "RR 1:%.2f", ratio);
        
        float fontScale = (draggingSL || draggingTP) ? 1.4f : 1.0f; 
        
        ImGui::SetWindowFontScale(fontScale);
        ImVec2 txtSz = ImGui::CalcTextSize(buf);
        ImGui::SetWindowFontScale(1.0f); 
        ImVec2 pos = ImPlot::PlotToPixels(ImPlotPoint(limits.X.Max, entryPrice));
        float offsetFromAxis = 126.0f; 
        float boxRight = pos.x - offsetFromAxis;
        float boxLeft = boxRight - txtSz.x - 20.0f;
        float halfHeight = (txtSz.y * 0.5f) + 6.0f;
        ImVec2 bgMin = ImVec2(boxLeft, pos.y - halfHeight);
        ImVec2 bgMax = ImVec2(boxRight, pos.y + halfHeight);
        ImVec2 txtPos = ImVec2(bgMin.x + 10.0f, bgMin.y + 6.0f);

        // Alpha blended colors
        ImU32 colText = IM_COL32(255, 215, 0, (int)(255 * alpha)); 
        ImU32 colBg = IM_COL32(20, 20, 20, (int)(230 * alpha));    
        
        draw->AddRectFilled(bgMin, bgMax, colBg, 4.0f);
        draw->AddRect(bgMin, bgMax, colBg, 4.0f); 
        draw->AddText(ImGui::GetFont(), ImGui::GetFontSize() * fontScale, txtPos, colText, buf);
    }

    // MODIFIED: DRAW TAG (Accepts Alpha)
    bool DrawTag(ImDrawList* draw, double yValue, ImVec4 color, const char* label, bool isDragging, bool isSmall, float offX, bool* outHovered, bool* pShowZones, float alpha, bool readOnly = false) {
        ImPlotRect limits = ImPlot::GetPlotLimits();
        ImVec2 pos = ImPlot::PlotToPixels(ImPlotPoint(limits.X.Max, yValue));
        
        char buf[64];
        if (isSmall) sprintf(buf, "%s", label); else sprintf(buf, "%s %.2f", label, yValue); 
        
        ImGui::SetWindowFontScale(1.0f);
        ImVec2 txtSz = ImGui::CalcTextSize(buf);
        
        float totalWidth = txtSz.x;

        float pad = isDragging ? 8.0f : 6.0f;
        float rightAnchor = pos.x - (isSmall ? offX : 0); 
        ImVec2 min = ImVec2(rightAnchor - totalWidth - (pad*2), pos.y - txtSz.y/2 - 4);
        ImVec2 max = ImVec2(rightAnchor, pos.y + txtSz.y/2 + 4);
        
        bool hov = ImGui::IsMouseHoveringRect(min, max);
        if (outHovered && hov) *outHovered = true;

        bool clk = false;
        if (hov && !readOnly) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            if (ImGui::IsMouseClicked(0)) {
                 clk = true;
            }
        }

        // Apply Alpha to Background
        ImU32 bgCol = IM_COL32(20, 20, 20, (int)(240 * alpha));
        draw->AddRectFilled(min, max, bgCol, 4);
        
        float borderThick = hov || isDragging ? 2.0f : 1.0f;
        draw->AddRect(min, max, ImGui::ColorConvertFloat4ToU32(color), 4, 0, borderThick);

        float textOffsetX = min.x + pad;
        draw->AddText(ImVec2(textOffsetX, min.y+4), ImGui::ColorConvertFloat4ToU32(color), buf);
        return clk;
    }
};

// =================================================================================
// IMPLEMENTASI TradeHistory::RecordClosed()
// Diletakkan di sini karena perlu akses full member LiveTrade
// =================================================================================
inline void TradeHistory::RecordClosed(const LiveTrade& t, const char* reason, double closeTime) {
    ClosedTrade rec;
    rec.id          = t.id;
    rec.symbol      = t.symbol;
    rec.type        = (t.type == TRADE_BUY) ? TH_TRADE_BUY : TH_TRADE_SELL;
    rec.typeStr     = (t.type == TRADE_BUY) ? "BUY" : "SELL";
    rec.entryPrice  = t.entryPrice;
    rec.closePrice  = t.closePrice;
    rec.profit      = t.profit;
    rec.closeReason = reason;
    rec.openTime    = t.openTime;
    rec.closeTime   = closeTime;
    rec.duration    = closeTime - t.openTime;
    rec.volume      = t.volume;
    rec.sl          = t.sl;
    rec.tp          = t.tp;
    closedTrades.push_back(rec);

    std::cout << "Trade #" << t.id << " CLOSED | "
              << rec.typeStr << " " << t.symbol
              << " | PnL: " << (rec.profit >= 0 ? "+" : "") << TH_FormatUSD(rec.profit)
              << " | Reason: " << reason << std::endl;
}

// =================================================================================
// CLASS TRADE MANAGER
// =================================================================================
class TradeManager {
public:
    std::vector<LiveTrade> trades; 
    int nextId = 1;
    double balance = 10000.0;
    double equity = 10000.0;
    const double CONTRACT_SIZE = 100.0; 

    // FIX: Pointer ke TradeHistory — Set dari main.cpp
    TradeHistory* history = nullptr;

    void OpenTrade(std::string symbol, TradeType type, double price, double vol, double time, bool pending, double candleIdx = 0) {
        trades.emplace_back(nextId++, symbol, type, price, vol, time, pending);
        trades.back().openCandleIdx = candleIdx; // ← simpan index candle entry
        g_SelectedTradeID = nextId - 1; 
        std::cout << "Open Trade #" << (nextId-1) << " Pair: " << symbol << std::endl;
    }

    void RenderAllVisuals(bool readOnly = false, const std::vector<Candle>* tfCandles = nullptr, const std::string* symbolFilter = nullptr) {
        // 0. Build trade zone table data untuk settings popup
        static std::vector<TradeZoneRow> s_zoneRows;
        s_zoneRows.clear();
        for (auto& t : trades) {
            if (t.isOpen && (!symbolFilter || t.symbol == *symbolFilter)) {
                s_zoneRows.push_back({
                    t.id, t.symbol,
                    (t.type == TRADE_BUY) ? "BUY" : "SELL",
                    t.entryPrice, t.profit, &t.showZones
                });
            }
        }
        g_tradeSettingsUI.SetTradeData(&s_zoneRows);

        bool isAnyTradeHovered = false;

        // 0. Ghost zones + kumpulkan request delete (skip delete di read-only)
        std::vector<int> toDelete;
        if (!readOnly) {
            for (int i = 0; i < (int)trades.size(); i++) {
                if (!trades[i].isOpen) {
                    int result = trades[i].RenderGhostZone(tfCandles);
                    if (result == 2) toDelete.push_back(i);
                    if (result >= 1) isAnyTradeHovered = true;
                }
            }
            // Hapus trade yang diminta delete (reverse order agar index aman)
            for (int i = (int)toDelete.size() - 1; i >= 0; i--) {
                std::cout << "Ghost zone deleted: Trade #" << trades[toDelete[i]].id << std::endl;
                trades.erase(trades.begin() + toDelete[i]);
            }
        } else {
            // read-only: render ghost zone visual saja, tanpa interaksi
            for (int i = 0; i < (int)trades.size(); i++) {
                if (!trades[i].isOpen) {
                    trades[i].RenderGhostZone(tfCandles);
                }
            }
        }

        // Render Background (Unselected, open)
        for (auto& t : trades) {
            if (t.isOpen && t.id != g_SelectedTradeID
                && (!symbolFilter || t.symbol == *symbolFilter)) {
                bool hovered = t.RenderVisuals(readOnly, tfCandles);
                if (hovered) isAnyTradeHovered = true;
            }
        }

        // Render Foreground (Selected)
        if (g_SelectedTradeID != -1) {
            for (auto& t : trades) {
                if (t.id == g_SelectedTradeID
                    && (!symbolFilter || t.symbol == *symbolFilter)) {
                    bool hovered = t.RenderVisuals(readOnly, tfCandles);
                    if (hovered) isAnyTradeHovered = true;
                    break;
                }
            }
        }

        // Global Deselect Logic + Unpin semua ghost zone (skip di read-only)
        if (!readOnly && !g_IsDraggingGlobal && !isAnyTradeHovered && ImPlot::IsPlotHovered()) {
            bool clickLeft  = ImGui::IsMouseClicked(ImGuiMouseButton_Left);
            bool clickRight = ImGui::IsMouseClicked(ImGuiMouseButton_Right);
            if (clickLeft || clickRight) {
                g_SelectedTradeID = -1;
                // Unpin semua ghost zone yang masih pinned
                for (auto& t : trades) {
                    if (!t.isOpen) t.ghostPinned = false;
                }
            }
        }

    }

    // FIX: UpdateAllLogic kirim currentTime + candleIdx ke LiveTrade
    void UpdateAllLogic(double currentPrice, double currentTime, double candleIdx = 0, const std::string* symbolFilter = nullptr) {
        double floatingPL = 0.0;
        for (auto& t : trades) {
            if (symbolFilter && t.symbol != *symbolFilter) continue;
            t.UpdateLogic(currentPrice, CONTRACT_SIZE, currentTime, candleIdx);
            if (t.isOpen) floatingPL += t.profit;
        }
        equity = balance + floatingPL;
    }

    // FIX: CheckAllHits record ke history + propagate candleIdx
    void CheckAllHits(const Candle& cd, double candleIdx = 0, const std::string* symbolFilter = nullptr) {
        for (auto& t : trades) {
            if (symbolFilter && t.symbol != *symbolFilter) continue;
            bool wasOpen = t.isOpen;
            t.CheckHitCandle(cd, CONTRACT_SIZE, candleIdx);
            if (wasOpen && !t.isOpen) {
                balance += t.profit;
                RecordToHistory(t);
            }
        }
    }
    
    // FIX: CloseTrade record ke history + kirim waktu + idx
    void CloseTrade(int id, double price, double currentTime = 0, double candleIdx = 0) {
        for (auto& t : trades) {
            if (t.id == id && t.isOpen) {
                t.Close(price, "Manual Close", currentTime, candleIdx);
                balance += t.profit;
                RecordToHistory(t);
            }
        }
    }

    // FIX: CloseAllSafe record ke history + kirim waktu + idx
    void CloseAllSafe(double currentPrice, double currentTime = 0, double candleIdx = 0) {
        for (auto& t : trades) {
            if (t.isOpen && !t.isLocked) {
                t.Close(currentPrice, "Close All Button", currentTime, candleIdx);
                balance += t.profit;
                RecordToHistory(t);
            }
        }
    }

    // =========================================================
    // CROSS-TF ZONE OVERLAY
    // Dipanggil dari non-primary tab (TF berbeda).
    // Menggunakan openTime/closeTime (epoch) → FindCandleIndexByTime
    // agar posisi zona akurat di semua TF, tanpa drag handle.
    // =========================================================
    void RenderZonesForTF(const std::vector<Candle>& candles) {
        if (trades.empty() || candles.empty()) return;

        ImPlotRect  limits = ImPlot::GetPlotLimits();
        ImDrawList* draw   = ImPlot::GetPlotDrawList();
        ImFont*     font   = ImGui::GetFont();
        float       fsz    = ImGui::GetFontSize() * 0.88f; // sedikit lebih kecil

        for (const auto& t : trades) {
            // --- 1. Hitung X kiri: openTime → candle index di TF ini ---
            int openIdx = ChartCanvas::FindCandleIndexByTime(candles, t.openTime);
            double xLeft = (double)openIdx;

            // --- 2. Hitung X kanan ---
            double xRight;
            if (t.isOpen) {
                // Trade masih terbuka → tarik ke kanan plot + offset 3 candle
                double candleW = (limits.X.Max - limits.X.Min) / 80.0;
                xRight = limits.X.Max + candleW * 3.0;
            } else {
                // Trade sudah close → gunakan closeTime → candle index
                int closeIdx = ChartCanvas::FindCandleIndexByTime(candles, t.closeTime);
                xRight = (double)closeIdx;
            }

            // Cull: zona sepenuhnya di luar view
            if (xRight < limits.X.Min || xLeft > limits.X.Max) continue;

            // --- 3. Tentukan warna zona ---
            bool   isLong    = (t.type == TRADE_BUY);
            float  baseAlpha = t.isOpen ? 0.13f : 0.07f; // ghost lebih redup

            // Zona Loss (entry → SL)
            if (t.slActive) {
                ImVec4 cLoss = ImVec4(
                    g_TradeSettings.lossZoneColor[0],
                    g_TradeSettings.lossZoneColor[1],
                    g_TradeSettings.lossZoneColor[2],
                    baseAlpha);
                double xs[2]  = { xLeft, xRight };
                double ys1[2] = { t.entryPrice, t.entryPrice };
                double ys2[2] = { t.sl, t.sl };
                ImPlot::SetNextFillStyle(cLoss);
                std::string lossId = "##xtz_loss_" + std::to_string(t.id);
                ImPlot::PlotShaded(lossId.c_str(), xs, ys1, ys2, 2);
            }

            // Zona Win (entry → TP)
            if (t.tpActive) {
                ImVec4 cWin = ImVec4(
                    g_TradeSettings.winZoneColor[0],
                    g_TradeSettings.winZoneColor[1],
                    g_TradeSettings.winZoneColor[2],
                    baseAlpha * 0.85f);
                double xs[2]  = { xLeft, xRight };
                double ys1[2] = { t.entryPrice, t.entryPrice };
                double ys2[2] = { t.tp, t.tp };
                ImPlot::SetNextFillStyle(cWin);
                std::string winId = "##xtz_win_" + std::to_string(t.id);
                ImPlot::PlotShaded(winId.c_str(), xs, ys1, ys2, 2);
            }

            // --- 4. Garis Entry (tipis, semi-transparan) ---
            {
                ImVec4 colE = (t.type == TRADE_BUY)
                    ? ImVec4(g_TradeSettings.entryColorBuy[0],  g_TradeSettings.entryColorBuy[1],
                             g_TradeSettings.entryColorBuy[2],  0.55f)
                    : ImVec4(g_TradeSettings.entryColorSell[0], g_TradeSettings.entryColorSell[1],
                             g_TradeSettings.entryColorSell[2], 0.55f);
                double xs[2] = { xLeft, xRight };
                double ys[2] = { t.entryPrice, t.entryPrice };
                ImPlot::SetNextLineStyle(colE, 1.0f);
                std::string eId = "##xtz_entry_" + std::to_string(t.id);
                ImPlot::PlotLine(eId.c_str(), xs, ys, 2);
            }

            // --- 5. Badge kecil di kanan zona ---
            // Hanya jika zona masih dalam view
            double badgeX = std::min(xRight, limits.X.Max - 0.5);
            double badgeY = t.entryPrice;
            // Geser badge ke TP (profit) atau SL (loss) agar tidak tumpuk garis entry
            if (t.isOpen) {
                if (isLong && t.tpActive)        badgeY = t.tp;
                else if (!isLong && t.slActive)  badgeY = t.sl;
            }
            badgeY = std::max(limits.Y.Min, std::min(limits.Y.Max, badgeY));

            ImVec2 pBadge = ImPlot::PlotToPixels(ImPlotPoint(badgeX, badgeY));

            char bufBadge[48];
            if (t.isOpen) {
                // Profit floating
                snprintf(bufBadge, sizeof(bufBadge),
                         t.profit >= 0 ? "+$%.2f" : "-$%.2f", fabs(t.profit));
            } else {
                // Closed: tampil result
                snprintf(bufBadge, sizeof(bufBadge),
                         t.profit >= 0 ? "W +$%.2f" : "L -$%.2f", fabs(t.profit));
            }

            ImVec2 txtSz = ImGui::CalcTextSize(bufBadge);
            float padX = 5.0f, padY = 2.0f;
            ImVec2 bMin = ImVec2(pBadge.x - txtSz.x - padX * 2, pBadge.y - txtSz.y - padY * 2);
            ImVec2 bMax = ImVec2(pBadge.x, pBadge.y);

            bool isProfit = (t.profit >= 0);
            ImU32 bgBadge = isProfit ? IM_COL32(0, 100, 60, 200) : IM_COL32(120, 20, 20, 200);
            draw->AddRectFilled(bMin, bMax, bgBadge, 3.0f);
            draw->AddText(font, fsz,
                          ImVec2(bMin.x + padX, bMin.y + padY),
                          IM_COL32(255, 255, 255, 230), bufBadge);

            // --- 6. Label type kecil di atas zona kiri ---
            {
                char bufType[24];
                snprintf(bufType, sizeof(bufType),
                         t.isOpen ? (isLong ? "▲ BUY" : "▼ SELL")
                                  : (isLong ? "BUY" : "SELL"));
                ImVec2 pType = ImPlot::PlotToPixels(ImPlotPoint(xLeft, t.entryPrice));
                ImU32 colType = isLong
                    ? IM_COL32(80, 200, 120, 200)
                    : IM_COL32(220, 80, 80, 200);
                draw->AddText(font, fsz,
                              ImVec2(pType.x + 4.0f, pType.y - fsz - 2.0f),
                              colType, bufType);
            }
        }
    }

private:
    void RecordToHistory(const LiveTrade& t) {
        if (history) {
            history->RecordClosed(t, t.closeReason.c_str(), t.closeTime);
        }
    }
};