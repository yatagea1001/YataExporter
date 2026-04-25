#pragma once
#include <vector>
#include <string>
#include <ctime>
#include <cstdio>
#include <algorithm>
#include <cmath>
#include "Candle.h"
#include "implot.h"

// =========================================================
// ChartAxisTicks v2.1 — TradingView-Style Mixed Labels
//
// PRINSIP: ImPlot handle posisi tick (nice numbers, no overlap).
//          Kita handle FORMAT per-tick berdasarkan boundary.
//
//   ImPlot generate tick:  22440, 22460, 22480, 22500, ...
//   Formatter cek candle di index tersebut:
//     - Candle di batas BULAN baru → "Mar"
//     - Candle di batas HARI baru   → "09 Apr"
//     - Candle biasa               → "14:30"
//
//   Hasilnya MIX LABEL seperti TradingView:
//     "14:30"  "09 Apr"  "14:30"  "14:30"  "10 Apr"  "14:30"
//
// ZOOM LEVEL menentukan mana boundary yang RELEVAN:
//
//   FMT_INTRADAY   (span < 1 hari):
//     batas bulan → "Mar"
//     batas hari   → "09 Apr"
//     biasa        → "14:30"
//
//   FMT_DAYS       (span 1-30 hari):
//     batas bulan → "Mar"
//     batas hari   → "09 Apr"
//     biasa        → "14:30"
//
//   FMT_MONTHS     (span 30-365 hari):
//     batas tahun  → "2026"
//     batas bulan  → "Mar"
//     biasa        → "09 Apr"
//
//   FMT_YEARS      (span > 365 hari):
//     batas tahun  → "2026"
//     biasa        → "Mar"
//
// LOGIKA BOUNDARY DETECTION:
//   Bandingkan candle[idx] vs candle[idx-1].
//   Jika tahun beda → YEAR boundary
//   Jika bulan beda → MONTH boundary
//   Jika tanggal beda → DAY boundary
//   Jika jam beda → HOUR boundary (tidak ditampilkan, fallthrough ke biasa)
//   Jika sama → REGULAR (biasa)
// =========================================================

enum TimeFormatLevel {
    FMT_INTRADAY = 0,   // < 1 hari   → regular: "14:30", day: "09 Apr"
    FMT_DAYS     = 1,   // 1-30 hari  → regular: "14:30", day: "09 Apr"
    FMT_MONTHS   = 2,   // 30-365 hr  → regular: "09 Apr", month: "Mar"
    FMT_YEARS    = 3,   // > 365 hari → regular: "Mar", year: "2026"
};

enum BoundaryType {
    BOUND_REGULAR = 0,
    BOUND_HOUR    = 1,
    BOUND_DAY     = 2,
    BOUND_MONTH   = 3,
    BOUND_YEAR    = 4,
};

// --- Context global untuk formatter callback ---
// Di-set setiap frame sebelum BeginPlot
static struct FormatterContext {
    const std::vector<Candle>* candles = nullptr;
    int  count    = 0;
    TimeFormatLevel fmtLevel = FMT_INTRADAY;
} g_fmtCtx;

// =========================================================
// DetectBoundary: bandingkan candle[idx] vs candle[idx-1]
// untuk menentukan tipe boundary yang candle idx wakili.
//
// Bukan berdasarkan "berapa jam gap", tapi berdasarkan
// komponen kalender yang BERUBAH (tm_year, tm_mon, dll).
// =========================================================
static BoundaryType DetectBoundary(const std::vector<Candle>& candles, int idx, int count) {
    if (idx <= 0 || idx >= count) return BOUND_REGULAR;

    long long prevTs = (long long)candles[idx - 1].time;
    long long currTs = (long long)candles[idx].time;
    if (prevTs <= 0 || currTs <= 0) return BOUND_REGULAR;

    time_t pt = (time_t)prevTs;
    time_t ct = (time_t)currTs;
    struct tm prevTm, currTm;

    #ifdef _WIN32
        localtime_s(&prevTm, &pt);
        localtime_s(&currTm, &ct);
    #else
        localtime_r(&pt, &prevTm);
        localtime_r(&ct, &currTm);
    #endif

    if (currTm.tm_year != prevTm.tm_year) return BOUND_YEAR;
    if (currTm.tm_mon  != prevTm.tm_mon)  return BOUND_MONTH;
    if (currTm.tm_mday != prevTm.tm_mday) return BOUND_DAY;
    if (currTm.tm_hour != prevTm.tm_hour) return BOUND_HOUR;
    return BOUND_REGULAR;
}

// =========================================================
// CandleTimeFormatter: callback ImPlot::SetupAxisFormat
//
// Dipanggil ImPlot untuk SETIAP tick.
//   value = candle index (double)
//   buff  = output buffer (ImPlot provided, biasanya 128 bytes)
//   size  = buffer size
//
// RETURN: jumlah karakter ditulis (int)
//
// ALUR:
//   1. Round value → idx (candle index)
//   2. DetectBoundary(idx) → tipe boundary
//   3. Berdasarkan (fmtLevel, boundary) → pilih format
//   4. strftime → buff → return strlen
// =========================================================
static int CandleTimeFormatter(double value, char* buff, int size, void* /*data*/) {
    if (!g_fmtCtx.candles || g_fmtCtx.count <= 0) {
        buff[0] = '\0';
        return 0;
    }

    int idx = (int)std::round(value);
    if (idx < 0 || idx >= g_fmtCtx.count) {
        buff[0] = '\0';
        return 0;
    }

    // --- Deteksi boundary candle ini ---
    BoundaryType boundary = DetectBoundary(*g_fmtCtx.candles, idx, g_fmtCtx.count);

    // --- Parse waktu candle ini ---
    time_t t = (time_t)(*g_fmtCtx.candles)[idx].time;
    struct tm tm;
    #ifdef _WIN32
        localtime_s(&tm, &t);
    #else
        localtime_r(&t, &tm);
    #endif

    // --- Pilih format berdasarkan (zoom level + boundary type) ---
    const char* fmt = "%H:%M"; // default

    switch (g_fmtCtx.fmtLevel) {

        // ============================================
        // FMT_INTRADAY: < 1 hari visible
        //   Boundary bulan → "Mar" (jarang terjadi di sini)
        //   Boundary hari   → "09 Apr"
        //   Biasa / Hour    → "14:30"
        // ============================================
        case FMT_INTRADAY:
            if      (boundary == BOUND_MONTH) fmt = "%b";          // "Mar"
            else if (boundary == BOUND_DAY)   fmt = "%d %b";       // "09 Apr"
            else                              fmt = "%H:%M";       // "14:30"
            break;

        // ============================================
        // FMT_DAYS: 1-30 hari visible
        //   Boundary bulan → "Mar"
        //   Boundary hari   → "09 Apr"
        //   Biasa           → "14:30"
        //
        // Contoh output axis:
        //   "14:30"  "14:30"  "09 Apr"  "14:30"  "14:30"  "10 Apr"  "Mar"  "14:30"
        // ============================================
        case FMT_DAYS:
            if      (boundary == BOUND_MONTH) fmt = "%b";          // "Mar"
            else if (boundary == BOUND_DAY)   fmt = "%d %b";       // "09 Apr"
            else                              fmt = "%H:%M";       // "14:30"
            break;

        // ============================================
        // FMT_MONTHS: 30-365 hari visible
        //   Boundary tahun → "2026"
        //   Boundary bulan → "Mar"
        //   Biasa / Day    → "09 Apr"
        //
        // Contoh output axis:
        //   "09 Apr"  "15 Apr"  "22 Apr"  "Mar"  "05 Mar"  "12 Mar"
        // ============================================
        case FMT_MONTHS:
            if      (boundary == BOUND_YEAR)  fmt = "%Y";          // "2026"
            else if (boundary == BOUND_MONTH) fmt = "%b";          // "Mar"
            else                              fmt = "%d %b";       // "09 Apr"
            break;

        // ============================================
        // FMT_YEARS: > 365 hari visible
        //   Boundary tahun → "2026"
        //   Biasa / Month  → "Mar"
        //
        // Contoh output axis:
        //   "Mar"  "Apr"  "May"  "Jun"  "Jul"  "2026"  "Jan"  "Feb"
        // ============================================
        case FMT_YEARS:
            if      (boundary == BOUND_YEAR)  fmt = "%Y";          // "2026"
            else                              fmt = "%b";          // "Mar"
            break;
    }

    strftime(buff, size, fmt, &tm);
    return (int)strlen(buff);
}

// =========================================================
// ChartAxisTicks: wrapper class
//
// Mode utama: ApplyFormatter() — pakai ImPlot built-in ticks
//             + TradingView-style mixed labels per boundary
//
// Mode lama: Build() + ApplyToPlot() — custom positions (deprecated)
// =========================================================
class ChartAxisTicks {
public:

    // =========================================================
    // ApplyFormatter: setup axis format + tentukan zoom level
    //
    // Panggil SETELAH BeginPlot, SEBELUM PlotX.
    //
    // Hitung visible time span → set fmtLevel.
    // Formatter callback akan otomatis format setiap tick
    // sesuai boundary detection.
    // =========================================================
    void ApplyFormatter(const std::vector<Candle>& candles, int start, int end,
                        bool showLabels = true) {
        // Set context untuk callback
        g_fmtCtx.candles  = &candles;
        g_fmtCtx.count    = (int)candles.size();
        g_fmtCtx.fmtLevel = FMT_INTRADAY;

        // Hitung time span visible
        int s = std::max(0, start);
        int e = std::min((int)candles.size() - 1, end);

        if (s <= e && e < (int)candles.size()) {
            long long t0 = (long long)candles[s].time;
            long long t1 = (long long)candles[e].time;
            double spanDays = (double)(t1 - t0) / 86400.0;

            if      (spanDays < 1.0)    g_fmtCtx.fmtLevel = FMT_INTRADAY; // < 1 hari
            else if (spanDays < 30.0)   g_fmtCtx.fmtLevel = FMT_DAYS;     // 1-30 hari
            else if (spanDays < 365.0)  g_fmtCtx.fmtLevel = FMT_MONTHS;   // 30-365 hari
            else                         g_fmtCtx.fmtLevel = FMT_YEARS;    // > 365 hari
        }

        // Apply formatter ke axis
        if (showLabels) {
            ImPlot::SetupAxisFormat(ImAxis_X1, CandleTimeFormatter, nullptr);
        } else {
            ImPlot::SetupAxis(ImAxis_X1, nullptr, ImPlotAxisFlags_NoTickLabels);
        }
    }

    // =========================================================
    // Helpers (untuk mode lama / utility)
    // =========================================================
    struct tm EpochToTm(long long timestamp) {
        time_t t = (time_t)timestamp;
        struct tm result;
        #ifdef _WIN32
            localtime_s(&result, &t);
        #else
            localtime_r(&t, &result);
        #endif
        return result;
    }

    std::string FormatTime(long long timestamp, const char* fmt) {
        struct tm tm = EpochToTm(timestamp);
        char buf[64];
        strftime(buf, sizeof(buf), fmt, &tm);
        return std::string(buf);
    }

    // =========================================================
    // OLD APPROACH (deprecated — kept as fallback)
    // =========================================================
    struct CustomTick {
        double Index;
        std::string Label;
    };

    enum TickWeight {
        WEIGHT_MINUTE = 0,
        WEIGHT_HOUR   = 1,
        WEIGHT_DAY    = 2,
        WEIGHT_MONTH  = 3,
        WEIGHT_YEAR   = 4,
    };

    std::vector<CustomTick> m_ticks;
    std::vector<const char*> m_labels;
    std::vector<double> m_positions;

    TickWeight GetTickWeight(long long prevTime, long long currTime) {
        if (prevTime <= 0) return WEIGHT_MINUTE;
        struct tm prev = EpochToTm(prevTime);
        struct tm curr = EpochToTm(currTime);
        if (curr.tm_year != prev.tm_year) return WEIGHT_YEAR;
        if (curr.tm_mon  != prev.tm_mon)  return WEIGHT_MONTH;
        if (curr.tm_mday != prev.tm_mday) return WEIGHT_DAY;
        if (curr.tm_hour != prev.tm_hour) return WEIGHT_HOUR;
        return WEIGHT_MINUTE;
    }

    std::string FormatByWeight(long long t, TickWeight w) {
        switch (w) {
            case WEIGHT_YEAR:   return FormatTime(t, "%Y");
            case WEIGHT_MONTH:  return FormatTime(t, "%b");
            case WEIGHT_DAY:    return FormatTime(t, "%d %b");
            case WEIGHT_HOUR:   return FormatTime(t, "%d %H:%M");
            case WEIGHT_MINUTE: return FormatTime(t, "%H:%M");
            default:            return FormatTime(t, "%H:%M");
        }
    }

    void Build(const std::vector<Candle>& candles, int start, int end,
               float zoomLevel, float barSpacingPx = 0.0f) {
        m_ticks.clear();
        m_positions.clear();
        m_labels.clear();
        if (candles.empty() || start >= end) return;

        int viewStart = std::max(0, start);
        int viewEnd   = std::min((int)candles.size() - 1, end);
        int visibleCount = viewEnd - viewStart + 1;
        if (visibleCount <= 0) return;

        const float labelWidthPx = 74.0f;
        float bspx = barSpacingPx;
        if (bspx <= 0.0f && visibleCount > 0) bspx = 900.0f / (float)visibleCount;
        if (bspx < 0.1f) bspx = 0.1f;
        int minGapBars = std::max(1, (int)std::ceil(labelWidthPx / bspx));

        struct Candidate { int index; TickWeight weight; };
        std::vector<Candidate> candidates;
        candidates.reserve(visibleCount / minGapBars + 16);

        for (int i = viewStart; i <= viewEnd; i++) {
            long long prevTime = (i > 0) ? (long long)candles[i - 1].time : 0LL;
            long long currTime = (long long)candles[i].time;
            TickWeight w = GetTickWeight(prevTime, currTime);
            if (w > WEIGHT_MINUTE) candidates.push_back({ i, w });
        }

        int startTick = viewStart - (viewStart % minGapBars);
        for (int i = startTick; i <= viewEnd; i += minGapBars) {
            if (i < viewStart) continue;
            bool hasBoundary = false;
            for (const auto& c : candidates)
                if (std::abs(c.index - i) < minGapBars) { hasBoundary = true; break; }
            if (!hasBoundary) {
                long long prevTime = (i > 0) ? (long long)candles[i - 1].time : 0LL;
                TickWeight w = GetTickWeight(prevTime, (long long)candles[i].time);
                candidates.push_back({ i, w });
            }
        }

        std::sort(candidates.begin(), candidates.end(),
            [](const Candidate& a, const Candidate& b){ return a.index < b.index; });

        std::vector<Candidate> kept;
        kept.reserve(candidates.size());
        for (const auto& cand : candidates) {
            if (kept.empty()) { kept.push_back(cand); continue; }
            Candidate& last = kept.back();
            int gap = cand.index - last.index;
            if (gap >= minGapBars) kept.push_back(cand);
            else if (cand.weight > last.weight) last = cand;
        }

        for (const auto& k : kept) {
            if (k.index < 0 || k.index >= (int)candles.size()) continue;
            long long t = (long long)candles[k.index].time;
            std::string label = FormatByWeight(t, k.weight);
            m_ticks.push_back({ (double)k.index, label });
        }
    }

    void ApplyToPlot(bool showLabels = true) {
        if (m_ticks.empty()) {
            ImPlot::SetupAxis(ImAxis_X1, nullptr, ImPlotAxisFlags_NoTickLabels);
            return;
        }
        m_positions.clear();
        m_labels.clear();
        m_positions.reserve(m_ticks.size());
        m_labels.reserve(m_ticks.size());
        for (const auto& t : m_ticks) {
            m_positions.push_back(t.Index);
            m_labels.push_back(t.Label.c_str());
        }
        if (showLabels)
            ImPlot::SetupAxisTicks(ImAxis_X1, m_positions.data(), (int)m_positions.size(), m_labels.data());
        else {
            ImPlot::SetupAxisTicks(ImAxis_X1, m_positions.data(), (int)m_positions.size());
            ImPlot::SetupAxis(ImAxis_X1, nullptr, ImPlotAxisFlags_NoTickLabels);
        }
    }
};
