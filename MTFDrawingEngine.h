#pragma once
#include "CDrawingManager.h"
#include "GlobalShapeManager.h"
#include "Candle.h"
#include <map>
#include <mutex>
#include <vector>
#include <string>

// 🔥 BARU: Include file spesialis teks kita
extern CDrawingManager g_draw;
extern GlobalShapeManager g_shapes;

// ================================================================
// 🔹 MTFDrawingEngine — epoch-based coordinate sync
// ================================================================
class MTFDrawingEngine {
public:
    CDrawingManager localDrawer;

   // FILE: MTFDrawingEngine.h

void RenderForActiveTF(const std::string& activeTF,
                       const std::map<std::string, std::vector<Candle>>& allCandles, // Ini akan berisi Data Replay saat mode Replay
                       std::mutex& candlesMutex) {
    
    std::lock_guard<std::mutex> lock(candlesMutex);
    
    auto it = allCandles.find(activeTF);
    if (it == allCandles.end()) return;
    
    // 'candles' di sini adalah SUMBER KEBENARAN (bisa Replay Slice atau Full Live)
    const std::vector<Candle>& candles = it->second; 
    if (candles.empty()) return;

    ImDrawList* drawList = ImPlot::GetPlotDrawList();

    // =========================================================
    // 🛑 PERBAIKAN DI SINI
    // =========================================================
    g_draw.RenderTFOverlays(
        activeTF,
        candles,  // <--- KITA LEMPAR DATA CANDLE YANG BENAR KE SINI
        drawList,
        [](const std::string& t) {
            try { return (float)std::stod(t); }
            catch (...) { return 0.0f; }
        },
        [](double price) { return (float)price; }
    );

    // ... sisa kode render localDrawer dan g_shapes biarkan saja ...


        // =========================================================
        // 🔹 Render shape aktif (editing user)
        // =========================================================
        localDrawer.Render(candles);

        // =========================================================
        // 🌍 RENDER UTAMA (GLOBAL SHAPES) — sudah di-handle di dalam
        // g_draw.Render(candles, false) via HandleEditing + g_shapes.Render.
        // JANGAN panggil g_shapes.Render lagi di sini → cegah double render!
        // =========================================================
        
        // 3. Render Popup Edit (Warna, Font Size, dll)
        g_draw.RenderShapePopup();
    }
};