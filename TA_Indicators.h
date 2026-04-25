#pragma once
// ================================================================
// TA_Indicators.h  (v2 — Incremental / MQL5 prev_calculated style)
//
// 10 Indicator menggunakan TA-Lib + Supertrend custom
//
// REPLAY SUPPORT:
//   UpdateLive() mengikuti pola MQL5 OnCalculate():
//   - prev_calculated > 0 → cuma hitung candle BARU (incremental)
//   - prev_calculated == 0 atau index mundur → full recalculate
//   - TA-Lib dipanggil dengan startIdx = prev - lookback
//     sehingga cuma memproses data baru, bukan seluruh history
//
// CARA PAKAI:
//   #include "TA_Indicators.h"  (di paling bawah Indicators.h)
//   TA_Initialize() dipanggil 1x di main.cpp saat startup
// ================================================================

#include "ta-lib/include/ta_libc.h"

// ================================================================
// 1. MACD (Moving Average Convergence Divergence)
// ================================================================
class MACDIndicator : public Indicator {
public:
    std::vector<double> macdLine, signalLine, histogram;
    int fastPeriod   = 12;
    int slowPeriod   = 26;
    int signalPeriod = 9;

    // MQL5-style: prev_calculated
    int prevCalc = 0;

    MACDIndicator() : Indicator("MACD", 26, ImVec4(0, 1, 1, 1), IND_PANEL) {}

    // ── Full Calculate (pertama kali / Apply settings) ───────────
    void Calculate(const std::vector<Candle>& candles) override {
        int n = (int)candles.size();
        macdLine.assign(n, NAN);
        signalLine.assign(n, NAN);
        histogram.assign(n, NAN);
        if (n < slowPeriod + signalPeriod) { prevCalc = n; return; }

        std::vector<double> close(n);
        for (int i = 0; i < n; i++) close[i] = candles[i].close;

        std::vector<double> oM(n), oS(n), oH(n);
        int beg = 0, nb = 0;
        TA_MACD(0, n - 1, close.data(),
                fastPeriod, slowPeriod, signalPeriod,
                &beg, &nb, oM.data(), oS.data(), oH.data());
        for (int i = 0; i < nb; i++) {
            macdLine[beg + i]   = oM[i];
            signalLine[beg + i] = oS[i];
            histogram[beg + i]  = oH[i];
        }
        prevCalc = n;
    }

    // ── Incremental Update (MQL5 prev_calculated pattern) ────────
    void UpdateLive(int index, double price, double volume, const std::vector<Candle>& candles) override {
        int n = std::min(index + 1, (int)candles.size());
        if (n <= 0) return;

        // Backward jump or first call → full recalculate
        if (prevCalc <= 0 || n <= prevCalc) {
            Calculate(candleSubset(candles, n));
            return;
        }

        // ── Incremental: hanya hitung candle baru ────────────────
        int lookback = slowPeriod + signalPeriod + 5;
        int startIdx = std::max(0, prevCalc - lookback);

        // Resize buffer (ISI BARU = NAN, lama tetap)
        macdLine.resize(n, NAN);
        signalLine.resize(n, NAN);
        histogram.resize(n, NAN);

        // Close price (perlu full karena TA-Lib index absolut)
        std::vector<double> close(n);
        for (int i = 0; i < n; i++) close[i] = candles[i].close;

        // TA-Lib hanya proses startIdx → n-1
        std::vector<double> oM(n), oS(n), oH(n);
        int beg = 0, nb = 0;
        TA_MACD(startIdx, n - 1, close.data(),
                fastPeriod, slowPeriod, signalPeriod,
                &beg, &nb, oM.data(), oS.data(), oH.data());

        // Tulis hanya nilai BARU (idx >= prevCalc)
        for (int i = 0; i < nb; i++) {
            int idx = beg + i;
            if (idx >= prevCalc) {
                macdLine[idx]   = oM[i];
                signalLine[idx] = oS[i];
                histogram[idx]  = oH[i];
            }
        }
        prevCalc = n;
    }

    void Render(int off, int count) override {
        if (!visible || macdLine.empty()) return;
        int n = (int)macdLine.size();
        if (off >= n) return;
        int pts = std::min(count, n - off);
        if (pts <= 0) return;

        ImPlot::SetNextFillStyle(ImVec4(0.5f, 0.5f, 1.0f, 0.45f));
        ImPlot::PlotBars("MACD Histogram", histogram.data() + off, pts, 0.67);
        ImPlot::SetNextLineStyle(ImVec4(0.2f, 0.8f, 1.0f, 1.0f), 1.5f);
        ImPlot::PlotLine("MACD Line", macdLine.data() + off, pts);
        ImPlot::SetNextLineStyle(ImVec4(1.0f, 0.5f, 0.1f, 1.0f), 1.5f);
        ImPlot::PlotLine("Signal Line", signalLine.data() + off, pts);
        ImPlot::PushStyleColor(ImPlotCol_Line, ImVec4(0.5f, 0.5f, 0.5f, 0.3f));
        double zero[] = { 0 };
        ImPlot::PlotInfLines("##MACD0", zero, 1, ImPlotInfLinesFlags_Horizontal);
        ImPlot::PopStyleColor();
    }

    bool RenderSettings(const std::vector<Candle>& candles) override {
        bool rem = false;
        ImGui::Text("MACD Settings");
        ImGui::Separator();
        if (ImGui::InputInt("Fast Period",    &fastPeriod))   { if (fastPeriod < 1) fastPeriod = 1; }
        if (ImGui::InputInt("Slow Period",    &slowPeriod))   { if (slowPeriod <= fastPeriod) slowPeriod = fastPeriod + 1; }
        if (ImGui::InputInt("Signal Period",  &signalPeriod)) { if (signalPeriod < 1) signalPeriod = 1; }
        if (ImGui::Button("Apply",  ImVec2(-1, 0))) { prevCalc = 0; Calculate(candles); }
        if (ImGui::Button("Remove", ImVec2(-1, 0))) rem = true;
        return rem;
    }

private:
    static std::vector<Candle> candleSubset(const std::vector<Candle>& src, int count) {
        if (count <= 0) return {};
        return std::vector<Candle>(src.begin(), src.begin() + std::min(count, (int)src.size()));
    }
};

// ================================================================
// 2. STOCHASTIC
// ================================================================
class StochIndicator : public Indicator {
public:
    std::vector<double> slowK, slowD;
    int fastKPeriod = 5;
    int slowKPeriod = 3;
    int slowDPeriod = 3;

    int prevCalc = 0;

    StochIndicator() : Indicator("Stochastic", 5, ImVec4(1, 0.8f, 0, 1), IND_PANEL) {}

    void Calculate(const std::vector<Candle>& candles) override {
        int n = (int)candles.size();
        slowK.assign(n, NAN);
        slowD.assign(n, NAN);
        if (n < fastKPeriod) { prevCalc = n; return; }

        std::vector<double> hi(n), lo(n), cl(n);
        for (int i = 0; i < n; i++) {
            hi[i] = candles[i].high; lo[i] = candles[i].low; cl[i] = candles[i].close;
        }
        std::vector<double> oK(n), oD(n);
        int beg = 0, nb = 0;
        TA_STOCH(0, n - 1, hi.data(), lo.data(), cl.data(),
                 fastKPeriod, slowKPeriod, TA_MAType_SMA,
                 slowDPeriod, TA_MAType_SMA,
                 &beg, &nb, oK.data(), oD.data());
        for (int i = 0; i < nb; i++) { slowK[beg + i] = oK[i]; slowD[beg + i] = oD[i]; }
        prevCalc = n;
    }

    void UpdateLive(int index, double price, double volume, const std::vector<Candle>& candles) override {
        int n = std::min(index + 1, (int)candles.size());
        if (n <= 0) return;
        if (prevCalc <= 0 || n <= prevCalc) { Calculate(candleSubset(candles, n)); return; }

        int lookback = fastKPeriod + slowKPeriod + slowDPeriod + 10;
        int startIdx = std::max(0, prevCalc - lookback);

        slowK.resize(n, NAN); slowD.resize(n, NAN);

        std::vector<double> hi(n), lo(n), cl(n);
        for (int i = 0; i < n; i++) { hi[i] = candles[i].high; lo[i] = candles[i].low; cl[i] = candles[i].close; }

        std::vector<double> oK(n), oD(n);
        int beg = 0, nb = 0;
        TA_STOCH(startIdx, n - 1, hi.data(), lo.data(), cl.data(),
                 fastKPeriod, slowKPeriod, TA_MAType_SMA,
                 slowDPeriod, TA_MAType_SMA,
                 &beg, &nb, oK.data(), oD.data());
        for (int i = 0; i < nb; i++) {
            int idx = beg + i;
            if (idx >= prevCalc) { slowK[idx] = oK[i]; slowD[idx] = oD[i]; }
        }
        prevCalc = n;
    }

    void Render(int off, int count) override {
        if (!visible || slowK.empty()) return;
        int n = (int)slowK.size();
        if (off >= n) return;
        int pts = std::min(count, n - off);
        if (pts <= 0) return;

        ImPlot::SetNextLineStyle(ImVec4(0.2f, 0.8f, 1.0f, 1.0f), 1.5f);
        ImPlot::PlotLine("Stoch K", slowK.data() + off, pts);
        ImPlot::SetNextLineStyle(ImVec4(1.0f, 0.4f, 0.2f, 1.0f), 1.5f);
        ImPlot::PlotLine("Stoch D", slowD.data() + off, pts);
        ImPlot::PushStyleColor(ImPlotCol_Line, ImVec4(1, 0, 0, 0.3f));
        double ob[] = { 80 }; ImPlot::PlotInfLines("##ST80", ob, 1, ImPlotInfLinesFlags_Horizontal);
        double os[] = { 20 }; ImPlot::PlotInfLines("##ST20", os, 1, ImPlotInfLinesFlags_Horizontal);
        ImPlot::PopStyleColor();
    }

    bool RenderSettings(const std::vector<Candle>& candles) override {
        bool rem = false;
        ImGui::Text("Stochastic Settings");
        ImGui::Separator();
        if (ImGui::InputInt("Fast K Period", &fastKPeriod)) { if (fastKPeriod < 1) fastKPeriod = 1; }
        if (ImGui::InputInt("Slow K Period", &slowKPeriod)) { if (slowKPeriod < 1) slowKPeriod = 1; }
        if (ImGui::InputInt("Slow D Period", &slowDPeriod)) { if (slowDPeriod < 1) slowDPeriod = 1; }
        if (ImGui::Button("Apply",  ImVec2(-1, 0))) { prevCalc = 0; Calculate(candles); }
        if (ImGui::Button("Remove", ImVec2(-1, 0))) rem = true;
        return rem;
    }

private:
    static std::vector<Candle> candleSubset(const std::vector<Candle>& src, int count) {
        if (count <= 0) return {};
        return std::vector<Candle>(src.begin(), src.begin() + std::min(count, (int)src.size()));
    }
};

// ================================================================
// 3. ATR (Average True Range)
// ================================================================
class ATRIndicator : public Indicator {
public:
    std::vector<double> values;
    int prevCalc = 0;

    ATRIndicator(int p = 14) : Indicator("ATR", p, ImVec4(1.0f, 0.6f, 0.1f, 1), IND_PANEL) {}

    void Calculate(const std::vector<Candle>& candles) override {
        int n = (int)candles.size();
        values.assign(n, NAN);
        if (n < period + 1) { prevCalc = n; return; }

        std::vector<double> hi(n), lo(n), cl(n);
        for (int i = 0; i < n; i++) { hi[i] = candles[i].high; lo[i] = candles[i].low; cl[i] = candles[i].close; }

        std::vector<double> out(n);
        int beg = 0, nb = 0;
        TA_ATR(0, n - 1, hi.data(), lo.data(), cl.data(), period, &beg, &nb, out.data());
        for (int i = 0; i < nb; i++) values[beg + i] = out[i];
        prevCalc = n;
    }

    void UpdateLive(int index, double price, double volume, const std::vector<Candle>& candles) override {
        int n = std::min(index + 1, (int)candles.size());
        if (n <= 0) return;
        if (prevCalc <= 0 || n <= prevCalc) { Calculate(candleSubset(candles, n)); return; }

        int lookback = period + 5;
        int startIdx = std::max(0, prevCalc - lookback);
        values.resize(n, NAN);

        std::vector<double> hi(n), lo(n), cl(n);
        for (int i = 0; i < n; i++) { hi[i] = candles[i].high; lo[i] = candles[i].low; cl[i] = candles[i].close; }

        std::vector<double> out(n);
        int beg = 0, nb = 0;
        TA_ATR(startIdx, n - 1, hi.data(), lo.data(), cl.data(), period, &beg, &nb, out.data());
        for (int i = 0; i < nb; i++) {
            int idx = beg + i;
            if (idx >= prevCalc) values[idx] = out[i];
        }
        prevCalc = n;
    }

    void Render(int off, int count) override {
        if (!visible || values.empty()) return;
        int n = (int)values.size();
        if (off >= n) return;
        int pts = std::min(count, n - off);
        if (pts <= 0) return;
        ImPlot::SetNextLineStyle(color, 1.5f);
        ImPlot::PlotLine("ATR", values.data() + off, pts);
    }

    bool RenderSettings(const std::vector<Candle>& candles) override {
        bool rem = false;
        ImGui::Text("ATR Settings");
        ImGui::Separator();
        if (ImGui::InputInt("Period", &period)) { if (period < 1) period = 1; }
        ImGui::ColorEdit4("Color", (float*)&color);
        if (ImGui::Button("Apply",  ImVec2(-1, 0))) { prevCalc = 0; Calculate(candles); }
        if (ImGui::Button("Remove", ImVec2(-1, 0))) rem = true;
        return rem;
    }

private:
    static std::vector<Candle> candleSubset(const std::vector<Candle>& src, int count) {
        if (count <= 0) return {};
        return std::vector<Candle>(src.begin(), src.begin() + std::min(count, (int)src.size()));
    }
};

// ================================================================
// 4. ADX (Average Directional Index)
// ================================================================
class ADXIndicator : public Indicator {
public:
    std::vector<double> adxLine, plusDI, minusDI;
    int prevCalc = 0;

    ADXIndicator(int p = 14) : Indicator("ADX", p, ImVec4(1, 1, 0, 1), IND_PANEL) {}

    void Calculate(const std::vector<Candle>& candles) override {
        int n = (int)candles.size();
        adxLine.assign(n, NAN); plusDI.assign(n, NAN); minusDI.assign(n, NAN);
        if (n < period * 2) { prevCalc = n; return; }

        std::vector<double> hi(n), lo(n), cl(n);
        for (int i = 0; i < n; i++) { hi[i] = candles[i].high; lo[i] = candles[i].low; cl[i] = candles[i].close; }

        { std::vector<double> o(n); int b=0, nb=0;
          TA_ADX(0, n-1, hi.data(), lo.data(), cl.data(), period, &b, &nb, o.data());
          for(int i=0;i<nb;i++) adxLine[b+i]=o[i]; }
        { std::vector<double> o(n); int b=0, nb=0;
          TA_PLUS_DI(0, n-1, hi.data(), lo.data(), cl.data(), period, &b, &nb, o.data());
          for(int i=0;i<nb;i++) plusDI[b+i]=o[i]; }
        { std::vector<double> o(n); int b=0, nb=0;
          TA_MINUS_DI(0, n-1, hi.data(), lo.data(), cl.data(), period, &b, &nb, o.data());
          for(int i=0;i<nb;i++) minusDI[b+i]=o[i]; }
        prevCalc = n;
    }

    void UpdateLive(int index, double price, double volume, const std::vector<Candle>& candles) override {
        int n = std::min(index + 1, (int)candles.size());
        if (n <= 0) return;
        if (prevCalc <= 0 || n <= prevCalc) { Calculate(candleSubset(candles, n)); return; }

        int lookback = period * 2 + 5;
        int startIdx = std::max(0, prevCalc - lookback);
        adxLine.resize(n, NAN); plusDI.resize(n, NAN); minusDI.resize(n, NAN);

        std::vector<double> hi(n), lo(n), cl(n);
        for (int i = 0; i < n; i++) { hi[i] = candles[i].high; lo[i] = candles[i].low; cl[i] = candles[i].close; }

        { std::vector<double> o(n); int b=0, nb=0;
          TA_ADX(startIdx, n-1, hi.data(), lo.data(), cl.data(), period, &b, &nb, o.data());
          for(int i=0;i<nb;i++){ int idx=b+i; if(idx>=prevCalc) adxLine[idx]=o[i]; } }
        { std::vector<double> o(n); int b=0, nb=0;
          TA_PLUS_DI(startIdx, n-1, hi.data(), lo.data(), cl.data(), period, &b, &nb, o.data());
          for(int i=0;i<nb;i++){ int idx=b+i; if(idx>=prevCalc) plusDI[idx]=o[i]; } }
        { std::vector<double> o(n); int b=0, nb=0;
          TA_MINUS_DI(startIdx, n-1, hi.data(), lo.data(), cl.data(), period, &b, &nb, o.data());
          for(int i=0;i<nb;i++){ int idx=b+i; if(idx>=prevCalc) minusDI[idx]=o[i]; } }
        prevCalc = n;
    }

    void Render(int off, int count) override {
        if (!visible || adxLine.empty()) return;
        int n = (int)adxLine.size();
        if (off >= n) return;
        int pts = std::min(count, n - off);
        if (pts <= 0) return;

        ImPlot::SetNextLineStyle(ImVec4(1, 1, 0, 1), 2.0f);
        ImPlot::PlotLine("ADX", adxLine.data() + off, pts);
        ImPlot::SetNextLineStyle(ImVec4(0.2f, 1, 0.2f, 0.8f), 1.0f);
        ImPlot::PlotLine("+DI", plusDI.data() + off, pts);
        ImPlot::SetNextLineStyle(ImVec4(1, 0.2f, 0.2f, 0.8f), 1.0f);
        ImPlot::PlotLine("-DI", minusDI.data() + off, pts);
        ImPlot::PushStyleColor(ImPlotCol_Line, ImVec4(1, 1, 0, 0.25f));
        double lvl[] = { 25 };
        ImPlot::PlotInfLines("##ADX25", lvl, 1, ImPlotInfLinesFlags_Horizontal);
        ImPlot::PopStyleColor();
    }

    bool RenderSettings(const std::vector<Candle>& candles) override {
        bool rem = false;
        ImGui::Text("ADX Settings");
        ImGui::Separator();
        if (ImGui::InputInt("Period", &period)) { if (period < 2) period = 2; }
        if (ImGui::Button("Apply",  ImVec2(-1, 0))) { prevCalc = 0; Calculate(candles); }
        if (ImGui::Button("Remove", ImVec2(-1, 0))) rem = true;
        return rem;
    }

private:
    static std::vector<Candle> candleSubset(const std::vector<Candle>& src, int count) {
        if (count <= 0) return {};
        return std::vector<Candle>(src.begin(), src.begin() + std::min(count, (int)src.size()));
    }
};

// ================================================================
// 5. CCI (Commodity Channel Index)
// ================================================================
class CCIIndicator : public Indicator {
public:
    std::vector<double> values;
    int prevCalc = 0;

    CCIIndicator(int p = 14) : Indicator("CCI", p, ImVec4(0.8f, 0.4f, 1, 1), IND_PANEL) {}

    void Calculate(const std::vector<Candle>& candles) override {
        int n = (int)candles.size();
        values.assign(n, NAN);
        if (n < period) { prevCalc = n; return; }

        std::vector<double> hi(n), lo(n), cl(n);
        for (int i = 0; i < n; i++) { hi[i] = candles[i].high; lo[i] = candles[i].low; cl[i] = candles[i].close; }

        std::vector<double> out(n);
        int beg = 0, nb = 0;
        TA_CCI(0, n - 1, hi.data(), lo.data(), cl.data(), period, &beg, &nb, out.data());
        for (int i = 0; i < nb; i++) values[beg + i] = out[i];
        prevCalc = n;
    }

    void UpdateLive(int index, double price, double volume, const std::vector<Candle>& candles) override {
        int n = std::min(index + 1, (int)candles.size());
        if (n <= 0) return;
        if (prevCalc <= 0 || n <= prevCalc) { Calculate(candleSubset(candles, n)); return; }

        int lookback = period + 5;
        int startIdx = std::max(0, prevCalc - lookback);
        values.resize(n, NAN);

        std::vector<double> hi(n), lo(n), cl(n);
        for (int i = 0; i < n; i++) { hi[i] = candles[i].high; lo[i] = candles[i].low; cl[i] = candles[i].close; }

        std::vector<double> out(n);
        int beg = 0, nb = 0;
        TA_CCI(startIdx, n - 1, hi.data(), lo.data(), cl.data(), period, &beg, &nb, out.data());
        for (int i = 0; i < nb; i++) {
            int idx = beg + i;
            if (idx >= prevCalc) values[idx] = out[i];
        }
        prevCalc = n;
    }

    void Render(int off, int count) override {
        if (!visible || values.empty()) return;
        int n = (int)values.size();
        if (off >= n) return;
        int pts = std::min(count, n - off);
        if (pts <= 0) return;

        ImPlot::SetNextLineStyle(color, 1.5f);
        ImPlot::PlotLine("CCI", values.data() + off, pts);
        ImPlot::PushStyleColor(ImPlotCol_Line, ImVec4(1, 0.3f, 0.3f, 0.35f));
        double ob[] = {  100 }; ImPlot::PlotInfLines("##CCI100",  ob, 1, ImPlotInfLinesFlags_Horizontal);
        double os[] = { -100 }; ImPlot::PlotInfLines("##CCI-100", os, 1, ImPlotInfLinesFlags_Horizontal);
        ImPlot::PopStyleColor();
        ImPlot::PushStyleColor(ImPlotCol_Line, ImVec4(0.5f, 0.5f, 0.5f, 0.25f));
        double zr[] = { 0 }; ImPlot::PlotInfLines("##CCI0", zr, 1, ImPlotInfLinesFlags_Horizontal);
        ImPlot::PopStyleColor();
    }

    bool RenderSettings(const std::vector<Candle>& candles) override {
        bool rem = false;
        ImGui::Text("CCI Settings");
        ImGui::Separator();
        if (ImGui::InputInt("Period", &period)) { if (period < 2) period = 2; }
        ImGui::ColorEdit4("Color", (float*)&color);
        if (ImGui::Button("Apply",  ImVec2(-1, 0))) { prevCalc = 0; Calculate(candles); }
        if (ImGui::Button("Remove", ImVec2(-1, 0))) rem = true;
        return rem;
    }

private:
    static std::vector<Candle> candleSubset(const std::vector<Candle>& src, int count) {
        if (count <= 0) return {};
        return std::vector<Candle>(src.begin(), src.begin() + std::min(count, (int)src.size()));
    }
};

// ================================================================
// 6. WILLIAMS %R
// ================================================================
class WilliamsRIndicator : public Indicator {
public:
    std::vector<double> values;
    int prevCalc = 0;

    WilliamsRIndicator(int p = 14) : Indicator("Williams %R", p, ImVec4(0.4f, 0.8f, 1, 1), IND_PANEL) {}

    void Calculate(const std::vector<Candle>& candles) override {
        int n = (int)candles.size();
        values.assign(n, NAN);
        if (n < period) { prevCalc = n; return; }

        std::vector<double> hi(n), lo(n), cl(n);
        for (int i = 0; i < n; i++) { hi[i] = candles[i].high; lo[i] = candles[i].low; cl[i] = candles[i].close; }

        std::vector<double> out(n);
        int beg = 0, nb = 0;
        TA_WILLR(0, n - 1, hi.data(), lo.data(), cl.data(), period, &beg, &nb, out.data());
        for (int i = 0; i < nb; i++) values[beg + i] = out[i];
        prevCalc = n;
    }

    void UpdateLive(int index, double price, double volume, const std::vector<Candle>& candles) override {
        int n = std::min(index + 1, (int)candles.size());
        if (n <= 0) return;
        if (prevCalc <= 0 || n <= prevCalc) { Calculate(candleSubset(candles, n)); return; }

        int lookback = period + 5;
        int startIdx = std::max(0, prevCalc - lookback);
        values.resize(n, NAN);

        std::vector<double> hi(n), lo(n), cl(n);
        for (int i = 0; i < n; i++) { hi[i] = candles[i].high; lo[i] = candles[i].low; cl[i] = candles[i].close; }

        std::vector<double> out(n);
        int beg = 0, nb = 0;
        TA_WILLR(startIdx, n - 1, hi.data(), lo.data(), cl.data(), period, &beg, &nb, out.data());
        for (int i = 0; i < nb; i++) {
            int idx = beg + i;
            if (idx >= prevCalc) values[idx] = out[i];
        }
        prevCalc = n;
    }

    void Render(int off, int count) override {
        if (!visible || values.empty()) return;
        int n = (int)values.size();
        if (off >= n) return;
        int pts = std::min(count, n - off);
        if (pts <= 0) return;

        ImPlot::SetNextLineStyle(color, 1.5f);
        ImPlot::PlotLine("Williams %R", values.data() + off, pts);
        ImPlot::PushStyleColor(ImPlotCol_Line, ImVec4(1, 0.3f, 0.3f, 0.35f));
        double ob[] = { -20 }; ImPlot::PlotInfLines("##WR20", ob, 1, ImPlotInfLinesFlags_Horizontal);
        double os[] = { -80 }; ImPlot::PlotInfLines("##WR80", os, 1, ImPlotInfLinesFlags_Horizontal);
        ImPlot::PopStyleColor();
    }

    bool RenderSettings(const std::vector<Candle>& candles) override {
        bool rem = false;
        ImGui::Text("Williams %%R Settings");
        ImGui::Separator();
        if (ImGui::InputInt("Period", &period)) { if (period < 2) period = 2; }
        ImGui::ColorEdit4("Color", (float*)&color);
        if (ImGui::Button("Apply",  ImVec2(-1, 0))) { prevCalc = 0; Calculate(candles); }
        if (ImGui::Button("Remove", ImVec2(-1, 0))) rem = true;
        return rem;
    }

private:
    static std::vector<Candle> candleSubset(const std::vector<Candle>& src, int count) {
        if (count <= 0) return {};
        return std::vector<Candle>(src.begin(), src.begin() + std::min(count, (int)src.size()));
    }
};

// ================================================================
// 7. MFI (Money Flow Index)
// ================================================================
class MFIIndicator : public Indicator {
public:
    std::vector<double> values;
    int prevCalc = 0;

    MFIIndicator(int p = 14) : Indicator("MFI", p, ImVec4(0.2f, 1, 0.6f, 1), IND_PANEL) {}

    void Calculate(const std::vector<Candle>& candles) override {
        int n = (int)candles.size();
        values.assign(n, NAN);
        if (n < period) { prevCalc = n; return; }

        std::vector<double> hi(n), lo(n), cl(n), vol(n);
        for (int i = 0; i < n; i++) {
            hi[i] = candles[i].high; lo[i] = candles[i].low;
            cl[i] = candles[i].close; vol[i] = (double)candles[i].volume;
        }

        std::vector<double> out(n);
        int beg = 0, nb = 0;
        TA_MFI(0, n - 1, hi.data(), lo.data(), cl.data(), vol.data(), period, &beg, &nb, out.data());
        for (int i = 0; i < nb; i++) values[beg + i] = out[i];
        prevCalc = n;
    }

    void UpdateLive(int index, double price, double volume, const std::vector<Candle>& candles) override {
        int n = std::min(index + 1, (int)candles.size());
        if (n <= 0) return;
        if (prevCalc <= 0 || n <= prevCalc) { Calculate(candleSubset(candles, n)); return; }

        int lookback = period + 5;
        int startIdx = std::max(0, prevCalc - lookback);
        values.resize(n, NAN);

        std::vector<double> hi(n), lo(n), cl(n), vol(n);
        for (int i = 0; i < n; i++) {
            hi[i] = candles[i].high; lo[i] = candles[i].low;
            cl[i] = candles[i].close; vol[i] = (double)candles[i].volume;
        }

        std::vector<double> out(n);
        int beg = 0, nb = 0;
        TA_MFI(startIdx, n - 1, hi.data(), lo.data(), cl.data(), vol.data(), period, &beg, &nb, out.data());
        for (int i = 0; i < nb; i++) {
            int idx = beg + i;
            if (idx >= prevCalc) values[idx] = out[i];
        }
        prevCalc = n;
    }

    void Render(int off, int count) override {
        if (!visible || values.empty()) return;
        int n = (int)values.size();
        if (off >= n) return;
        int pts = std::min(count, n - off);
        if (pts <= 0) return;

        ImPlot::SetNextLineStyle(color, 1.5f);
        ImPlot::PlotLine("MFI", values.data() + off, pts);
        ImPlot::PushStyleColor(ImPlotCol_Line, ImVec4(1, 0.3f, 0.3f, 0.35f));
        double ob[] = { 80 }; ImPlot::PlotInfLines("##MFI80", ob, 1, ImPlotInfLinesFlags_Horizontal);
        double os[] = { 20 }; ImPlot::PlotInfLines("##MFI20", os, 1, ImPlotInfLinesFlags_Horizontal);
        ImPlot::PopStyleColor();
    }

    bool RenderSettings(const std::vector<Candle>& candles) override {
        bool rem = false;
        ImGui::Text("MFI Settings");
        ImGui::Separator();
        if (ImGui::InputInt("Period", &period)) { if (period < 2) period = 2; }
        ImGui::ColorEdit4("Color", (float*)&color);
        if (ImGui::Button("Apply",  ImVec2(-1, 0))) { prevCalc = 0; Calculate(candles); }
        if (ImGui::Button("Remove", ImVec2(-1, 0))) rem = true;
        return rem;
    }

private:
    static std::vector<Candle> candleSubset(const std::vector<Candle>& src, int count) {
        if (count <= 0) return {};
        return std::vector<Candle>(src.begin(), src.begin() + std::min(count, (int)src.size()));
    }
};

// ================================================================
// 8. ROC (Rate of Change)
// ================================================================
class ROCIndicator : public Indicator {
public:
    std::vector<double> values;
    int prevCalc = 0;

    ROCIndicator(int p = 10) : Indicator("ROC", p, ImVec4(1, 0.6f, 0.8f, 1), IND_PANEL) {}

    void Calculate(const std::vector<Candle>& candles) override {
        int n = (int)candles.size();
        values.assign(n, NAN);
        if (n < period + 1) { prevCalc = n; return; }

        std::vector<double> cl(n);
        for (int i = 0; i < n; i++) cl[i] = candles[i].close;

        std::vector<double> out(n);
        int beg = 0, nb = 0;
        TA_ROC(0, n - 1, cl.data(), period, &beg, &nb, out.data());
        for (int i = 0; i < nb; i++) values[beg + i] = out[i];
        prevCalc = n;
    }

    void UpdateLive(int index, double price, double volume, const std::vector<Candle>& candles) override {
        int n = std::min(index + 1, (int)candles.size());
        if (n <= 0) return;
        if (prevCalc <= 0 || n <= prevCalc) { Calculate(candleSubset(candles, n)); return; }

        int lookback = period + 5;
        int startIdx = std::max(0, prevCalc - lookback);
        values.resize(n, NAN);

        std::vector<double> cl(n);
        for (int i = 0; i < n; i++) cl[i] = candles[i].close;

        std::vector<double> out(n);
        int beg = 0, nb = 0;
        TA_ROC(startIdx, n - 1, cl.data(), period, &beg, &nb, out.data());
        for (int i = 0; i < nb; i++) {
            int idx = beg + i;
            if (idx >= prevCalc) values[idx] = out[i];
        }
        prevCalc = n;
    }

    void Render(int off, int count) override {
        if (!visible || values.empty()) return;
        int n = (int)values.size();
        if (off >= n) return;
        int pts = std::min(count, n - off);
        if (pts <= 0) return;

        ImPlot::SetNextLineStyle(color, 1.5f);
        ImPlot::PlotLine("ROC", values.data() + off, pts);
        ImPlot::PushStyleColor(ImPlotCol_Line, ImVec4(0.5f, 0.5f, 0.5f, 0.25f));
        double zero[] = { 0 };
        ImPlot::PlotInfLines("##ROC0", zero, 1, ImPlotInfLinesFlags_Horizontal);
        ImPlot::PopStyleColor();
    }

    bool RenderSettings(const std::vector<Candle>& candles) override {
        bool rem = false;
        ImGui::Text("ROC Settings");
        ImGui::Separator();
        if (ImGui::InputInt("Period", &period)) { if (period < 1) period = 1; }
        ImGui::ColorEdit4("Color", (float*)&color);
        if (ImGui::Button("Apply",  ImVec2(-1, 0))) { prevCalc = 0; Calculate(candles); }
        if (ImGui::Button("Remove", ImVec2(-1, 0))) rem = true;
        return rem;
    }

private:
    static std::vector<Candle> candleSubset(const std::vector<Candle>& src, int count) {
        if (count <= 0) return {};
        return std::vector<Candle>(src.begin(), src.begin() + std::min(count, (int)src.size()));
    }
};

// ================================================================
// 9. OBV (On Balance Volume)
// ================================================================
class OBVIndicator : public Indicator {
public:
    std::vector<double> values;
    int prevCalc = 0;

    OBVIndicator() : Indicator("OBV", 1, ImVec4(0.5f, 1, 0.5f, 1), IND_PANEL) {}

    void Calculate(const std::vector<Candle>& candles) override {
        int n = (int)candles.size();
        values.assign(n, NAN);
        if (n < 2) { prevCalc = n; return; }

        std::vector<double> cl(n), vol(n);
        for (int i = 0; i < n; i++) { cl[i] = candles[i].close; vol[i] = (double)candles[i].volume; }

        std::vector<double> out(n);
        int beg = 0, nb = 0;
        TA_OBV(0, n - 1, cl.data(), vol.data(), &beg, &nb, out.data());
        for (int i = 0; i < nb; i++) values[beg + i] = out[i];
        prevCalc = n;
    }

    void UpdateLive(int index, double price, double volume, const std::vector<Candle>& candles) override {
        int n = std::min(index + 1, (int)candles.size());
        if (n <= 0) return;
        if (prevCalc <= 0 || n <= prevCalc) { Calculate(candleSubset(candles, n)); return; }

        // OBV perlu data dari awal karena cumulative
        int startIdx = 0;
        values.resize(n, NAN);

        std::vector<double> cl(n), vol(n);
        for (int i = 0; i < n; i++) { cl[i] = candles[i].close; vol[i] = (double)candles[i].volume; }

        std::vector<double> out(n);
        int beg = 0, nb = 0;
        TA_OBV(startIdx, n - 1, cl.data(), vol.data(), &beg, &nb, out.data());
        for (int i = 0; i < nb; i++) {
            int idx = beg + i;
            if (idx >= prevCalc) values[idx] = out[i];
        }
        prevCalc = n;
    }

    void Render(int off, int count) override {
        if (!visible || values.empty()) return;
        int n = (int)values.size();
        if (off >= n) return;
        int pts = std::min(count, n - off);
        if (pts <= 0) return;

        ImPlot::SetNextLineStyle(color, 1.5f);
        ImPlot::PlotLine("OBV", values.data() + off, pts);
    }

    bool RenderSettings(const std::vector<Candle>& candles) override {
        bool rem = false;
        ImGui::Text("OBV Settings");
        ImGui::Separator();
        ImGui::ColorEdit4("Color", (float*)&color);
        if (ImGui::Button("Remove", ImVec2(-1, 0))) rem = true;
        return rem;
    }

private:
    static std::vector<Candle> candleSubset(const std::vector<Candle>& src, int count) {
        if (count <= 0) return {};
        return std::vector<Candle>(src.begin(), src.begin() + std::min(count, (int)src.size()));
    }
};

// ================================================================
// 10. SUPERTREND (Custom — incremental)
// ================================================================
class SupertrendIndicator : public Indicator {
public:
    std::vector<double> bullLine, bearLine;
    std::vector<int>    dir;

    int    atrPeriod  = 10;
    double multiplier = 3.0;
    ImVec4 colorBull = ImVec4(0.15f, 0.85f, 0.40f, 1.0f);
    ImVec4 colorBear = ImVec4(0.95f, 0.25f, 0.25f, 1.0f);

    int prevCalc = 0;

    SupertrendIndicator() : Indicator("Supertrend", 10, ImVec4(0, 1, 0.5f, 1), IND_OVERLAY) {}

    void Calculate(const std::vector<Candle>& candles) override {
        int n = (int)candles.size();
        bullLine.assign(n, NAN); bearLine.assign(n, NAN); dir.assign(n, 1);
        if (n < atrPeriod + 2) { prevCalc = n; return; }

        // ATR
        std::vector<double> atr(n, 0.0);
        double trSum = 0.0;
        for (int i = 1; i <= atrPeriod; i++) {
            double tr = std::max({ candles[i].high - candles[i].low,
                                   std::abs(candles[i].high - candles[i-1].close),
                                   std::abs(candles[i].low - candles[i-1].close) });
            trSum += tr;
        }
        atr[atrPeriod] = trSum / atrPeriod;
        for (int i = atrPeriod + 1; i < n; i++) {
            double tr = std::max({ candles[i].high - candles[i].low,
                                   std::abs(candles[i].high - candles[i-1].close),
                                   std::abs(candles[i].low - candles[i-1].close) });
            atr[i] = (atr[i-1] * (atrPeriod - 1) + tr) / atrPeriod;
        }

        // Bands
        std::vector<double> upperBasic(n, 0.0), lowerBasic(n, 0.0);
        for (int i = atrPeriod; i < n; i++) {
            double hl2 = (candles[i].high + candles[i].low) * 0.5;
            upperBasic[i] = hl2 + multiplier * atr[i];
            lowerBasic[i] = hl2 - multiplier * atr[i];
        }

        std::vector<double> upperF(n, 0.0), lowerF(n, 0.0);
        upperF[atrPeriod] = upperBasic[atrPeriod];
        lowerF[atrPeriod] = lowerBasic[atrPeriod];

        for (int i = atrPeriod + 1; i < n; i++) {
            upperF[i] = (upperBasic[i] < upperF[i-1] || candles[i-1].close > upperF[i-1])
                        ? upperBasic[i] : upperF[i-1];
            lowerF[i] = (lowerBasic[i] > lowerF[i-1] || candles[i-1].close < lowerF[i-1])
                        ? lowerBasic[i] : lowerF[i-1];
        }

        dir[atrPeriod] = 1;
        for (int i = atrPeriod + 1; i < n; i++) {
            int prev = dir[i-1];
            if      (prev == -1 && candles[i].close > upperF[i-1]) dir[i] =  1;
            else if (prev ==  1 && candles[i].close < lowerF[i-1]) dir[i] = -1;
            else                                                   dir[i] =  prev;
            if (dir[i] ==  1) bullLine[i] = lowerF[i];
            else              bearLine[i] = upperF[i];
        }
        prevCalc = n;
    }

    // ── Supertrend incremental: mulai dari prevCalc - lookback ───
    void UpdateLive(int index, double price, double volume, const std::vector<Candle>& candles) override {
        int n = std::min(index + 1, (int)candles.size());
        if (n <= 0) return;
        if (prevCalc <= 0 || n <= prevCalc) { Calculate(candleSubset(candles, n)); return; }

        // Supertrend butuh semua history karena lookback band
        // tapi cukup mulai dari prevCalc - (atrPeriod+1) untuk akurat
        int startFrom = std::max(0, prevCalc - atrPeriod - 5);
        bullLine.resize(n, NAN); bearLine.resize(n, NAN); dir.resize(n, 1);

        // ATR dari startFrom
        std::vector<double> atr(n, 0.0);
        if (startFrom <= atrPeriod) {
            // Butuh hitung dari awal
            double trSum = 0.0;
            for (int i = 1; i <= atrPeriod && i < n; i++) {
                double tr = std::max({ candles[i].high - candles[i].low,
                                       std::abs(candles[i].high - candles[i-1].close),
                                       std::abs(candles[i].low - candles[i-1].close) });
                trSum += tr;
            }
            if (n > atrPeriod) atr[atrPeriod] = trSum / atrPeriod;
            for (int i = atrPeriod + 1; i < n; i++) {
                double tr = std::max({ candles[i].high - candles[i].low,
                                       std::abs(candles[i].high - candles[i-1].close),
                                       std::abs(candles[i].low - candles[i-1].close) });
                atr[i] = (atr[i-1] * (atrPeriod - 1) + tr) / atrPeriod;
            }
        } else {
            for (int i = startFrom; i < n; i++) {
                double tr = std::max({ candles[i].high - candles[i].low,
                                       std::abs(candles[i].high - candles[i-1].close),
                                       std::abs(candles[i].low - candles[i-1].close) });
                if (i == startFrom) atr[i] = tr; // Approx untuk awal
                else atr[i] = (atr[i-1] * (atrPeriod - 1) + tr) / atrPeriod;
            }
        }

        // Bands + direction dari startFrom
        std::vector<double> upperF(n, 0.0), lowerF(n, 0.0);
        if (startFrom > 0) {
            upperF[startFrom-1] = bullLine[startFrom-1];
            if (std::isnan(upperF[startFrom-1])) upperF[startFrom-1] = candles[startFrom-1].high;
            lowerF[startFrom-1] = bearLine[startFrom-1];
            if (std::isnan(lowerF[startFrom-1])) lowerF[startFrom-1] = candles[startFrom-1].low;
        }

        for (int i = std::max(atrPeriod, startFrom); i < n; i++) {
            double hl2 = (candles[i].high + candles[i].low) * 0.5;
            double ub = hl2 + multiplier * atr[i];
            double lb = hl2 - multiplier * atr[i];

            if (i > startFrom || startFrom == 0) {
                upperF[i] = (ub < upperF[i-1] || candles[i-1].close > upperF[i-1]) ? ub : upperF[i-1];
                lowerF[i] = (lb > lowerF[i-1] || candles[i-1].close < lowerF[i-1]) ? lb : lowerF[i-1];
            }

            if (i > 0) {
                int prev = dir[i-1];
                if (prev == -1 && candles[i].close > upperF[i-1]) dir[i] =  1;
                else if (prev == 1 && candles[i].close < lowerF[i-1]) dir[i] = -1;
                else dir[i] = prev;
            }

            if (dir[i] == 1)  bullLine[i] = lowerF[i];
            else               bearLine[i] = upperF[i];
        }
        prevCalc = n;
    }

    void Render(int off, int count) override {
        if (!visible || bullLine.empty()) return;
        int n = (int)bullLine.size();
        if (off >= n) return;
        int pts = std::min(count, n - off);
        if (pts <= 0) return;

        ImPlot::SetNextLineStyle(colorBull, 2.5f);
        ImPlot::PlotLine("Supertrend Bull", bullLine.data() + off, pts);
        ImPlot::SetNextLineStyle(colorBear, 2.5f);
        ImPlot::PlotLine("Supertrend Bear", bearLine.data() + off, pts);
    }

    bool RenderSettings(const std::vector<Candle>& candles) override {
        bool rem = false;
        ImGui::Text("Supertrend Settings");
        ImGui::Separator();
        if (ImGui::InputInt("ATR Period", &atrPeriod)) {
            if (atrPeriod < 1) atrPeriod = 1;
            if (atrPeriod > 200) atrPeriod = 200;
        }
        if (ImGui::InputDouble("Multiplier", &multiplier, 0.1, 0.5, "%.1f")) {
            if (multiplier < 0.1) multiplier = 0.1;
            if (multiplier > 20.0) multiplier = 20.0;
        }
        ImGui::ColorEdit4("Bull Color", (float*)&colorBull);
        ImGui::ColorEdit4("Bear Color", (float*)&colorBear);
        if (ImGui::Button("Apply",  ImVec2(-1, 0))) { prevCalc = 0; Calculate(candles); }
        if (ImGui::Button("Remove", ImVec2(-1, 0))) rem = true;
        return rem;
    }

private:
    static std::vector<Candle> candleSubset(const std::vector<Candle>& src, int count) {
        if (count <= 0) return {};
        return std::vector<Candle>(src.begin(), src.begin() + std::min(count, (int)src.size()));
    }
};
