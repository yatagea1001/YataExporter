#include "Indicators.h"

// 1. Definisi Variabel Global (Disini memori dialokasikan)
std::vector<Indicator*> g_activeIndicators;

// 2. Implementasi Fungsi AddIndicator
void AddIndicator(Indicator* newInd, const std::vector<Candle>& currentCandles) {
    if (newInd) {
        // Hitung dulu sebelum dimasukkan agar siap tampil
        newInd->Calculate(currentCandles);
        g_activeIndicators.push_back(newInd);
    }
}

// 3. Implementasi Fungsi ClearIndicators
void ClearIndicators() {
    for (auto* ind : g_activeIndicators) {
        delete ind; // Hapus memori heap
    }
    g_activeIndicators.clear();
}

// 4. Implementasi Fungsi RecalculateAllIndicators
void RecalculateAllIndicators(const std::vector<Candle>& currentCandles) {
    for (auto* ind : g_activeIndicators) {
        ind->Calculate(currentCandles);
    }
}