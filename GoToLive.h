#pragma once
// ================================================================
// GoToLive.h — Modular "Go To Live" manager untuk semua ChartTab
// ================================================================
//
// Setiap tab punya state animasi sendiri (disimpan di ChartTabState).
// Fungsi-fungsi ini bekerja murni pada ChartTabState sehingga
// semua tab (primary, non-primary, multi-tab) bisa Go To Live
// secara independen tanpa saling ganggu.
//
// CARA PAKAI (di RenderSingleChartWindow):
//
//   1. Setelah cs di-sync, bridge global → tab utama:
//        GoToLive::SyncFromGlobals(tab, isAnimatingToLive, animFloatingIndex);
//
//   2. Setelah replay-play-mode block, jalankan animasi:
//        GoToLive::Update(tab, bufferCount, global_idx_offset,
//                         g_rightMarginCandles,
//                         pGlobalVCI, pGlobalAutoFit);
//
//   3. Dalam BeginPlot, segera setelah pPos & pSz tersedia:
//        GoToLive::SavePlotBounds(tab);
//
//   4. Setelah ImPlot::PopStyleColor(4):
//        GoToLive::RenderButton(tab, bufferCount, global_idx_offset,
//                               replayMode, livePrice,
//                               pGlobalVCI, pGlobalAutoFit);
//
// TRIGGER DARI LUAR (misal replay exit di RenderReplayPanel):
//        GoToLive::TriggerAllPrimary(g_chartManager,
//                                    &g_chart.viewCenterIndex,
//                                    &g_chart.autoFitY);
// ================================================================

#include "imgui.h"
#include "implot.h"
#include "MultiChart.h"
#include <cmath>
#include <string>

namespace GoToLive {

// ────────────────────────────────────────────────────────────────
// SyncFromGlobals
// Bridge variabel legacy (static file-scope) → tab->state
// Dipanggil di awal RenderSingleChartWindow untuk tab utama
// agar perubahan dari luar (replay exit) ikut masuk ke tab state.
// ────────────────────────────────────────────────────────────────
inline void SyncFromGlobals(ChartTab* tab,
                             bool legacyIsAnimating,
                             double legacyAnimFloat)
{
    if (!tab || !tab->usesGlobalData) return;
    // Hanya adopt dari global jika global baru saja di-set
    // (tapi tab state masih false). Bukan sebaliknya.
    if (legacyIsAnimating && !tab->state.isAnimatingToLive) {
        tab->state.isAnimatingToLive = true;
        tab->state.animFloatingIndex = legacyAnimFloat;
    }
}

// ────────────────────────────────────────────────────────────────
// SavePlotBounds
// Dipanggil di dalam BeginPlot setelah pPos & pSz tersedia.
// Simpan ke tab->state agar RenderButton bisa pakai setelah
// EndSubplots (di mana ImPlot sudah tidak tersedia).
// ────────────────────────────────────────────────────────────────
inline void SavePlotBounds(ChartTab* tab)
{
    if (!tab) return;
    tab->state.savedPlotPos    = ImPlot::GetPlotPos();
    tab->state.savedPlotSize   = ImPlot::GetPlotSize();
    tab->state.plotBoundsReady = true;
}

// ────────────────────────────────────────────────────────────────
// Trigger
// Set animasi pada tab ini mulai dari posisi viewCenter saat ini.
// pGlobalVCI / pGlobalAutoFit : pointer ke g_chart.viewCenterIndex
//   dan g_chart.autoFitY (hanya untuk tab utama, nullptr untuk lain)
// ────────────────────────────────────────────────────────────────
inline void Trigger(ChartTab* tab,
                    int*  pGlobalVCI     = nullptr,
                    bool* pGlobalAutoFit = nullptr)
{
    if (!tab) return;
    ChartTabState& cs = tab->state;
    cs.isAnimatingToLive = true;
    cs.animFloatingIndex = (double)cs.viewCenterIndex;
    cs.autoFitY = true;
    if (pGlobalAutoFit) *pGlobalAutoFit = true;
    // pGlobalVCI tidak diubah di Trigger — cukup autoFit
}

// ────────────────────────────────────────────────────────────────
// TriggerAllPrimary
// Convenience: trigger semua tab usesGlobalData sekaligus.
// Dipanggil dari RenderReplayPanel saat keluar dari replay.
// ────────────────────────────────────────────────────────────────
inline void TriggerAllPrimary(MultiChartManager& mgr,
                               int*  pGlobalVCI     = nullptr,
                               bool* pGlobalAutoFit = nullptr)
{
    for (auto* t : mgr.tabs) {
        if (t && t->usesGlobalData) {
            Trigger(t, pGlobalVCI, pGlobalAutoFit);
        }
    }
}

// ────────────────────────────────────────────────────────────────
// Update
// Jalankan smooth-lerp tiap frame menuju candle live.
// Dipanggil SEBELUM menghitung localViewCenter.
//
// pGlobalVCI / pGlobalAutoFit : pointer ke g_chart.viewCenterIndex
//   dan g_chart.autoFitY — nullptr jika bukan tab utama.
//
// Returns: true jika animasi sedang berjalan (perlu redraw)
// ────────────────────────────────────────────────────────────────
inline bool Update(ChartTab* tab,
                   int    bufferCount,
                   int    global_idx_offset,
                   float  rightMarginCandles,
                   int*   pGlobalVCI     = nullptr,
                   bool*  pGlobalAutoFit = nullptr)
{
    if (!tab || !tab->state.isAnimatingToLive || bufferCount <= 0)
        return false;

    ChartTabState& cs    = tab->state;
    double globalTarget  = (double)(global_idx_offset + bufferCount - 1)
                         + (double)rightMarginCandles;

    // Init animasi dari posisi saat ini kalau belum / lompat terlalu jauh
    if (cs.animFloatingIndex == 0.0 ||
        std::abs(cs.animFloatingIndex - (double)cs.viewCenterIndex) > 5000)
        cs.animFloatingIndex = (double)cs.viewCenterIndex;

    // Lerp 15% per frame → smooth tapi tidak terlalu lambat
    double diff = globalTarget - cs.animFloatingIndex;
    cs.animFloatingIndex += diff * 0.15;
    int newIdx = (int)cs.animFloatingIndex;

    cs.viewCenterIndex = newIdx;
    if (pGlobalVCI) *pGlobalVCI = newIdx;

    // Selesai? snap ke target
    if (std::abs(globalTarget - cs.animFloatingIndex) < 0.5) {
        cs.isAnimatingToLive = false;
        cs.animFloatingIndex = 0.0;
        cs.viewCenterIndex   = (int)globalTarget;
        cs.autoFitY          = true;
        if (pGlobalVCI)     *pGlobalVCI     = (int)globalTarget;
        if (pGlobalAutoFit) *pGlobalAutoFit = true;
    }
    return true;
}

// ────────────────────────────────────────────────────────────────
// SyncToGlobals
// Bridge tab->state → variabel legacy (tab utama saja).
// Dipanggil setelah Update() agar extern di RenderNavigationPanel
// tetap sinkron.
// ────────────────────────────────────────────────────────────────
inline void SyncToGlobals(ChartTab* tab,
                           bool&   legacyIsAnimating,
                           double& legacyAnimFloat)
{
    if (!tab || !tab->usesGlobalData) return;
    legacyIsAnimating = tab->state.isAnimatingToLive;
    legacyAnimFloat   = tab->state.animFloatingIndex;
}

// ────────────────────────────────────────────────────────────────
// RenderButton
// Render tombol ">> GO LIVE" overlay di pojok kanan bawah chart.
// Muncul jika:
//   • user scroll kiri → live candle off-screen (> 50 candle dari live)
//   • ATAU price line tidak terlihat secara vertikal
// Tidak muncul jika:
//   • sedang replay
//   • sedang animasi
//   • live candle masih kelihatan DAN harga masih dalam range Y
//
// livePrice   : harga live terakhir (close candle / MarketWatch)
// pGlobalVCI / pGlobalAutoFit : untuk tab utama saja (nullptr lain)
// ────────────────────────────────────────────────────────────────
inline void RenderButton(ChartTab* tab,
                         int    bufferCount,
                         int    global_idx_offset,
                         bool   replayMode,
                         double livePrice,
                         int*   pGlobalVCI     = nullptr,
                         bool*  pGlobalAutoFit = nullptr)
{
    if (replayMode || bufferCount <= 0 || !tab) return;
    if (!tab->state.plotBoundsReady)              return;
    if (tab->state.isAnimatingToLive)             return;

    ChartTabState& cs  = tab->state;
    ImVec2 plotPos     = cs.savedPlotPos;
    ImVec2 plotSize    = cs.savedPlotSize;

    double globalTargetIndex = (double)(global_idx_offset + bufferCount - 1);

    // ── Kondisi 1: live candle off-screen ke kiri ──────────────
    bool liveOffScreen = (cs.viewCenterIndex < (int)(globalTargetIndex - 50));

    // ── Kondisi 2: price line off-screen vertikal ──────────────
    bool priceOffScreen = (livePrice > 1e-5) &&
                          (livePrice < cs.y_min || livePrice > cs.y_max);

    if (!liveOffScreen && !priceOffScreen) return;

    // ── Layout tombol (pojok kanan bawah area plot) ────────────
    const float BTN_W = 105.0f;
    const float BTN_H =  28.0f;
    const float PAD   =  14.0f;

    ImVec2 btnMin = ImVec2(
        plotPos.x + plotSize.x - BTN_W - PAD,
        plotPos.y + plotSize.y - BTN_H - PAD);
    ImVec2 btnMax = ImVec2(btnMin.x + BTN_W, btnMin.y + BTN_H);

    // ── Visual tombol (custom draw, bukan ImGui Button biasa) ──
    ImDrawList* dl  = ImGui::GetWindowDrawList();
    ImVec2      mp  = ImGui::GetIO().MousePos;
    bool        hov = (mp.x >= btnMin.x && mp.x <= btnMax.x &&
                       mp.y >= btnMin.y && mp.y <= btnMax.y);

    ImU32 bgCol  = hov ? IM_COL32(50, 140, 255, 235)
                       : IM_COL32(20,  90, 200, 210);
    ImU32 border = IM_COL32(100, 180, 255, 180);
    dl->AddRectFilled(btnMin, btnMax, bgCol, 6.0f);
    dl->AddRect      (btnMin, btnMax, border, 6.0f, 0, 1.2f);

    const char* lbl = ">> GO LIVE";
    ImVec2      tsz = ImGui::CalcTextSize(lbl);
    dl->AddText(ImVec2(btnMin.x + (BTN_W - tsz.x) * 0.5f,
                       btnMin.y + (BTN_H - tsz.y) * 0.5f),
                IM_COL32(255, 255, 255, 255), lbl);

    // ── Hit-test via InvisibleButton ───────────────────────────
    std::string btnId = "##GTL_" + std::to_string(tab->id);
    ImGui::SetCursorScreenPos(btnMin);
    if (ImGui::InvisibleButton(btnId.c_str(), ImVec2(BTN_W, BTN_H))) {
        Trigger(tab, pGlobalVCI, pGlobalAutoFit);
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Kembali ke candle live");
}

} // namespace GoToLive
