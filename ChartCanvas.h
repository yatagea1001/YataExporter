#pragma once
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include "Candle.h"  // pastikan Candle punya: double open,high,low,close; std::string datetime; double time;

/// ===============================================================
/// 🧭 ChartCanvas — World-space mapping (time <-> index, price <-> pixel)
/// ===============================================================
class ChartCanvas {
public:
    ChartCanvas() {}
    ~ChartCanvas() {}
    // -----------------------------------------------------------------------
    // [BARU] FUNGSI STABILISASI REPLAY & MASA DEPAN
    // -----------------------------------------------------------------------
    static double GetStableX(double targetTime, const std::vector<Candle>& candles, double tfSeconds) {
        if (candles.empty()) return 0.0;

        const Candle& lastCandle = candles.back();
        int lastIndex = (int)candles.size() - 1;

        // KASUS 1: DATA ADA (Masa Lalu / Sekarang) -> Gunakan Index Asli
        if (targetTime <= lastCandle.time) {
            // Binary Search agar cepat & akurat
            auto it = std::lower_bound(candles.begin(), candles.end(), targetTime, 
                [](const Candle& c, double t) { return c.time < t; });
            
            int idx = (int)std::distance(candles.begin(), it);

            // Jika waktu tepat cocok dengan candle
            if (idx < (int)candles.size() && candles[idx].time == targetTime) {
                return (double)idx;
            }

            // Interpolasi antar candle — PAKAI GAP NYATA (bukan tfSeconds)
            // Ini menangani market gap (weekend, holiday, session off)
            if (idx > 0 && idx < (int)candles.size()) {
                double prevTime = (double)candles[idx - 1].time;
                double nextTime = (double)candles[idx].time;
                double gap = nextTime - prevTime;
                if (gap > 0) {
                    double frac = (targetTime - prevTime) / gap;
                    return (double)(idx - 1) + frac;
                }
            }

            // Edge: idx == 0 (sebelum candle pertama)
            if (idx == 0) {
                double firstTime = (double)candles[0].time;
                if (lastIndex > 0) {
                    double secondTime = (double)candles[1].time;
                    double gap = secondTime - firstTime;
                    if (gap > 0) {
                        double frac = (targetTime - firstTime) / gap;
                        return frac; // bisa negatif (sebelum candle pertama)
                    }
                }
                return 0.0;
            }
            // Edge: idx >= size (tidak seharusnya terjadi di KASUS 1, tapi safety)
            if (idx >= (int)candles.size()) return (double)lastIndex;
            return (double)idx;
        }

        // KASUS 2: DATA KOSONG (Masa Depan) -> Proyeksi Relatif
        // Gunakan RATA-RATA GAP dari N candle terakhir (bukan tfSeconds statis)
        // Ini memberikan estimasi yang lebih akurat saat ada gap market
        else {
            double timeDiff = targetTime - (double)lastCandle.time;
            
            // Hitung rata-rata gap nyata dari candle terakhir
            double avgGap = tfSeconds; // fallback default
            int lookback = std::min(10, lastIndex); // sample 10 candle terakhir
            if (lookback >= 1) {
                double totalGap = (double)candles[lastIndex].time 
                               - (double)candles[lastIndex - lookback].time;
                avgGap = totalGap / (double)lookback;
            }
            // Safety: jangan biarkan avgGap absurd
            if (avgGap <= 0 || avgGap > tfSeconds * 100) avgGap = tfSeconds;
            
            double projectedBars = timeDiff / avgGap;
            return (double)lastIndex + projectedBars;
        }
    }
    // Di dalam ChartCanvas.h, di dalam class ChartCanvas { ... }
    // 💡 FUNGSI HELPER BARU 💡
    // Mengembalikan durasi candle (dalam detik) berdasarkan nama Timeframe
    static double GetTimePerCandle(const std::string& tf) {
        if (tf == "M1") return 60.0;
        if (tf == "M5") return 300.0;
        if (tf == "M15") return 900.0;
        if (tf == "M30") return 1800.0;
        if (tf == "H1") return 3600.0;
        if (tf == "H4") return 14400.0;
        if (tf == "D1") return 86400.0;
        // Default (jika tidak ketemu, kembalikan 1 menit)
        return 60.0;
    }

    // cari index candle terdekat berdasarkan waktu
    static int FindCandleIndexByTime(const std::vector<Candle>& candles, double t) {
        if (candles.empty()) return -1;
        int lo = 0, hi = (int)candles.size() - 1;
        if (t <= candles.front().time) return 0;
        if (t >= candles.back().time) return hi;

        while (lo <= hi) {
            int mid = (lo + hi) / 2;
            double tm = candles[mid].time;
            if (tm == t) return mid;
            if (tm < t) lo = mid + 1;
            else hi = mid - 1;
        }

        if (lo >= (int)candles.size()) return (int)candles.size() - 1;
        if (hi < 0) return 0;
        double dlo = fabs(candles[lo].time - t);
        double dhi = fabs(candles[hi].time - t);
        return (dhi <= dlo) ? hi : lo;
    }

    // konversi waktu (datetime string) ke double
    static double TimeStrToDouble(const std::string& datetime) {
        try { return std::stod(datetime); }
        catch (...) { return 0.0; }
    }

    // konversi waktu-harga ke posisi pixel ImPlot
    static ImVec2 ToScreen(const std::vector<Candle>& candles, double time, double price) {
        int idx = FindCandleIndexByTime(candles, time);
        return ImPlot::PlotToPixels(ImPlotPoint((double)idx, price));
    }

    // ambil waktu dari posisi index candle (integer)
    static double IndexToTime(const std::vector<Candle>& candles, int idx) {
        if (candles.empty()) return 0.0;
        idx = std::clamp(idx, 0, (int)candles.size() - 1);
        return candles[idx].time;
    }

    // =========================================================
    // GetStableTime: INVERSE dari GetStableX
    // Konversi plot X coordinate (float index) → epoch time
    //
    // KASUS 1: Index di dalam range candle → cari candle[idx_floor],
    //          interpolasi waktu antar candle
    // KASUS 2: Index di luar range (masa depan) → proyeksi linier
    // =========================================================
    static double GetStableTime(double plotX, const std::vector<Candle>& candles, double tfSeconds) {
        if (candles.empty()) return 0.0;

        int floorIdx = (int)std::floor(plotX);
        int lastIndex = (int)candles.size() - 1;

        // KASUS 1: Index valid (di dalam range data) → interpolasi pakai GAP NYATA
        if (floorIdx >= 0 && floorIdx < lastIndex) {
            double timeAtFloor = (double)candles[floorIdx].time;
            double timeAtNext  = (double)candles[floorIdx + 1].time;
            double fraction = plotX - (double)floorIdx; // 0.0 .. 1.0
            // Pakai gap nyata antar candle, bukan tfSeconds statis
            return timeAtFloor + fraction * (timeAtNext - timeAtFloor);
        }

        // KASUS 2: Masa depan (index >= last candle) → proyeksi pakai rata-rata gap
        if (floorIdx >= lastIndex) {
            // Hitung rata-rata gap nyata dari candle terakhir
            double avgGap = tfSeconds;
            int lookback = std::min(10, lastIndex);
            if (lookback >= 1) {
                double totalGap = (double)candles[lastIndex].time 
                               - (double)candles[lastIndex - lookback].time;
                avgGap = totalGap / (double)lookback;
            }
            if (avgGap <= 0 || avgGap > tfSeconds * 100) avgGap = tfSeconds;
            
            return (double)candles[lastIndex].time + (plotX - (double)lastIndex) * avgGap;
        }

        // KASUS 3: Sebelum candle pertama (floorIdx < 0)
        if (lastIndex > 0) {
            double timeAtZero = (double)candles[0].time;
            double timeAtOne  = (double)candles[1].time;
            double firstGap  = timeAtOne - timeAtZero;
            if (firstGap > 0) {
                return timeAtZero + (double)plotX * firstGap;
            }
        }
        return (double)candles[0].time;
    }

    // buat garis berdasarkan waktu & harga (langsung render)
    static void DrawLineByTime(ImDrawList* drawList, const std::vector<Candle>& candles,
                               double time0, double price0, double time1, double price1,
                               ImU32 color, float thickness = 1.5f) {
        int i0 = FindCandleIndexByTime(candles, time0);
        int i1 = FindCandleIndexByTime(candles, time1);
        ImVec2 p0 = ImPlot::PlotToPixels(ImPlotPoint((double)i0, price0));
        ImVec2 p1 = ImPlot::PlotToPixels(ImPlotPoint((double)i1, price1));
        drawList->AddLine(p0, p1, color, thickness);
    }

    // buat kotak OB/FVG berdasarkan waktu & harga
    static void DrawRectByTime(ImDrawList* drawList, const std::vector<Candle>& candles,
                               double time0, double price0, double time1, double price1,
                               ImU32 color, float thickness = 1.2f, bool filled = false) {
        int i0 = FindCandleIndexByTime(candles, time0);
        int i1 = FindCandleIndexByTime(candles, time1);
        ImVec2 p0 = ImPlot::PlotToPixels(ImPlotPoint((double)i0, price0));
        ImVec2 p1 = ImPlot::PlotToPixels(ImPlotPoint((double)i1, price1));
        if (filled)
            drawList->AddRectFilled(p0, p1, color, 3.0f);
        else
            drawList->AddRect(p0, p1, color, 3.0f, 0, thickness);
    }
};
