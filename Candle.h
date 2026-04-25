#pragma once
#include <string>
#include <vector>

// ─────────────────────────────────────────────────
// Satu baris harga pada footprint (bid/ask per level)
// Diisi oleh wasm_push_footprint() dari tick live
// ─────────────────────────────────────────────────
struct FootprintLevel {
    double price   = 0.0;
    double buyVol  = 0.0;   // aggressor BUY  (side "B" dari Hyperliquid)
    double sellVol = 0.0;   // aggressor SELL (side "A" dari Hyperliquid)
};

struct Candle {
    double open   = 0.0;
    double high   = 0.0;
    double low    = 0.0;
    double close  = 0.0;
    std::string datetime;  // misalnya "2025-10-24 10:15"
    double time   = 0.0;   // 🧭 epoch time (convert dari datetime)
    double volume = 0.0;

    // ── ORDER FLOW DATA ──────────────────────────
    // Diisi dari tick live → wasm_push_footprint()
    // Merge ke HTF saat wasm_rebuild_all_htfs()
    // Sorted descending by price (high → low)
    std::vector<FootprintLevel> footprint;
};
