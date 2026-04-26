// ============================================================
// JarvisBridge.h — Bridge antara Jarvis AI dan Chart Engine
// Phase 6: Drawing + Swing Analysis + Key Levels + Auto-Context
//
// Fungsi-fungsi di sini DIDEKLARASI di sini,
// tapi DI-DEFINE/ISI di main.cpp
// (karena butuh akses ke g_symbol, g_allCandles, g_shapeMgr, dll)
// ============================================================
#pragma once

#include <string>
#include "nlohmann/json.hpp"

using json = nlohmann::json;

// ============================================================
// JARVIS BRIDGE FUNCTIONS
// Dipanggil dari AiAssistant.h saat AI mengembalikan actions[]
// Didefinisikan di main.cpp
// ============================================================

// Switch symbol di chart (buka pair baru, load data via websocket)
void JarvisBridge_AddSymbol(const std::string& symbol);

// Tambah indicator ke active chart
// indicator_name: "sma", "ema", "rsi", "macd", "bb", dll (case-insensitive)
// period: lookback period (default 14)
void JarvisBridge_AddIndicator(const std::string& indicator_name, int period);

// Ambil symbol yang sedang aktif (untuk konteks AI)
std::string JarvisBridge_GetActiveSymbol();

// Auto-context ringan (selalu dikirim setiap chat)
// Return JSON: {"symbol":"XAUUSD","timeframe":"M15","price":2345,
//               "indicators":[...],"trend":"BULLISH",
//               "resistance":2360,"support":2310,
//               "last_swing_high":2358,"last_swing_low":2335}
std::string JarvisBridge_GetChartStatus();

// Full swing analysis data (untuk tool chart_analyze_swing)
// Return JSON: {"symbol":...,"total_candles":5000,
//               "swing_highs":[{index,price,datetime,label},...],
//               "swing_lows":[...],"trend":"BULLISH",
//               "current_candle":{O,H,L,C}}
std::string JarvisBridge_GetSwingAnalysis();

// Key support/resistance levels (untuk tool chart_get_key_levels)
// Return JSON: {"symbol":...,"resistances":[{price,test_count,distance_pct,strength},...],
//               "supports":[...],"nearest_resistance":2360,"nearest_support":2310}
std::string JarvisBridge_GetKeyLevels();

// ============================================================
// DRAW TOOLS — AI bisa menggambar langsung di chart
// Dipanggil dari AiAssistant.h saat AI mengembalikan draw actions
// Didefinisikan di main.cpp (akses g_shapeMgr, g_allCandles)
// ============================================================

// Gambar garis di chart (trendline, support/resistance line)
// time0/time1: index candle (-1 = candle terakhir)
// color: hex string, contoh: "#FFD700"
// extendLeft/extendRight: perpanjang garis
// label: teks opsional di ujung garis
void JarvisBridge_DrawLine(double time0, double price0, double time1, double price1,
                           const std::string& color = "#FFD700",
                           float thickness = 1.5f,
                           bool extendLeft = false, bool extendRight = false,
                           const std::string& label = "");

// Gambar rectangle/zone di chart (order block, supply/demand zone)
// fillColor: hex string warna fill area
// fillOpacity: transparansi fill (0.0 - 1.0)
// label: teks opsional di dalam rectangle
void JarvisBridge_DrawRect(double time0, double price0, double time1, double price1,
                           const std::string& color = "#4488FF",
                           const std::string& fillColor = "#4488FF",
                           float fillOpacity = 0.15f,
                           const std::string& label = "");

// Gambar Fibonacci Retracement di chart
void JarvisBridge_DrawFib(double time0, double price0, double time1, double price1,
                           const std::string& color = "#FFD700");

// Tulis teks/label di chart
void JarvisBridge_DrawText(double time, double price,
                           const std::string& text = "Label",
                           const std::string& color = "#FFFFFF",
                           float fontSize = 16.0f);

// Gambar Elliot Wave / pola multi-titik di chart
// times: kosong = tersebar merata, isi = index candle tiap titik
void JarvisBridge_DrawElliot(const std::vector<double>& times,
                             const std::vector<double>& prices,
                             const std::string& color = "#FF9900",
                             float thickness = 1.5f);
