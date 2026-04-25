#pragma once
#include <vector>
#include <string>
#include <cmath>
#include "imgui.h"
#include "implot.h"
#include "Candle.h"

// ==========================================
// 1. ENUM TIPE INDIKATOR (WAJIB UNTUK LAYOUT)
// ==========================================
enum IndicatorType {
    IND_OVERLAY, // Nempel di Candle (SMA, EMA, Bollinger)
    IND_PANEL    // Punya kotak sendiri di bawah (Volume, RSI, MACD)
};

// ==========================================
// 1b. PRICE SOURCE (Apply To) — MT5 Style
// ==========================================
enum PriceSource {
    PS_CLOSE = 0,
    PS_OPEN,
    PS_HIGH,
    PS_LOW,
    PS_MEDIAN,    // (High + Low) / 2
    PS_TYPICAL,   // (High + Low + Close) / 3
    PS_WEIGHTED   // (High + Low + Close + Open) / 4
};

static inline const char* PriceSourceName(PriceSource ps) {
    switch (ps) {
        case PS_CLOSE:    return "Close";
        case PS_OPEN:     return "Open";
        case PS_HIGH:     return "High";
        case PS_LOW:      return "Low";
        case PS_MEDIAN:   return "Median (HL/2)";
        case PS_TYPICAL:  return "Typical (HLC/3)";
        case PS_WEIGHTED: return "Weighted (HLCC/4)";
        default:          return "Close";
    }
}

static inline double GetPrice(const Candle& c, PriceSource src) {
    switch (src) {
        case PS_OPEN:     return c.open;
        case PS_HIGH:     return c.high;
        case PS_LOW:      return c.low;
        case PS_CLOSE:    return c.close;
        case PS_MEDIAN:   return (c.high + c.low) * 0.5;
        case PS_TYPICAL:  return (c.high + c.low + c.close) / 3.0;
        case PS_WEIGHTED: return (c.high + c.low + c.close + c.open) / 4.0;
        default:          return c.close;
    }
}

// ==========================================
// BASE CLASS
// ==========================================
class Indicator {
public:
    std::string name;
    ImVec4 color;
    bool visible          = true;
    bool markedForRemoval = false;  // set true → dihapus setelah render loop
    int period;
    IndicatorType type;

    Indicator(std::string n, int p, ImVec4 c, IndicatorType t) 
        : name(n), period(p), color(c), type(t) {}
    
    virtual ~Indicator() {}

    // Hitung total (saat pertama load / ganti setting)
    virtual void Calculate(const std::vector<Candle>& candles) = 0;
    
    // Update Tick-by-Tick (Live Replay Mode)
    virtual void UpdateLive(int index, double price, double volume, const std::vector<Candle>& candles) {
        // Default: Do nothing (override di child class yang perlu)
    }

    // Render grafik
    virtual void Render(int global_offset, int count) = 0;
    
    // Render tombol setting
    virtual bool RenderSettings(const std::vector<Candle>& candles) = 0;
};

// ==========================================
// KELAS VOLUME (Style: Stick ala Indicatorslama + Logic Smart)
// ==========================================
// ==========================================
// VOLUME INDICATOR
// ==========================================
class VolumeIndicator : public Indicator {
public:
    std::vector<double> values;

    // Warna default hijau transparan
    VolumeIndicator(int p = 14, ImVec4 c = ImVec4(0, 1, 0, 0.5f)) 
        : Indicator("Volume", p, c, IND_PANEL) {}

    void Calculate(const std::vector<Candle>& candles) override {
        values.clear();
        values.reserve(candles.size());
        for (const auto& c : candles) {
            values.push_back((double)c.volume);
        }
    }

    void Render(int global_offset, int count) override {
        if (values.empty()) return;
        int max_size = (int)values.size();
        if (global_offset >= max_size) return;

        int available = max_size - global_offset;
        int points_to_draw = (count < available) ? count : available;

        if (points_to_draw <= 0) return;

        // Volume biasanya dirender sebagai Bar Chart
        ImPlot::SetNextFillStyle(color);
        ImPlot::PlotBars(name.c_str(), values.data() + global_offset, points_to_draw);
    }

    bool RenderSettings(const std::vector<Candle>& candles) override {
        bool request_remove = false;
        ImGui::Text("Settings for %s", name.c_str());
        ImGui::ColorEdit4("Color", (float*)&color);
        
        if (ImGui::Button("Apply")) Calculate(candles); // Volume jarang butuh recalc, tapi biar konsisten
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1));
        if (ImGui::Button("Remove")) request_remove = true;
        ImGui::PopStyleColor();

        return request_remove;
    }
};
// ==========================================
// 📈 SMA INDICATOR (MT5 STYLE) ✅
// ==========================================
class SMAIndicator : public Indicator {
public:
    std::vector<double> values;
    PriceSource priceSource = PS_CLOSE;

    SMAIndicator(int p = 14, ImVec4 c = ImVec4(1, 0.5f, 0, 1)) 
        : Indicator("SMA", p, c, IND_OVERLAY) {}

    void Calculate(const std::vector<Candle>& candles) override {
        values.clear();
        if (candles.empty() || period < 1) return;
        
        values.resize(candles.size(), NAN);

        // Running Sum untuk efisiensi O(n)
        double runningSum = 0.0;
        
        for (size_t i = 0; i < candles.size(); ++i) {
            runningSum += GetPrice(candles[i], priceSource);
            
            // Hapus candle terlalu jauh dari window
            if (i >= period) {
                runningSum -= GetPrice(candles[i - period], priceSource);
            }
            
            // Mulai hitung SMA setelah period terpenuhi
            if (i >= period - 1) {
                values[i] = runningSum / period;
            }
        }
    }

    void UpdateLive(int index, double price, double volume, const std::vector<Candle>& candles) override {
        if (index < period - 1 || index >= values.size()) return;
        if (index >= (int)values.size()) values.resize(index + 1, NAN);

        // Hitung ulang SMA untuk candle terakhir
        double sum = 0;
        for (int i = 0; i < period - 1; i++) {
            sum += GetPrice(candles[index - 1 - i], priceSource);
        }
        sum += price; // Harga live
        values[index] = sum / period;
    }

    void Render(int global_offset, int count) override {
        if (!visible || values.empty()) return;
        int max_size = (int)values.size();
        if (global_offset >= max_size) return;
        int points = std::min(count, max_size - global_offset);
        if (points <= 0) return;

        ImPlot::SetNextLineStyle(color, 2.0f);
        ImPlot::PlotLine(name.c_str(), values.data() + global_offset, points);
    }

    bool RenderSettings(const std::vector<Candle>& candles) override {
        bool req_rem = false;
        ImGui::Text("SMA Settings");
        if (ImGui::InputInt("Period", &period)) {
            if (period < 1) period = 1;
            if (period > 500) period = 500;
        }
        // Apply To (Price Source)
        if (ImGui::BeginCombo("Apply To", PriceSourceName(priceSource))) {
            for (int i = 0; i <= PS_WEIGHTED; i++) {
                bool sel = (priceSource == (PriceSource)i);
                if (ImGui::Selectable(PriceSourceName((PriceSource)i), sel))
                    priceSource = (PriceSource)i;
                if (sel) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::ColorEdit4("Color", (float*)&color);
        if (ImGui::Button("Apply", ImVec2(-1, 0))) Calculate(candles);
        if (ImGui::Button("Remove", ImVec2(-1, 0))) req_rem = true;
        return req_rem;
    }
};

// ==========================================
// 📉 EMA INDICATOR (MT5 STYLE) ✅
// ==========================================
class EMAIndicator : public Indicator {
public:
    std::vector<double> values;
    PriceSource priceSource = PS_CLOSE;

    EMAIndicator(int p = 20, ImVec4 c = ImVec4(0, 0.8f, 1, 1)) 
        : Indicator("EMA", p, c, IND_OVERLAY) {}

    void Calculate(const std::vector<Candle>& candles) override {
        values.clear();
        if (candles.empty() || period < 1) return;
        
        values.resize(candles.size(), NAN);

        // Multiplier EMA: 2 / (Period + 1)
        double multiplier = 2.0 / (period + 1);
        
        // Inisialisasi: Gunakan SMA untuk nilai pertama
        double sma_sum = 0.0;
        for (int i = 0; i < period && i < (int)candles.size(); ++i) {
            sma_sum += GetPrice(candles[i], priceSource);
        }
        
        if (period > (int)candles.size()) return; // Safety check
        
        // EMA pertama = SMA
        values[period - 1] = sma_sum / period;
        
        // EMA selanjutnya: EMA = (Price - EMA_prev) * Multiplier + EMA_prev
        for (size_t i = period; i < candles.size(); ++i) {
            double prevEMA = values[i - 1];
            values[i] = (GetPrice(candles[i], priceSource) - prevEMA) * multiplier + prevEMA;
        }
    }

    void UpdateLive(int index, double price, double volume, const std::vector<Candle>& candles) override {
        if (index < period || index >= (int)values.size()) return;
        
        double multiplier = 2.0 / (period + 1);
        double prevEMA = values[index - 1];
        
        if (!std::isnan(prevEMA)) {
            values[index] = (price - prevEMA) * multiplier + prevEMA;
        }
    }

    void Render(int global_offset, int count) override {
        if (!visible || values.empty()) return;
        int max_size = (int)values.size();
        if (global_offset >= max_size) return;
        int points = std::min(count, max_size - global_offset);
        if (points <= 0) return;

        ImPlot::SetNextLineStyle(color, 2.0f);
        ImPlot::PlotLine(name.c_str(), values.data() + global_offset, points);
    }

    bool RenderSettings(const std::vector<Candle>& candles) override {
        bool req_rem = false;
        ImGui::Text("EMA Settings");
        if (ImGui::InputInt("Period", &period)) {
            if (period < 1) period = 1;
            if (period > 500) period = 500;
        }
        // Apply To (Price Source)
        if (ImGui::BeginCombo("Apply To", PriceSourceName(priceSource))) {
            for (int i = 0; i <= PS_WEIGHTED; i++) {
                bool sel = (priceSource == (PriceSource)i);
                if (ImGui::Selectable(PriceSourceName((PriceSource)i), sel))
                    priceSource = (PriceSource)i;
                if (sel) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::ColorEdit4("Color", (float*)&color);
        if (ImGui::Button("Apply", ImVec2(-1, 0))) Calculate(candles);
        if (ImGui::Button("Remove", ImVec2(-1, 0))) req_rem = true;
        return req_rem;
    }
};

// ==========================================
// 📊 BOLLINGER BANDS (MT5 STYLE) ✅
// ==========================================
class BollingerIndicator : public Indicator {
public:
    std::vector<double> upper, middle, lower;
    double deviation; // Standard Deviation Multiplier
    PriceSource priceSource = PS_CLOSE;

    BollingerIndicator(int p = 20, double dev = 2.0) 
        : Indicator("Bollinger", p, ImVec4(1, 1, 1, 0.5f), IND_OVERLAY), deviation(dev) {}

    void Calculate(const std::vector<Candle>& candles) override {
        size_t n = candles.size();
        upper.assign(n, NAN);
        middle.assign(n, NAN);
        lower.assign(n, NAN);

        if (candles.empty() || period < 2) return;

        for (size_t i = 0; i < n; ++i) {
            if (i < (size_t)(period - 1)) continue;
            
            // 1. Hitung SMA (Middle Band)
            double sum = 0;
            for (int j = 0; j < period; ++j) {
                sum += GetPrice(candles[i - j], priceSource);
            }
            double sma = sum / period;
            middle[i] = sma;

            // 2. Hitung Standard Deviation
            double sumSq = 0;
            for (int j = 0; j < period; ++j) {
                double diff = GetPrice(candles[i - j], priceSource) - sma;
                sumSq += diff * diff;
            }
            double stdDev = std::sqrt(sumSq / period);

            // 3. Upper & Lower Bands
            upper[i] = sma + (stdDev * deviation);
            lower[i] = sma - (stdDev * deviation);
        }
    }

    void UpdateLive(int index, double price, double volume, const std::vector<Candle>& candles) override {
        if (index < period - 1 || index >= (int)upper.size()) return;

        // Recalculate SMA dengan harga live
        double sum = 0;
        for (int i = 0; i < period - 1; i++) {
            sum += GetPrice(candles[index - 1 - i], priceSource);
        }
        sum += price;
        double sma = sum / period;
        middle[index] = sma;

        // Recalculate StdDev
        double sumSq = 0;
        for (int i = 0; i < period - 1; i++) {
            double diff = GetPrice(candles[index - 1 - i], priceSource) - sma;
            sumSq += diff * diff;
        }
        double diffLive = price - sma;
        sumSq += diffLive * diffLive;
        double stdDev = std::sqrt(sumSq / period);

        upper[index] = sma + (stdDev * deviation);
        lower[index] = sma - (stdDev * deviation);
    }

    void Render(int global_offset, int count) override {
        if (!visible || upper.empty()) return;
        int max_size = (int)upper.size();
        if (global_offset >= max_size) return;
        int points = std::min(count, max_size - global_offset);
        if (points <= 0) return;

        // Render Shaded Area (Cloud)
        ImPlot::SetNextFillStyle(ImVec4(color.x, color.y, color.z, 0.15f));
        ImPlot::PlotShaded("##BB_Cloud", lower.data() + global_offset, upper.data() + global_offset, points);

        // Render Lines
        ImPlot::SetNextLineStyle(ImVec4(color.x, color.y, color.z, 0.7f), 1.0f);
        ImPlot::PlotLine("BB Upper", upper.data() + global_offset, points);
        
        ImPlot::SetNextLineStyle(ImVec4(color.x, color.y, color.z, 0.9f), 1.5f);
        ImPlot::PlotLine("BB Middle", middle.data() + global_offset, points);
        
        ImPlot::SetNextLineStyle(ImVec4(color.x, color.y, color.z, 0.7f), 1.0f);
        ImPlot::PlotLine("BB Lower", lower.data() + global_offset, points);
    }

    bool RenderSettings(const std::vector<Candle>& candles) override {
        bool req_rem = false;
        ImGui::Text("Bollinger Bands Settings");
        if (ImGui::InputInt("Period", &period)) {
            if (period < 2) period = 2;
            if (period > 500) period = 500;
        }
        if (ImGui::InputDouble("Deviation", &deviation, 0.1, 0.5, "%.1f")) {
            if (deviation < 0.1) deviation = 0.1;
            if (deviation > 5.0) deviation = 5.0;
        }
        // Apply To (Price Source)
        if (ImGui::BeginCombo("Apply To", PriceSourceName(priceSource))) {
            for (int i = 0; i <= PS_WEIGHTED; i++) {
                bool sel = (priceSource == (PriceSource)i);
                if (ImGui::Selectable(PriceSourceName((PriceSource)i), sel))
                    priceSource = (PriceSource)i;
                if (sel) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::ColorEdit4("Color", (float*)&color);
        if (ImGui::Button("Apply", ImVec2(-1, 0))) Calculate(candles);
        if (ImGui::Button("Remove", ImVec2(-1, 0))) req_rem = true;
        return req_rem;
    }
};

// ==========================================
// 📊 RSI INDICATOR (MT5 SIMPLIFIED) ✅
// ==========================================
class RSIIndicator : public Indicator {
public:
    std::vector<double> values;
    PriceSource priceSource = PS_CLOSE;

    RSIIndicator(int p = 14, ImVec4 c = ImVec4(0, 0.8f, 1, 1)) 
        : Indicator("RSI", p, c, IND_PANEL) {}

    void Calculate(const std::vector<Candle>& candles) override {
        values.clear();
        if (candles.empty() || period < 1) return;
        
        values.resize(candles.size(), NAN);

        // Wilder's Smoothing Method (MT5 Default)
        double avgGain = 0.0;
        double avgLoss = 0.0;

        // 1. Hitung gain/loss pertama (Simple Average)
        for (int i = 1; i <= period && i < (int)candles.size(); ++i) {
            double change = GetPrice(candles[i], priceSource) - GetPrice(candles[i - 1], priceSource);
            if (change > 0) {
                avgGain += change;
            } else {
                avgLoss += std::abs(change);
            }
        }
        
        avgGain /= period;
        avgLoss /= period;

        // 2. Hitung RSI pertama
        if (period < (int)candles.size()) {
            double rs = (avgLoss == 0) ? 100.0 : avgGain / avgLoss;
            values[period] = 100.0 - (100.0 / (1.0 + rs));
        }

        // 3. Smoothing selanjutnya (Wilder's Method)
        for (size_t i = period + 1; i < candles.size(); ++i) {
            double change = GetPrice(candles[i], priceSource) - GetPrice(candles[i - 1], priceSource);
            double gain = (change > 0) ? change : 0.0;
            double loss = (change < 0) ? std::abs(change) : 0.0;

            // Wilder's Smoothing: (Prev * (period-1) + Current) / period
            avgGain = (avgGain * (period - 1) + gain) / period;
            avgLoss = (avgLoss * (period - 1) + loss) / period;

            double rs = (avgLoss == 0) ? 100.0 : avgGain / avgLoss;
            values[i] = 100.0 - (100.0 / (1.0 + rs));
        }
    }

    void UpdateLive(int index, double price, double volume, const std::vector<Candle>& candles) override {
        if (index <= period || index >= (int)values.size()) return;

        // Ambil avgGain & avgLoss dari RSI sebelumnya (reverse engineering)
        double prevRSI = values[index - 1];
        if (std::isnan(prevRSI)) return;

        // Reverse formula: RS = (100 - RSI) / RSI
        double rs = (100.0 - prevRSI) / prevRSI;
        double avgGain = rs / (1.0 + rs);
        double avgLoss = 1.0 / (1.0 + rs);

        // Update dengan harga live
        double prevPrice = GetPrice(candles[index - 1], priceSource);
        double change = price - prevPrice;
        double gain = (change > 0) ? change : 0.0;
        double loss = (change < 0) ? std::abs(change) : 0.0;

        avgGain = (avgGain * (period - 1) + gain) / period;
        avgLoss = (avgLoss * (period - 1) + loss) / period;

        rs = (avgLoss == 0) ? 100.0 : avgGain / avgLoss;
        values[index] = 100.0 - (100.0 / (1.0 + rs));
    }

    void Render(int global_offset, int count) override {
        if (!visible || values.empty()) return;
        int max_size = (int)values.size();
        if (global_offset >= max_size) return;
        int points = std::min(count, max_size - global_offset);
        if (points <= 0) return;

        // Main Line
        ImPlot::SetNextLineStyle(color, 2.0f);
        ImPlot::PlotLine(name.c_str(), values.data() + global_offset, points);

        // Overbought (70) & Oversold (30) Lines
        ImPlot::PushStyleColor(ImPlotCol_Line, ImVec4(1, 0, 0, 0.4f));
        double over[] = {70, 70};
        ImPlot::PlotInfLines("##Over70", over, 1, ImPlotInfLinesFlags_Horizontal);
        ImPlot::PopStyleColor();

        ImPlot::PushStyleColor(ImPlotCol_Line, ImVec4(0, 1, 0, 0.4f));
        double under[] = {30, 30};
        ImPlot::PlotInfLines("##Under30", under, 1, ImPlotInfLinesFlags_Horizontal);
        ImPlot::PopStyleColor();

        // Midline (50)
        ImPlot::PushStyleColor(ImPlotCol_Line, ImVec4(0.5f, 0.5f, 0.5f, 0.3f));
        double mid[] = {50, 50};
        ImPlot::PlotInfLines("##Mid50", mid, 1, ImPlotInfLinesFlags_Horizontal);
        ImPlot::PopStyleColor();
    }

    bool RenderSettings(const std::vector<Candle>& candles) override {
        bool req_rem = false;
        ImGui::Text("RSI Settings (Wilder's Smoothing)");
        if (ImGui::InputInt("Period", &period)) {
            if (period < 2) period = 2;
            if (period > 100) period = 100;
        }
        // Apply To (Price Source)
        if (ImGui::BeginCombo("Apply To", PriceSourceName(priceSource))) {
            for (int i = 0; i <= PS_WEIGHTED; i++) {
                bool sel = (priceSource == (PriceSource)i);
                if (ImGui::Selectable(PriceSourceName((PriceSource)i), sel))
                    priceSource = (PriceSource)i;
                if (sel) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::ColorEdit4("Color", (float*)&color);
        if (ImGui::Button("Apply", ImVec2(-1, 0))) Calculate(candles);
        if (ImGui::Button("Remove", ImVec2(-1, 0))) req_rem = true;
        return req_rem;
    }
};

// ==========================================
// 🌐 GLOBAL MANAGER
// ==========================================
extern std::vector<Indicator*> g_activeIndicators;
void AddIndicator(Indicator* newInd, const std::vector<Candle>& currentCandles);
void ClearIndicators();
void RecalculateAllIndicators(const std::vector<Candle>& currentCandles);

// ==========================================================
// TA-LIB INDICATORS (MACD, Stoch, ATR, ADX, CCI, WilliamsR,
//  MFI, ROC, OBV, Supertrend)
// ==========================================================
#include "TA_Indicators.h"
