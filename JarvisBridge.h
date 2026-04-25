// ============================================================
// JarvisBridge.h — Bridge antara Jarvis AI dan Chart Engine
// Phase 5: Swing Analysis + Key Levels + Auto-Context
//
// Fungsi-fungsi di sini DIDEKLARASI di sini,
// tapi DI-DEFINE/ISI di main.cpp
// (karena butuh akses ke g_symbol, g_allCandles, dll)
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
