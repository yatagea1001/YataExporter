#pragma once
#include "implot.h"
#include "imgui.h"
#include "Candle.h"
#include <vector>
#include <map>
#include <algorithm>
#include <cstdio>
#include <cmath>
#include <string>

// ==============================================================================
// 📊 ORDER FLOW / FOOTPRINT RENDERER — PROFESSIONAL EDITION
//
// Berisi DUA mode render:
//
//  1. DrawFootprint()        → Overlay ringan di atas candle biasa (mode lama)
//  2. DrawFootprintProfile() → Mode MMT: candle tipis + grid harga kiri/kanan
//                              tick-by-tick live, angka berubah real-time
//                              setiap price line masuk ke area harga
// ==============================================================================

// =============================================================================
// REPLAY DRAW PARAMS
// Pass dari CReplayManager saat replay aktif.
// Kalau nullptr = render normal (semua row footprint tampil penuh).
//
// Cara kerja:
//   tickProgress 0.0→1.0 menggambarkan posisi harga dalam candle.
//   currentState.high/low = range harga yang sudah "dikunjungi" saat ini.
//   Row footprint di luar [replayLow, replayHigh] → SKIP (belum terjadi).
//   Volume di-scale proporsional ke progress → angka "tumbuh" live.
// =============================================================================
struct ReplayDrawParams {
    int   replayIndex = -1;    // index candle yang sedang dianimasikan
    float high        = 0.0f;  // currentState.high dari CReplayManager
    float low         = 0.0f;  // currentState.low  dari CReplayManager
    float progress    = 1.0f;  // tickProgress 0.0→1.0
};

class OrderFlowRenderer {
public:
    // ── PER-TAB STATE ─────────────────────────────────────────────
    // Seperti GPUCandleRenderer, setiap ChartTab punya instance sendiri.
    // symbol  → menentukan tickSize otomatis (BTC=1.0, XAU=0.1, EUR=0.00001)
    // tickSize → satuan harga terkecil untuk clustering yang akurat
    // fpZoom  → density teks (Ctrl+Scroll per-tab)
    std::string symbol   = "";
    float       tickSize = 1.0f;   // default BTC-style
    float       fpZoom   = 1.0f;

    // ── NAKED VPOC TRACKING ───────────────────────────────────────
    // "Naked VPOC" = VPOC dari session lalu yang BELUM dikunjungi harga.
    //
    // Analogi: mobil parkir + lampu sein menyala.
    // Mobil lain (trader) yang lewat langsung tau:
    //   "ada sesuatu yang belum selesai di level ini"
    // → level ini jadi MAGNET harga sampai harga revisit ke sana.
    //
    // Kalau harga sudah kembali menyentuh VPOC → "lampu sein mati"
    // → NakedVPOC dihapus dari list → tidak tampil lagi di chart.
    // ─────────────────────────────────────────────────────────────
    struct NakedVPOC {
        float  price;        // harga VPOC session itu
        int    sessionEnd;   // index candle terakhir session (untuk posisi label)
        bool   isAbove;      // true = VPOC di atas harga sekarang (resistance)
                             // false = VPOC di bawah harga sekarang (support)
        bool   dismissed = false; // 🔥 "Mobil lain matikan lampu sein"
                                  // Trader klik label → "aku sudah tau level ini"
                                  // → level hilang dari chart sesi ini
                                  // → tidak ikut UpdateNakedVPOCs berikutnya
    };

    std::vector<NakedVPOC> nakedVPOCs;    // list semua VPOC yang belum direvisit
    bool showNakedVPOC = true;             // toggle on/off dari UI
    int  nakedSessionBars = 1440;          // 1 session = 1440 candle M1 (1 hari)

    // ── FOOTPRINT VISUAL SETTINGS ────────────────────────────────────
    // Semua setting ini dikontrol dari OrderFlowSettingsUI.h
    // Bisa di-toggle ON/OFF atau di-sliders langsung dari panel settings.
    //
    // Mode 0 = Ask/Bid (split buy-sell, default)
    // Mode 1 = Delta Only (hanya tampil angka delta per level)
    // Mode 2 = Dominant (hanya tampil sisi pemenang)
    int  fpDisplayMode       = 0;      // 0=Ask|Bid, 1=Delta, 2=Dominant

    bool showVolumeNumbers    = true;   // tampilkan angka volume di dalam box
    bool showDeltaLabel       = true;   // tampilkan "+123K" di atas candle
    bool showMiniDeltaBar     = true;   // tampilkan mini histogram di bawah candle (mode Bar)
    bool showStackedImbalance = true;   // tampilkan zona stacked imbalance
    float imbalanceRatio      = 3.0f;   // rasio threshold (default 3.0x)
    bool showAbsorption       = true;   // tampilkan absorption detection (ABB/ABS)
    bool showSinglePrints     = false;  // tampilkan single print highlight
    bool showImbalance        = true;   // tampilkan imbalance outline
    bool showPOCHighlight     = true;   // tampilkan POC gold outline
    float heatmapBaseOpacity  = 0.65f;  // base opacity heatmap (0.1 - 1.0)

    // ── FOOTPRINT COLOR SETTINGS ────────────────────────────────────
    // Semua warna custom, dikontrol dari color picker di settings UI.
    // Default values cocok dengan tema gelap (dark background).
    ImVec4 fpColorBuy         = ImVec4(0.15f, 0.85f, 0.35f, 1.0f);  // hijau (buy/ask)
    ImVec4 fpColorSell        = ImVec4(0.85f, 0.15f, 0.15f, 1.0f);  // merah (sell/bid)
    ImVec4 fpColorDeltaPos    = ImVec4(0.31f, 1.00f, 0.51f, 1.0f);  // hijau terang (delta+)
    ImVec4 fpColorDeltaNeg    = ImVec4(1.00f, 0.35f, 0.35f, 1.0f);  // merah terang (delta-)
    ImVec4 fpColorPOC         = ImVec4(1.00f, 0.84f, 0.00f, 1.0f);  // emas (Point of Control)
    ImVec4 fpColorImbalBuy    = ImVec4(0.39f, 1.00f, 0.39f, 1.0f);  // hijau cerah (imbalance buy)
    ImVec4 fpColorImbalSell   = ImVec4(1.00f, 0.39f, 0.39f, 1.0f);  // merah cerah (imbalance sell)
    ImVec4 fpColorAbsorbBuy   = ImVec4(1.00f, 0.65f, 0.00f, 1.0f);  // oranye (absorption buy)
    ImVec4 fpColorAbsorbSell  = ImVec4(0.78f, 0.31f, 1.00f, 1.0f);  // ungu (absorption sell)

    // ── FOOTPRINT FONT SCALE ────────────────────────────────────────
    float fpFontScale         = 1.0f;   // 0.70 - 1.50 (default 1.0)

    // ── VP VISUAL TOGGLES (simplified) ──────────────────────────────
    bool showVPLabels         = true;   // VAH, VAL, VPOC label text
    bool showVAShading        = true;   // Value Area kuning transparan
    bool showVPOCLine         = true;   // VPOC horizontal dashed line

    // ── HELPER: convert ImVec4 color to IM_COL32 ────────────────────
    ImU32 Col4(const ImVec4& c, int a = 255) const {
        return IM_COL32((int)(c.x * 255), (int)(c.y * 255), (int)(c.z * 255), a);
    }

    // Panggil setelah tab dibuat / symbol berubah
    void SetSymbol(const std::string& sym) {
        symbol = sym;
        // Auto-detect tickSize berdasarkan simbol
        if (sym == "XAUUSD")       tickSize = 0.1f;
        else if (sym == "EURUSD"
              || sym == "GBPUSD")  tickSize = 0.00001f;
        else if (sym == "ETHUSDT") tickSize = 0.01f;
        else                       tickSize = 1.0f;  // BTCUSDT dll
    }

    // live=true  → candle aktif (tick masuk realtime) → tampil desimal: 200.1K jedak jedug
    // live=false → candle closed (history)            → genap tanpa desimal: 200K bersih
    static void FormatVolumeUSD(char* buf, int bufSize, double val, bool live = false) {
        double absVal = std::abs(val);
        if (absVal >= 1'000'000'000.0)
            snprintf(buf, bufSize, "%.1fB", val / 1'000'000'000.0);
        else if (absVal >= 1'000'000.0)
            snprintf(buf, bufSize, "%.1fM", val / 1'000'000.0);
        else if (absVal >= 1'000.0)
            snprintf(buf, bufSize, live ? "%.1fK" : "%.0fK", val / 1'000.0);
        else
            snprintf(buf, bufSize, live ? "%.1f"  : "%.0f",  val);
    }
    // =========================================================================
    // MODE 1 — DrawFootprint (Overlay di atas candle, clustering ala Profile)
    // Candle body tetap digambar oleh GPUCandleRenderer di bawahnya.
    // Boxes pakai algoritma clustering yang sama dengan DrawFootprintProfile()
    // sehingga tampilannya rapi, bersih, dan enak dibaca.
    // =========================================================================
    void DrawFootprint(const std::vector<Candle>& candles,
                       const ReplayDrawParams* rp = nullptr) {
        if (candles.empty()) return;

        ImVec2 pA = ImPlot::PlotToPixels(ImPlotPoint(0, 0));
        ImVec2 pB = ImPlot::PlotToPixels(ImPlotPoint(1, 0));
        float barW = std::abs(pB.x - pA.x);
        if (barW < 4.0f) return;

        // ── Zoom Tier (sama persis dengan DrawFootprintProfile) ──────────────
        int   zoomTier;
        float MIN_BOX_H;
        if      (barW >= 80.0f) { zoomTier = 0; MIN_BOX_H = 16.0f; }
        else if (barW >= 50.0f) { zoomTier = 1; MIN_BOX_H = 20.0f; }
        else if (barW >= 28.0f) { zoomTier = 2; MIN_BOX_H = 34.0f; }
        else if (barW >= 12.0f) { zoomTier = 3; MIN_BOX_H = 56.0f; }
        else                    { zoomTier = 4; MIN_BOX_H = 9999.0f; }
        if (fpZoom > 0.01f) MIN_BOX_H /= fpZoom;

        ImDrawList*  dl     = ImPlot::GetPlotDrawList();
        ImPlotRect   limits = ImPlot::GetPlotLimits();
        int startIdx = std::max(0,    (int)limits.X.Min - 2);
        int endIdx   = std::min((int)candles.size() - 1, (int)limits.X.Max + 2);

        ImVec2 plotPos  = ImPlot::GetPlotPos();
        ImVec2 plotSize = ImPlot::GetPlotSize();
        float screenTop = plotPos.y;
        float screenBot = plotPos.y + plotSize.y;

        for (int i = startIdx; i <= endIdx; ++i) {
            const Candle& c = candles[i];
            if (zoomTier >= 4) continue; // terlalu zoom out → skip

            // ── REPLAY FILTER ────────────────────────────────────────────────
            // Saat replay aktif, hanya tampilkan row yang sudah "dikunjungi"
            // harga (dalam range [replayLow, replayHigh]).
            // Volume di-scale ke tickProgress supaya angka "tumbuh" live.
            bool isReplayCandle = (rp && i == rp->replayIndex && rp->progress < 0.99f);
            std::vector<FootprintLevel> fp_filtered;
            const std::vector<FootprintLevel>* fp_src = &c.footprint;
            if (isReplayCandle && !c.footprint.empty()) {
                float margin = tickSize * 0.6f;
                for (const auto& fp : c.footprint) {
                    if ((float)fp.price >= rp->low  - margin &&
                        (float)fp.price <= rp->high + margin) {
                        FootprintLevel scaled   = fp;
                        scaled.buyVol  *= (double)rp->progress;
                        scaled.sellVol *= (double)rp->progress;
                        fp_filtered.push_back(scaled);
                    }
                }
                fp_src = &fp_filtered;
            }
            if (fp_src->empty()) continue;

            // Live candle = candle terakhir ATAU sedang dianimasikan replay
            bool isLive = (i == (int)candles.size() - 1) || isReplayCandle;

            float x_px   = ImPlot::PlotToPixels(ImPlotPoint(i, c.close)).x;
            float half_w = barW * 0.42f;   // sedikit lebih sempit dari lebar candle
            float xLeft  = x_px - half_w;
            float xMid   = x_px;
            float xRight = x_px + half_w;
            float yHigh  = ImPlot::PlotToPixels(ImPlotPoint(i, c.high)).y;

            // ── Kalkulasi POC + Total ─────────────────────────────────────────
            float  pocPrice   = 0.0f;
            double pocVolRaw  = 0.0, totalBuy = 0.0, totalSell = 0.0;
            for (const auto& fp : *fp_src) {
                double rv = fp.buyVol + fp.sellVol;
                if (rv > pocVolRaw) { pocVolRaw = rv; pocPrice = (float)fp.price; }
                totalBuy  += fp.buyVol;
                totalSell += fp.sellVol;
            }

            // ── Hitung tinggi row mentah (untuk clustering) ───────────────────
            float rawRowH = 2.0f;
            if (fp_src->size() >= 2) {
                rawRowH = std::abs(
                    ImPlot::PlotToPixels(ImPlotPoint(i, (*fp_src)[1].price)).y -
                    ImPlot::PlotToPixels(ImPlotPoint(i, (*fp_src)[0].price)).y);
            }

            // ── Clustering (identik dengan DrawFootprintProfile) ─────────────
            struct Cluster { float yTop, yBot; double buyVol, sellVol, peakBuy, peakSell; bool hasPOC; };
            std::vector<Cluster> clusters;
            double aBuy = 0, aSell = 0, peakB = 0, peakS = 0, peakV = 0;
            bool   cPoc = false;
            float  anchorY = ImPlot::PlotToPixels(ImPlotPoint(i, (*fp_src)[0].price)).y;
            float  firstY  = anchorY;

            for (size_t j = 0; j < fp_src->size(); ++j) {
                const auto& fp   = (*fp_src)[j];
                float        yRow = ImPlot::PlotToPixels(ImPlotPoint(i, fp.price)).y;

                aBuy  += fp.buyVol; aSell += fp.sellVol;
                double tv = fp.buyVol + fp.sellVol;
                if (tv > peakV) { peakV = tv; peakB = fp.buyVol; peakS = fp.sellVol; }
                if (fp.price == pocPrice) cPoc = true;

                bool isLast = (j == fp_src->size() - 1);
                if (std::abs(yRow - anchorY) >= MIN_BOX_H || isLast) {
                    float top  = std::min(firstY, yRow) - (rawRowH * 0.5f);
                    float bot  = std::max(firstY, yRow) + (rawRowH * 0.5f);
                    float minH = (zoomTier <= 1) ? 16.0f : MIN_BOX_H * 0.45f;
                    if (bot - top < minH) { float mid = (top+bot)*0.5f; top = mid-minH*0.5f; bot = mid+minH*0.5f; }
                    clusters.push_back({top, bot, aBuy, aSell, peakB, peakS, cPoc});
                    if (!isLast) {
                        aBuy = aSell = peakB = peakS = peakV = 0; cPoc = false;
                        anchorY = ImPlot::PlotToPixels(ImPlotPoint(i, (*fp_src)[j+1].price)).y;
                        firstY  = anchorY;
                    }
                }
            }

            // ── Render Clusters ───────────────────────────────────────────────
            double maxClusterVol = 1.0;
            for (const auto& cl : clusters)
                if (cl.buyVol + cl.sellVol > maxClusterVol) maxClusterVol = cl.buyVol + cl.sellVol;

            for (const auto& cl : clusters) {
                if (cl.yBot < screenTop || cl.yTop > screenBot) continue;

                bool iBuy  = (cl.sellVol > 0.001) && (cl.buyVol  / cl.sellVol >= 3.0) && cl.buyVol  > (maxClusterVol * 0.1);
                bool iSell = (cl.buyVol  > 0.001) && (cl.sellVol / cl.buyVol  >= 3.0) && cl.sellVol > (maxClusterVol * 0.1);

                // Heatmap opacity — semakin tebal volume, semakin solid
                float ratio   = (float)((cl.buyVol + cl.sellVol) / maxClusterVol);
                float bgAlpha = heatmapBaseOpacity * (0.30f + (0.70f * ratio));

                dl->AddRectFilled(ImVec2(xLeft, cl.yTop+1), ImVec2(xMid,   cl.yBot-1),
                                  ImGui::GetColorU32(ImVec4(fpColorSell.x, fpColorSell.y, fpColorSell.z, bgAlpha)));
                dl->AddRectFilled(ImVec2(xMid,  cl.yTop+1), ImVec2(xRight, cl.yBot-1),
                                  ImGui::GetColorU32(ImVec4(fpColorBuy.x, fpColorBuy.y, fpColorBuy.z, bgAlpha)));

                // Outline: POC, Imbalance hijau/merah terang
                if (showPOCHighlight && cl.hasPOC)
                    dl->AddRect(ImVec2(xLeft, cl.yTop), ImVec2(xRight, cl.yBot), Col4(fpColorPOC),  0, 0, 2.0f);
                else if (showImbalance && iBuy)
                    dl->AddRect(ImVec2(xMid,  cl.yTop), ImVec2(xRight, cl.yBot), Col4(fpColorImbalBuy), 0, 0, 2.0f);
                else if (showImbalance && iSell)
                    dl->AddRect(ImVec2(xLeft, cl.yTop), ImVec2(xMid,   cl.yBot), Col4(fpColorImbalSell), 0, 0, 2.0f);
                else
                    dl->AddLine(ImVec2(xLeft, cl.yBot), ImVec2(xRight, cl.yBot), IM_COL32(255,255,255,18), 1.0f);

                // Teks angka volume
                float boxH = cl.yBot - cl.yTop;
                float ty   = (cl.yTop + cl.yBot) * 0.5f;

                if (showVolumeNumbers && zoomTier <= 1) {
                    char tS[16], tB[16];
                    FormatVolumeUSD(tS, sizeof(tS), cl.sellVol, isLive);
                    FormatVolumeUSD(tB, sizeof(tB), cl.buyVol,  isLive);
                    ImVec2 szS = ImGui::CalcTextSize(tS), szB = ImGui::CalcTextSize(tB);
                    ImU32  cS  = iSell ? IM_COL32(255,255,255,255) : IM_COL32(255,200,200,255);
                    ImU32  cB  = iBuy  ? IM_COL32(255,255,255,255) : IM_COL32(200,255,200,255);
                    float  txS = xLeft + (half_w * 0.5f) - (szS.x * 0.5f);
                    float  txB = xMid  + (half_w * 0.5f) - (szB.x * 0.5f);
                    if (boxH >= szS.y - 2.0f) {
                        dl->AddText(ImVec2(txS, ty - szS.y*0.5f), cS, tS);
                        dl->AddText(ImVec2(txB, ty - szB.y*0.5f), cB, tB);
                    }
                } else if (showVolumeNumbers) {
                    bool   domIsBuy = (cl.peakBuy >= cl.peakSell);
                    double domVol   = domIsBuy ? cl.peakBuy : cl.peakSell;
                    char   tD[16];
                    FormatVolumeUSD(tD, sizeof(tD), domVol, isLive);
                    ImVec2 szD  = ImGui::CalcTextSize(tD);
                    float  txD  = (xLeft + xRight) * 0.5f - szD.x * 0.5f;
                    ImU32  cD   = domIsBuy ? IM_COL32(180,255,180,255) : IM_COL32(255,180,180,255);
                    if (boxH >= szD.y - 2.0f)
                        dl->AddText(ImVec2(txD, ty - szD.y*0.5f), cD, tD);
                }
            }

            // ── Delta Total (di atas candle, dengan background) ───────────────
            if (showDeltaLabel && zoomTier <= 3) {
                double totalDelta = totalBuy - totalSell;
                char   td[24], tmp[22];
                FormatVolumeUSD(tmp, sizeof(tmp), std::abs(totalDelta), isLive);
                snprintf(td, sizeof(td), "%s%s", totalDelta >= 0 ? "+" : "-", tmp);
                ImVec2 szD = ImGui::CalcTextSize(td);
                ImU32  cD  = totalDelta >= 0 ? Col4(fpColorDeltaPos) : Col4(fpColorDeltaNeg);
                // Background hitam tipis supaya angka tetap terbaca di atas candle body
                dl->AddRectFilled(ImVec2(x_px - szD.x*0.5f - 2, yHigh - szD.y - 6),
                                  ImVec2(x_px + szD.x*0.5f + 2, yHigh - 2), IM_COL32(0,0,0,160));
                // Font scale support
                float fsz = ImGui::GetFontSize() * fpFontScale;
                dl->AddText(ImGui::GetFont(), fsz, ImVec2(x_px - szD.x*0.5f, yHigh - szD.y - 4), cD, td);
            }
        }
    }
    // =========================================================================
    // MODE   1 — DrawFootprintBar (Aggressive Volume Footprint)
    //
    // Visual: Candle tipis (wick only, 2px) di posisi normal.
    //         Per price level: horizontal bar merah tumbuh ke KIRI (sell/bid)
    //                          horizontal bar hijau tumbuh ke KANAN (buy/ask)
    //         Panjang bar PROPORSIONAL terhadap volume (bukan warna opacity).
    //
    // Fitur tambahan vs mode 1 & 2:
    //   ★ Proportional Bar (bukan fixed box)
    //   ★ Absorption Detection (volume besar di ekstrem candle → reverse)
    //   ★ Stacked Imbalance Zone (3+ imbalance berturut → stripe highlight)
    //   ★ Delta bar kecil di bawah candle (mini histogram)
    //
    // Logika cluster & zoom IDENTIK dengan mode 1 dan 2.
    // =========================================================================
    void DrawFootprintBar(const std::vector<Candle>& candles,
                          const ReplayDrawParams* rp = nullptr) {
        if (candles.empty()) return;

        ImVec2 pA = ImPlot::PlotToPixels(ImPlotPoint(0, 0));
        ImVec2 pB = ImPlot::PlotToPixels(ImPlotPoint(1, 0));
        float barW = std::abs(pB.x - pA.x);
        if (barW < 4.0f) return;

        // ── Zoom Tier (identik mode 1 & 2) ──────────────────────────────────
        int   zoomTier;
        float MIN_BOX_H;
        if      (barW >= 80.0f) { zoomTier = 0; MIN_BOX_H = 14.0f; }
        else if (barW >= 50.0f) { zoomTier = 1; MIN_BOX_H = 18.0f; }
        else if (barW >= 28.0f) { zoomTier = 2; MIN_BOX_H = 32.0f; }
        else if (barW >= 12.0f) { zoomTier = 3; MIN_BOX_H = 54.0f; }
        else                    { zoomTier = 4; MIN_BOX_H = 9999.0f; }
        // fpZoom per-tab instance: >1 = lebih detail, <1 = lebih cluster
        if (fpZoom > 0.01f) MIN_BOX_H /= fpZoom;
        // fpZoom per-tab: >1.0 → lebih detail (MIN_BOX_H kecil), <1.0 → lebih cluster
        if (fpZoom > 0.01f) MIN_BOX_H /= fpZoom;

        ImDrawList*  dl     = ImPlot::GetPlotDrawList();
        ImPlotRect   limits = ImPlot::GetPlotLimits();
        int startIdx = std::max(0,    (int)limits.X.Min - 2);
        int endIdx   = std::min((int)candles.size() - 1, (int)limits.X.Max + 2);

        ImVec2 plotPos  = ImPlot::GetPlotPos();
        ImVec2 plotSize = ImPlot::GetPlotSize();
        float screenTop = plotPos.y;
        float screenBot = plotPos.y + plotSize.y;

        // Lebar maksimum bar tiap sisi (kiri = sell, kanan = buy)
        // Sisakan sedikit gap antar candle agar tidak bertabrakan
        const float MAX_BAR_HALF = barW * 0.46f;

        for (int i = startIdx; i <= endIdx; ++i) {
            const Candle& c = candles[i];

            float x_px  = ImPlot::PlotToPixels(ImPlotPoint(i, c.close)).x;
            float yHigh = ImPlot::PlotToPixels(ImPlotPoint(i, c.high)).y;
            float yLow  = ImPlot::PlotToPixels(ImPlotPoint(i, c.low)).y;
            float yOpen = ImPlot::PlotToPixels(ImPlotPoint(i, c.open)).y;

            // ── REPLAY FILTER ────────────────────────────────────────────────
            bool isReplayCandle = (rp && i == rp->replayIndex && rp->progress < 0.99f);
            std::vector<FootprintLevel> fp_filtered;
            const std::vector<FootprintLevel>* fp_src = &c.footprint;
            if (isReplayCandle && !c.footprint.empty()) {
                float margin = tickSize * 0.6f;
                for (const auto& fp : c.footprint) {
                    if ((float)fp.price >= rp->low  - margin &&
                        (float)fp.price <= rp->high + margin) {
                        FootprintLevel scaled   = fp;
                        scaled.buyVol  *= (double)rp->progress;
                        scaled.sellVol *= (double)rp->progress;
                        fp_filtered.push_back(scaled);
                    }
                }
                fp_src = &fp_filtered;
            }

            // Live candle = candle terakhir ATAU sedang dianimasikan replay
            bool isLive = (i == (int)candles.size() - 1) || isReplayCandle;
            float yClose= ImPlot::PlotToPixels(ImPlotPoint(i, c.close)).y;
            bool  isBull = (c.close >= c.open);

            // ── A. CANDLE TIPIS: wick saja, body 2px ─────────────────────────
            ImU32 wickCol = isBull
                ? IM_COL32(70, 210, 110, 220)
                : IM_COL32(220, 70,  70,  220);
            ImU32 bodyCol = isBull
                ? IM_COL32(50, 180,  90, 200)
                : IM_COL32(200, 50,  50, 200);

            // Wick (full high-low)
            dl->AddLine(ImVec2(x_px, yHigh), ImVec2(x_px, yLow), wickCol, 1.5f);
            // Body tipis (open→close), lebar 3px
            float bodyTop = std::min(yOpen, yClose);
            float bodyBot = std::max(yOpen, yClose);
            if (bodyBot - bodyTop < 2.0f) bodyBot = bodyTop + 2.0f;
            dl->AddRectFilled(ImVec2(x_px - 1.5f, bodyTop),
                              ImVec2(x_px + 1.5f, bodyBot), bodyCol);

            if (fp_src->empty()) continue;
            if (zoomTier >= 4) continue;

            // ── B. KALKULASI POC + TOTAL (identik mode 1 & 2) ────────────────
            float  pocPrice  = 0.0f;
            double pocVolRaw = 0.0, totalBuy = 0.0, totalSell = 0.0;
            double globalMax = 1.0; // max single-side vol untuk normalisasi bar
            for (const auto& fp : *fp_src) {
                double rv = fp.buyVol + fp.sellVol;
                if (rv > pocVolRaw) { pocVolRaw = rv; pocPrice = (float)fp.price; }
                totalBuy  += fp.buyVol;
                totalSell += fp.sellVol;
                if (fp.buyVol  > globalMax) globalMax = fp.buyVol;
                if (fp.sellVol > globalMax) globalMax = fp.sellVol;
            }

            // ── C. CLUSTERING (IDENTIK mode 1 & 2) ───────────────────────────
            float rawRowH = 2.0f;
            if (fp_src->size() >= 2) {
                rawRowH = std::abs(
                    ImPlot::PlotToPixels(ImPlotPoint(i, (*fp_src)[1].price)).y -
                    ImPlot::PlotToPixels(ImPlotPoint(i, (*fp_src)[0].price)).y);
            }

            struct Cluster {
                float  yTop, yBot;
                double buyVol, sellVol, peakBuy, peakSell;
                bool   hasPOC;
                float  priceCenter; // untuk absorption detection
            };
            std::vector<Cluster> clusters;
            double aBuy=0, aSell=0, peakB=0, peakS=0, peakV=0;
            bool   cPoc=false;
            float  anchorY    = ImPlot::PlotToPixels(ImPlotPoint(i, (*fp_src)[0].price)).y;
            float  firstY     = anchorY;
            float  firstPrice = (float)(*fp_src)[0].price;

            for (size_t j = 0; j < fp_src->size(); ++j) {
                const auto& fp   = (*fp_src)[j];
                float        yRow = ImPlot::PlotToPixels(ImPlotPoint(i, fp.price)).y;

                aBuy  += fp.buyVol; aSell += fp.sellVol;
                double tv = fp.buyVol + fp.sellVol;
                if (tv > peakV) { peakV=tv; peakB=fp.buyVol; peakS=fp.sellVol; }
                if (fp.price == pocPrice) cPoc = true;

                bool isLast = (j == fp_src->size()-1);
                if (std::abs(yRow - anchorY) >= MIN_BOX_H || isLast) {
                    float top = std::min(firstY, yRow) - rawRowH * 0.5f;
                    float bot = std::max(firstY, yRow) + rawRowH * 0.5f;
                    float minH = (zoomTier <= 1) ? 14.0f : MIN_BOX_H * 0.45f;
                    if (bot - top < minH) {
                        float mid=(top+bot)*0.5f;
                        top=mid-minH*0.5f; bot=mid+minH*0.5f;
                    }
                    float pc = ((float)fp.price + firstPrice) * 0.5f;
                    clusters.push_back({top, bot, aBuy, aSell, peakB, peakS, cPoc, pc});
                    if (!isLast) {
                        aBuy=aSell=peakB=peakS=peakV=0; cPoc=false;
                        anchorY    = ImPlot::PlotToPixels(ImPlotPoint(i, (*fp_src)[j+1].price)).y;
                        firstY     = anchorY;
                        firstPrice = (float)(*fp_src)[j+1].price;
                    }
                }
            }

            // ── D. DETEKSI STACKED IMBALANCE ─────────────────────────────────
            // Tandai setiap cluster apakah bagian dari stacked imbalance (3+ berturut)
            const float IMBAL_RATIO = imbalanceRatio; // dari settings UI
            const float IMBAL_MIN   = 0.08f; // minimal 8% dari max vol
            std::vector<int> stackBuyTag(clusters.size(), 0);   // +1 = stacked buy
            std::vector<int> stackSellTag(clusters.size(), 0);  // +1 = stacked sell

            // Scan forward untuk buy stacks
            {
                int streak = 0;
                for (int k = 0; k < (int)clusters.size(); ++k) {
                    auto& cl = clusters[k];
                    bool isImbalBuy = (cl.sellVol > 0.001)
                        && (cl.buyVol / cl.sellVol >= IMBAL_RATIO)
                        && (cl.buyVol > globalMax * IMBAL_MIN);
                    streak = isImbalBuy ? streak+1 : 0;
                    if (streak >= 3) {
                        // Tandai mundur
                        for (int back=k; back>=k-streak+1 && back>=0; --back)
                            stackBuyTag[back] = 1;
                    }
                }
            }
            // Scan forward untuk sell stacks
            {
                int streak = 0;
                for (int k = 0; k < (int)clusters.size(); ++k) {
                    auto& cl = clusters[k];
                    bool isImbalSell = (cl.buyVol > 0.001)
                        && (cl.sellVol / cl.buyVol >= IMBAL_RATIO)
                        && (cl.sellVol > globalMax * IMBAL_MIN);
                    streak = isImbalSell ? streak+1 : 0;
                    if (streak >= 3) {
                        for (int back=k; back>=k-streak+1 && back>=0; --back)
                            stackSellTag[back] = 1;
                    }
                }
            }

            // ── E. DETEKSI ABSORPTION (skip jika disabled) ─────────────────────
            if (!showAbsorption && !showStackedImbalance) {
                // Langsung skip detection jika keduanya OFF
                // Tapi kita masih perlu array kosong untuk render loop
            }
            if (!showAbsorption) {
                // Skip absorption detection tapi tetap jalankan stacked imbalance
            }

            // ── E. DETEKSI ABSORPTION ─────────────────────────────────────────
            // Absorption Buy: cluster dekat LOW dengan buyVol >> sellVol + vol tinggi
            // Absorption Sell: cluster dekat HIGH dengan sellVol >> buyVol + vol tinggi
            float priceLow  = (float)c.low;
            float priceHigh = (float)c.high;
            float priceRange = priceHigh - priceLow;
            if (priceRange < 0.001f) priceRange = 0.001f;
            const float ABSORB_ZONE = 0.20f; // 20% dari range candle = zona ekstrem
            const double ABSORB_MIN = globalMax * 0.40; // minimal 40% dari max vol

            std::vector<int> absorbTag(clusters.size(), 0);
            // 0=none, 1=absorb_buy (bullish), -1=absorb_sell (bearish)
            for (int k = 0; k < (int)clusters.size(); ++k) {
                auto& cl = clusters[k];
                double totalCl = cl.buyVol + cl.sellVol;
                // Near LOW zone
                bool nearLow  = (std::abs(cl.priceCenter - priceLow)  / priceRange) < ABSORB_ZONE;
                // Near HIGH zone
                bool nearHigh = (std::abs(cl.priceCenter - priceHigh) / priceRange) < ABSORB_ZONE;

                if (nearLow  && cl.buyVol > 2.0*cl.sellVol && totalCl > ABSORB_MIN)
                    absorbTag[k] =  1; // buyers absorb sellers at low → bullish
                if (nearHigh && cl.sellVol > 2.0*cl.buyVol && totalCl > ABSORB_MIN)
                    absorbTag[k] = -1; // sellers absorb buyers at high → bearish
            }

            // ── F. RENDER BARS ────────────────────────────────────────────────
            for (int k = 0; k < (int)clusters.size(); ++k) {
                const auto& cl = clusters[k];
                if (cl.yBot < screenTop || cl.yTop > screenBot) continue;

                float ty    = (cl.yTop + cl.yBot) * 0.5f;
                float boxH  = cl.yBot - cl.yTop;
                float pad   = 1.2f; // gap vertikal antar bar

                // Panjang bar proporsional terhadap globalMax
                float sellBarW = (float)(cl.sellVol / globalMax) * MAX_BAR_HALF;
                float buyBarW  = (float)(cl.buyVol  / globalMax) * MAX_BAR_HALF;
                sellBarW = std::min(sellBarW, MAX_BAR_HALF);
                buyBarW  = std::min(buyBarW,  MAX_BAR_HALF);

                float sellX0 = x_px - sellBarW; // bar merah dari xMid ke kiri
                float buyX1  = x_px + buyBarW;  // bar hijau dari xMid ke kanan

                // ── Alpha: sedikit lebih terang untuk cluster volume tinggi ──
                float volRatio = (float)((cl.buyVol+cl.sellVol) / (globalMax*2.0));
                float alpha    = heatmapBaseOpacity * (0.55f + 0.40f * std::min(volRatio, 1.0f));

                // ── Warna dasar ──
                ImU32 cSell = ImGui::GetColorU32(ImVec4(fpColorSell.x, fpColorSell.y, fpColorSell.z, alpha));
                ImU32 cBuy  = ImGui::GetColorU32(ImVec4(fpColorBuy.x, fpColorBuy.y, fpColorBuy.z, alpha));

                // ── Override: Stacked imbalance → warna lebih terang/solid ──
                if (showStackedImbalance && stackBuyTag[k])
                    cBuy  = ImGui::GetColorU32(ImVec4(fpColorImbalBuy.x, fpColorImbalBuy.y, fpColorImbalBuy.z, 0.90f));
                if (showStackedImbalance && stackSellTag[k])
                    cSell = ImGui::GetColorU32(ImVec4(fpColorImbalSell.x, fpColorImbalSell.y, fpColorImbalSell.z, 0.90f));

                // ── Override: Absorption → oranye/ungu ──
                ImU32 cAbsorb = 0;
                if (showAbsorption && absorbTag[k] ==  1) cAbsorb = Col4(fpColorAbsorbBuy, 230);
                if (showAbsorption && absorbTag[k] == -1) cAbsorb = Col4(fpColorAbsorbSell, 230);

                // ── Gambar Bar Sell (merah, tumbuh ke KIRI dari xMid) ─────────
                if (sellBarW > 0.5f)
                    dl->AddRectFilled(ImVec2(sellX0, cl.yTop + pad),
                                      ImVec2(x_px,   cl.yBot - pad), cSell);

                // ── Gambar Bar Buy (hijau, tumbuh ke KANAN dari xMid) ─────────
                if (buyBarW > 0.5f)
                    dl->AddRectFilled(ImVec2(x_px,  cl.yTop + pad),
                                      ImVec2(buyX1, cl.yBot - pad), cBuy);

                // ── Garis tengah pemisah (vertikal tipis di xMid) ────────────
                dl->AddLine(ImVec2(x_px, cl.yTop + pad),
                            ImVec2(x_px, cl.yBot - pad),
                            IM_COL32(255,255,255, 25), 1.0f);

                // ── Outline: POC = emas, Absorption = warna khusus ───────────
                if (showPOCHighlight && cl.hasPOC) {
                    dl->AddRect(ImVec2(sellX0 - 1, cl.yTop),
                                ImVec2(buyX1  + 1, cl.yBot),
                                Col4(fpColorPOC), 0, 0, 2.0f);
                    // Naked POC line ke kanan (preview)
                    dl->AddLine(ImVec2(buyX1, ty), ImVec2(buyX1 + 12, ty),
                                Col4(fpColorPOC, 140), 1.5f);
                }
                else if (cAbsorb) {
                    // Box outline warna absorption di seluruh lebar bar
                    dl->AddRect(ImVec2(sellX0, cl.yTop),
                                ImVec2(buyX1,  cl.yBot), cAbsorb, 0, 0, 2.0f);
                    // Marker kecil di sisi yang absorb
                    const char* absLabel = (absorbTag[k] == 1) ? "ABB" : "ABS";
                    ImVec2 szAb = ImGui::CalcTextSize(absLabel);
                    float  abX  = (absorbTag[k] == 1)
                                    ? sellX0 - szAb.x - 3
                                    : buyX1  + 3;
                    dl->AddText(ImVec2(abX, ty - szAb.y*0.5f), cAbsorb, absLabel);
                }
                else if (stackBuyTag[k]) {
                    // Border tipis kanan untuk zona stacked buy
                    dl->AddLine(ImVec2(buyX1, cl.yTop), ImVec2(buyX1, cl.yBot),
                                IM_COL32(80, 255, 120, 200), 2.0f);
                }
                else if (stackSellTag[k]) {
                    // Border tipis kiri untuk zona stacked sell
                    dl->AddLine(ImVec2(sellX0, cl.yTop), ImVec2(sellX0, cl.yBot),
                                IM_COL32(255, 80, 80, 200), 2.0f);
                }
                else {
                    // Garis pemisah antar level (sangat tipis)
                    dl->AddLine(ImVec2(sellX0, cl.yBot),
                                ImVec2(buyX1,  cl.yBot),
                                IM_COL32(255,255,255, 15), 1.0f);
                }

                // ── Teks Angka Volume ─────────────────────────────────────────
                if (showVolumeNumbers && zoomTier <= 1) {
                    // Mode zoom in: tampil angka sell (kiri) & buy (kanan)
                    char tS[16], tB[16];
                    FormatVolumeUSD(tS, sizeof(tS), cl.sellVol, isLive);
                    FormatVolumeUSD(tB, sizeof(tB), cl.buyVol,  isLive);
                    ImVec2 szS = ImGui::CalcTextSize(tS);
                    ImVec2 szB = ImGui::CalcTextSize(tB);

                    // Sell label: di dalam bar merah, rata kanan (dekat xMid)
                    float txS = x_px - szS.x - 3.0f;
                    float txB = x_px + 3.0f;

                    ImU32 cTxtS = stackSellTag[k]
                                    ? IM_COL32(255,255,255,255)
                                    : IM_COL32(255, 210, 210, 230);
                    ImU32 cTxtB = stackBuyTag[k]
                                    ? IM_COL32(255,255,255,255)
                                    : IM_COL32(200, 255, 210, 230);

                    if (boxH >= szS.y - 2.0f) {
                        // Shadow tipis supaya teks terbaca di atas bar
                        dl->AddText(ImVec2(txS+1, ty - szS.y*0.5f+1), IM_COL32(0,0,0,140), tS);
                        dl->AddText(ImVec2(txS,   ty - szS.y*0.5f),   cTxtS, tS);
                        dl->AddText(ImVec2(txB+1, ty - szB.y*0.5f+1), IM_COL32(0,0,0,140), tB);
                        dl->AddText(ImVec2(txB,   ty - szB.y*0.5f),   cTxtB, tB);
                    }
                } else if (showVolumeNumbers && zoomTier == 2) {
                    // Mode zoom medium: tampil angka dominant saja
                    bool   domBuy = (cl.buyVol >= cl.sellVol);
                    double domVol = domBuy ? cl.buyVol : cl.sellVol;
                    char   tD[16];
                    FormatVolumeUSD(tD, sizeof(tD), domVol, isLive);
                    ImVec2 szD = ImGui::CalcTextSize(tD);
                    float  txD = domBuy
                                    ? x_px + 3.0f
                                    : x_px - szD.x - 3.0f;
                    ImU32  cD  = domBuy
                                    ? IM_COL32(200,255,210,220)
                                    : IM_COL32(255,210,210,220);
                    if (boxH >= szD.y - 2.0f) {
                        dl->AddText(ImVec2(txD+1, ty-szD.y*0.5f+1), IM_COL32(0,0,0,120), tD);
                        dl->AddText(ImVec2(txD,   ty-szD.y*0.5f),   cD, tD);
                    }
                }
                // zoomTier >= 3: tidak tampil teks, hanya bar warna
            }

            // ── G. DELTA TOTAL (di atas candle) ─────────────────────────────
            if (showDeltaLabel && zoomTier <= 3) {
                double totalDelta = totalBuy - totalSell;
                char   td[24], tmp[22];
                FormatVolumeUSD(tmp, sizeof(tmp), std::abs(totalDelta), isLive);
                snprintf(td, sizeof(td), "%s%s", totalDelta >= 0 ? "+" : "-", tmp);
                ImVec2 szD = ImGui::CalcTextSize(td);
                ImU32  cD  = totalDelta >= 0
                                ? Col4(fpColorDeltaPos)
                                : Col4(fpColorDeltaNeg);
                // Background hitam agar terbaca
                dl->AddRectFilled(
                    ImVec2(x_px - szD.x*0.5f - 2, yHigh - szD.y - 7),
                    ImVec2(x_px + szD.x*0.5f + 2, yHigh - 1),
                    IM_COL32(0, 0, 0, 160));
                float fsz = ImGui::GetFontSize() * fpFontScale;
                dl->AddText(ImGui::GetFont(), fsz, ImVec2(x_px - szD.x*0.5f, yHigh - szD.y - 5), cD, td);
            }

            // ── H. MINI DELTA BAR (di bawah candle, seperti histogram kecil) ─
            if (showMiniDeltaBar && zoomTier <= 2) {
                double totalDelta  = totalBuy - totalSell;
                double maxPossible = totalBuy + totalSell;
                if (maxPossible > 0.001) {
                    float  deltaRatio = (float)(std::abs(totalDelta) / maxPossible);
                    float  miniBarW   = MAX_BAR_HALF * deltaRatio;
                    float  miniY1     = yLow + 4.0f;
                    float  miniY2     = yLow + 8.0f;
                    ImU32  cMini      = totalDelta >= 0
                                            ? Col4(fpColorDeltaPos, 200)
                                            : Col4(fpColorDeltaNeg, 200);
                    if (totalDelta >= 0)
                        dl->AddRectFilled(ImVec2(x_px, miniY1), ImVec2(x_px + miniBarW, miniY2), cMini);
                    else
                        dl->AddRectFilled(ImVec2(x_px - miniBarW, miniY1), ImVec2(x_px, miniY2), cMini);
                }
            }
        }
    }

   // =========================================================================
    // MODE 2 — DrawFootprintProfile (VISUAL UPGRADE!)
    // =========================================================================
    void DrawFootprintProfile(const std::vector<Candle>& candles,
                              const ReplayDrawParams* rp = nullptr) {
        if (candles.empty()) return;

        ImVec2 pA = ImPlot::PlotToPixels(ImPlotPoint(0, 0));
        ImVec2 pB = ImPlot::PlotToPixels(ImPlotPoint(1, 0));
        float barW = std::abs(pB.x - pA.x);

        if (barW < 4.0f) return;

        // ── TENTUKAN ZOOM TIER ────────────────────────────────────────────────
        int   zoomTier;
        float MIN_BOX_H;   
        if      (barW >= 80.0f) { zoomTier = 0; MIN_BOX_H = 16.0f;   }  
        else if (barW >= 50.0f) { zoomTier = 1; MIN_BOX_H = 20.0f;   }  
        else if (barW >= 28.0f) { zoomTier = 2; MIN_BOX_H = 34.0f;   }  
        else if (barW >= 12.0f) { zoomTier = 3; MIN_BOX_H = 56.0f;   }  
        else                    { zoomTier = 4; MIN_BOX_H = 9999.0f; }
        if (fpZoom > 0.01f) MIN_BOX_H /= fpZoom;  

        ImDrawList* dl     = ImPlot::GetPlotDrawList();
        ImPlotRect  limits = ImPlot::GetPlotLimits();
        int startIdx = std::max(0,    (int)limits.X.Min - 2);
        int endIdx   = std::min((int)candles.size() - 1, (int)limits.X.Max + 2);

        ImVec2 plotPos  = ImPlot::GetPlotPos();
        ImVec2 plotSize = ImPlot::GetPlotSize();
        float screenTop = plotPos.y;
        float screenBot = plotPos.y + plotSize.y;

        for (int i = startIdx; i <= endIdx; ++i) {
            const Candle& c = candles[i];

            // ── A. GAMBAR CANDLE TIPIS (WICK) ────────────────────────────────
            float x_px  = ImPlot::PlotToPixels(ImPlotPoint(i, c.close)).x;
            float yHigh = ImPlot::PlotToPixels(ImPlotPoint(i, c.high)).y;
            float yLow  = ImPlot::PlotToPixels(ImPlotPoint(i, c.low)).y;

            // ── REPLAY FILTER ────────────────────────────────────────────────
            bool isReplayCandle = (rp && i == rp->replayIndex && rp->progress < 0.99f);
            std::vector<FootprintLevel> fp_filtered;
            const std::vector<FootprintLevel>* fp_src = &c.footprint;
            if (isReplayCandle && !c.footprint.empty()) {
                float margin = tickSize * 0.6f;
                for (const auto& fp : c.footprint) {
                    if ((float)fp.price >= rp->low  - margin &&
                        (float)fp.price <= rp->high + margin) {
                        FootprintLevel scaled   = fp;
                        scaled.buyVol  *= (double)rp->progress;
                        scaled.sellVol *= (double)rp->progress;
                        fp_filtered.push_back(scaled);
                    }
                }
                fp_src = &fp_filtered;
            }

            // Live candle = candle terakhir ATAU sedang dianimasikan replay
            bool isLive = (i == (int)candles.size() - 1) || isReplayCandle;

            bool isBull   = c.close >= c.open;
            // Warna Wick diperhalus agar tidak nabrak warna Imbalance
            ImU32 wickCol = isBull ? IM_COL32(80, 200, 120, 180) : IM_COL32(200, 80, 80, 180);
            
            dl->AddLine(ImVec2(x_px, yHigh), ImVec2(x_px, yLow), wickCol, 2.0f);

            if (fp_src->empty()) continue;
            if (zoomTier >= 4) continue; // terlalu zoom out → skip boxes, cukup tampil wick

            // ── B. PERSIAPAN DATA & CLUSTERING (Logika Asli Anda Dipertahankan) ──
            float colW   = barW * 0.45f;
            float xLeft  = x_px - colW;
            float xMid   = x_px;
            float xRight = x_px + colW;

            float pocPrice   = 0.0f;
            double pocVolRaw = 0.0, totalBuy = 0.0, totalSell = 0.0;
            for (const auto& fp : *fp_src) {
                double rv = fp.buyVol + fp.sellVol;
                if (rv > pocVolRaw) { pocVolRaw = rv; pocPrice = (float)fp.price; }
                totalBuy += fp.buyVol; totalSell += fp.sellVol;
            }

            float rawRowH = 2.0f;
            if (fp_src->size() >= 2) {
                rawRowH = std::abs(ImPlot::PlotToPixels(ImPlotPoint(i, (*fp_src)[1].price)).y - 
                                   ImPlot::PlotToPixels(ImPlotPoint(i, (*fp_src)[0].price)).y);
            }

            struct Cluster { float yTop, yBot; double buyVol, sellVol, peakBuy, peakSell; bool hasPOC; };
            std::vector<Cluster> clusters;
            double aBuy = 0, aSell = 0, peakB = 0, peakS = 0, peakV = 0;
            bool cPoc = false;
            float anchorY = ImPlot::PlotToPixels(ImPlotPoint(i, (*fp_src)[0].price)).y;
            float firstY  = anchorY;

            for (size_t j = 0; j < fp_src->size(); ++j) {
                const auto& fp = (*fp_src)[j];
                float yRow = ImPlot::PlotToPixels(ImPlotPoint(i, fp.price)).y;

                aBuy  += fp.buyVol; aSell += fp.sellVol;
                double tv = fp.buyVol + fp.sellVol;
                if (tv > peakV) { peakV = tv; peakB = fp.buyVol; peakS = fp.sellVol; }
                if (fp.price == pocPrice) cPoc = true;

                bool isLast = (j == fp_src->size() - 1);
                if (std::abs(yRow - anchorY) >= MIN_BOX_H || isLast) {
                    float top = std::min(firstY, yRow) - (rawRowH * 0.5f);
                    float bot = std::max(firstY, yRow) + (rawRowH * 0.5f);
                    float minH = (zoomTier <= 1) ? 16.0f : MIN_BOX_H * 0.45f;
                    if (bot - top < minH) { float mid = (top + bot) * 0.5f; top = mid - minH * 0.5f; bot = mid + minH * 0.5f; }
                    
                    clusters.push_back({top, bot, aBuy, aSell, peakB, peakS, cPoc});
                    if (!isLast) {
                        aBuy = aSell = peakB = peakS = peakV = 0; cPoc = false;
                        anchorY = ImPlot::PlotToPixels(ImPlotPoint(i, (*fp_src)[j+1].price)).y;
                        firstY = anchorY;
                    }
                }
            }

            // ── C. RENDER CLUSTERS (VISUAL UPGRADE DI SINI) ───────────────────
            double maxClusterVol = 1.0;
            for (const auto& cl : clusters)
                if (cl.buyVol + cl.sellVol > maxClusterVol) maxClusterVol = cl.buyVol + cl.sellVol;

            for (const auto& cl : clusters) {
                if (cl.yBot < screenTop || cl.yTop > screenBot) continue;

                bool netBuy = (cl.buyVol >= cl.sellVol);
                // Deteksi Imbalance (Syarat: Minimal Vol tertentu & ratio dari settings)
                bool iBuy   = (cl.sellVol > 0.001) && (cl.buyVol  / cl.sellVol >= imbalanceRatio) && cl.buyVol > (maxClusterVol * 0.1);
                bool iSell  = (cl.buyVol  > 0.001) && (cl.sellVol / cl.buyVol  >= imbalanceRatio) && cl.sellVol > (maxClusterVol * 0.1);

                // 🔥 UPGRADE 1: Heatmap Opacity (dari settings)
                float ratio = (float)((cl.buyVol + cl.sellVol) / maxClusterVol);
                float bgAlpha = heatmapBaseOpacity * (0.30f + (0.70f * ratio)); 
                
                // Pisahkan warna Kiri (Merah) dan Kanan (Hijau)
                ImU32 bgColSell = ImGui::GetColorU32(ImVec4(fpColorSell.x, fpColorSell.y, fpColorSell.z, bgAlpha));
                ImU32 bgColBuy  = ImGui::GetColorU32(ImVec4(fpColorBuy.x, fpColorBuy.y, fpColorBuy.z, bgAlpha));

                // Gambar Kotak Kiri dan Kanan Terpisah
                dl->AddRectFilled(ImVec2(xLeft, cl.yTop + 1.0f), ImVec2(xMid, cl.yBot - 1.0f), bgColSell);
                dl->AddRectFilled(ImVec2(xMid, cl.yTop + 1.0f), ImVec2(xRight, cl.yBot - 1.0f), bgColBuy);

                // 🔥 UPGRADE 2: Outline POC & Imbalance
                if (showPOCHighlight && cl.hasPOC) {
                    dl->AddRect(ImVec2(xLeft, cl.yTop), ImVec2(xRight, cl.yBot), Col4(fpColorPOC), 0, 0, 2.0f);
                    dl->AddLine(ImVec2(xRight, cl.yBot), ImVec2(xRight + 15.0f, cl.yBot), Col4(fpColorPOC, 150), 2.0f);
                }
                else if (showImbalance && iBuy) {
                    dl->AddRect(ImVec2(xMid, cl.yTop), ImVec2(xRight, cl.yBot), Col4(fpColorImbalBuy), 0, 0, 2.0f);
                }
                else if (showImbalance && iSell) {
                    dl->AddRect(ImVec2(xLeft, cl.yTop), ImVec2(xMid, cl.yBot), Col4(fpColorImbalSell), 0, 0, 2.0f);
                } else {
                    // Border tipis pemisah antar level (optional)
                    dl->AddLine(ImVec2(xLeft, cl.yBot), ImVec2(xRight, cl.yBot), IM_COL32(255,255,255,20), 1.0f);
                }

                // 🔥 UPGRADE 3: Presisi Posisi Teks (PERFECT CENTER)
                float boxH = cl.yBot - cl.yTop;
                float ty   = (cl.yTop + cl.yBot) * 0.5f;

                if (showVolumeNumbers && zoomTier <= 1) {
                    char tS[16], tB[16];
                    FormatVolumeUSD(tS, sizeof(tS), cl.sellVol, isLive);
                    FormatVolumeUSD(tB, sizeof(tB), cl.buyVol,  isLive);
                    ImVec2 szS = ImGui::CalcTextSize(tS);
                    ImVec2 szB = ImGui::CalcTextSize(tB);
                    
                    float textY = ty - szS.y * 0.5f;
                    
                    // Warna teks: Kalau background gelap, teks putih. Kalau Imbalance, warna menyala.
                    ImU32 cS = iSell ? IM_COL32(255, 255, 255, 255) : IM_COL32(255, 200, 200, 255);
                    ImU32 cB = iBuy  ? IM_COL32(255, 255, 255, 255) : IM_COL32(200, 255, 200, 255);

                    // CENTER MATH: Taruh tepat di tengah-tengah area kiri (sell) dan area kanan (buy)
                    float txS = xLeft + (colW * 0.5f) - (szS.x * 0.5f);
                    float txB = xMid  + (colW * 0.5f) - (szB.x * 0.5f);

                    // Tampilkan hanya jika kotak cukup tinggi
                    if (boxH >= szS.y - 2.0f) {
                        dl->AddText(ImVec2(txS, textY), cS, tS);
                        dl->AddText(ImVec2(txB, textY), cB, tB);
                    }
                }
                else if (showVolumeNumbers) {
                    // TIER 2-4: Zoom out (Dominan Volume)
                    bool   domIsBuy = (cl.peakBuy >= cl.peakSell);
                    double domVol   = domIsBuy ? cl.peakBuy : cl.peakSell;
                    char   tD[16];
                    FormatVolumeUSD(tD, sizeof(tD), domVol, isLive);
                    ImVec2 szD  = ImGui::CalcTextSize(tD);
                    
                    // Text persis di tengah seluruh candle
                    float  textX = (xLeft + xRight) * 0.5f - szD.x * 0.5f;
                    float  textY = ty - szD.y * 0.5f;
                    ImU32  cD   = domIsBuy ? IM_COL32(180, 255, 180, 255) : IM_COL32(255, 180, 180, 255);

                    if (boxH >= szD.y - 2.0f) {
                        dl->AddText(ImVec2(textX, textY), cD, tD);
                    }
                }
            }

            // ── D. DELTA TOTAL ───────────────────────────────
            if (showDeltaLabel && zoomTier <= 3) {
                char td[24], tmp[22];
                double totalDelta = totalBuy - totalSell;
                FormatVolumeUSD(tmp, sizeof(tmp), std::abs(totalDelta), isLive);
                snprintf(td, sizeof(td), "%s%s", totalDelta >= 0 ? "+" : "-", tmp);
                
                ImVec2 szD = ImGui::CalcTextSize(td);
                ImU32  cD  = totalDelta >= 0 ? Col4(fpColorDeltaPos) : Col4(fpColorDeltaNeg);
                
                // Tambahkan background hitam transparan agar angka Delta selalu terbaca
                dl->AddRectFilled(ImVec2(x_px - szD.x * 0.5f - 2, yHigh - szD.y - 6.0f),
                                  ImVec2(x_px + szD.x * 0.5f + 2, yHigh - 2.0f), IM_COL32(0,0,0,150));
                float fsz = ImGui::GetFontSize() * fpFontScale;
                dl->AddText(ImGui::GetFont(), fsz, ImVec2(x_px - szD.x * 0.5f, yHigh - szD.y - 4.0f), cD, td);
            }
        }
    }

    // =========================================================================
    // MODE 4 — DrawVolumeProfile  (Session Volume Profile / VPOC)
    //
    // Render horizontal bars di sisi KANAN chart area, tumbuh ke kiri.
    // Aggregasi buy_vol + sell_vol dari SEMUA candle yang visible di viewport.
    //
    // Visual:
    //   ┌────────────────────────────────────┬──────────────┐
    //   │         Candle Chart (normal)      │ ██████ $67K  │  ← VP bars
    //   │                                    │ ████████VPOC │  ← gold outline
    //   │                                    │ ████  $66K   │
    //   └────────────────────────────────────┴──────────────┘
    //   ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ VPOC ─ ─ ─ ─  ← garis putus emas full-width
    //   ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░  ← VA shading amber 70%
    //
    // Data: c.footprint[].{price, buyVol, sellVol} — sama persis dari server.
    // Dipanggil SETELAH ImPlot::PlotX (di dalam BeginPlot ... EndPlot).
    //
    // Cara pakai di main.cpp:
    //   if (tab.renderMode == RENDER_VOLUME_PROFILE)
    //       tab.orderFlowRenderer.DrawVolumeProfile(tab.candles);
    // =========================================================================
    void DrawVolumeProfile(const std::vector<Candle>& candles) {
        if (candles.empty()) return;

        ImDrawList* dl      = ImPlot::GetPlotDrawList();
        ImPlotRect  limits  = ImPlot::GetPlotLimits();
        ImVec2      plotPos  = ImPlot::GetPlotPos();
        ImVec2      plotSize = ImPlot::GetPlotSize();

        int startIdx = std::max(0, (int)limits.X.Min - 1);
        int endIdx   = std::min((int)candles.size() - 1, (int)limits.X.Max + 1);

        // ── A. ROW SIZE otomatis berdasarkan tickSize ─────────────────────
        // BTC  (tick=1.0)     → rowSize = $100  → detail cukup, tidak terlalu ramai
        // ETH  (tick=0.01)    → rowSize = $1
        // XAU  (tick=0.1)     → rowSize = $10
        // Forex (tick=0.00001)→ rowSize = 0.001 pip → auto-cluster wajar
        float rowSize = tickSize * 100.0f;
        if (rowSize < 1e-7f) rowSize = 1e-7f;

        // ── B. ACCUMULATE VOLUME KE BINS ─────────────────────────────────
        // Key = rowIndex (integer), Value = {buyVol, sellVol}
        std::map<int, std::pair<double,double>> bins;

        for (int i = startIdx; i <= endIdx; ++i) {
            const Candle& c = candles[i];
            for (const auto& fp : c.footprint) {
                int rowIdx = (int)std::floor((double)fp.price / (double)rowSize);
                bins[rowIdx].first  += fp.buyVol;
                bins[rowIdx].second += fp.sellVol;
            }
        }
        if (bins.empty()) return;

        // ── C. CARI VPOC + VALUE AREA 70% ────────────────────────────────
        double totalVol  = 0.0;
        double maxBinVol = 0.0;
        int    vpocIdx   = bins.begin()->first;

        for (auto& [idx, bv] : bins) {
            double tv = bv.first + bv.second;
            totalVol += tv;
            if (tv > maxBinVol) { maxBinVol = tv; vpocIdx = idx; }
        }

        // Sort bins descending by volume → kumpulkan sampai 70%
        std::vector<std::pair<double,int>> sortedBins;
        sortedBins.reserve(bins.size());
        for (auto& [idx, bv] : bins)
            sortedBins.push_back({ bv.first + bv.second, idx });
        std::sort(sortedBins.begin(), sortedBins.end(),
                  [](const auto& a, const auto& b){ return a.first > b.first; });

        double cumul    = 0.0;
        int    vaLoIdx  = vpocIdx;
        int    vaHiIdx  = vpocIdx;
        for (auto& [vol, idx] : sortedBins) {
            cumul += vol;
            if (idx < vaLoIdx) vaLoIdx = idx;
            if (idx > vaHiIdx) vaHiIdx = idx;
            if (cumul / totalVol >= 0.70) break;
        }

        // ── D. LAYOUT: profile panel di sisi kanan chart ─────────────────
        // Lebar panel = 18% lebar chart, max 130px, min 40px
        float profileW  = std::min(std::max(plotSize.x * 0.18f, 40.0f), 130.0f);
        float rightEdge = plotPos.x + plotSize.x;   // tepi kanan chart
        float profileX  = rightEdge - profileW;     // tepi kiri panel VP

        // ── E. VALUE AREA SHADING (amber transparan, full plot width) ─────
        if (showVAShading) {
            float vaLoPrice = (float)(vaLoIdx * rowSize);
            float vaHiPrice = (float)((vaHiIdx + 1) * rowSize);
            float yVaTop = ImPlot::PlotToPixels(ImPlotPoint(0, (double)vaHiPrice)).y;
            float yVaBot = ImPlot::PlotToPixels(ImPlotPoint(0, (double)vaLoPrice)).y;
            yVaTop = std::max(yVaTop, plotPos.y);
            yVaBot = std::min(yVaBot, plotPos.y + plotSize.y);
            if (yVaBot > yVaTop)
                dl->AddRectFilled(
                    ImVec2(plotPos.x, yVaTop),
                    ImVec2(rightEdge, yVaBot),
                    IM_COL32(239, 159, 39, 18));  // amber sangat transparan
        }  // end showVAShading

        // ── F. RENDER BARS (stacked sell|buy, tumbuh dari rightEdge ke kiri) ─
        // Live bin = bin yang mengandung close price candle terakhir
        // → label-nya tampil desimal (jedak jedug ikut tick)
        int liveRowIdx = INT_MIN;
        if (!candles.empty()) {
            const Candle& lastC = candles.back();
            liveRowIdx = (int)std::floor((double)lastC.close / (double)rowSize);
        }

        for (auto& [rowIdx, bv] : bins) {
            double buyV   = bv.first;
            double sellV  = bv.second;
            double totalV = buyV + sellV;
            if (totalV < 0.001) continue;

            // Bin yang mengandung harga close live candle → tampil desimal
            bool isLiveBin = (rowIdx == liveRowIdx);

            float rowPriceLo = (float)(rowIdx       * rowSize);
            float rowPriceHi = (float)((rowIdx + 1) * rowSize);

            float yTop = ImPlot::PlotToPixels(ImPlotPoint(0, (double)rowPriceHi)).y;
            float yBot = ImPlot::PlotToPixels(ImPlotPoint(0, (double)rowPriceLo)).y;

            // Clip ke area layar
            if (yBot < plotPos.y || yTop > plotPos.y + plotSize.y) continue;
            yTop = std::max(yTop, plotPos.y);
            yBot = std::min(yBot, plotPos.y + plotSize.y);
            float rowH = yBot - yTop;
            if (rowH < 1.0f) continue;

            float pad = (rowH > 3.0f) ? 0.7f : 0.0f;

            bool isVPOC = (rowIdx == vpocIdx);
            bool isInVA = (rowIdx >= vaLoIdx && rowIdx <= vaHiIdx);

            // Alpha: volume lebih besar → lebih solid
            float ratio = (float)(totalV / maxBinVol);
            float alpha = isVPOC ? 1.0f : (0.40f + 0.55f * ratio);

            // Panjang bar total proporsional ke maxBinVol
            float totalBarW = ratio * profileW;
            float sellBarW  = (float)(sellV / totalV) * totalBarW;
            float buyBarW   = totalBarW - sellBarW;

            // Sell (merah/coral) — kiri dari rightEdge sampai split point
            // Buy  (hijau/teal)  — kanan, mepet ke rightEdge
            // Layout: [  sell  |  buy  ]← rightEdge
            float splitX = rightEdge - buyBarW;
            float barX0  = rightEdge - totalBarW;

            // Warna: VA lebih cerah, non-VA lebih redup
            ImU32 cSell, cBuy;
            if (isVPOC) {
                cSell = IM_COL32(220, 80,  80,  255);
                cBuy  = IM_COL32(50,  210, 100, 255);
            } else if (isInVA) {
                cSell = ImGui::GetColorU32(ImVec4(0.82f, 0.22f, 0.22f, alpha));
                cBuy  = ImGui::GetColorU32(ImVec4(0.18f, 0.80f, 0.38f, alpha));
            } else {
                cSell = ImGui::GetColorU32(ImVec4(0.65f, 0.20f, 0.20f, alpha * 0.75f));
                cBuy  = ImGui::GetColorU32(ImVec4(0.15f, 0.62f, 0.30f, alpha * 0.75f));
            }

            // Gambar sell bar (kiri)
            if (sellBarW > 0.5f)
                dl->AddRectFilled(
                    ImVec2(barX0,  yTop + pad),
                    ImVec2(splitX, yBot - pad), cSell);

            // Gambar buy bar (kanan)
            if (buyBarW > 0.5f)
                dl->AddRectFilled(
                    ImVec2(splitX,    yTop + pad),
                    ImVec2(rightEdge, yBot - pad), cBuy);

            // VPOC: outline emas + volume label
            if (isVPOC) {
                dl->AddRect(
                    ImVec2(barX0 - 1.0f, yTop),
                    ImVec2(rightEdge + 1.0f, yBot),
                    IM_COL32(255, 215, 0, 255), 0, 0, 1.5f);
            }

            // Label angka total volume (hanya kalau baris cukup tinggi)
            if (rowH >= 10.0f && totalBarW >= 28.0f) {
                char tVol[16];
                FormatVolumeUSD(tVol, sizeof(tVol), totalV, isLiveBin);
                ImVec2 szV = ImGui::CalcTextSize(tVol);
                if (szV.x < totalBarW - 4.0f) {
                    float ty = (yTop + yBot) * 0.5f - szV.y * 0.5f;
                    // Shadow
                    dl->AddText(ImVec2(barX0 + 3.0f + 1, ty + 1),
                                IM_COL32(0, 0, 0, 160), tVol);
                    ImU32 cTxt = isVPOC
                        ? IM_COL32(255, 240, 160, 255)
                        : IM_COL32(220, 220, 220, 200);
                    dl->AddText(ImVec2(barX0 + 3.0f, ty), cTxt, tVol);
                }
            }

            // Garis pemisah antar row (sangat tipis)
            dl->AddLine(
                ImVec2(barX0, yBot),
                ImVec2(rightEdge, yBot),
                IM_COL32(255, 255, 255, 10), 1.0f);
        }

        // ── G. VPOC HORIZONTAL LINE — full chart width ────────────────────
        // Garis putus-putus emas memanjang ke seluruh lebar chart
        if (showVPOCLine) {
            float vpocMid = (float)(vpocIdx * rowSize + rowSize * 0.5f);
            float yVpoc   = ImPlot::PlotToPixels(ImPlotPoint(0, (double)vpocMid)).y;
            if (yVpoc >= plotPos.y && yVpoc <= plotPos.y + plotSize.y) {
                // Dashed line manual (ImDrawList tidak support native dashed)
                const float DASH = 6.0f, GAP = 4.0f;
                float x = plotPos.x;
                while (x < rightEdge - profileW) {  // hanya di area candle, bukan panel VP
                    float x2 = std::min(x + DASH, rightEdge - profileW);
                    dl->AddLine(ImVec2(x, yVpoc), ImVec2(x2, yVpoc),
                                IM_COL32(255, 215, 0, 160), 1.5f);
                    x += DASH + GAP;
                }

                // Label "VPOC $xxx" di tepi kiri panel VP
                char vpocLabel[32];
                snprintf(vpocLabel, sizeof(vpocLabel), "VPOC $%.0f", (double)vpocMid);
                ImVec2 szL = ImGui::CalcTextSize(vpocLabel);
                float  lx  = profileX + 3.0f;
                float  ly  = yVpoc - szL.y - 2.0f;
                dl->AddRectFilled(
                    ImVec2(lx - 2, ly - 1),
                    ImVec2(lx + szL.x + 2, ly + szL.y + 1),
                    IM_COL32(0, 0, 0, 190));
                dl->AddText(ImVec2(lx, ly), IM_COL32(255, 215, 0, 255), vpocLabel);
            }
        }  // end showVPOCLine

        // ── H. VAH / VAL LEVEL LINES + LABELS ────────────────────────────
        if (showVPLabels) {
            auto drawVALevel = [&](float price, const char* label) {
                float y = ImPlot::PlotToPixels(ImPlotPoint(0, (double)price)).y;
                if (y < plotPos.y || y > plotPos.y + plotSize.y) return;
                // Garis tipis di panel VP saja
                dl->AddLine(
                    ImVec2(profileX, y), ImVec2(rightEdge, y),
                    IM_COL32(239, 159, 39, 110), 1.0f);
                // Label kecil
                ImVec2 sz = ImGui::CalcTextSize(label);
                dl->AddText(
                    ImVec2(profileX + 3.0f, y - sz.y - 1.0f),
                    IM_COL32(239, 159, 39, 210), label);
            };
            drawVALevel((float)((vaHiIdx + 1) * rowSize), "VAH");
            drawVALevel((float)(vaLoIdx        * rowSize), "VAL");
        }  // end showVPLabels
    }

    // =========================================================================
    // NAKED VPOC — UpdateNakedVPOCs
    // =========================================================================
    // Dipanggil setelah candle history selesai di-load (rebuildFullFromDB).
    //
    // Cara kerja (analogi AGI):
    //   1. OBSERVE  → scan semua candle, bagi per session (tiap N bar = 1 session)
    //   2. SIMULATE → tiap session cari VPOC-nya
    //   3. CHECK    → setelah session itu, apakah harga pernah revisit VPOC?
    //   4. DECIDE   → kalau BELUM revisit = "lampu sein masih nyala" = simpan
    //                 kalau SUDAH revisit = "lampu sein mati" = buang
    //   5. RESULT   → nakedVPOCs berisi hanya level yang masih relevan
    // =========================================================================
    void UpdateNakedVPOCs(const std::vector<Candle>& candles) {
        nakedVPOCs.clear();
        if (candles.size() < 2) return;

        float rowSize = tickSize * 100.0f;
        if (rowSize < 1e-7f) rowSize = 1e-7f;

        int totalCandles = (int)candles.size();
        // Batasi: scan maksimal 30 session ke belakang
        // (terlalu banyak = noise, terlalu sedikit = miss level penting)
        int maxSessions = 30;
        int startSession = std::max(0, totalCandles / nakedSessionBars - maxSessions);

        for (int s = startSession; s < totalCandles / nakedSessionBars; ++s) {
            int sesStart = s * nakedSessionBars;
            int sesEnd   = std::min(sesStart + nakedSessionBars - 1, totalCandles - 2);
            // Candle terakhir (live) tidak ikut session — bisa berubah

            // ── A. Hitung VPOC session ini ────────────────────────────────
            std::map<int, double> bins;
            for (int i = sesStart; i <= sesEnd; ++i) {
                const Candle& c = candles[i];
                for (const auto& fp : c.footprint) {
                    int rowIdx = (int)std::floor((double)fp.price / (double)rowSize);
                    bins[rowIdx] += fp.buyVol + fp.sellVol;
                }
            }
            if (bins.empty()) continue;

            // Cari bin dengan volume terbesar = VPOC
            int    vpocIdx = bins.begin()->first;
            double maxVol  = 0.0;
            for (auto& [idx, vol] : bins) {
                if (vol > maxVol) { maxVol = vol; vpocIdx = idx; }
            }
            float vpocPrice = (float)(vpocIdx * rowSize + rowSize * 0.5f);

            // ── B. Cek apakah harga revisit VPOC setelah session berakhir ─
            // "Mobil lain" (harga) yang lewat level ini = VPOC sudah dikunjungi
            // Cukup 1 candle yang low ≤ vpocPrice ≤ high = revisit
            bool revisited = false;
            for (int i = sesEnd + 1; i < totalCandles; ++i) {
                const Candle& c = candles[i];
                if (c.low <= vpocPrice && c.high >= vpocPrice) {
                    revisited = true;
                    break;
                }
            }

            // ── C. Kalau belum revisit = Naked VPOC ──────────────────────
            if (!revisited) {
                float lastPrice = candles[totalCandles - 1].close;
                nakedVPOCs.push_back({
                    vpocPrice,
                    sesEnd,
                    vpocPrice > lastPrice  // true = resistance (di atas), false = support
                });
            }
        }
    }

    // =========================================================================
    // NAKED VPOC — DrawNakedVPOCs
    // =========================================================================
    // Render semua Naked VPOC sebagai garis putus-putus horizontal.
    //
    // Visual language (seperti lampu sein):
    //   ── Resistance (di atas harga) → merah/oranye redup
    //      "harga mau naik, ada tembok di sini"
    //   ── Support (di bawah harga)   → hijau/biru redup
    //      "harga mau turun, ada lantai di sini"
    //   ── Label "nVPOC $xxx" di tepi kiri → trader langsung tau tanpa tanya
    //   ── Makin dekat ke harga = makin terang (prioritas visual)
    // =========================================================================
    void DrawNakedVPOCs(const std::vector<Candle>& candles) {
        if (!showNakedVPOC) return;
        if (nakedVPOCs.empty()) return;
        if (candles.empty()) return;

        ImDrawList* dl       = ImPlot::GetPlotDrawList();
        ImPlotRect  limits   = ImPlot::GetPlotLimits();
        ImVec2      plotPos  = ImPlot::GetPlotPos();
        ImVec2      plotSize = ImPlot::GetPlotSize();

        float lastPrice = candles.back().close;
        float priceRange = (float)(limits.Y.Max - limits.Y.Min);
        if (priceRange <= 0) return;

        for (auto& nv : nakedVPOCs) {
            // Skip kalau sudah di-dismiss oleh trader ("lampu sein sudah dimatikan")
            if (nv.dismissed) continue;
            // Skip kalau diluar visible range
            if (nv.price < (float)limits.Y.Min || nv.price > (float)limits.Y.Max) continue;

            float yPx = ImPlot::PlotToPixels(ImPlotPoint(0, (double)nv.price)).y;
            if (yPx < plotPos.y || yPx > plotPos.y + plotSize.y) continue;

            // ── Makin dekat ke harga = makin terang (alpha makin tinggi) ──
            float dist     = std::abs(nv.price - lastPrice);
            float maxDist  = priceRange * 0.5f;
            float proximity = 1.0f - std::min(dist / maxDist, 1.0f); // 0.0~1.0
            int   alpha    = (int)(120 + proximity * 135);            // 120~255

            // ── Warna: resistance = merah, support = hijau ────────────────
            ImU32 lineColor, textColor;
            if (nv.isAbove) {
                // Resistance — "tembok di atas"
                lineColor = IM_COL32(255, 80,  80,  alpha);
                textColor = IM_COL32(255, 120, 120, 255);
            } else {
                // Support — "lantai di bawah"
                lineColor = IM_COL32(80,  220, 120, alpha);
                textColor = IM_COL32(120, 255, 150, 255);
            }

            // ── Garis putus-putus horizontal full-width ───────────────────
            const float DASH = 8.0f, GAP = 5.0f;
            float x = plotPos.x;
            while (x < plotPos.x + plotSize.x) {
                float x2 = std::min(x + DASH, plotPos.x + plotSize.x);
                dl->AddLine(ImVec2(x, yPx), ImVec2(x2, yPx), lineColor, 1.2f);
                x += DASH + GAP;
            }

            // ── Label "nVPOC $xxx" di tepi kiri chart ─────────────────────
            // "n" prefix = Naked, trader profesional langsung paham
            char label[32];
            if (tickSize < 0.001f)
                snprintf(label, sizeof(label), "nVPOC %.5f", nv.price);
            else if (tickSize < 1.0f)
                snprintf(label, sizeof(label), "nVPOC %.2f", nv.price);
            else
                snprintf(label, sizeof(label), "nVPOC $%.0f", nv.price);

            ImVec2 sz  = ImGui::CalcTextSize(label);
            float  lx  = plotPos.x + 4.0f;
            float  ly  = yPx - sz.y - 2.0f;

            // ── Label interaktif — klik = dismiss ("matikan lampu sein") ──
            // Trader klik label nVPOC → "aku sudah tau level ini, sembunyikan"
            // Pakai ImGui InvisibleButton supaya bisa detect klik
            ImGui::SetCursorScreenPos(ImVec2(lx - 2, ly - 1));
            std::string btnId = std::string("##nvpoc_") + label;
            bool clicked = ImGui::InvisibleButton(btnId.c_str(),
                ImVec2(sz.x + 8.0f, sz.y + 2.0f));

            // Hover = kasih tanda ke trader bahwa ini bisa diklik
            bool hovered = ImGui::IsItemHovered();
            ImU32 bgColor = hovered
                ? IM_COL32(60, 60, 60, 220)  // lebih terang saat hover
                : IM_COL32(0,  0,  0,  180);

            // Background label
            dl->AddRectFilled(
                ImVec2(lx - 2, ly - 1),
                ImVec2(lx + sz.x + 6, ly + sz.y + 1),
                bgColor);

            // Border tipis saat hover — sinyal "bisa diklik"
            if (hovered) {
                dl->AddRect(
                    ImVec2(lx - 2, ly - 1),
                    ImVec2(lx + sz.x + 6, ly + sz.y + 1),
                    textColor, 2.0f, 0, 1.0f);
            }

            dl->AddText(ImVec2(lx, ly), textColor, label);

            // Tooltip saat hover
            if (hovered)
                ImGui::SetTooltip("Klik untuk sembunyikan level ini\n"
                                  "(Naked VPOC sudah kamu catat)");

            // Dismiss kalau diklik — "lampu sein dimatikan trader"
            if (clicked) nv.dismissed = true;

            // ── Segitiga kecil di tepi kiri (arah: atas=resistance, bawah=support) ─
            // Seperti panah arah lampu sein — trader tau ini resistance atau support
            float tx = plotPos.x + sz.x + 10.0f;
            float ty = yPx;
            if (nv.isAbove) {
                // Segitiga ke atas = resistance
                dl->AddTriangleFilled(
                    ImVec2(tx,        ty - 5.0f),
                    ImVec2(tx - 4.0f, ty + 3.0f),
                    ImVec2(tx + 4.0f, ty + 3.0f),
                    lineColor);
            } else {
                // Segitiga ke bawah = support
                dl->AddTriangleFilled(
                    ImVec2(tx,        ty + 5.0f),
                    ImVec2(tx - 4.0f, ty - 3.0f),
                    ImVec2(tx + 4.0f, ty - 3.0f),
                    lineColor);
            }
        }
    }
};