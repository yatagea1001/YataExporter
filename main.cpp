// ==================================================================================
// MAIN.CPP - TRADING SYSTEM ENGINE (PORTED FOR WEB/WASM + DESKTOP)
// ==================================================================================
#define IMGUI_USE_WCHAR32 // Membuka jalur 32-bit untuk Emoji tingkat tinggi
#ifndef __EMSCRIPTEN__
#include <glad/glad.h>
#endif

#include "imgui.h"
#include "implot.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <GLFW/glfw3.h>
#include <vector>
#include <string>
#include "ta-lib/include/ta_libc.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <chrono>
#include <optional>
#include <algorithm>
#include <cmath>
#include <map>
#include <cctype>
#include <cstdint>
#include <cstring>

#include <mutex>
#include <set>        // untuk per-symbol trade iteration
#include <functional> // 🔥 WAJIB: Untuk membungkus Main Loop
#ifdef __EMSCRIPTEN__
    #include <emscripten.h>
    #include <emscripten/html5.h> // <--- INI WAJIB DI ATAS
#endif


// 🔥 WAJIB: Header untuk WebAssembly
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
EM_JS(void, UpdateWebCursor, (int cursor_type), {
    var canvas = document.querySelector('canvas.emscripten') || document.querySelector('canvas');
    if (!canvas) return;

    // Mapping tipe kursor ImGui ke CSS murni
    switch(cursor_type) {
        case -1: // ImGuiMouseCursor_None (saat crosshair chart aktif)
            canvas.style.cursor = 'none'; 
            break;
        case 0:  // ImGuiMouseCursor_Arrow
            canvas.style.cursor = 'default'; 
            break;
        case 1:  // ImGuiMouseCursor_TextInput
            canvas.style.cursor = 'text'; 
            break;
        case 2:  // ImGuiMouseCursor_ResizeAll (Panah 4 arah)
            canvas.style.cursor = 'move'; 
            break;
        case 3:  // ImGuiMouseCursor_ResizeNS (Atas - Bawah)
            canvas.style.cursor = 'ns-resize'; 
            break;
        case 4:  // ImGuiMouseCursor_ResizeEW (Kiri - Kanan)
            canvas.style.cursor = 'ew-resize'; 
            break;
        case 5:  // ImGuiMouseCursor_ResizeNESW (Diagonal 1)
            canvas.style.cursor = 'nesw-resize'; 
            break;
        case 6:  // ImGuiMouseCursor_ResizeNWSE (Diagonal 2)
            canvas.style.cursor = 'nwse-resize'; 
            break;
        case 7:  // ImGuiMouseCursor_Hand (Jari klik)
            canvas.style.cursor = 'pointer'; 
            break;
        case 8:  // ImGuiMouseCursor_NotAllowed (Dilarang)
            canvas.style.cursor = 'not-allowed'; 
            break;
        default:
            canvas.style.cursor = 'default'; 
            break;
    }
});
#endif
#include <emscripten/fetch.h>
#include "nlohmann/json.hpp"

using json = nlohmann::json;
extern "C" {
    EMSCRIPTEN_KEEPALIVE
    void wasm_login_success(const char* email) {
        // Simpan email ke variabel global C++ untuk ditampilkan di UI ImGui
        printf("User %s berhasil login ke Engine C++\n", email);
    }
}



// --- HEADER CUSTOM ---
#include "Candle.h"
#include "TextureHelper.h"
#include "ToolbarUI.h"
#include "ChartCanvas.h"

// ⚠️ URUTAN INCLUDE VITAL
#include "GlobalShapeManager.h"

GlobalShapeManager g_shapes;
#include "ElliotDrawing.h"
#include "CDrawingManager.h"
CDrawingManager g_draw;

// ====== CROSSHAIR: Include renderer (per-tab via tab->crosshair) ======
#include "CrosshairRenderer.h"
// g_crossState masih dipakai sementara oleh code drawing (backward compat)
CrosshairState g_crossState;
std::map<int, CDrawingManager> g_tabDrawMgrs; // per-tab draw manager untuk tab non-utama


#include "MTFDrawingEngine.h"
MTFDrawingEngine g_mtfDraw;

#include "TradeModule.h"
#include "TradePanelUI.h"

// ====== TRADE HISTORY: Include setelah TradeModule.h ======
#include "TradeHistory.h"
#include "HistoryPanelUI.h"
#include "TradeSettingsUI.h"





#include "CReplayManager.h"
CReplayManager g_replay;

#include "ChartAxisTicks.h"
ChartAxisTicks g_axisTicks;

#include "GPUCandleRenderer.h"
GPUCandleRenderer g_gpuRenderer;

// OrderFlowRenderer.h sudah di-include via MultiChart.h (per-tab instance)
// #include "OrderFlowRenderer.h"  ← tidak perlu lagi di sini
// Tidak ada lagi g_showFootprint / g_fpMode global

#include "Indicators.h"
#include "UI_IndicatorList.h"
#include "MultiChart.h"      // 🗂️ Multi chart tab manager
#include "OrderFlowSettingsUI.h"  // ⚙️ Order Flow settings panel
#include "UI_OrderBook.h"          // 📊 Order Book real-time panel
#include "GoToLive.h"        // 🎯 Go To Live per-tab animation
#include "ChartInteraction.h"  // 🕹️ Reusable chart interaction (drag/zoom/Y-resize/touch/panY)
#include "UI_ChartTabs.h"    // 🖥️ Tab UI + tombol [+ Chart]
#include "SymbolPickerUI.h"  // 🔍 Manajer simbol terpusat
#include "UI_ObjectTree.h"  // 🗂️ Object Tree Panel (Drawing List) + Right Icon Bar
#include "CandleStyleManager.h"  // 🎨 Manager style candle/line/area
#include "DisplaySettingsUI.h"    // Pengaturan Tampilan (Font, Tema, Warna)
#include "CreatorEngine.h" // <--- Pastikan ini ada
#include "MarketWatchPanel.h"
#include "AiAssistant.h" 
#include "JarvisBridge.h" 



// ===================================================
// 🌍 GLOBAL STATE
// ===================================================
#ifndef __EMSCRIPTEN__
ix::WebSocket ws;
#endif
// --- HEADER ---
#include <sys/stat.h> // Untuk mkdir
// GLOBAL VARIABLE (Biar bisa diatur dari JS)
bool g_showCreatorMode = false; // Default MATI (User biasa gak lihat)
static bool isAnimatingToLive = false; 
static double animFloatingIndex = 0.0; 
 bool IsCursorOnBorder() {
        ImGuiMouseCursor cursor = ImGui::GetMouseCursor();
        
        // Cek apakah kursor saat ini adalah kursor untuk me-resize layout/window
        return (cursor == ImGuiMouseCursor_ResizeEW ||   // Kiri-Kanan <->
                cursor == ImGuiMouseCursor_ResizeNS ||   // Atas-Bawah ^v
                cursor == ImGuiMouseCursor_ResizeNWSE || // Diagonal 
                cursor == ImGuiMouseCursor_ResizeNESW || // Diagonal /
                cursor == ImGuiMouseCursor_ResizeAll);   // Panah 4 arah
    }
// =========================================================
// 🌍 GLOBAL FONT MANAGER (ARSENAL FONT)
// =========================================================
// Kita siapkan slot untuk 3 jenis font
ImFont* g_fontBold   = nullptr;  // Bold dari Roboto.ttf FontNo=1
ImFont* g_fontItalic = nullptr;  // Italic dari Roboto.ttf FontNo=2
ImFont* g_fonts[3]; 
const char* g_fontNames[] = { "MODERN (Roboto)", "CLASSIC (Times)", "HACKER (Code)" };
int g_selectedFontIdx = 0; // 0 = Default
// FUNGSI RAHASIA (Dipanggil dari JS my_dev_logic.js)
extern "C" {
    #ifdef __EMSCRIPTEN__
    EMSCRIPTEN_KEEPALIVE
    #endif
    void ToggleCreatorMode(bool active) {
        g_showCreatorMode = active;
        printf("🛠️ Creator Mode Set: %d\n", active);
    }
}
// --- Toolbar & UI Scale ---
float g_iconSize = 38.0f;           // Default Toolbar Size
float g_tradePanelScale = 1.0f;     // Default Trade Panel Scale

// --- Warna Trading (Buy/Sell) ---
ImVec4 g_colBuy  = ImVec4(0.0f, 0.6f, 0.2f, 1.0f); // Hijau
ImVec4 g_colSell = ImVec4(0.8f, 0.1f, 0.1f, 1.0f); // Merah

// --- Live Tick State (untuk warna kedip Trade Panel) ---
struct LiveTickState {
    double price  = 0.0;
    double time   = 0.0;
    bool   hasNew = false;
};
static LiveTickState g_liveTick;

// --- Warna Tema UI (Background/Text) ---
bool g_isLightMode = false; // False = Dark Mode, True = Light Mode
ImVec4 g_colorBg   = ImVec4(0.1f, 0.1f, 0.1f, 1.0f); // Background Utama
ImVec4 g_colorText = ImVec4(1.0f, 1.0f, 1.0f, 1.0f); // Warna Teks
ImVec4 g_colorPanel= ImVec4(0.15f, 0.15f, 0.15f, 1.0f); // Warna Panel/Child
ImVec4 g_colorHeader = ImVec4(0.2f, 0.2f, 0.2f, 1.0f);
// Update fungsi Preset biar header-nya otomatis menyesuaikan
void ApplyThemePreset(bool light) {
    g_isLightMode = light;
    if (light) {
        // LIGHT MODE
        g_colorBg     = ImVec4(0.95f, 0.95f, 0.95f, 1.0f); 
        g_colorText   = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);    
        g_colorPanel  = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);    
        g_colorHeader = ImVec4(0.9f, 0.9f, 0.9f, 1.0f); // Abu terang buat header
        // Update warna candle juga biar cocok
        g_gpuRenderer.colorBull = ImVec4(0.0f, 0.6f, 0.3f, 1.0f); // Hijau Gelap
        g_gpuRenderer.colorBear = ImVec4(0.8f, 0.2f, 0.2f, 1.0f); // Merah
        ImGui::StyleColorsLight(); 
    } else {
        // DARK MODE
        g_colorBg     = ImVec4(0.08f, 0.08f, 0.08f, 1.0f); 
        g_colorText   = ImVec4(0.9f, 0.9f, 0.9f, 1.0f);    
        g_colorPanel  = ImVec4(0.12f, 0.12f, 0.12f, 1.0f); 
        g_colorHeader = ImVec4(0.15f, 0.15f, 0.15f, 1.0f); // Abu gelap buat header
        // Warna candle default dark mode
        g_gpuRenderer.colorBull = ImVec4(0.0f, 1.0f, 0.0f, 1.0f); // Hijau Neon
        g_gpuRenderer.colorBear = ImVec4(1.0f, 0.0f, 0.0f, 1.0f); // Merah Neon
        ImGui::StyleColorsDark(); 
    }
}
// ==========================================================
// [YATA PROTOCOL] DEKLARASI FUNGSI GLOBAL (ANTI-ERROR)
// ==========================================================
// Kita kenalkan dulu struktur Candle agar sinkron dengan Desktop


// Status untuk sinkronisasi JS
// 0 = Aman, 1 = Update Tick, 2 = GAP DETECTED (Minta Resync)
extern "C" {
    EMSCRIPTEN_KEEPALIVE int wasm_get_data_status(); 
}

int global_data_status = 0;
/// ==================================================================================
// 2. FUNGSI LOAD & SAVE SETTINGS (JSON)
// ==================================================================================

extern "C" {
    EMSCRIPTEN_KEEPALIVE
    void LoadSettings() {
        std::ifstream i("/data/user_settings.json"); // Path IDBFS
        if (i.is_open()) {
            json j;
            i >> j;
            
            // --- LOAD UKURAN ---
            if (j.contains("icon_size"))   g_iconSize = j["icon_size"];
            if (j.contains("trade_scale")) g_tradePanelScale = j["trade_scale"];
            
            // --- LOAD WARNA TRADING ---
            if (j.contains("col_buy")) {
                auto c = j["col_buy"]; g_colBuy = ImVec4(c[0], c[1], c[2], c[3]);
            }
            if (j.contains("col_sell")) {
                auto c = j["col_sell"]; g_colSell = ImVec4(c[0], c[1], c[2], c[3]);
            }

            // --- LOAD TEMA WARNA ---
            if (j.contains("is_light")) g_isLightMode = j["is_light"];
            if (j.contains("font_idx"))  g_selectedFontIdx = j["font_idx"];
            
            // 1. Reset ke Preset dulu (Supaya kalau ada warna baru, dia dapet defaultnya)
            ApplyThemePreset(g_isLightMode); 

            // 2. Timpa dengan Custom Color User (Kalau ada di file save)
            if (j.contains("col_bg")) {
                auto c = j["col_bg"]; g_colorBg = ImVec4(c[0], c[1], c[2], c[3]);
            }
            if (j.contains("col_text")) {
                auto c = j["col_text"]; g_colorText = ImVec4(c[0], c[1], c[2], c[3]);
            }
            if (j.contains("col_panel")) {
                auto c = j["col_panel"]; g_colorPanel = ImVec4(c[0], c[1], c[2], c[3]);
            }
            // 🔥 INI BARU: Load Warna Header/Tab
            if (j.contains("col_header")) {
                auto c = j["col_header"]; g_colorHeader = ImVec4(c[0], c[1], c[2], c[3]);
            }

            printf("✅ LOAD SUKSES: Settings & Theme Loaded!\n");
            // Apply font setelah load
            g_selectedFontIdx = std::clamp(g_selectedFontIdx, 0, 2);
            if (g_fonts[g_selectedFontIdx])
                ImGui::GetIO().FontDefault = g_fonts[g_selectedFontIdx];
            i.close();
        } else {
            printf("⚠️ Settings belum ada, pakai default.\n");
            ApplyThemePreset(false); // Default Dark
        }
    }
}

// --- FUNGSI SAVE ---
void SaveSettings() {
    json j;
    // Simpan Ukuran
    j["icon_size"] = g_iconSize;
    j["trade_scale"] = g_tradePanelScale;
    
    // Simpan Warna Trading
    j["col_buy"]  = { g_colBuy.x, g_colBuy.y, g_colBuy.z, g_colBuy.w };
    j["col_sell"] = { g_colSell.x, g_colSell.y, g_colSell.z, g_colSell.w };

    // Simpan Tema
    j["is_light"]  = g_isLightMode;
    j["col_bg"]    = { g_colorBg.x, g_colorBg.y, g_colorBg.z, g_colorBg.w };
    j["col_text"]  = { g_colorText.x, g_colorText.y, g_colorText.z, g_colorText.w };
    j["col_panel"] = { g_colorPanel.x, g_colorPanel.y, g_colorPanel.z, g_colorPanel.w };
    
    // 🔥 INI BARU: Simpan Warna Header
    j["col_header"] = { g_colorHeader.x, g_colorHeader.y, g_colorHeader.z, g_colorHeader.w };

    // Simpan Font
    j["font_idx"] = g_selectedFontIdx;

    // Tulis ke File System
    std::ofstream o("/data/user_settings.json");
    o << j << std::endl;
    o.close();

    printf("💾 Menyimpan settings lengkap ke IndexedDB...\n");

    // SYNC: Paksa browser simpan ke harddisk
    #ifdef __EMSCRIPTEN__
        EM_ASM(
            FS.syncfs(false, function(err) {
                if(err) console.log("Gagal Save:", err);
                else console.log("Sukses Save ke Disk!");
            });
        );
    #endif
}
MarketWatchPanel g_marketWatch;  // Logic Keuangan (Database)   // Logic Visual (Garis Geser)

ImTextureID texIconIndicator = 0;
ImTextureID texIconSymbol    = 0;
ImTextureID texCandleStyle   = 0;   // 🕯️ candle.png — icon candle style button

ImTextureID texCursor = 0;
ImTextureID texLine = 0;
ImTextureID texFib = 0;
ImTextureID texRect = 0;
ImTextureID texBrush = 0;
ImTextureID texText = 0;
ImTextureID texElliot = 0;
ImTextureID texTrash = 0;

// Variabel Baru
ImTextureID texPopupCopy = 0;
ImTextureID texPopupColor = 0;
ImTextureID texPopupThick = 0;
ImTextureID texPopupLock = 0;
ImTextureID texTrash1 = 0;

ImTextureID texPopupCopy1 = 0;
ImTextureID texPopupColor1 = 0;
ImTextureID texPopupThick1 = 0;
ImTextureID texPopupLock1 = 0;
ImTextureID texTrash2 = 0;
ImTextureID texPopupSetting = 0;
ImTextureID texIconGold = 0;
ImTextureID texIconEuro = 0;
ImTextureID texIconPound = 0;
ImTextureID texIconBTC = 0;
ImTextureID texIconETH = 0;
ImTextureID texEyeShow = 0;
ImTextureID texEyeHide = 0;
ImTextureID texIndSettings = 0;
ImTextureID texMaximize = 0;
ImTextureID texAddChart = 0;
ImTextureID texOrderFlow = 0;
ImTextureID texTreeObj     = 0; // icon show/hide UI Object Tree
ImTextureID texMarketWatch = 0; // icon show/hide Market Watch
ImTextureID texReplayBtn   = 0; // 🔥 BARU: Variabel untuk gambar PNG REPLAY

float g_js_pan_delta_x = 0.0f;
float g_js_pan_delta_y = 0.0f;
float g_js_zoom_delta  = 0.0f;

float g_js_touch_start_x = -1.0f;
float g_js_touch_start_y = -1.0f;
bool g_isTouchActive = false; // Flag: Apakah user sedang pakai Jari?
bool  g_isMobile           = false; // Layar < 900px → layout mobile
bool  g_layoutJustSwitched = false; // sinyal swap layout saat device berganti
float g_navbarHeight       = 0.0f;  // Tinggi navbar → chart offset ke bawahnya

long long lastBarTime = 0;
bool replayStarted = false;
 bool        g_replayActive       = false;
bool         g_replayGateActive   = false; // Gate: saat true, live feed di-skip (replay aktif)
bool         g_primaryBulkLoading = false; // Gate: saat true, rebuildFullFromDB sedang jalan
bool         g_cancelReplayRequested = false; // 🔥 Set dari wasm_cancel_replay() → main loop handle
bool         g_replayMode           = false; // 🔥 Mirror replayMode dari RenderMainUI — bisa diakses global
                                            // → wasm_push_tick skip M1 update (hanya MarketWatch)
                                            // → wasm_push_candle KASUS3 boleh push_back (historical order)

// ─── LAZY LOAD PREPEND SYSTEM ────────────────────────────────────────────
// wasm_begin_prepend / wasm_prepend_candle / wasm_end_prepend
// dipanggil dari JS saat user scroll kiri dan IDB punya data lebih lama.
// Alur: begin → push N candles (oldest→newest) → end (merge+offset+rebuild HTF)
static std::vector<Candle> g_prependBuffer;   // temp buffer antara begin..end
static bool                g_isPrepending    = false;
bool                       g_lazyLoadPending = false; // true = JS sedang fetch IDB older data
                                                       // C++ set true via EM_ASM, JS clear via wasm_set_lazy_load_done
    bool        g_chartMaximized     = false;
    std::string g_pendingAddChartTF  = "";
    double      g_replayCutoffTime   = 0.0;  // Unix timestamp batas candle replay
// ============================================================
// (DrawAxisLabel, CrosshairState, g_crossState, UpdateAndDrawCrosshairY → pindah ke CrosshairRenderer.h)

// Multi-Timeframe Storage
extern std::map<std::string, std::vector<Candle>> g_allCandles;
std::map<std::string, std::vector<Candle>> g_allCandles;
std::map<std::string, int> g_tfIndices;
std::string g_activeTF = ""; // Default kosong → picker dulu
std::mutex g_candlesMutex;

std::string g_symbol = ""; // Default kosong → picker dulu
double currentPrice = 0.0;
// =========================================================
// 🌍 MULTI-PAIR CONFIGURATION
// =========================================================

// Daftar Pair yang tersedia (Harus sama dengan di Python Server & JS)
const std::vector<std::string> g_availableSymbols = {
    "XAUUSD", "EURUSD", "GBPUSD", "BTCUSDT", "ETHUSDT"
};

// =========================================================
// 🔄 LOGIC GANTI PAIR (SWAP MEMORY)
// =========================================================
// Forward declaration — g_chart belum dideklarasi di titik ini.
// Implementasi ada setelah g_chart (line ~1090+).
void ResetChartViewState();

void SwitchSymbol(const std::string& newSym) {
    // 1. Cek apakah pair sudah sama?
    if (g_symbol == newSym) return;

    printf("🔄 [C++] Switching Symbol: %s -> %s\n", g_symbol.c_str(), newSym.c_str());

    // 2. CLEAR MEMORY LAMA — hanya key TF global (M1..D1)
    // Key SYMBOL_TF milik tab non-utama TIDAK dihapus!
    {
        std::lock_guard<std::mutex> lock(g_candlesMutex);
        static const char* primaryKeys[] = {"M1","M5","M15","M30","H1","H4","D1"};
        for (auto* k : primaryKeys) {
            g_allCandles.erase(k);
            g_tfIndices.erase(k);
        }
    }

    // 🔥 FIX: Clear GPU instances LANGSUNG di sini, tidak tunggu JS wasm_clear_chart
    // Tanpa ini, frame antara SwitchSymbol() dan SetActiveSymbol() masih render
    // stale GPU buffer dari symbol lama (21000 instance → candle panjang random)
    for (auto* _t : g_chartManager.tabs) {
        if (_t->usesGlobalData) {
            _t->gpuRenderer.ClearInstances();
            _t->state.viewCenterIndex = -1;
            _t->state.autoFitY        = true;
            _t->state.y_min           = 0.0;
            _t->state.y_max           = 0.0;
        }
    }

    // Reset global view state (g_chart belum visible di sini, pakai helper)
    ResetChartViewState();
    g_lazyLoadPending = false;
    // Reset lazy state primary tab
    for (auto* _t : g_chartManager.tabs)
        if (_t->usesGlobalData) {
            _t->lazyPending   = false;
            _t->noMoreHistory = false;
            break;
        }

    // 3. SET ACTIVE SYMBOL BARU
    g_symbol = newSym;

    // Sync mainTab->symbol agar GetLivePrice(tab->symbol) bekerja untuk primary tab
    for (auto* _t : g_chartManager.tabs)
        if (_t->usesGlobalData) { _t->symbol = newSym; break; }

    #ifdef __EMSCRIPTEN__
        std::string cmd = "SetActiveSymbol('" + newSym + "');";
        emscripten_run_script(cmd.c_str());
    #endif
}

// ============================================================
// JARVIS BRIDGE — Real Execution (Phase 2)
// Dipanggil dari AiAssistant.h saat AI mengembalikan tool calls
// ============================================================

void JarvisBridge_AddSymbol(const std::string& symbol) {
    printf("[Jarvis Bridge] AddSymbol: %s\n", symbol.c_str());

    // Validasi: symbol harus ada di daftar available
    const auto& available = g_availableSymbols;
    bool found = false;
    for (const auto& s : available) {
        if (s == symbol) { found = true; break; }
    }

    if (!found) {
        printf("[Jarvis Bridge] Symbol '%s' not in available list, adding anyway\n", symbol.c_str());
    }

    // Panggil SwitchSymbol yang sudah ada di main.cpp
    SwitchSymbol(symbol);
}

void JarvisBridge_AddIndicator(const std::string& indicator_name, int period) {
    printf("[Jarvis Bridge] AddIndicator: %s(%d)\n", indicator_name.c_str(), period);

    if (indicator_name.empty()) {
        printf("[Jarvis Bridge] Indicator name is empty!\n");
        return;
    }

    // Cek apakah sudah ada indicator yang sama (hindari duplikat)
    for (auto* existing : g_activeIndicators) {
        if (existing && existing->name == indicator_name && existing->period == period) {
            printf("[Jarvis Bridge] %s(%d) already exists, skipping\n", indicator_name.c_str(), period);
            return;
        }
    }

    // Ambil data candle saat ini untuk Calculate()
    std::lock_guard<std::mutex> lock(g_candlesMutex);
    const auto& candles = g_allCandles[g_activeTF];

    if (candles.empty()) {
        printf("[Jarvis Bridge] No candle data for %s, cannot add indicator\n", g_activeTF.c_str());
        return;
    }

    // Buat indicator object berdasarkan nama
    std::string ind_lower = indicator_name;
    // Convert to lowercase
    for (auto& c : ind_lower) c = std::tolower(c);

    Indicator* newInd = nullptr;

    if (ind_lower == "sma") {
        newInd = new SMAIndicator(period, ImVec4(1, 0.8f, 0.2f, 1));
    } else if (ind_lower == "ema") {
        newInd = new EMAIndicator(period, ImVec4(1, 0.5f, 0.1f, 1));
    } else if (ind_lower == "rsi") {
        newInd = new RSIIndicator(period, ImVec4(0, 1, 1, 1));
    } else if (ind_lower == "bb" || ind_lower == "bollinger") {
        newInd = new BollingerIndicator(period, 2.0);
    } else if (ind_lower == "macd") {
        newInd = new MACDIndicator();
    } else if (ind_lower == "stochastic" || ind_lower == "stoch") {
        newInd = new StochIndicator();
    } else if (ind_lower == "atr") {
        newInd = new ATRIndicator();
    } else if (ind_lower == "adx") {
        newInd = new ADXIndicator();
    } else if (ind_lower == "cci") {
        newInd = new CCIIndicator();
    } else if (ind_lower == "williams" || ind_lower == "williamsr") {
        newInd = new WilliamsRIndicator();
    } else if (ind_lower == "supertrend") {
        newInd = new SupertrendIndicator();
    } else if (ind_lower == "mfi") {
        newInd = new MFIIndicator();
    } else if (ind_lower == "roc") {
        newInd = new ROCIndicator();
    } else if (ind_lower == "obv") {
        newInd = new OBVIndicator();
    } else if (ind_lower == "volume") {
        newInd = new VolumeIndicator(0, ImVec4(0.2f, 0.8f, 0.2f, 0.6f));
    } else {
        printf("[Jarvis Bridge] Unknown indicator: %s\n", indicator_name.c_str());
        return;
    }

    // Tambahkan ke chart
    if (newInd) {
        AddIndicator(newInd, candles);
        printf("[Jarvis Bridge] %s(%d) added successfully\n", indicator_name.c_str(), period);
    }
}

// =========================================================
// SWING POINT DETECTION — untuk AI analisa teknikal
// Diletakkan di atas GetChartStatus karena dipakai di dalamnya
// =========================================================
struct SwingPoint {
    int candle_index;
    std::string type;  // "HIGH" or "LOW"
    double price;
    std::string datetime_str;
};

static std::vector<SwingPoint> DetectSwingPoints(
    const std::vector<Candle>& candles, int lookback = 5)
{
    std::vector<SwingPoint> swings;
    int n = (int)candles.size();
    if (n < lookback * 2 + 1) return swings;

    for (int i = lookback; i < n - lookback; i++) {
        bool isHigh = true, isLow = true;
        for (int j = 1; j <= lookback; j++) {
            if (candles[i].high <= candles[i-j].high ||
                candles[i].high <= candles[i+j].high) isHigh = false;
            if (candles[i].low  >= candles[i-j].low  ||
                candles[i].low  >= candles[i+j].low)  isLow  = false;
        }
        if (isHigh)
            swings.push_back({i, "HIGH", candles[i].high, candles[i].datetime});
        if (isLow)
            swings.push_back({i, "LOW",  candles[i].low,  candles[i].datetime});
    }
    return swings;
}

static std::string DetermineTrend(const std::vector<SwingPoint>& swings) {
    if (swings.size() < 4) return "SIDEWAYS";

    std::vector<double> highs, lows;
    for (const auto& s : swings) {
        if (s.type == "HIGH") highs.push_back(s.price);
        else                   lows.push_back(s.price);
    }

    if (highs.size() < 2 || lows.size() < 2) return "SIDEWAYS";

    int hh = 0, hl = 0, lh = 0, ll = 0;
    int checkCount = std::min((int)highs.size(), 4);
    for (int i = (int)highs.size() - checkCount; i < (int)highs.size() - 1; i++) {
        if (i >= 0 && highs[i] < highs[i+1]) hh++; else lh++;
    }
    checkCount = std::min((int)lows.size(), 4);
    for (int i = (int)lows.size() - checkCount; i < (int)lows.size() - 1; i++) {
        if (i >= 0 && lows[i] < lows[i+1]) hl++; else ll++;
    }

    if (hh >= 2 && hl >= 2) return "BULLISH";
    if (lh >= 2 && ll >= 2) return "BEARISH";
    if (hh >= 1 && hl >= 1) return "BULLISH_WEAK";
    if (lh >= 1 && ll >= 1) return "BEARISH_WEAK";
    return "SIDEWAYS";
}

std::string JarvisBridge_GetActiveSymbol() {
    return g_symbol;
}

std::string JarvisBridge_GetChartStatus() {
    json status;
    status["symbol"] = g_symbol.empty() ? "NONE" : g_symbol;
    status["timeframe"] = g_activeTF.empty() ? "NONE" : g_activeTF;

    // Current price (last candle close)
    double price = 0.0;
    if (!g_allCandles.empty() && g_allCandles.count(g_activeTF) && !g_allCandles[g_activeTF].empty()) {
        price = g_allCandles[g_activeTF].back().close;
    }
    status["price"] = price;

    // List active indicators
    json indArray = json::array();
    for (const auto* ind : g_activeIndicators) {
        if (ind) {
            indArray.push_back(ind->name);
        }
    }
    status["indicators"] = indArray;

    // ── Swing-based trend & key levels (auto-context ringan) ──
    std::string trend = "SIDEWAYS";
    double resistance = 0.0;
    double support = 0.0;
    double lastSwingHigh = 0.0;
    double lastSwingLow = 0.0;

    if (g_allCandles.count(g_activeTF) && !g_allCandles[g_activeTF].empty()) {
        const auto& candles = g_allCandles[g_activeTF];
        auto swings = DetectSwingPoints(candles, 5);

        if (!swings.empty()) {
            trend = DetermineTrend(swings);
            for (int i = (int)swings.size() - 1; i >= 0; i--) {
                if (swings[i].type == "HIGH") { lastSwingHigh = swings[i].price; break; }
            }
            for (int i = (int)swings.size() - 1; i >= 0; i--) {
                if (swings[i].type == "LOW") { lastSwingLow = swings[i].price; break; }
            }
            double globalHigh = 0.0, globalLow = 999999999.0;
            for (const auto& s : swings) {
                if (s.type == "HIGH" && s.price > globalHigh) globalHigh = s.price;
                if (s.type == "LOW"  && s.price < globalLow)  globalLow  = s.price;
            }
            resistance = globalHigh;
            support = globalLow;
        }
    }

    status["trend"]           = trend;
    status["resistance"]      = resistance;
    status["support"]         = support;
    status["last_swing_high"] = lastSwingHigh;
    status["last_swing_low"]  = lastSwingLow;

    return status.dump();
}

// Full swing analysis — untuk tool chart_analyze_swing
std::string JarvisBridge_GetSwingAnalysis() {
    json result;
    result["symbol"]    = g_symbol.empty() ? "NONE" : g_symbol;
    result["timeframe"] = g_activeTF.empty() ? "NONE" : g_activeTF;
    result["total_candles"] = 0;

    json swingHighs = json::array();
    json swingLows  = json::array();

    if (g_allCandles.count(g_activeTF) && !g_allCandles[g_activeTF].empty()) {
        const auto& candles = g_allCandles[g_activeTF];
        result["total_candles"] = candles.size();
        result["first_candle"]  = candles.front().datetime;
        result["last_candle"]   = candles.back().datetime;

        auto swings = DetectSwingPoints(candles, 5);

        int shCount = 0, slCount = 0;
        for (const auto& s : swings) {
            json point;
            point["index"]    = s.candle_index;
            point["price"]    = s.price;
            point["datetime"] = s.datetime_str;

            bool isHighest = true, isLowest = true;
            for (const auto& other : swings) {
                if (s.type == "HIGH" && other.type == "HIGH" && other.price > s.price) isHighest = false;
                if (s.type == "LOW"  && other.type == "LOW"  && other.price < s.price) isLowest = false;
            }

            if (s.type == "HIGH") {
                point["label"] = isHighest ? "TERTINGGI" : "";
                swingHighs.push_back(point);
                shCount++;
            } else {
                point["label"] = isLowest ? "TERENDAH" : "";
                swingLows.push_back(point);
                slCount++;
            }
        }

        result["swing_high_count"] = shCount;
        result["swing_low_count"]  = slCount;
        result["trend"] = DetermineTrend(swings);

        const Candle& last = candles.back();
        result["current_candle"] = {
            {"open", last.open}, {"high", last.high},
            {"low", last.low},   {"close", last.close}
        };
    }

    result["swing_highs"] = swingHighs;
    result["swing_lows"]  = swingLows;
    return result.dump();
}

// Key levels — untuk tool chart_get_key_levels
std::string JarvisBridge_GetKeyLevels() {
    json result;
    result["symbol"]    = g_symbol.empty() ? "NONE" : g_symbol;
    result["timeframe"] = g_activeTF.empty() ? "NONE" : g_activeTF;

    if (g_allCandles.count(g_activeTF) && !g_allCandles[g_activeTF].empty()) {
        const auto& candles = g_allCandles[g_activeTF];
        auto swings = DetectSwingPoints(candles, 5);

        struct LevelTest {
            double price;
            int testCount;
            std::string type;
        };
        std::vector<LevelTest> levels;

        for (const auto& s : swings) {
            bool found = false;
            for (auto& lv : levels) {
                double diff = std::abs(lv.price - s.price) / s.price;
                if (diff < 0.005) {
                    lv.testCount++;
                    found = true;
                    break;
                }
            }
            if (!found) {
                levels.push_back({s.price, 1, s.type});
            }
        }

        std::sort(levels.begin(), levels.end(),
            [](const LevelTest& a, const LevelTest& b) { return a.testCount > b.testCount; });

        json resistances = json::array();
        json supports    = json::array();
        double currentPrice = candles.back().close;

        for (const auto& lv : levels) {
            json lvl;
            lvl["price"]      = lv.price;
            lvl["test_count"] = lv.testCount;
            double distPct = ((lv.price - currentPrice) / currentPrice) * 100.0;
            lvl["distance_pct"] = distPct;
            lvl["strength"] = lv.testCount >= 3 ? "KUAT" : lv.testCount >= 2 ? "SEDANG" : "LEMAH";

            if (lv.price > currentPrice) resistances.push_back(lvl);
            else                         supports.push_back(lvl);
        }

        result["resistances"] = resistances;
        result["supports"]    = supports;
        result["current_price"] = currentPrice;
        result["trend"] = DetermineTrend(swings);

        double nearestHigh = 0, nearestLow = 0;
        for (const auto& s : swings) {
            if (s.type == "HIGH" && s.price > currentPrice) {
                if (nearestHigh == 0 || s.price < nearestHigh) nearestHigh = s.price;
            }
            if (s.type == "LOW" && s.price < currentPrice) {
                if (nearestLow == 0 || s.price > nearestLow) nearestLow = s.price;
            }
        }
        result["nearest_resistance"] = nearestHigh;
        result["nearest_support"]    = nearestLow;
    }

    return result.dump();
}

// ============================================================
// DRAW TOOLS — Jarvis AI bisa menggambar langsung di chart
// Menggunakan GlobalShapeManager (g_shapes)
// ============================================================

// Helper: Parse hex color string (#RRGGBB or #RRGGBBAA) to ImVec4
static ImVec4 JarvisHexToColor(const std::string& hex, float alpha = 1.0f) {
    ImVec4 col(1, 1, 1, alpha);
    if (hex.size() >= 7 && hex[0] == '#') {
        unsigned int r, g, b;
        sscanf(hex.c_str() + 1, "%02x%02x%02x", &r, &g, &b);
        col.x = (float)r / 255.0f;
        col.y = (float)g / 255.0f;
        col.z = (float)b / 255.0f;
        col.w = alpha;
    }
    return col;
}

// Helper: Resolve candle time index (-1 = last candle)
static double ResolveCandleTime(double timeIndex) {
    const auto& candles = g_allCandles[g_activeTF];
    if (candles.empty()) return 0;

    int lastIdx = (int)candles.size() - 1;
    int idx = (int)timeIndex;
    if (idx < 0 || idx > lastIdx) idx = lastIdx;

    // Return the actual time value of the candle at that index
    // The time is stored as unix timestamp in Candle.time
    return (double)candles[idx].time;
}

void JarvisBridge_DrawLine(double time0, double price0, double time1, double price1,
                           const std::string& color, float thickness,
                           bool extendLeft, bool extendRight, const std::string& label) {
    printf("[Jarvis Bridge] DrawLine: %.2f -> %.2f, color=%s\n", price0, price1, color.c_str());

    ImVec4 col = JarvisHexToColor(color);
    double t0 = ResolveCandleTime(time0);
    double t1 = ResolveCandleTime(time1);

    g_shapes.AddShape("LINE", t0, price0, t1, price1, col, thickness, false);

    // If label provided, add text at the end point
    if (!label.empty()) {
        GlobalShape textShape;
        textShape.id = g_shapes.GenerateUUID();
        textShape.type = "TEXT";
        textShape.time0 = t1;
        textShape.price0 = price1;
        textShape.textContent = label;
        textShape.fontSize = 14.0f;
        textShape.color = col;
        textShape.textBg = true;
        textShape.textBgColor = ImVec4(0, 0, 0, 0.7f);
        g_shapes.AddShape(textShape);
    }

    // Apply extend flags to the last added LINE shape
    auto& shapes = g_shapes.GetEditableShapes();
    if (!shapes.empty()) {
        auto& lastShape = shapes.back();
        if (lastShape.type == "TEXT" && shapes.size() >= 2) {
            // Label text was added after LINE, so the LINE is second-to-last
            auto& lineShape = shapes[shapes.size() - 2];
            if (lineShape.type == "LINE") {
                lineShape.extendLeft = extendLeft;
                lineShape.extendRight = extendRight;
            }
        } else if (lastShape.type == "LINE") {
            lastShape.extendLeft = extendLeft;
            lastShape.extendRight = extendRight;
        }
    }
}

void JarvisBridge_DrawRect(double time0, double price0, double time1, double price1,
                           const std::string& color, const std::string& fillColor,
                           float fillOpacity, const std::string& label) {
    printf("[Jarvis Bridge] DrawRect: %.2f - %.2f, color=%s\n", price0, price1, color.c_str());

    ImVec4 col = JarvisHexToColor(color);
    ImVec4 fillCol = JarvisHexToColor(fillColor, fillOpacity);
    double t0 = ResolveCandleTime(time0);
    double t1 = ResolveCandleTime(time1);

    g_shapes.AddShape("RECT", t0, price0, t1, price1, col, 1.2f, true);

    // Configure the rectangle shape
    auto& shapes = g_shapes.GetEditableShapes();
    if (!shapes.empty()) {
        auto& rectShape = shapes.back();
        if (rectShape.type == "RECT") {
            rectShape.filled = true;
            rectShape.fillColor = JarvisHexToColor(fillColor);
            rectShape.fillOpacity = fillOpacity;
            rectShape.rectBorderVisible = true;

            // Add label inside rectangle
            if (!label.empty()) {
                rectShape.rectLabel = label;
                rectShape.rectFontSize = 10.0f;
                rectShape.textColor = ImVec4(1, 1, 1, 0.9f);
                rectShape.rectTextAlign = 1;  // Center
                rectShape.rectVertAlign = 2;  // Bottom
            }
        }
    }
}

void JarvisBridge_DrawFib(double time0, double price0, double time1, double price1,
                           const std::string& color) {
    printf("[Jarvis Bridge] DrawFib: %.2f -> %.2f, color=%s\n", price0, price1, color.c_str());

    ImVec4 col = JarvisHexToColor(color);
    double t0 = ResolveCandleTime(time0);
    double t1 = ResolveCandleTime(time1);

    g_shapes.AddShape("FIB", t0, price0, t1, price1, col, 1.0f, false);

    // The Fibonacci shape will automatically use defaultFibConfig from GlobalShapeManager
    // which includes standard levels (0, 0.236, 0.382, 0.5, 0.618, 0.786, 1.0, 1.618)
}

void JarvisBridge_DrawText(double time, double price,
                           const std::string& text, const std::string& color,
                           float fontSize) {
    printf("[Jarvis Bridge] DrawText: \"%s\" at %.2f, color=%s\n", text.c_str(), price, color.c_str());

    ImVec4 col = JarvisHexToColor(color);
    double t = ResolveCandleTime(time);

    GlobalShape textShape;
    textShape.id = g_shapes.GenerateUUID();
    textShape.type = "TEXT";
    textShape.time0 = t;
    textShape.price0 = price;
    textShape.textContent = text;
    textShape.fontSize = fontSize;
    textShape.color = col;
    textShape.textBg = true;
    textShape.textBgColor = ImVec4(0, 0, 0, 0.6f);
    g_shapes.AddShape(textShape);
}

void JarvisBridge_DrawElliot(const std::vector<double>& times,
                             const std::vector<double>& prices,
                             const std::string& color, float thickness) {
    printf("[Jarvis Bridge] DrawElliot: %zu points, color=%s\n", prices.size(), color.c_str());

    if (prices.size() < 2) {
        printf("[Jarvis Bridge] Elliot needs at least 2 points!\n");
        return;
    }

    ImVec4 col = JarvisHexToColor(color);
    const auto& candles = g_allCandles[g_activeTF];
    int lastIdx = candles.empty() ? 0 : (int)candles.size() - 1;

    std::vector<double> resolvedTimes;
    if (times.empty()) {
        // Distribute points evenly across the visible candle range
        int startIdx = std::max(0, lastIdx - 200);
        double step = (double)(lastIdx - startIdx) / (double)(prices.size() - 1);
        for (size_t i = 0; i < prices.size(); i++) {
            int idx = startIdx + (int)(step * (double)i);
            if (idx > lastIdx) idx = lastIdx;
            resolvedTimes.push_back((double)candles[idx].time);
        }
    } else {
        for (size_t i = 0; i < times.size() && i < prices.size(); i++) {
            int idx = (int)times[i];
            if (idx < 0 || idx > lastIdx) idx = lastIdx;
            resolvedTimes.push_back((double)candles[idx].time);
        }
        // Fill remaining if times is shorter than prices
        while (resolvedTimes.size() < prices.size()) {
            resolvedTimes.push_back((double)candles[lastIdx].time);
        }
    }

    g_shapes.AddElliotShape(resolvedTimes, prices, col);
}

// ====== TRADE MANAGER: Live & Replay (terpisah) ======
TradeManager g_liveManager;    // mode live/demo
TradeManager g_replayManager;  // mode replay

// Helper: pointer ke manager yang sedang aktif
static inline TradeManager& ActiveTM() {
    return g_replay.active ? g_replayManager : g_liveManager;
}

// ====== TRADE HISTORY: Live & Replay (terpisah) ======
TradeHistory g_liveHistory;
TradeHistory g_replayHistory;

// =========================================================
// 🔥 wasm_clear_chart — dipanggil JS saat switch symbol PRIMARY TAB
// Hanya hapus key TF global (M1, M5, M15, M30, H1, H4, D1).
// KEY MILIK TAB NON-UTAMA (SYMBOL_M1, SYMBOL_M15, dst) TIDAK DIHAPUS!
// Tanpa ini, saat primary switch symbol → g_allCandles.clear() ikut hapus
// data XAUUSD_M30 milik Tab2 → Tab2 spinner padahal datanya masih valid.
// =========================================================
extern "C" {

// 🔥 wasm_get_replay_gate — dipanggil dari JS untuk cek apakah replay gate aktif
// Return 1 = replay aktif (gate ON), 0 = normal live mode (gate OFF)
// Dipakai di websocket_orderflow.js untuk tahu apakah perlu bypass gate
EMSCRIPTEN_KEEPALIVE
int wasm_get_replay_gate() {
    return g_replayGateActive ? 1 : 0;
}

// 🔥 wasm_cancel_replay — dipanggil dari data_check.js saat data IDB tidak ada.
// TIDAK boleh akses local var main() seperti replayMode/replayStarted/g_chart.
// Cukup set flag → main loop handle di frame berikutnya (safe, no race).
EMSCRIPTEN_KEEPALIVE
void wasm_cancel_replay() {
    if (!g_replayActive) return; // sudah live, tidak perlu reset
    g_cancelReplayRequested = true;
    printf("[DATA_CHECK] wasm_cancel_replay() — flag set, main loop akan handle\n");
}

EMSCRIPTEN_KEEPALIVE
void wasm_clear_chart() {
    std::lock_guard<std::mutex> lock(g_candlesMutex);

    // 🔥 FIX: Hanya hapus key TF global (tanpa prefix symbol)
    // Key non-primary tab selalu ada underscore: "XAUUSD_M1", "BTCUSDT_H4", dst
    // Key primary tab: "M1", "M5", "M15", "M30", "H1", "H4", "D1"
    static const char* primaryKeys[] = {"M1","M5","M15","M30","H1","H4","D1"};
    for (auto* k : primaryKeys) {
        g_allCandles.erase(k);
        g_tfIndices.erase(k);
    }

    // 🔥 FIX: Reset view state — CRITICAL!
    // Tanpa ini, saat switch XAUUSD(y≈3000) → EURUSD(y≈1.15):
    //   - viewCenterIndex masih 21000 → view kosong (data cuma 1000)
    //   - y_min/y_max masih 2990~3100 → autoFitY lerp 0.15/frame
    //     butuh ~20 frame untuk konverge ke 1.15 → chart rusak selama itu
    //   - lazyLoadPending masih true → lazy load terkunci permanen
    ResetChartViewState();  // g_chart.viewCenterIndex=-1, y_min/max=0, autoFitY=true
    // 🔥 FIX: TIDAK reset g_lazyLoadPending di sini!
    // wasm_clear_chart dipanggil dari:
    //   1. SetActiveSymbol (symbol switch) — g_lazyLoadPending di-reset di SwitchSymbol()
    //   2. onNearLeftEdge (lazy rebuild) — JANGAN reset, lazy masih in-progress!
    // Kalau di-reset di sini, saat lazy rebuild: C++ pikir lazy selesai → fire trigger lagi
    // → JS blok tapi C++ terus call tiap 30 frame → "kesetrum"

    // Reset GPU + view state semua tab yang pakai global data
    for (auto* tab : g_chartManager.tabs) {
        if (tab->usesGlobalData) {
            tab->gpuRenderer.ClearInstances();
            tab->state.viewCenterIndex = -1;
            tab->state.autoFitY        = true;
            tab->state.y_min           = 0.0;
            tab->state.y_max           = 0.0;
            tab->state.inertiaVelocity = 0.0f;
        }
    }

    printf("[CLEAR_CHART] primary TF keys cleared (non-primary tabs preserved)\n");
}
} // extern "C"
// ================================================================
// CATATAN GLOBAL VARIABLES yang diperlukan (tambahkan ke header/globals):
// ================================================================
 bool g_showMainPanel    = true;   // Toggle panel utama (bulat kiri atas)
 bool g_showPendingPanel = false;  // Toggle panel pending (bulat kanan bawah)
 float g_lotSize         = 0.01f;
 

// =========================================================
// 🟢 THE ORACLE: SMART PRICE FEEDER (FIXED)
// =========================================================
double GetMasterPrice() {
    // 1. REPLAY MODE → ambil dari replay state / candle index
    if (g_replay.active) {
        if (g_replay.currentState.price > 0.00001)
            return g_replay.currentState.price;
        if (g_allCandles.count("M1") && !g_allCandles["M1"].empty()) {
            int idx = g_replay.currentIndex;
            if (idx >= 0 && idx < (int)g_allCandles["M1"].size())
                return g_allCandles["M1"][idx].close;
        }
    }
    // 2. LIVE MODE → prioritaskan push tick terbaru (g_liveTick)
    if (g_liveTick.price > 0.00001)
        return g_liveTick.price;
    // 3. Fallback ke candle terakhir (saat app baru start, belum ada tick)
    if (g_allCandles.count(g_activeTF) && !g_allCandles[g_activeTF].empty())
        return g_allCandles[g_activeTF].back().close;
    return 0.0;
}
double GetMasterTime() {
    // 1. REPLAY MODE
    if (g_replay.active) {
        // Cek data base M1
        if (g_allCandles.count("M1") && !g_allCandles["M1"].empty()) {
            int idx = g_replay.currentIndex;
            if (idx >= 0 && idx < g_allCandles["M1"].size()) {
                return g_allCandles["M1"][idx].time;
            }
        }
    } 
    
    // 2. LIVE MODE
    if (g_allCandles.count(g_activeTF) && !g_allCandles[g_activeTF].empty()) {
        return g_allCandles[g_activeTF].back().time;
    }
    
    return 0.0;
}

// Helper: konversi timestamp → candle index di active TF
// Dipakai di semua call site CheckAllHits / UpdateAllLogic
static double GetActiveTFCandleIndex(double timestamp) {
    auto& c = g_allCandles[g_activeTF];
    if (c.empty()) return 0.0;
    // upper_bound → iterator ke candle PERTAMA yang time > timestamp
    // --it → candle terakhir yang time <= timestamp
    auto it = std::upper_bound(c.begin(), c.end(), timestamp,
        [](double t, const Candle& cd) { return t < cd.time; });
    if (it == c.begin()) return 0.0;
    --it;
    return (double)(it - c.begin());
}

// Replay Pointer
std::vector<Candle>* g_replaySourceTF = nullptr;
int* g_replayIndexPtr = nullptr;

// Legacy References
std::vector<Candle>& candles = g_allCandles["M1"];
int& currentIndex = g_tfIndices["M1"];

// Flags Global State
bool isDrawingActive = false;
bool ignoreNextDrag = false;
bool isCutoffDragging = false;
bool isTradeDragging = false;
bool needAutoMargin = true;
float initialMargin = 50.0f;
// === RIGHT MARGIN (agar live candle tidak nempel ke kanan) ===
float g_rightMarginCandles = 0.0f;       // default: ruang kosong di kanan dalam satuan 'candle count'
bool  g_rightMarginDragging = false;      // untuk draggable overlay (opsional)
float g_rightDragStartX = 0.0f;
int   g_rightMarginMin = 0;
int   g_rightMarginMax = 1000;


struct CReplayCutoff {
    bool active = false;
    bool showConfirmation = false;
    double lineIndex = 0.0;
} g_replayCutoff;
static std::string g_lastSymbol = "";
// 🖥️ ENUM DAN UI STATE
enum Tool { TOOL_NONE, TOOL_CURSOR, TOOL_LINE, TOOL_RECT, TOOL_FIB, TOOL_ERASER };

struct UIState {
    bool showRightPanel = true;
    Tool activeTool = TOOL_CURSOR;

    int openIndicatorSettingsIndex = -1; // Indikator yang sedang diedit
    bool requestOpenSettings = false;    // 🔥 TAMBAHAN: Bendera untuk minta buka pop-up
} g_uiState;


// 2. HELPER: MEMBUAT TEXT SUPAYA BISA DI-KLIK SEPERTI LINK
bool TextLink(const char* label) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.3f, 0.7f, 1.0f, 1.0f));
    ImGui::Text("%s", label);
    ImGui::PopStyleColor();
    if (ImGui::IsItemHovered()) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        ImVec2 min = ImGui::GetItemRectMin();
        ImVec2 max = ImGui::GetItemRectMax();
        min.y = max.y;
        ImGui::GetWindowDrawList()->AddLine(min, max, ImGui::GetColorU32(ImVec4(0.3f, 0.7f, 1.0f, 1.0f)));
    }
    return ImGui::IsItemClicked();
}
// =========================================================
// STRUCT INTERAKSI CHART (SMART DETECTION + PHYSICS)
// =========================================================
struct CChartInteraction {
    double y_min = 0.0, y_max = 0.0;
    bool autoFitY = true;
    bool replayMode = false;
    float zoomLevel = 150.0f;
    float targetZoom = 150.0f;
    int viewCenterIndex = 0;
    bool blockOnce = false;

    // 🔥 VARIABLE FISIKA (INERTIA & SMOOTHING)
    double dragAccumulator = 0.0; 
    float inertiaVelocity = 0.0f;
    bool isInertiaActive = false;
    const float FRICTION = 0.92f;

    // 🔥 STATE MACHINE — dipindah ke struct agar bisa di-reset dari wasm_notify_touch_end
    bool isResizingY    = false;
    bool isPanConfirmed = false;
    bool lastTouchState = false;
    ImVec2 panStartPos  = ImVec2(0, 0);

    // Reset semua state touch sekaligus (dipanggil saat touchend)
    void ResetTouchState() {
        isResizingY     = false;
        isPanConfirmed  = false;
        lastTouchState  = false;
        isInertiaActive = false;
        g_js_pan_delta_x = 0.0f;
        g_js_pan_delta_y = 0.0f;
        g_js_touch_start_x = -1.0f;
        g_js_touch_start_y = -1.0f;
    }
    void HandleChartInteraction() {
    // 1. CEK FLAG BORDER
    if (IsCursorOnBorder()) {
        // Abaikan semua interaksi chart jika kursor sedang di perbatasan layout
        return; 
    }

    // 2. CEK APAKAH IMGUI SEDANG SIBUK DENGAN WINDOW LAIN
    if (ImGui::GetIO().WantCaptureMouse && !ImPlot::IsPlotHovered()) {
        return; // Jangan geser chart kalau lagi klik tombol/UI lain
    }
}

    void InitIfNeeded(double lmin, double lmax) {
        if (y_min == 0.0 && y_max == 0.0) { y_min = lmin; y_max = lmax; }
    }

    void HandleHorizontalZoom() {
        if (!ImPlot::IsPlotHovered() || IsCursorOnBorder()) return;
        if (!ImPlot::IsPlotHovered()) return;
        ImGuiIO& io = ImGui::GetIO();
        float scrollAmount = 0.0f;

        // 1. DARI MOUSE WHEEL
        if (fabs(io.MouseWheel) > 0.0f) {
            scrollAmount = io.MouseWheel;
            #ifdef __EMSCRIPTEN__
            scrollAmount *= 3.0f;
            #endif
        }
        // 2. DARI CUBIT 2 JARI (WEB)
        #ifdef __EMSCRIPTEN__
        if (g_js_zoom_delta != 0.0f) {
            scrollAmount = g_js_zoom_delta * 0.1f; 
            g_js_zoom_delta = 0.0f; 
        }
        #endif

        // 3. TERAPKAN ZOOM SMOOTH
        if (fabs(scrollAmount) > 0.0f) {
            targetZoom -= scrollAmount * (zoomLevel * 0.03f);
            targetZoom = std::clamp(targetZoom, 5.0f, 2000.0f);
        }
        zoomLevel += (targetZoom - zoomLevel) * 0.15f;
    }

    bool HandleChartDrag(int &currentIndex, int start, int end,
                         double localMin, double localMax, bool blockInteraction) {

        ImGuiIO& io = ImGui::GetIO();
        ImVec2 plotPos  = ImPlot::GetPlotPos();
        ImVec2 plotSize = ImPlot::GetPlotSize();

        // =========================================================
        // 🔥 FIX #1: DETEKSI ZONA Y-AXIS DULU, SEBELUM APAPUN
        // Y-axis label ada di KANAN chart (x > chartRightEdge)
        // Estimasi lebar Y-axis label ~70px (bisa disesuaikan)
        // =========================================================
        float chartRightEdge = plotPos.x + plotSize.x;
        float yAxisLabelWidth = 70.0f; // Lebar area label harga di kanan

        // Mouse: cukup cek apakah x melewati batas kanan plot
        bool mouseZone = (io.MousePos.x > chartRightEdge);

        // FIX #2: Touch zone seharusnya DI KANAN chartRightEdge, bukan di dalam
        // Dulu: >= (chartRightEdge - 20) → masuk ke dalam chart → SALAH
        // Sekarang: >= chartRightEdge → tepat di area label Y-axis → BENAR
        float touchZoneWidth = 70.0f; // Sama dengan yAxisLabelWidth
        bool touchZone = (g_isTouchActive &&
                          g_js_touch_start_x >= chartRightEdge &&
                          g_js_touch_start_x <= (chartRightEdge + touchZoneWidth));
        bool isHoveringScaleY = mouseZone || touchZone;

        // Ubah kursor jadi ns-resize saat hover di zona Y-axis
        if (mouseZone && !ImGui::IsMouseDown(0)) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
        }

        // FIX #3: Hitung shouldBlock TAPI kecualikan kasus sedang di zona Y-axis
        // Dulu: isUIBusy memblokir Y-axis karena IsPlotHovered() = false di sana
        bool isBorder  = IsCursorOnBorder();
        // FIX: Jika di zona Y-axis, jangan anggap UIBusy (ImPlot tidak hover di Y-axis)
        bool isUIBusy  = (io.WantCaptureMouse && !ImPlot::IsPlotHovered() && !isHoveringScaleY);
        bool shouldBlock = blockInteraction || isUIBusy || isBorder;

        bool inputDown  = ImGui::IsMouseDown(ImGuiMouseButton_Left) || g_isTouchActive;

        // STOP INERTIA JIKA ADA INTERAKSI LAIN
        if (shouldBlock && !isHoveringScaleY) {
            isInertiaActive = false;
            inertiaVelocity = 0.0f;
            #ifdef __EMSCRIPTEN__
            g_js_pan_delta_x = 0.0f; g_js_pan_delta_y = 0.0f;
            #endif
            return false;
        }

        // =========================================================
        // 🔥 1. LOGIKA INERTIA (AUTO PILOT SAAT DILEPAS)
        // =========================================================
        if (isInertiaActive && !inputDown) {
            dragAccumulator += inertiaVelocity;
            int indexStep = (int)dragAccumulator;
            
            if (indexStep != 0) {
                viewCenterIndex += indexStep;
                dragAccumulator -= indexStep;
            }

            inertiaVelocity *= FRICTION; // Ngerem pelan-pelan

            if (fabs(inertiaVelocity) < 0.01f) {
                isInertiaActive = false;
                inertiaVelocity = 0.0f;
            }
            
            // Clamp View
            int maxIdx = 0;
            if (g_allCandles.count(g_activeTF)) maxIdx = (int)g_allCandles[g_activeTF].size() - 1;
            viewCenterIndex = std::clamp(viewCenterIndex, 0, maxIdx + (int)(zoomLevel * 5.0f));

            return true; // Redraw terus
        }

        if (inputDown) isInertiaActive = false; // Stop jika disentuh lagi

        // Static State Machine — sudah jadi member struct, tidak perlu static lagi

        // Static Variables untuk Panning Physics — sudah jadi member struct
        
        // DETEKSI FRAME PERTAMA KLIK
        bool isClickStarting = io.MouseClicked[0];
        #ifdef __EMSCRIPTEN__
        if (g_isTouchActive && !lastTouchState) isClickStarting = true;
        lastTouchState = g_isTouchActive;
        #endif

        // A. PENENTUAN NIAT (FRAME 1)
        if (inputDown && isClickStarting && !blockInteraction) {
            inertiaVelocity = 0.0f; // Reset speed

            if (isHoveringScaleY) {
                isResizingY = true; // KUNCI ke Mode Resize
                autoFitY = false;
                #ifdef __EMSCRIPTEN__
                g_js_pan_delta_y = 0.0f; g_js_pan_delta_x = 0.0f;
                #endif
                return true; 
            } else {
                isResizingY = false; // KUNCI ke Mode Pan
                
                // Siapkan Anti-Getar
                panStartPos = io.MousePos;
                isPanConfirmed = false; 
            }
        }

        // B. RESET STATE SAAT LEPAS
        if (!inputDown) {
            isResizingY = false;
            isPanConfirmed = false;
        }

        // =========================================================
        // 3. EKSEKUSI LOGIKA (RESIZE Y)
        // =========================================================
        if (isResizingY && inputDown) {
            float deltaY = 0.0f;
            if (g_isTouchActive) {
                #ifdef __EMSCRIPTEN__
                deltaY = g_js_pan_delta_y;
                g_js_pan_delta_y = 0.0f; g_js_pan_delta_x = 0.0f; 
                #endif
            } else {
                deltaY = io.MouseDelta.y;
            }

            if (deltaY != 0.0f) {
                // 🔥 Sensitivitas lebih terasa (2x lebih responsif)
                float sensitivity = 0.01f;
                double scaleFactor = std::clamp(1.0 + (deltaY * sensitivity), 0.7, 1.3);
                double currentRange = y_max - y_min;
                
                // 🔥 FIX: Pivot di harga sejajar posisi mouse Y (bukan midPrice)
                // Hasilnya scale terasa "elastis" dari titik yang user tunjuk
                double pivotPrice = y_min + (currentRange * 0.5); // default: mid
                if (plotSize.y > 0.0f) {
                    float mouseRatioY = (io.MousePos.y - plotPos.y) / plotSize.y;
                    mouseRatioY = std::clamp(mouseRatioY, 0.0f, 1.0f);
                    // ImPlot: y-axis terbalik (0=atas=harga max, 1=bawah=harga min)
                    pivotPrice = y_max - (mouseRatioY * currentRange);
                }
                
                double newRange = currentRange * scaleFactor;
                double pivotRatio = (currentRange > 0) ? ((pivotPrice - y_min) / currentRange) : 0.5;
                y_min = pivotPrice - (pivotRatio * newRange);
                y_max = y_min + newRange;
            }
            return true;
        }

        // =========================================================
        // 4. EKSEKUSI LOGIKA (PANNING CHART + PHYSICS)
        // =========================================================
        // Jika tidak resizing, dan tidak diblokir -> Masuk Mode Pan
        if (!isResizingY && !blockInteraction && !isCutoffDragging) {
            
            // 🛡️ ANTI-GETAR / THRESHOLD (5 PIXEL)
            // Biar chart gak goyang kalau jari cuma nempel/tremor
            if (!isPanConfirmed) {
                ImVec2 cur = io.MousePos;
                float distSq = (cur.x - panStartPos.x)*(cur.x - panStartPos.x) + 
                               (cur.y - panStartPos.y)*(cur.y - panStartPos.y);
                
                if (distSq < 25.0f) { 
                    #ifdef __EMSCRIPTEN__
                    g_js_pan_delta_x = 0.0f; g_js_pan_delta_y = 0.0f; // Buang data getaran
                    #endif
                    return true; // Chart diam membatu
                } else {
                    isPanConfirmed = true; // Kunci terbuka!
                }
            }

            // --- HITUNG DELTA GERAKAN ---
            float directDelta = 0.0f;

            // WASM Touch
            #ifdef __EMSCRIPTEN__
            if (g_isTouchActive) {
                 if (g_js_pan_delta_x != 0 || g_js_pan_delta_y != 0) {
                     if (fabs(g_js_pan_delta_x) > fabs(g_js_pan_delta_y)) {
                          float pxPerCandle = plotSize.x / zoomLevel;
                          if (pxPerCandle <= 0.001f) pxPerCandle = 1.0f;
                          
                          // Sensitivitas Touch
                          directDelta = -(g_js_pan_delta_x / pxPerCandle) * 1.5f; 
                     } else {
                          // Geser Harga Y (Pan Manual)
                          double range = y_max - y_min;
                          if (plotSize.y > 0) {
                               double pDelta = g_js_pan_delta_y * (range / plotSize.y);
                               y_min += pDelta; y_max += pDelta; autoFitY = false;
                          }
                     }
                     g_js_pan_delta_x = 0; g_js_pan_delta_y = 0;
                 }
            }
            #endif

            // Mouse Desktop (Klik di area chart)
            if (io.MouseDown[0] && !mouseZone) { 
                 if (fabs(io.MouseDelta.y) > fabs(io.MouseDelta.x) * 1.5) {
                     // Mouse Drag Y (Pan Harga)
                     double dY = io.MouseDelta.y * (y_max - y_min) * 0.002;
                     y_min += dY; y_max += dY; autoFitY = false;
                 } else {
                     // Mouse Drag X (Pan Waktu)
                     float dX = io.MouseDelta.x * 1.2f; // Sensitivitas Mouse
                     if (dX > 50) dX = 50; if (dX < -50) dX = -50;
                     double w = end - start;
                     directDelta = -(float)((dX * (w / plotSize.x)));
                 }
            }

            // 🔥 APPLY GERAKAN & REKAM MOMENTUM 🔥
            if (directDelta != 0.0f) {
                dragAccumulator += directDelta;
                inertiaVelocity = directDelta; // Simpan speed buat inertia pas dilepas nanti

                int step = (int)dragAccumulator;
                if (step != 0) {
                    viewCenterIndex += step;
                    dragAccumulator -= step;
                }
            }
            
            // Limit View
            int maxIdx = 0;
            if (g_allCandles.count(g_activeTF)) maxIdx = (int)g_allCandles[g_activeTF].size() - 1;
            viewCenterIndex = std::clamp(viewCenterIndex, 0, maxIdx + (int)(zoomLevel * 5.0f));

            return true;
        }

        if (ImGui::IsMouseDoubleClicked(0)) autoFitY = true;

        return false;
    }
    
    void AutoFitY(double lmin, double lmax) {
        if (autoFitY) { y_min = lmin; y_max = lmax; }
    }
} g_chart;

// =========================================================
// 🔥 ResetChartViewState — reset g_chart saat switch symbol
// Dipisah jadi fungsi karena g_chart belum visible di SwitchSymbol/wasm_clear_chart
// (deklarasi struct ada di bawah, tapi fungsi-fungsi itu di atas)
// =========================================================
void ResetChartViewState() {
    g_chart.viewCenterIndex = -1;    // -1 = resolve ke live end di render loop
    g_chart.autoFitY        = true;
    g_chart.y_min           = 0.0;   // 0 = signal "snap" (bukan lerp)
    g_chart.y_max           = 0.0;
    g_chart.inertiaVelocity = 0.0f;
}

// ===============================================================
// 🛠️ HELPER FUNCTIONS (Network & Data)
// ===============================================================
size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* s) {
    size_t total = size * nmemb;
    s->append((char*)contents, total);
    return total;
}
#ifndef __EMSCRIPTEN__
bool LoadCandleFromServer(std::vector<Candle>& outCandles, const std::string& url) {
    // ⚠️ CATATAN: Di WebAssembly, CURL standar ini mungkin memblokir thread.
    // Sebaiknya gunakan emscripten_fetch jika ingin fully async di web.
    // Tapi untuk kompatibilitas kode, kita biarkan struktur ini.

    CURL* curl = curl_easy_init();
    if (!curl) return false;

    std::string readBuffer;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) return false;

    try {
        auto j = json::parse(readBuffer);
        outCandles.clear();
        for (auto& c : j) {
            Candle candle;
            candle.datetime = c["t"];
            candle.open  = c["o"];
            candle.high  = c["h"];
            candle.low   = c["l"];
            candle.close = c["c"];

            if (c.contains("time")) {
                candle.time = c["time"];
            } else {
                struct tm tm{};
                std::istringstream ss(candle.datetime);
                ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
                #ifdef _WIN32
                candle.time = _mkgmtime(&tm);
                #else
                candle.time = timegm(&tm);
                #endif
            }
            outCandles.push_back(candle);
        }
        return true;
    } catch (...) { return false; }
}
#else
bool LoadCandleFromServer(std::vector<Candle>&, const std::string&) {
    printf("[WEB] curl disabled, skipping server download.\n");
    return false;
}
#endif

// 💾 SAVE FUNCTION
bool SaveCandlesToFile(const std::string& tf, const std::string& filename) {
    // Di Web, file system bersifat virtual (MEMFS/IDBFS).
    // Data tersimpan di memori browser sementara.
    std::lock_guard<std::mutex> lock(g_candlesMutex);
    if (g_allCandles.find(tf) == g_allCandles.end()) return false;

    const auto& candles = g_allCandles[tf];
    std::ofstream ofs(filename, std::ios::binary);
    if (!ofs) return false;

    const char* magic = "TRAD";
    ofs.write(magic, 4);
    size_t count = candles.size();
    ofs.write(reinterpret_cast<const char*>(&count), sizeof(count));

    for (const auto& c : candles) {
        ofs.write(reinterpret_cast<const char*>(&c.open), sizeof(double));
        ofs.write(reinterpret_cast<const char*>(&c.high), sizeof(double));
        ofs.write(reinterpret_cast<const char*>(&c.low), sizeof(double));
        ofs.write(reinterpret_cast<const char*>(&c.close), sizeof(double));
        ofs.write(reinterpret_cast<const char*>(&c.time), sizeof(double));
        ofs.write(reinterpret_cast<const char*>(&c.volume), sizeof(double));

        size_t len = c.datetime.length();
        ofs.write(reinterpret_cast<const char*>(&len), sizeof(len));
        ofs.write(c.datetime.c_str(), len);
    }
    ofs.close();
    return true;
}

// 📂 LOAD FUNCTION
bool LoadCandlesFromFile(const std::string& tf, const std::string& filename) {
    std::ifstream ifs(filename, std::ios::binary);
    if (!ifs) return false;

    char magicBuffer[5] = {0};
    ifs.read(magicBuffer, 4);
    if (std::string(magicBuffer) != "TRAD") { ifs.close(); return false; }

    size_t count = 0;
    ifs.read(reinterpret_cast<char*>(&count), sizeof(count));
    if (count == 0 || count > 5000000) { ifs.close(); return false; }

    std::vector<Candle> tmp;
    tmp.reserve(count);

    for (size_t i = 0; i < count; ++i) {
        Candle c;
        if (!ifs.read(reinterpret_cast<char*>(&c.open), sizeof(double))) break;
        if (!ifs.read(reinterpret_cast<char*>(&c.high), sizeof(double))) break;
        if (!ifs.read(reinterpret_cast<char*>(&c.low), sizeof(double))) break;
        if (!ifs.read(reinterpret_cast<char*>(&c.close), sizeof(double))) break;
        if (!ifs.read(reinterpret_cast<char*>(&c.time), sizeof(double))) break;
        if (!ifs.read(reinterpret_cast<char*>(&c.volume), sizeof(double))) break;

        size_t len = 0;
        if (!ifs.read(reinterpret_cast<char*>(&len), sizeof(len))) break;
        if (len > 50) break;

        char buf[51];
        if (!ifs.read(buf, len)) break;
        buf[len] = '\0';
        c.datetime = buf;
        tmp.push_back(c);
    }
    ifs.close();
    if (tmp.empty()) return false;

    std::lock_guard<std::mutex> lock(g_candlesMutex);
    g_allCandles[tf] = std::move(tmp);
    g_tfIndices[tf] = (int)g_allCandles[tf].size() - 1;
    return true;
}
// =====================================================
// Helper FormatTime (epoch -> "YYYY-MM-DD HH:MM:SS")
// =====================================================
std::string FormatTime(long long epoch) {
    time_t tt = (time_t)epoch;
    struct tm tm{};
#ifdef _WIN32
    gmtime_s(&tm, &tt);
#else
    gmtime_r(&tt, &tm);
#endif

    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
    return std::string(buf);
}
// ================================================================
// 🔌 WebAssembly WebSocket Bridge
// ================================================================
#ifdef __EMSCRIPTEN__
extern "C" {

void OnWebSocketMessage(const char* raw){
    printf("[WASM] WS Message: %s\n", raw);

    try {
        json j = json::parse(raw);

        if (j.contains("price"))
            currentPrice = j["price"].get<double>();

        if (j.contains("time")) {
            long long t = j["time"];
            double price = j["price"];

            std::lock_guard<std::mutex> lock(g_candlesMutex);
            auto& m1 = g_allCandles["M1"];

            if (m1.empty() || t > (long long)m1.back().time) {
                Candle c;
                c.time = t;
                c.datetime = FormatTime(t);
                c.open = c.high = c.low = c.close = price;
                c.volume = 1;
                m1.push_back(c);
            } else {
                Candle& cur = m1.back();
                cur.high  = std::max(cur.high, price);
                cur.low   = std::min(cur.low,  price);
                cur.close = price;
                cur.volume++;
            }
        }

    } catch (...) {
        printf("[WASM] WS Parse failed\n");
    }
}

} // extern "C"
#endif


// ==================================================================================
// 2. FUNGSI BUILDER BARU (LEBIH PRESISI)
// ==================================================================================
std::vector<Candle> BuildTimeframeFromM1(const std::vector<Candle>& m1_data, int seconds) {
    std::vector<Candle> result;
    if (m1_data.empty()) return result;
    
    // Estimasi ukuran biar memori gak kaget
    result.reserve(m1_data.size() / (seconds / 60));

    Candle currentTFCandle = {};
    bool isBuilding = false;
    long long currentBucketEnd = 0;

    for (const auto& m1 : m1_data) {
        long long m1_time = (long long)m1.time;
        // Hitung waktu awal bucket (misal H1: 08:00, 09:00)
        long long bucketStart = m1_time - (m1_time % seconds);
        long long bucketEnd = bucketStart + seconds;

        // Jika ini candle baru (waktunya sudah lewat bucket sebelumnya)
        if (!isBuilding || m1_time >= currentBucketEnd) {
            // Push candle yang sudah jadi
            if (isBuilding) {
                result.push_back(currentTFCandle);
            }
            
            // Reset untuk candle baru
            isBuilding = true;
            currentBucketEnd = bucketEnd;
            
            currentTFCandle.time = (double)bucketStart;
            currentTFCandle.datetime = FormatTime(bucketStart);
            currentTFCandle.open = m1.open;
            currentTFCandle.high = m1.high;
            currentTFCandle.low = m1.low;
            currentTFCandle.close = m1.close;
            currentTFCandle.volume = m1.volume;
        } else {
            // Update candle yang sedang berjalan
            currentTFCandle.high = std::max(currentTFCandle.high, m1.high);
            currentTFCandle.low = std::min(currentTFCandle.low, m1.low);
            currentTFCandle.close = m1.close; // Close selalu update
            currentTFCandle.volume += m1.volume;
        }
    }
    // Push sisa terakhir
    if (isBuilding) result.push_back(currentTFCandle);
    
    return result;
}
void ReinitReplayForSymbol() {
    g_replay.Pause();
    g_replay.active = false;
    g_replay.linkedTFs.clear(); // Buang semua pointer lama yang basi

    if (!g_allCandles["M1"].empty())
        g_replay.Init(&g_allCandles["M1"], g_replay.speed);

    static const std::vector<std::pair<std::string,int>> tfs = {
        {"M5",300},{"M15",900},{"M30",1800},{"H1",3600},{"H4",14400}
    };
    for (auto& t : tfs)
        if (!g_allCandles[t.first].empty())
            g_replay.LinkTF(&g_allCandles[t.first], &g_tfIndices[t.first]);

    g_replaySourceTF = &g_allCandles[g_activeTF];
    g_replayIndexPtr = &g_tfIndices[g_activeTF];

    // Re-attach callback
    g_replay.OnCandleChange = [](int idx) {
        std::lock_guard<std::mutex> lock(g_candlesMutex);
        g_tfIndices["M1"] = idx;
        if (!g_allCandles["M1"].empty() && idx < (int)g_allCandles["M1"].size()) {
            const Candle& cd = g_allCandles["M1"][idx];
            double cidx = GetActiveTFCandleIndex(cd.time); // ← index di active TF
            // Cek SL/TP + update logic untuk trade di replay (filter per simbol)
            g_replayManager.CheckAllHits(cd, cidx, &g_symbol);

            // Opsional: Update profit floating juga biar angka di tabel update pas digeser slider
            g_replayManager.UpdateAllLogic(cd.close, cd.time, cidx, &g_symbol);
        }
    };
    printf("✅ [REPLAY] Re-init: %s\n", g_symbol.c_str());
}
// =====================================================
// 🔌 WASM BRIDGE (FINAL VERSION) 
// =====================================================
#ifdef __EMSCRIPTEN__
extern "C" {
   
// -----------------------------------------------
// SET PRIMARY BULK LOADING FLAG
// Dipanggil dari JS rebuildFullFromDB() sebelum & sesudah loop bulk push.
// Saat true: wasm_push_tick skip M1 update → tidak ganggu historical load.
//            wasm_push_candle KASUS3 boleh push_back (urutan historical).
// Saat false: kembali normal (bar-close race fix aktif).
// -----------------------------------------------
EMSCRIPTEN_KEEPALIVE
void wasm_set_primary_loading(int loading) {
    g_primaryBulkLoading = (loading != 0);
    printf("[WASM] g_primaryBulkLoading = %d\n", loading);
}

// =====================================================================
// LAZY LOAD PREPEND — 5 fungsi untuk sistem kanan→kiri live mode
// =====================================================================

// 1. Buka sesi prepend — reset buffer
EMSCRIPTEN_KEEPALIVE
void wasm_begin_prepend() {
    g_prependBuffer.clear();
    g_isPrepending = true;
}

// 2. Push satu candle ke buffer prepend (oldest→newest, dipanggil per-candle dari JS)
//    Tidak butuh mutex — single-threaded JS, mutex hanya di wasm_end_prepend
EMSCRIPTEN_KEEPALIVE
void wasm_prepend_candle(double open, double high, double low,
                         double close, double t, double volume) {
    if (!g_isPrepending) return;
    Candle c;
    c.open=open; c.high=high; c.low=low; c.close=close;
    c.time=t;    c.volume=(volume > 0) ? volume : 1.0;
    g_prependBuffer.push_back(c);
}

// 3. Tutup sesi — merge buffer ke depan M1, offset semua viewCenterIndex, rebuild HTF
EMSCRIPTEN_KEEPALIVE
void wasm_end_prepend() {
    g_isPrepending = false;
    if (g_prependBuffer.empty()) return;

    std::lock_guard<std::mutex> lock(g_candlesMutex);
    auto& m1 = g_allCandles["M1"];

    // Sort oldest→newest (data IDB sudah sorted, tapi aman kalau di-sort lagi)
    std::sort(g_prependBuffer.begin(), g_prependBuffer.end(),
        [](const Candle& a, const Candle& b){ return a.time < b.time; });

    // Buang candle yang overlapping dengan data yang sudah ada di M1
    if (!m1.empty()) {
        double firstExisting = m1.front().time;
        // Hapus semua dari buffer yang time-nya >= firstExisting (sudah ada)
        auto cutoff = std::lower_bound(g_prependBuffer.begin(), g_prependBuffer.end(),
                        firstExisting, [](const Candle& c, double t){ return c.time < t; });
        g_prependBuffer.erase(cutoff, g_prependBuffer.end());
    }
    if (g_prependBuffer.empty()) {
        printf("[PREPEND] Buffer kosong setelah filter overlap, skip.\n");
        return;
    }

    int N = (int)g_prependBuffer.size();

    // Gabung: prependBuffer (lama) + m1 yang ada (baru) → satu vector besar
    g_prependBuffer.insert(g_prependBuffer.end(), m1.begin(), m1.end());
    m1 = std::move(g_prependBuffer);
    g_prependBuffer.clear();

    // ── OFFSET SEMUA viewCenterIndex ──────────────────────────────────
    // Prepend geser semua index +N. Tanpa ini, view loncat ke candle lama.
    // g_chart (primary tab legacy) dan tiap tab state harus di-offset.
    g_chart.viewCenterIndex += N;
    for (auto* tab : g_chartManager.tabs) {
        if (tab->usesGlobalData && tab->state.viewCenterIndex >= 0)
            tab->state.viewCenterIndex += N;
    }

    // Update g_tfIndices M1 ke akhir data baru
    g_tfIndices["M1"] = (int)m1.size() - 1;

    printf("[PREPEND] +%d candles prepended → M1 total = %zu\n", N, m1.size());
    // Caller (JS) bertanggung jawab panggil wasm_rebuild_all_htfs() setelah ini
}

// 4. JS memanggil ini setelah lazy load selesai → buka gate untuk trigger berikutnya
EMSCRIPTEN_KEEPALIVE
void wasm_set_lazy_load_done() {
    g_lazyLoadPending = false;
    // Juga clear lazyPending untuk primary tab (backward compat)
    for (auto* t : g_chartManager.tabs)
        if (t->usesGlobalData) { t->lazyPending = false; break; }
    printf("[LAZY] g_lazyLoadPending = false, siap trigger berikutnya\n");
}

// 🔥 PER-TAB LAZY CALLBACKS
// JS panggil ini setelah rebuild selesai untuk tab tertentu
EMSCRIPTEN_KEEPALIVE
void wasm_set_tab_lazy_done(int tabId) {
    ChartTab* t = g_chartManager.GetById(tabId);
    if (t) {
        t->lazyPending = false;
        printf("[LAZY] Tab[%d] lazyPending = false\n", tabId);
    }
    // Kalau tab utama, sync global juga
    if (t && t->usesGlobalData) g_lazyLoadPending = false;
}

// JS panggil ini kalau server konfirmasi tidak ada data lebih lama lagi
EMSCRIPTEN_KEEPALIVE
void wasm_set_tab_no_more_history(int tabId) {
    ChartTab* t = g_chartManager.GetById(tabId);
    if (t) {
        t->noMoreHistory = true;
        t->lazyPending   = false;
        printf("[LAZY] Tab[%d] noMoreHistory = true\n", tabId);
    }
    if (t && t->usesGlobalData) g_lazyLoadPending = false;
}

// 5. JS butuh tahu waktu candle tertua yang sudah ada di WASM
//    untuk query IDB: getOlderCandlesFromDB(sym, before=this_time)
EMSCRIPTEN_KEEPALIVE
double wasm_get_oldest_loaded_time() {
    auto it = g_allCandles.find("M1");
    if (it == g_allCandles.end() || it->second.empty()) return 0.0;
    return it->second.front().time;
}

// =========================================================
// 🔥 VIEW ANCHOR — simpan & restore posisi view saat lazy rebuild
//
// Sebelum clear+rebuild: JS panggil wasm_get_view_anchor()
//   → C++ simpan: waktu candle tengah layar + zoom level
// Setelah rebuild: JS panggil wasm_set_view_anchor()
//   → C++ cari candle dengan waktu terdekat → set viewCenterIndex ke situ
//   → User tidak kehilangan posisi scroll
// =========================================================
static double g_viewAnchorTime = 0.0;
static float  g_viewAnchorZoom = 0.0f;

EMSCRIPTEN_KEEPALIVE
void wasm_save_view_anchor() {
    // Simpan waktu candle yang sedang di tengah layar + zoom level
    ChartTab* tab = g_chartManager.GetActiveTab();
    if (!tab) { g_viewAnchorTime = 0; return; }

    std::string tf = tab->usesGlobalData ? g_activeTF : tab->timeframe;
    int vcIdx = tab->usesGlobalData ? g_chart.viewCenterIndex : tab->state.viewCenterIndex;
    float zoom = tab->usesGlobalData ? g_chart.zoomLevel : tab->state.zoomLevel;

    std::lock_guard<std::mutex> lk(g_candlesMutex);
    std::string key = tab->usesGlobalData ? tf : (tab->symbol + "_" + tf);
    if (g_allCandles.count(key) && !g_allCandles[key].empty()) {
        int idx = std::clamp(vcIdx, 0, (int)g_allCandles[key].size() - 1);
        g_viewAnchorTime = g_allCandles[key][idx].time;
        g_viewAnchorZoom = zoom;
        printf("[VIEW] Anchor saved: time=%.0f, zoom=%.0f\n", g_viewAnchorTime, g_viewAnchorZoom);
    } else {
        g_viewAnchorTime = 0;
        g_viewAnchorZoom = 0;
    }
}

EMSCRIPTEN_KEEPALIVE
void wasm_restore_view_anchor() {
    // Cari candle terdekat dengan waktu yang disimpan → set viewCenterIndex
    if (g_viewAnchorTime <= 0) return;

    ChartTab* tab = g_chartManager.GetActiveTab();
    if (!tab) return;

    std::string tf = tab->usesGlobalData ? g_activeTF : tab->timeframe;

    std::lock_guard<std::mutex> lk(g_candlesMutex);
    std::string key = tab->usesGlobalData ? tf : (tab->symbol + "_" + tf);
    if (!g_allCandles.count(key) || g_allCandles[key].empty()) return;

    auto& vec = g_allCandles[key];
    // Binary search untuk candle terdekat
    auto it = std::lower_bound(vec.begin(), vec.end(), g_viewAnchorTime,
        [](const Candle& c, double t) { return c.time < t; });
    int newIdx = (it != vec.end()) ? (int)std::distance(vec.begin(), it) : (int)vec.size() - 1;

    if (tab->usesGlobalData) {
        g_chart.viewCenterIndex = newIdx;
        g_chart.autoFitY = true;
        if (g_viewAnchorZoom > 0) g_chart.zoomLevel = g_viewAnchorZoom;
    }
    tab->state.viewCenterIndex = newIdx;
    tab->state.autoFitY = true;
    if (g_viewAnchorZoom > 0) tab->state.zoomLevel = g_viewAnchorZoom;

    printf("[VIEW] Anchor restored: idx=%d (time=%.0f, zoom=%.0f)\n",
        newIdx, g_viewAnchorTime, g_viewAnchorZoom);

    g_viewAnchorTime = 0;
    g_viewAnchorZoom = 0;
}

// =========================================================
// 🔥 VIEW ANCHOR PER-TAB — untuk lazy rebuild tab non-primary
// Sama seperti wasm_save/restore_view_anchor tapi target tab spesifik
// (bukan active tab). Dipanggil dari JS: onNearLeftEdgeTab()
// =========================================================
// Per-tab anchor storage (map tabId → {time, zoom})
static std::map<int, std::pair<double,float>> g_tabViewAnchors;

EMSCRIPTEN_KEEPALIVE
void wasm_save_view_anchor_tab(int tabId) {
    ChartTab* tab = g_chartManager.GetById(tabId);
    if (!tab) return;

    std::string key = tab->usesGlobalData
        ? tab->timeframe
        : (tab->symbol + "_" + tab->timeframe);

    std::lock_guard<std::mutex> lk(g_candlesMutex);
    if (!g_allCandles.count(key) || g_allCandles[key].empty()) return;

    int vcIdx = tab->state.viewCenterIndex;
    int idx = std::clamp(vcIdx, 0, (int)g_allCandles[key].size() - 1);
    double anchorTime = g_allCandles[key][idx].time;
    float  anchorZoom = tab->state.zoomLevel;

    g_tabViewAnchors[tabId] = {anchorTime, anchorZoom};
    printf("[VIEW TAB%d] Anchor saved: time=%.0f, zoom=%.0f\n", tabId, anchorTime, anchorZoom);
}

EMSCRIPTEN_KEEPALIVE
void wasm_restore_view_anchor_tab(int tabId) {
    auto it = g_tabViewAnchors.find(tabId);
    if (it == g_tabViewAnchors.end() || it->second.first <= 0) return;

    ChartTab* tab = g_chartManager.GetById(tabId);
    if (!tab) return;

    double savedTime = it->second.first;
    float  savedZoom = it->second.second;

    std::string key = tab->usesGlobalData
        ? tab->timeframe
        : (tab->symbol + "_" + tab->timeframe);

    std::lock_guard<std::mutex> lk(g_candlesMutex);
    if (!g_allCandles.count(key) || g_allCandles[key].empty()) return;

    auto& vec = g_allCandles[key];
    auto cit = std::lower_bound(vec.begin(), vec.end(), savedTime,
        [](const Candle& c, double t) { return c.time < t; });
    int newIdx = (cit != vec.end()) ? (int)std::distance(vec.begin(), cit) : (int)vec.size() - 1;

    tab->state.viewCenterIndex = newIdx;
    tab->state.autoFitY = true;
    if (savedZoom > 0) tab->state.zoomLevel = savedZoom;

    printf("[VIEW TAB%d] Anchor restored: idx=%d (time=%.0f, zoom=%.0f)\n",
        tabId, newIdx, savedTime, savedZoom);

    g_tabViewAnchors.erase(tabId);
}

// -----------------------------------------------
// 1. PUSH CANDLE (FULL BAR) — OPSIONAL UNTUK REPLAY
// -----------------------------------------------
EMSCRIPTEN_KEEPALIVE
void wasm_push_candle(double open, double high, double low, double close, double t, double volume) {
    // Gate replay: saat replay aktif, live feed di-skip (websocket.js tetap simpan ke IDB)
    if (g_replayGateActive) return;
    // Cek replay
    if (g_replayCutoff.active || replayStarted) return;

    std::lock_guard<std::mutex> lock(g_candlesMutex);
    // 🔥 INI YANG TADI ERROR (DEKLARASI DULU)
        Candle newCandle; 
        newCandle.open = open;
        newCandle.high = high;
        newCandle.low = low;
        newCandle.close = close;
        newCandle.time = t;
        newCandle.volume = volume;

    Candle c;
    c.time     = (double)t;
    c.datetime = FormatTime((long long)t);
    c.open     = open;
    c.high     = high;
    c.low      = low;
    c.close    = close;
    c.volume   = volume;

    auto& m1 = g_allCandles["M1"];

    if (m1.empty()) {
        m1.push_back(c);
    } 
    else {
        Candle& last = m1.back();
        
        // KASUS 1: Data Masa Depan (Normal Live Trading)
        if (c.time > last.time) {
            m1.push_back(c);
        } 
        // KASUS 2: Data Waktu Sama (Update Live Bar)
        else if ((long long)c.time == (long long)last.time) {
            last.high = std::max(last.high, c.high);
            last.low  = std::min(last.low, c.low);
            last.close = c.close;
            last.volume += c.volume;
        } 
        // 🔥 KASUS 3: DATA MASA LALU — dua skenario berbeda:
        //
        // A) g_primaryBulkLoading = true → rebuildFullFromDB sedang jalan.
        //    Ini historical candle dari IDB → push_back biasa.
        //    wasm_push_tick sudah diblokir, tidak ada live candle yang nyasar duluan.
        //
        // B) g_primaryBulkLoading = false → data sudah live.
        //    Ini bar-close server yang tiba SETELAH wasm_push_tick sudah push candle barX+60.
        //    c.time=barX < last.time=barX+60 → ⚠️ JANGAN push_back, duplikat!
        //    Cari candle barX yang sudah ada → finalize OHLC dari server.
        else {
            if (g_primaryBulkLoading) {
                // Skenario A: historical bulk load → push biasa
                m1.push_back(c);
            } else {
                // Skenario B: bar-close terlambat → search & finalize
                for (int i = (int)m1.size() - 1; i >= 0; i--) {
                    if ((long long)m1[i].time == (long long)c.time) {
                        // Finalize OHLC dari server (lebih akurat dari tick akumulasi)
                        m1[i].open  = c.open;
                        m1[i].high  = std::max(m1[i].high, c.high);
                        m1[i].low   = std::min(m1[i].low,  c.low);
                        m1[i].close = c.close;
                        // Volume TIDAK di-overwrite — sudah diakumulasi wasm_push_tick
                        break;
                    }
                    if ((long long)m1[i].time < (long long)c.time) break;
                }
                // Tidak ketemu → truly old history → skip (no push)
            }
        }
    }

    // Update index JS
    if (!replayStarted && g_activeTF == "M1")
        g_tfIndices["M1"] = (int)m1.size() - 1;
}

// ──────────────────────────────────────────────────────────────────
// ─────────────────────────────────────────────────────────────────────────
// wasm_push_footprint — dipanggil dari websocket.js per tick/level
//
//   symbol   = simbol sumber data ("BTCUSDT", "XAUUSD", dll)
//   time_val = epoch seconds (M1 bar time)
//   price    = level harga
//   buy_vol  = volume aggressor buy  (side "B" Hyperliquid)
//   sell_vol = volume aggressor sell (side "A" Hyperliquid)
//
// 🔥 ROUTING BERDASARKAN SYMBOL:
//    symbol == g_symbol  → inject HANYA ke primary keys ("M1", "M5", dst)
//    symbol != g_symbol  → inject HANYA ke non-primary keys ("BTCUSDT_M1", dst)
//
//    INI MENCEGAH DOUBLE-INJECT:
//    Sebelumnya: footprint_data untuk non-primary tab JUGA mengisi primary (bug!)
//    Sekarang:   setiap inject tahu persis mana tujuannya.
// ─────────────────────────────────────────────────────────────────────────
EMSCRIPTEN_KEEPALIVE
void wasm_push_footprint(const char* symbol, double time_val, double price, double buy_vol, double sell_vol,
                         int fromIDB)  // 🔥 fromIDB=1 → IDB load, bypass gate replay
{
    // Gate replay: skip live footprint saat replay aktif
    // KECUALI: fromIDB=1 → data dari IDB (historis), bukan live feed → boleh masuk
    if (g_replayGateActive && !fromIDB) return;

    std::lock_guard<std::mutex> lock(g_candlesMutex);

    static const std::vector<std::pair<std::string,int>> tfs = {
        {"M1", 60}, {"M5", 300}, {"M15", 900}, {"M30", 1800},
        {"H1", 3600}, {"H4", 14400}, {"D1", 86400}
    };

    std::string symStr = symbol ? symbol : "";
    long long time_ll = (long long)time_val;

    // Helper: inject footprint ke satu vector candle
    auto injectFP = [&](std::vector<Candle>& candles, int tfSec) {
        if (candles.empty()) return;
        long long bucket = time_ll - (time_ll % (long long)tfSec);
        for (int i = (int)candles.size() - 1; i >= 0; i--) {
            long long ct = (long long)candles[i].time;
            if (ct == bucket) {
                bool merged = false;
                for (auto& lvl : candles[i].footprint) {
                    if (std::abs(lvl.price - price) < 1e-9) {
                        lvl.buyVol  += buy_vol;
                        lvl.sellVol += sell_vol;
                        merged = true;
                        break;
                    }
                }
                if (!merged) {
                    FootprintLevel lvl;
                    lvl.price   = price;
                    lvl.buyVol  = buy_vol;
                    lvl.sellVol = sell_vol;
                    candles[i].footprint.push_back(lvl);
                    std::sort(candles[i].footprint.begin(), candles[i].footprint.end(),
                        [](const FootprintLevel& a, const FootprintLevel& b){
                            return a.price > b.price;
                        });
                }
                break;
            }
            if (ct < bucket) break;
        }
    };

    bool isPrimary = (symStr == g_symbol);

    for (const auto& tf : tfs) {
        // ── PRIMARY keys: inject ke ["M1"], ["M15"], dst ───────────────────
        // Hanya kalau symbol cocok dengan tab utama
        if (isPrimary && g_allCandles.count(tf.first))
            injectFP(g_allCandles[tf.first], tf.second);

        // ── NON-PRIMARY keys: inject ke ["BTCUSDT_M1"], dst ────────────────
        // Selalu inject kalau keynya ADA — ini cover:
        //   1. Tab non-primary beda symbol (normal case)
        //   2. Tab non-primary SAMA symbol dengan tab utama (BUG 8 fix)
        //      → tab utama baca ["M15"], non-primary baca ["BTCUSDT_M15"]
        //      → keduanya dapat footprint sekarang
        std::string key = symStr + "_" + tf.first;
        if (g_allCandles.count(key))
            injectFP(g_allCandles[key], tf.second);
    }
}
    EMSCRIPTEN_KEEPALIVE
    void wasm_mouse_move(float x, float y) {
        ImGuiIO& io = ImGui::GetIO();
        io.MousePos = ImVec2(x, y);
    }

    // 2. KLIK / SENTUH STATUS
    EMSCRIPTEN_KEEPALIVE
    void wasm_mouse_click(int button, int down) {
        ImGuiIO& io = ImGui::GetIO();
        io.MouseDown[button] = (down != 0);
    }

    // 3. SCROLL RODA MOUSE
    EMSCRIPTEN_KEEPALIVE
    void wasm_mouse_wheel(float delta) {
        ImGuiIO& io = ImGui::GetIO();
        io.MouseWheel += delta;
    }

    // 4. TERIMA DELTA GESER (PENTING BUAT PANNING)
    EMSCRIPTEN_KEEPALIVE
    void wasm_pan_chart_pixels(float dx, float dy) {
        // Masukkan ke variabel global yang kita pakai di struct Chart
        g_js_pan_delta_x += dx; 
        g_js_pan_delta_y += dy;
    }

    // 5. TERIMA DELTA ZOOM (PENTING BUAT CUBIT)
    EMSCRIPTEN_KEEPALIVE
    void wasm_zoom_chart(float delta) {
        g_js_zoom_delta += delta;
    }

    // 6. NOTIFIKASI AWAL SENTUH (PENTING BUAT SMART ZONE DETECTOR)
    EMSCRIPTEN_KEEPALIVE
    void wasm_notify_touch_start(float x, float y) {
        g_isTouchActive = true;
        
        g_js_touch_start_x = x; 
        g_js_touch_start_y = y;
        
        g_js_pan_delta_x = 0.0f;
        g_js_pan_delta_y = 0.0f;
    }

    // 7. 🔥 NOTIFIKASI AKHIR SENTUH — INI YANG HILANG!
    // Reset semua state touch sekaligus agar tidak "nyangkut"
    EMSCRIPTEN_KEEPALIVE
    void wasm_notify_touch_end() {
        g_isTouchActive = false;        // ← Ini yang bikin g_isTouchActive nyangkut di 1
        g_chart.ResetTouchState();      // ← Reset isResizingY, isPanConfirmed, deltas, dll
        
        // Pastikan ImGui tahu mouse sudah dilepas
        ImGuiIO& io = ImGui::GetIO();
        io.MouseDown[0] = false;
        io.MouseDown[1] = false;
    }

/// Tambahkan fungsi helper ini di atas atau di dalam extern "C"
ImGuiKey MapBrowserKeyToImGuiKey(int key) {
    switch (key) {
        case 8:   return ImGuiKey_Backspace;
        case 9:   return ImGuiKey_Tab;
        case 13:  return ImGuiKey_Enter;
        case 37:  return ImGuiKey_LeftArrow;
        case 38:  return ImGuiKey_UpArrow;
        case 39:  return ImGuiKey_RightArrow;
        case 40:  return ImGuiKey_DownArrow;
        case 46:  return ImGuiKey_Delete;
        case 32:  return ImGuiKey_Space;
        default:  return ImGuiKey_None;
    }
}

    EMSCRIPTEN_KEEPALIVE
    void wasm_send_char(int char_code) {
        ImGuiIO& io = ImGui::GetIO();
        io.AddInputCharacter((unsigned int)char_code);
    }

    EMSCRIPTEN_KEEPALIVE
    void wasm_send_key(int key, int down) {
        ImGuiIO& io = ImGui::GetIO();
        ImGuiKey imgui_key = MapBrowserKeyToImGuiKey(key);
        
        if (imgui_key != ImGuiKey_None) {
            // Menggunakan fungsi terbaru untuk ImGui v1.87+
            io.AddKeyEvent(imgui_key, (down == 1));
        }
}

// ==============================================================================
// UPDATE TICK: FORCE TICK COUNT MODE (MT5 STYLE)
// ==============================================================================
extern "C" {

EMSCRIPTEN_KEEPALIVE
int wasm_push_tick(const char* symbol, double price, double vol, double t) {
    
    std::string symStr = symbol;

    // 1. UPDATE MARKET WATCH (Tetap pakai vol asli untuk info real volume di tabel)
    g_marketWatch.UpdateTick(symStr, price, vol);

    // 2. UPDATE CANDLE LIVE UNTUK TAB NON-UTAMA (symbol berbeda dari main tab)
    // Tanpa ini: Market Watch ✅ tapi candle BTCUSD di tab non-utama tidak bergerak live
    if (symStr != g_symbol && !g_replayCutoff.active && !replayStarted) {
        double vAdd = (vol > 0 && vol <= 50) ? vol : 1.0;
        long long bTime = (long long)t;

        // Cek apakah ada tab non-utama yang butuh symbol ini
        bool hasTab = false;
        for (auto* tab : g_chartManager.tabs) {
            if (!tab->usesGlobalData && tab->symbol == symStr) { hasTab = true; break; }
        }
        if (!hasTab) return 0; // Tidak ada yang butuh → hemat CPU

        std::lock_guard<std::mutex> lk(g_candlesMutex);

        // Update semua TF untuk symbol ini di g_allCandles["BTCUSD_M1"], dst
        static const std::vector<std::pair<std::string,int>> allTFs = {
            {"M1",60},{"M5",300},{"M15",900},{"M30",1800},{"H1",3600},{"H4",14400},{"D1",86400}
        };
        for (auto& tf : allTFs) {
            std::string key = symStr + "_" + tf.first;
            if (!g_allCandles.count(key) || g_allCandles[key].empty()) continue;
            auto& vec = g_allCandles[key];
            long long bucket = bTime - (bTime % tf.second);
            if (bucket > (long long)vec.back().time) {
                Candle nc; nc.time=(double)bucket;
                nc.open=nc.high=nc.low=nc.close=price; nc.volume=vAdd;
                vec.push_back(nc);
            } else if ((long long)vec.back().time == bucket) {
                Candle& last = vec.back();
                last.high=std::max(last.high,price); last.low=std::min(last.low,price);
                last.close=price; last.volume+=vAdd;
            }
        }
        return 1;
    }

    // 3. FILTER: bukan symbol utama dan tidak ada tab yang butuh
    // Guard: kalau g_symbol kosong (picker belum selesai) → skip
    if (g_symbol.empty() || symStr != g_symbol) return 0;

    // Gate replay: skip live tick saat replay aktif
    if (g_replayGateActive) return 0;
    // 4. CEK REPLAY
    if (g_replayCutoff.active || replayStarted) return 0;

    // 🔥 GATE BULK LOAD: rebuildFullFromDB sedang jalan (yield antar batch)
    // MarketWatch sudah diupdate di step 1. M1/HTF jangan disentuh — akan
    // merusak urutan historical (candle live masuk duluan sebelum history).
    // Saat g_primaryBulkLoading selesai, HTF di-rebuild via wasm_rebuild_all_htfs.
    if (g_primaryBulkLoading) return 0;
    
    // Update Global Price
    currentPrice = price;
    g_liveTick.price  = price;
    g_liveTick.time   = (double)t;
    g_liveTick.hasNew = true;

    std::lock_guard<std::mutex> lock(g_candlesMutex);
    // 🔥 PERBAIKAN LOGIKA MERGER (MT5 TICK STYLE) 🔥
    // =================================================================
    // Kita abaikan parameter 'vol' yang dikirim API (karena itu Real Volume/Lot).
    // Kita anggap setiap panggilan fungsi ini adalah 1 Tick (perubahan harga).
    // Kecuali: Jika 'vol' yang dikirim SANGAT KECIL (misal <= 50),
    // kita asumsikan itu adalah "Batch Tick Count" dari Javascript.
    
    double volumeToAdd = 1.0; 

    // OPSI: Kalau JS kamu ngirim data batch (sekali kirim isinya akumulasi 5 tick)
    // Gunakan logika ini. Kalau JS kirim per-transaksi, ini otomatis jadi 1.
    if (vol > 0 && vol <= 50) {
       volumeToAdd = vol; 
    } else {
       volumeToAdd = 1.0; // Paksa 1, abaikan nilai ribuan/jutaan
    }
    // =================================================================

    long long barTime = (long long)t;
    long long input_m1_bucket = barTime - (barTime % 60);

    auto& m1 = g_allCandles["M1"];

    // ⚙️ LOGIKA MERGER KE CHART M1
    if (m1.empty() || input_m1_bucket > (long long)m1.back().time) {
        // --- NEW CANDLE (CANDLE BARU) ---
        Candle c;
        c.time = (double)input_m1_bucket;
        c.open = c.high = c.low = c.close = price;
        
        // Mulai hitungan dari 1 (atau batch size)
        c.volume = volumeToAdd; 
        
        m1.push_back(c);
    } else {
        // --- UPDATE CANDLE (SEDANG BERJALAN) ---
        Candle& c = m1.back();
        c.high = std::max(c.high, price);
        c.low  = std::min(c.low, price);
        c.close = price;
        
        // AKUMULASI: Tambahkan 1 tick lagi ke total yang ada
        c.volume += volumeToAdd; 
    }

    // Update Index M1
    if (!replayStarted && g_activeTF == "M1")
        g_tfIndices["M1"] = (int)m1.size() - 1;

    // ⚙️ LOGIKA MERGER KE TIMEFRAME LAIN (M5, H1, dst)
    static std::vector<std::pair<std::string, int>> tfs = {
        {"M1", 60}, {"M5",300}, {"M15",900}, {"M30",1800}, {"H1",3600}, {"H4",14400}, {"D1",86400}
    };

    for (auto& tf : tfs) {
        std::string tfName = tf.first;
        int tfSec = tf.second;
        
        auto& candles = g_allCandles[tfName];
        long long bucket = barTime - (barTime % tfSec);

        if (candles.empty() || bucket > (long long)candles.back().time) {
            Candle nc;
            nc.time = (double)bucket;
            nc.open = nc.high = nc.low = nc.close = price;
            nc.volume = volumeToAdd; // Reset
            candles.push_back(nc);
        } else {
            Candle& last = candles.back();
            if ((long long)last.time == bucket) {
                last.high = std::max(last.high, price);
                last.low  = std::min(last.low, price);
                last.close = price;
                last.volume += volumeToAdd; // Akumulasi
            }
        }
        
        if (!replayStarted && g_activeTF == tfName) {
             g_tfIndices[tfName] = (int)candles.size() - 1;
        }
    }

    // ================================================================
    // FIX: Update g_allCandles["SYMBOL_TF"] untuk tab non-utama
    // yang memiliki symbol SAMA dengan g_symbol (primary tab).
    //
    // Bug sebelumnya: block 2 hanya jalan kalau symStr != g_symbol.
    // Akibatnya tab non-utama XAUUSD tidak pernah dapat tick live
    // karena "XAUUSD_M1" tidak pernah diupdate ketika g_symbol=XAUUSD.
    // ================================================================
    {
        bool hasSameSymTab = false;
        for (auto* tab : g_chartManager.tabs) {
            if (!tab->usesGlobalData && tab->symbol == symStr) {
                hasSameSymTab = true; break;
            }
        }
        if (hasSameSymTab) {
            static const std::vector<std::pair<std::string,int>> allTFs2 = {
                {"M1",60},{"M5",300},{"M15",900},{"M30",1800},
                {"H1",3600},{"H4",14400},{"D1",86400}
            };
            for (auto& tf : allTFs2) {
                std::string key = symStr + "_" + tf.first;
                if (!g_allCandles.count(key) || g_allCandles[key].empty()) continue;
                auto& vec = g_allCandles[key];
                long long bucket = barTime - (barTime % (long long)tf.second);
                if (bucket > (long long)vec.back().time) {
                    Candle nc; nc.time = (double)bucket;
                    nc.open = nc.high = nc.low = nc.close = price;
                    nc.volume = volumeToAdd;
                    vec.push_back(nc);
                } else if ((long long)vec.back().time == bucket) {
                    Candle& last = vec.back();
                    last.high  = std::max(last.high, price);
                    last.low   = std::min(last.low,  price);
                    last.close = price;
                    last.volume += volumeToAdd;
                }
            }
        }
    }

    return 1;
}

// =====================================================================
// WASM BRIDGE — TAB NON-UTAMA (multi-chart)
// Key g_allCandles untuk tab non-utama = "SYMBOL_TF"
// =====================================================================
#ifdef __EMSCRIPTEN__
extern "C" {

EMSCRIPTEN_KEEPALIVE
void wasm_push_candle_for_tab(int tabId,
                               double open, double high, double low,
                               double close, double t, double volume)
{
    ChartTab* tab = g_chartManager.GetById(tabId);
    if (!tab || tab->usesGlobalData) return;

    // SELALU push ke key "SYMBOL_M1" karena JS selalu kirim M1 candles.
    std::string key = tab->symbol + "_M1";
    std::lock_guard<std::mutex> lock(g_candlesMutex);
    auto& vec = g_allCandles[key];

    Candle c;
    c.time     = t;
    c.open     = open;  c.high = high;
    c.low      = low;   c.close = close;
    c.volume   = (volume > 0) ? volume : 1.0;
    c.datetime = FormatTime((long long)t);

    // 🔥 FIX: PUSH TANPA FILTER — jangan cek ordering!
    // Alasan: JS sort bisa tidak sempurna (floating point, string time di IDB).
    // Kalau cek `c.time > vec.back().time`, candle yang urutannya salah sedikit
    // akan di-DROP diam-diam → chart bolong.
    // Sort & dedup dilakukan di wasm_rebuild_htfs_for_tab() setelah semua masuk.
    vec.push_back(c);
}

EMSCRIPTEN_KEEPALIVE
void wasm_rebuild_htfs_for_tab(int tabId)
{
    ChartTab* tab = g_chartManager.GetById(tabId);
    if (!tab || tab->usesGlobalData) return;

    std::string sym  = tab->symbol;
    std::string m1Key = sym + "_M1";

    std::lock_guard<std::mutex> lock(g_candlesMutex);
    if (!g_allCandles.count(m1Key) || g_allCandles[m1Key].empty()) return;

    auto& m1 = g_allCandles[m1Key];
    std::sort(m1.begin(), m1.end(), [](const Candle& a, const Candle& b){ return a.time < b.time; });
    auto last = std::unique(m1.begin(), m1.end(), [](const Candle& a, const Candle& b){
        return (long long)a.time == (long long)b.time;
    });
    m1.erase(last, m1.end());

    static const std::vector<std::pair<std::string,int>> targets = {
        {"M5",300},{"M15",900},{"M30",1800},{"H1",3600},{"H4",14400},{"D1",86400}
    };
    for (auto& tf : targets) {
        std::string key = sym + "_" + tf.first;
        auto& dst = g_allCandles[key];
        dst.clear();
        dst.reserve(m1.size() / (tf.second / 60) + 100);
        for (const auto& cm : m1) {
            long long bucket = (long long)cm.time - ((long long)cm.time % tf.second);
            if (dst.empty() || (long long)dst.back().time != bucket) {
                Candle nc;
                nc.time = (double)bucket;
                nc.open = cm.open; nc.high = cm.high;
                nc.low  = cm.low;  nc.close = cm.close;
                nc.volume = (cm.volume > 0) ? cm.volume : 1.0;
                // 🔥 Copy footprint M1 ke HTF candle baru
                nc.footprint = cm.footprint;
                dst.push_back(nc);
            } else {
                Candle& bl = dst.back();
                bl.high = std::max(bl.high, cm.high);
                bl.low  = std::min(bl.low,  cm.low);
                bl.close = cm.close;
                bl.volume += (cm.volume > 0) ? cm.volume : 1.0;
                // 🔥 Merge footprint M1 ke HTF candle yang sudah ada
                for (const auto& lvM1 : cm.footprint) {
                    bool merged = false;
                    for (auto& lvHTF : bl.footprint) {
                        if (std::abs(lvHTF.price - lvM1.price) < 0.005) {
                            lvHTF.buyVol  += lvM1.buyVol;
                            lvHTF.sellVol += lvM1.sellVol;
                            merged = true; break;
                        }
                    }
                    if (!merged) bl.footprint.push_back(lvM1);
                }
                if (!bl.footprint.empty()) {
                    std::sort(bl.footprint.begin(), bl.footprint.end(),
                        [](const FootprintLevel& a, const FootprintLevel& b){
                            return a.price > b.price;
                        });
                }
            }
        }
    }
    // Setelah rebuild selesai:
    // - Kalau ada view anchor tersimpan (dari lazy rebuild), JANGAN reset view.
    //   restore_view_anchor_tab() akan dipanggil oleh JS setelah fungsi ini selesai.
    // - Kalau tidak ada anchor (first load / switch symbol), reset ke live end.
    if (g_tabViewAnchors.count(tabId)) {
        // Anchor ada → biarkan, restore_view_anchor_tab() handle nanti
        tab->state.autoFitY = true;
    } else {
        tab->state.viewCenterIndex = -1;
        tab->state.autoFitY        = true;
    }
    tab->isLoading             = false;  // 🔥 FIX: Loading selesai, live feed boleh masuk

    // Sync symbol ke orderFlowRenderer instance tab ini
    // (symbol bisa berubah jika tab ganti symbol → tickSize harus ikut update)
    tab->InitOrderFlow();

    printf("[WASM] HTF rebuilt for Tab[%d] %s — M1=%zu candles, view reset ke live end\n",
        tabId, sym.c_str(), m1.size());
}

EMSCRIPTEN_KEEPALIVE
void wasm_clear_tab(int tabId)
{
    ChartTab* tab = g_chartManager.GetById(tabId);
    if (!tab || tab->usesGlobalData) return;

    std::lock_guard<std::mutex> lock(g_candlesMutex);
    static const char* allTFs[] = {"M1","M5","M15","M30","H1","H4","D1"};
    for (auto* tf : allTFs) {
        g_allCandles.erase(tab->symbol + "_" + std::string(tf));
    }
    tab->state.viewCenterIndex = -1;
    tab->state.autoFitY        = true;
    tab->isLoading             = true;  // 🔥 FIX: Tandai sedang loading
    tab->lazyPending           = false; // 🔥 Reset lazy state saat symbol berubah
    tab->noMoreHistory         = false; // 🔥 Symbol baru → history belum diketahui
    printf("[WASM] Tab[%d] cleared + isLoading=true\n", tabId);
}

// =========================================================
// 🔥 BARU: LIVE FEED UNTUK TAB NON-UTAMA
// Dipanggil dari JS saat bar/tick masuk untuk symbol yang BUKAN CURRENT_SYMBOL
// tapi ada tab non-utama yang butuh data ini.
// =========================================================
EMSCRIPTEN_KEEPALIVE
void wasm_push_candle_for_symbol(const char* sym,
                                  double open, double high, double low,
                                  double close, double t, double volume)
{
    std::string symbol(sym);
    
    // Cek apakah ada tab non-utama yang pakai symbol ini DAN TIDAK sedang loading
    bool hasReadyTab = false;
    for (auto* tab : g_chartManager.tabs) {
        if (!tab->usesGlobalData && tab->symbol == symbol) {
            if (tab->isLoading) {
                // 🔥 FIX: Ada tab yang sedang bulk-load dari IDB!
                // JANGAN push live data ke SYMBOL_M1 karena akan merusak
                // ordering (live time >> history time) → semua history di-drop.
                return;
            }
            hasReadyTab = true;
        }
    }
    if (!hasReadyTab) return; // Tidak ada tab ready yang butuh → skip

    std::string m1Key = symbol + "_M1";
    std::lock_guard<std::mutex> lock(g_candlesMutex);
    auto& vec = g_allCandles[m1Key];
    
    if (vec.empty()) return; // Belum pernah load dari IDB → skip

    Candle cd;
    cd.time   = t;
    cd.open   = open;  cd.high = high;
    cd.low    = low;   cd.close = close;
    cd.volume = (volume > 0) ? volume : 1.0;

    if (t > vec.back().time) {
        // Candle baru
        vec.push_back(cd);
    } else if ((long long)t == (long long)vec.back().time) {
        // Update candle terakhir (same minute)
        Candle& last = vec.back();
        last.high  = std::max(last.high, high);
        last.low   = std::min(last.low,  low);
        last.close = close;
        last.volume += (volume > 0) ? volume : 1.0;
    } else {
        return; // Data lama, skip
    }

    // ── INCREMENTAL HTF UPDATE ──
    // Tidak perlu full rebuild. Cukup update candle terakhir per-TF.
    static const std::pair<const char*, int> htfTargets[] = {
        {"M5",300}, {"M15",900}, {"M30",1800}, {"H1",3600}, {"H4",14400}, {"D1",86400}
    };
    
    for (auto& tf : htfTargets) {
        std::string key = symbol + "_" + tf.first;
        auto& dst = g_allCandles[key];
        
        long long bucket = (long long)t - ((long long)t % tf.second);
        
        if (!dst.empty() && (long long)dst.back().time == bucket) {
            // Update candle terakhir
            Candle& bl = dst.back();
            bl.high  = std::max(bl.high, high);
            bl.low   = std::min(bl.low,  low);
            bl.close = close;
            bl.volume += (volume > 0) ? volume : 1.0;
        } else if (dst.empty() || bucket > (long long)dst.back().time) {
            // Candle baru di TF ini
            Candle nc;
            nc.time   = (double)bucket;
            nc.open   = open;  nc.high = high;
            nc.low    = low;   nc.close = close;
            nc.volume = (volume > 0) ? volume : 1.0;
            dst.push_back(nc);
        }
    }
}

} // extern "C"
#endif // __EMSCRIPTEN__

} // extern C
// =========================================================
// REBUILD HTF (GENERATOR M5, H1, D1) - FIXED VOLUME SUM
// =========================================================
extern "C" {

EMSCRIPTEN_KEEPALIVE
void wasm_rebuild_all_htfs() {
    std::lock_guard<std::mutex> lock(g_candlesMutex);
    
    // 1. Cek Sumber Data (M1)
    if (g_allCandles.find("M1") == g_allCandles.end()) return;
    auto& m1 = g_allCandles["M1"];
    if (m1.empty()) return;

    // (HTF rebuild log ringkas di bawah)

    // 2. Sortir M1 (PENTING: Biar urutan waktu benar)
    std::sort(m1.begin(), m1.end(), [](const Candle& a, const Candle& b) {
        return a.time < b.time;
    });

    // 3. Hapus Duplikat M1 (Opsional tapi bagus)
    auto last = std::unique(m1.begin(), m1.end(), [](const Candle& a, const Candle& b){
        return (long long)a.time == (long long)b.time;
    });
    m1.erase(last, m1.end());

    // 4. Daftar Timeframe Target
    static std::vector<std::pair<std::string, int>> targets = {
        {"M5", 300}, 
        {"M15", 900}, 
        {"M30", 1800}, 
        {"H1", 3600}, 
        {"H4", 14400}, 
        {"D1", 86400}
    };

    // 5. LOOP GENERATOR (Rumus "BuildTimeframeFromM1" kita taruh sini biar jelas)
    for (auto& t : targets) {
        std::string tfName = t.first;
        int tfSec = t.second;

        // Reset data lama di TF ini
        g_allCandles[tfName].clear();
        auto& targetVec = g_allCandles[tfName];
        
        // Optimasi memori
        targetVec.reserve(m1.size() / (tfSec / 60) + 100);

        // LOGIKA AKUMULASI (MERGER)
        for (const auto& cM1 : m1) {
            long long m1Time = (long long)cM1.time;
            long long bucket = m1Time - (m1Time % tfSec); // Bulatkan waktu

            if (targetVec.empty() || (long long)targetVec.back().time != bucket) {
                // --- NEW CANDLE ---
                Candle nc;
                nc.time = (double)bucket;
                nc.open = cM1.open;
                nc.high = cM1.high;
                nc.low  = cM1.low;
                nc.close = cM1.close;
                
                // 🔥 VALIDASI VOLUME (Agar tidak 0)
                // Kita ambil volume M1 sebagai modal awal
                nc.volume = (cM1.volume > 0) ? cM1.volume : 1.0;

                // 🔥 FOOTPRINT: copy level M1 ke HTF candle baru
                nc.footprint = cM1.footprint;
                
                targetVec.push_back(nc);
            } else {
                // --- UPDATE EXISTING CANDLE ---
                Candle& last = targetVec.back();
                last.high = std::max(last.high, cM1.high);
                last.low  = std::min(last.low, cM1.low);
                last.close = cM1.close;

                // 🔥 RUMUS RAHASIA: PAKAI += (TAMBAH), JANGAN = (TIMPA)
                double volToAdd = (cM1.volume > 0) ? cM1.volume : 1.0;
                last.volume += volToAdd;

                // 🔥 FOOTPRINT MERGE: gabungkan level M1 ke HTF candle yang sama
                for (const auto& lvM1 : cM1.footprint) {
                    bool merged = false;
                    for (auto& lvHTF : last.footprint) {
                        if (std::abs(lvHTF.price - lvM1.price) < 0.005) {
                            lvHTF.buyVol  += lvM1.buyVol;
                            lvHTF.sellVol += lvM1.sellVol;
                            merged = true;
                            break;
                        }
                    }
                    if (!merged) last.footprint.push_back(lvM1);
                }
                // Re-sort descending setelah merge
                if (!last.footprint.empty()) {
                    std::sort(last.footprint.begin(), last.footprint.end(),
                        [](const FootprintLevel& a, const FootprintLevel& b){
                            return a.price > b.price;
                        });
                }
            }
        }

        // Update Index Grafik
        g_tfIndices[tfName] = (int)targetVec.size() - 1;
        // (log per-TF dihapus — lihat summary di bawah)
    }

    // 🔥 LOG RINGKAS: 1 baris gantikan 7 baris per-TF
    printf("[WASM] HTF OK — M1:%zu M5:%zu M15:%zu H1:%zu H4:%zu\n",
        g_allCandles.count("M1")  ? g_allCandles["M1"].size()  : 0,
        g_allCandles.count("M5")  ? g_allCandles["M5"].size()  : 0,
        g_allCandles.count("M15") ? g_allCandles["M15"].size() : 0,
        g_allCandles.count("H1")  ? g_allCandles["H1"].size()  : 0,
        g_allCandles.count("H4")  ? g_allCandles["H4"].size()  : 0
    );

    ReinitReplayForSymbol();

    // 🔥 UPDATE NAKED VPOC — scan semua session, cari VPOC yang belum direvisit
    // Dipanggil di sini karena HTF sudah selesai rebuild = data M1 sudah lengkap
    // Analogi: setelah semua mobil parkir → kasih tau siapa yang masih lampu sein nyala
    for (auto* tab : g_chartManager.tabs) {
        if (!tab) continue;
        auto& m1 = g_allCandles["M1"];
        if (!m1.empty())
            tab->orderFlowRenderer.UpdateNakedVPOCs(m1);
    }
}

// ── ORDER BOOK WASM BRIDGE ─────────────────────────────────────────────────
// Dipanggil dari JS websocket_orderflow.js saat data l2Book masuk
// Implementasi di sini karena extern "C" tidak boleh di .h file
void wasm_clear_orderbook(const char* symbol) {
    OB_Clear(std::string(symbol));
}

void wasm_push_orderbook_level(const char* symbol, float price, float size, int isBid) {
    OB_PushLevel(std::string(symbol), price, size, isBid != 0);
}
// ── ORDER BOOK SNAPSHOT WASM BRIDGE ─────────────────────────────────────────
// Dipanggil dari JS websocket_ob.js setiap 1 detik (tick snapshot analytics)
EMSCRIPTEN_KEEPALIVE
void wasm_push_ob_snapshot(const char* symbol, double timestamp,
                           float imbalance, float rise_ratio_60) {
    OB_PushSnapshot(std::string(symbol), timestamp, imbalance, rise_ratio_60);
}

EMSCRIPTEN_KEEPALIVE
void wasm_clear_ob_snapshot(const char* symbol) {
    OB_ClearSnapshot(std::string(symbol));
}
}
}
#endif// extern C
// ===============================================================
// 🔌 WebSocket Logic
// ===============================================================
#ifndef __EMSCRIPTEN__
void InitWebSocket() {
    // ⚠️ CATATAN WEB: Pastikan ws://127.0.0.1:8765 bisa diakses dari browser.
    // Biasanya browser butuh wss:// (Secure) atau localhost yang tepat.
    if (ws.getReadyState() == ix::ReadyState::Open) return;
    ws.setUrl("ws://127.0.0.1:8765");
    ws.setOnMessageCallback([](const ix::WebSocketMessagePtr &msg) {
        if (msg->type == ix::WebSocketMessageType::Message) {
            try {
                auto j = json::parse(msg->str);
                if (j.contains("type") && j["type"] == "tick") {
                    double price = j["price"].get<double>();
                    currentPrice = price;
                    long long barTime = 0;
                    if (j.contains("bar_time") && !j["bar_time"].is_null()) barTime = j["bar_time"];
                    if (barTime == 0) return;

                    std::lock_guard<std::mutex> lock(g_candlesMutex);
                    auto &m1 = g_allCandles["M1"];

                    if (m1.empty() || barTime > (long long)m1.back().time) {
                        Candle c;
                        c.time = (double)barTime;
                        c.datetime = FormatTime(barTime); // Pakai helper
                        c.open = c.high = c.low = c.close = price;
                        c.volume = 1;
                        m1.push_back(c);
                        if (!replayStarted && g_activeTF == "M1")
                            g_tfIndices["M1"] = (int)m1.size() - 1;
                    } else {
                        Candle &cur = m1.back();
                        cur.high = std::max(cur.high, price);
                        cur.low = std::min(cur.low, price);
                        cur.close = price;
                        cur.volume += 1;
                    }

                    // Propagasi HTF Realtime
                    static std::map<std::string, int> tf_seconds = {
                        {"M5", 300}, {"M15", 900}, {"M30", 1800}, {"H1", 3600}, {"H4", 14400}
                    };
                    for (auto& pair : g_allCandles) {
                        const std::string& tf = pair.first;
                        if (tf == "M1") continue;
                        if (tf_seconds.find(tf) == tf_seconds.end()) continue;
                        int duration = tf_seconds[tf];
                        std::vector<Candle>& vec = pair.second;
                        long long bucketTime = barTime - (barTime % duration);

                        if (vec.empty() || (long long)vec.back().time < bucketTime) {
                            Candle nc;
                            nc.time = (double)bucketTime;
                            nc.datetime = FormatTime(bucketTime); // Pakai helper
                            nc.open = nc.high = nc.low = nc.close = price;
                            nc.volume = 1;
                            vec.push_back(nc);
                             if (g_activeTF == tf) RecalculateAllIndicators(vec);
                        } else {
                            if ((long long)vec.back().time == bucketTime) {
                                Candle& cur = vec.back();
                                cur.high = std::max(cur.high, price);
                                cur.low = std::min(cur.low, price);
                                cur.close = price;
                                cur.volume += 1;
                            }
                        }
                        if (!replayStarted && g_activeTF == tf)
                             g_tfIndices[tf] = (int)vec.size() - 1;
                    }
                }
            } catch (...) {}
        }
    });
    ws.start();
}
#else
void InitWebSocket() {
    printf("[WEB] WebSocket disabled in WASM build.\n");
}
#endif
// =========================================================
// 🎨 HELPER: CUSTOM VECTOR ICONS (ALAT GAMBAR CANGGIH)
// =========================================================
enum IconType { ICON_REPLAY, ICON_PLAY, ICON_PAUSE, ICON_PREV, ICON_NEXT };

bool IconButton(const char* str_id, IconType iconType, bool active, const ImVec2& size = ImVec2(36, 36)) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return false;

    // 1. Setup ID & Posisi
    ImGuiContext& g = *GImGui;
    const ImGuiID id = window->GetID(str_id);
    const ImRect bb(window->DC.CursorPos, ImVec2(window->DC.CursorPos.x + size.x, window->DC.CursorPos.y + size.y));
    
    ImGui::ItemSize(bb);
    if (!ImGui::ItemAdd(bb, id)) return false;

    // 2. Logika Interaksi (Click/Hover)
    bool hovered, held;
    bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);

    // 3. Tentukan Warna (TradingView Style)
    // - Aktif/Running: BIRU TERANG (Glow)
    // - Hover: Putih Terang
    // - Biasa: Abu-abu
    ImU32 colIcon;
    if (active) colIcon = IM_COL32(0, 180, 255, 255);       // Biru (Aktif)
    else if (hovered) colIcon = IM_COL32(255, 255, 255, 255); // Putih (Hover)
    else colIcon = IM_COL32(180, 180, 180, 255);              // Abu (Mati)

    // Render Background Tipis kalau di-hover (biar user tau ini tombol)
    if (hovered || active) {
        ImU32 colBg = active ? IM_COL32(0, 140, 255, 40) : IM_COL32(255, 255, 255, 20);
        window->DrawList->AddRectFilled(bb.Min, bb.Max, colBg, 5.0f); // Rounding 5px
    }

    // 4. GAMBAR IKON MANUAL (VECTOR DRAWING)
    ImVec2 center = ImVec2(bb.Min.x + size.x * 0.5f, bb.Min.y + size.y * 0.5f);
    float scale = size.x * 0.35f; // Ukuran ikon relatif terhadap tombol

    ImDrawList* dl = window->DrawList;
    dl->PathClear();

    switch (iconType) {
        case ICON_REPLAY: {
            // Gambar Panah Melingkar (Replay)
            // Arc (Lingkaran tidak penuh)
            dl->PathArcTo(center, scale, -0.7f, 3.8f, 12); 
            dl->PathStroke(colIcon, 0, 2.0f);
            
            // Kepala Panah
            ImVec2 arrowPos = ImVec2(center.x + scale * 0.7f, center.y - scale * 0.7f);
            dl->AddTriangleFilled(
                ImVec2(arrowPos.x, arrowPos.y - 4), 
                ImVec2(arrowPos.x, arrowPos.y + 6), 
                ImVec2(arrowPos.x + 8, arrowPos.y + 2), 
                colIcon
            );
            break;
        }
        case ICON_PLAY: {
            // Segitiga Play
            float s = scale * 0.8f;
            dl->AddTriangleFilled(
                ImVec2(center.x - s + 2, center.y - s),
                ImVec2(center.x - s + 2, center.y + s),
                ImVec2(center.x + s + 2, center.y),
                colIcon
            );
            break;
        }
        case ICON_PAUSE: {
            // Dua Garis Vertikal
            float w = scale * 0.3f;
            float h = scale * 0.8f;
            dl->AddRectFilled(ImVec2(center.x - w - 2, center.y - h), ImVec2(center.x - 2, center.y + h), colIcon);
            dl->AddRectFilled(ImVec2(center.x + 2, center.y - h), ImVec2(center.x + w + 2, center.y + h), colIcon);
            break;
        }
        case ICON_PREV: {
            // Garis + Segitiga Kiri
            float s = scale * 0.7f;
            dl->AddRectFilled(ImVec2(center.x - s - 2, center.y - s), ImVec2(center.x - s + 1, center.y + s), colIcon); // Bar
            dl->AddTriangleFilled(
                ImVec2(center.x + s, center.y - s),
                ImVec2(center.x + s, center.y + s),
                ImVec2(center.x - s + 3, center.y),
                colIcon
            ); // Arrow
            break;
        }
        case ICON_NEXT: {
            // Segitiga Kanan + Garis
            float s = scale * 0.7f;
            dl->AddTriangleFilled(
                ImVec2(center.x - s, center.y - s),
                ImVec2(center.x - s, center.y + s),
                ImVec2(center.x + s - 3, center.y),
                colIcon
            ); // Arrow
            dl->AddRectFilled(ImVec2(center.x + s - 1, center.y - s), ImVec2(center.x + s + 2, center.y + s), colIcon); // Bar
            break;
        }
    }

    return pressed;
}
// TOOLBAR GAMBAR (DRAWING TOOLS) - Versi Ikon PNG
// TOOLBAR GAMBAR (DRAWING TOOLS) - Versi Bisa Resize (Responsive)
void RenderTopToolbar() { 
    // Flags: NoTitleBar agar bersih, tapi NoResize DIHAPUS agar jendela bisa ditarik manual
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBackground; 

    // Posisi Default — Mobile: mulai di bawah navbar
    float toolbarTopY = g_isMobile ? (g_navbarHeight + 4.0f) : 40.0f;
    ImGui::SetNextWindowPos(ImVec2(40, toolbarTopY), ImGuiCond_FirstUseEver); 
    ImGui::SetNextWindowSize(ImVec2(400, 70), ImGuiCond_FirstUseEver); 

    // Mulai Toolbar
    if (ImGui::Begin("Tools", nullptr, flags)) {
        
        // --- 1. FITUR KLIK KANAN (UNTUK SETTING UKURAN) ---
        // Kalau user klik kanan di area toolbar, muncul menu rahasia
        if (ImGui::BeginPopupContextWindow()) {
            ImGui::Text("⚙️ UI Settings");
            ImGui::Separator();
            
            // Slider untuk mengubah ukuran ikon realtime!
            // Min: 20px (Kecil), Max: 80px (Jumbo buat HP)
          // Min: 20px (Kecil), Max: 80px (Jumbo buat HP)
            if (ImGui::SliderFloat("Icon Size", &g_iconSize, 20.0f, 80.0f, "%.0f px")) {
                // Saat digeser, ukuran berubah visual, tapi belum save ke file
            }

            // 🔥 LOGIKA BARU: Simpan hanya jika user MELEPAS mouse dari slider
            if (ImGui::IsItemDeactivatedAfterEdit()) {
                SaveSettings(); // <--- Panggil fungsi Save kita tadi
            }
            
            if (ImGui::Button("Reset Default")) {
                g_iconSize = 40.0f;
                SaveSettings(); // Simpan juga saat reset
            }
            
            ImGui::EndPopup();
        }

        // --- 2. LOGIKA LAYOUT ---
        ImVec2 winSize = ImGui::GetWindowSize();
        // Deteksi apakah toolbar sedang bentuk memanjang ke bawah (Vertical)
        bool isVertical = winSize.y > winSize.x; 

        // Atur spasi antar tombol
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, isVertical ? ImVec2(0, 10) : ImVec2(5, 5));

        // Helper Layout Next
        auto LayoutNext = [&]() {
            if (!isVertical) ImGui::SameLine();
        };

        // Helper Center Item (Sekarang mengikuti g_iconSize)
        auto CenterItem = [&]() {
            if (isVertical) {
                float availX = ImGui::GetContentRegionAvail().x;
                float off = (availX - g_iconSize) * 0.5f; // Pakai g_iconSize
                if (off > 0.0f) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + off);
            }
        };

        // Ukuran Tombol Dinamis
        ImVec2 btnSize = ImVec2(g_iconSize, g_iconSize);

        // ── Resolve draw manager: tab utama = g_draw, tab lain = per-tab ──
        CDrawingManager& adraw = [&]() -> CDrawingManager& {
            ChartTab* _at = g_chartManager.GetActiveTab();
            if (_at && !_at->usesGlobalData) return g_tabDrawMgrs[_at->id];
            return g_draw;
        }();

        // ================= TOMBOL 1: CURSOR =================
        CenterItem(); 
        bool isCursor = (!adraw.isDrawing && g_uiState.activeTool == TOOL_CURSOR);
        
        // Perhatikan parameter ke-4: btnSize
        if (ToolIconButton("##BtnCursor", texCursor, isCursor, btnSize)) { 
            adraw.StopDrawing();          
            g_uiState.activeTool = TOOL_CURSOR; 
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Cursor");
        
        LayoutNext(); 

        // ================= TOMBOL 2: LINE =================
        CenterItem();
        bool isLine = (adraw.activeTool == DrawShape::LINE && adraw.isDrawing);
        
        if (ToolIconButton("##BtnLine", texLine, isLine, btnSize)) { 
            g_uiState.activeTool = TOOL_LINE; 
            adraw.StartDrawing(DrawShape::LINE); 
            // 🚩 JALUR BARU: Jika ini adalah sentuhan JARI, buat kursor mouse "Amnesia"
            
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Garis Trend");
        LayoutNext();
        // ================= TOMBOL 3: FIBONACCI =================
        CenterItem();
        bool isFib = (adraw.activeTool == DrawShape::FIB && adraw.isDrawing);
        
        if (ToolIconButton("##BtnFib", texFib, isFib, btnSize)) { 
            g_uiState.activeTool = TOOL_FIB; 
            adraw.StartDrawing(DrawShape::FIB); 
       
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("fibonacci");
        LayoutNext();
        // ================= TOMBOL 4: RECTANGLE =================
        CenterItem();
        bool isRect = (adraw.activeTool == DrawShape::RECT && adraw.isDrawing);
        
        if (ToolIconButton("##BtnRect", texRect, isRect, btnSize)) { 
            g_uiState.activeTool = TOOL_RECT; 
            adraw.StartDrawing(DrawShape::RECT); 
        // 🚩 JALUR BARU: Jika ini adalah sentuhan JARI, buat kursor mouse "Amnesia"
      
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Segi Empat");
    LayoutNext();
        // ================= TOMBOL 5: BRUSH (Freehand) =================
        CenterItem();
        // Cek apakah tool BRUSH sedang aktif di engine drawing
        bool isBrush = (adraw.activeTool == DrawShape::BRUSH && adraw.isDrawing);
        
        // Tombol
        if (ToolIconButton("##BtnBrush", texBrush, isBrush, btnSize)) {
             // 1. Update State UI (Opsional, biar rapi aja)
             // g_uiState.activeTool = TOOL_BRUSH; // (Uncomment kalau sudah bikin enumnya)
             
             // 2. 🔥 NYALAKAN MESIN BRUSH
             // Ini akan memanggil brushHandler.Start() nanti saat layar disentuh
             adraw.StartDrawing(DrawShape::BRUSH);
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Freehand Brush");
        LayoutNext();

       // ================= TOMBOL 6: TEXT =================
        CenterItem();
        // Cek apakah tool TEXT sedang aktif (agar tombol menyala/highlight)
        bool isText = (adraw.activeTool == DrawShape::TEXT && adraw.isDrawing);
        
        if (ToolIconButton("##BtnText", texText, isText, btnSize)) {
             // 1. Reset state UI lain jika perlu (agar tidak bentrok)
             g_uiState.activeTool = TOOL_NONE; 
             
             // 2. 🔥 PANGGIL FUNGSI UTAMA
             // Karena di dalam CDrawingManager kita sudah pasang:
             // "if (type == TEXT) textHandler.Start();"
             // Maka baris ini SUDAH CUKUP untuk menyalakan mesin teks baru.
             adraw.StartDrawing(DrawShape::TEXT);
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Insert Text (Click on Chart)");
        LayoutNext();
    // ================= TOMBOL 7: ELLIOT WAVE =================
        CenterItem();
        bool isElliot = (adraw.activeTool == DrawShape::ELLIOT && adraw.isDrawing);

        if (ToolIconButton("##BtnElliot", texElliot, isElliot, btnSize)) {
            g_uiState.activeTool = TOOL_NONE; 
            
            // 🚩 UBAH INI: Panggil fungsi yang ada mesin Elliot-nya
            adraw.StartElliotDrawing(); 
       // 🚩 JALUR BARU: Jika ini adalah sentuhan JARI, buat kursor mouse "Amnesia"
       
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Garis Gelombang");
    LayoutNext();
    // ================= TOMBOL JARVIS AI =================
    CenterItem();
    RenderJarvisToolbarButton();    // ← TAMBAH INI
    LayoutNext();
        // ================= SEPARATOR =================
        if (isVertical) {
            ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
        } else {
            ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
            ImGui::SameLine();
        }


        // ================= TOMBOL 8: TRASH =================
        CenterItem();
        if (ToolIconButton("##BtnTrash", texTrash, false, btnSize)) { 
            g_shapes.Clear(); 
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Hapus Semua");

        ImGui::PopStyleVar();
    }
    ImGui::End();
}
// PANEL KHUSUS REPLAY & BACKTEST (Masa Depan)
// ==================================================================================
// 🎬 REPLAY SETUP POPUP — Pilih Symbol|TF|Tanggal sebelum replay mulai
// ==================================================================================
struct ReplaySetupState {
    bool  open        = false;
    int   selSym      = 0;
    int   selTF       = 0;
    int   startYear=2024, startMon=1,  startDay=1,  startHour=0,  startMin=0;
    int   endYear  =2024, endMon  =12, endDay  =31, endHour  =23, endMin  =59;
    bool  dataChecked = false;
    bool  dataFound   = false;
    int   startIndex  = -1;
    int   endIndex    = -1;
    int   totalCandles= 0;
    std::string infoMsg;
    // Layer tambahan yang akan dibuka saat replay dimulai
    bool  extraTFs[7] = {false,false,false,false,false,false,false}; // M1,M5,M15,M30,H1,H4,D1
} g_replaySetup;

static double ReplaySetupToUnix(int y,int mo,int d,int h,int mi) {
    struct tm t{}; t.tm_year=y-1900; t.tm_mon=mo-1; t.tm_mday=d;
    t.tm_hour=h; t.tm_min=mi; t.tm_sec=0; t.tm_isdst=-1;
    return (double)mktime(&t);
}

static void ReplaySetupCheckData() {
    auto& st = g_replaySetup;
    st.dataChecked=true; st.dataFound=false;
    st.startIndex=-1; st.endIndex=-1; st.totalCandles=0; st.infoMsg="";
    double tS=ReplaySetupToUnix(st.startYear,st.startMon,st.startDay,st.startHour,st.startMin);
    double tE=ReplaySetupToUnix(st.endYear,st.endMon,st.endDay,st.endHour,st.endMin);
    if (tE<=tS) { st.infoMsg="Tanggal akhir harus lebih dari tanggal mulai!"; return; }
    std::lock_guard<std::mutex> lk(g_candlesMutex);
    if (!g_allCandles.count("M1")||g_allCandles["M1"].empty()) {
        st.infoMsg="Data M1 belum dimuat. Pastikan koneksi aktif."; return;
    }
    auto& m1=g_allCandles["M1"];
    int si=-1,ei=-1;
    for (int i=0;i<(int)m1.size();i++) {
        if (m1[i].time>=tS && si==-1) si=i;
        if (m1[i].time<=tE) ei=i;
    }
    if (si==-1||ei==-1||si>=ei) {
        st.infoMsg="Tidak ada data di rentang ini. Coba rentang lain."; return;
    }
    st.startIndex=si; st.endIndex=ei; st.totalCandles=ei-si+1; st.dataFound=true;
    char buf[128]; snprintf(buf,sizeof(buf),"Data tersedia: %d candle M1",st.totalCandles);
    st.infoMsg=buf;
}

static void RenderReplaySetupPopup(bool& replayMode, bool& replayStarted) {
    auto& st=g_replaySetup;
    if (!st.open) return;
    ImGui::OpenPopup("##ReplaySetup");
    ImVec2 center=ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center,ImGuiCond_Appearing,ImVec2(0.5f,0.5f));
    ImGui::SetNextWindowSize(ImVec2(360,0),ImGuiCond_Appearing);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,10.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(16,14));
    ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.10f,0.11f,0.16f,0.97f));
    ImGui::PushStyleColor(ImGuiCol_Border,  ImVec4(0.25f,0.28f,0.40f,1.f));
    ImGui::PushStyleColor(ImGuiCol_Text,    ImVec4(0.88f,0.88f,0.92f,1.f));
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.14f,0.15f,0.22f,1.f));
    ImGui::PushStyleColor(ImGuiCol_Header,        ImVec4(0.20f,0.40f,0.80f,0.45f));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.25f,0.48f,0.90f,0.55f));

    static const char* _syms[]={"XAUUSD","EURUSD","GBPUSD","BTCUSDT","ETHUSDT"};
    static const char* _tfs[] ={"M1","M5","M15","M30","H1","H4","D1"};

    if (ImGui::BeginPopupModal("##ReplaySetup",&st.open,
        ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_AlwaysAutoResize)) {

        ImGui::TextColored(ImVec4(0.4f,0.8f,1.f,1.f),"SETUP REPLAY");
        ImGui::SameLine(ImGui::GetContentRegionAvail().x-18.f);
        ImGui::PushStyleColor(ImGuiCol_Button,ImVec4(0,0,0,0));
        if (ImGui::SmallButton(u8"✕")) { st.open=false; ImGui::CloseCurrentPopup(); }
        ImGui::PopStyleColor();
        ImGui::Separator(); ImGui::Spacing();

        float colW=80.f;
        ImGui::Text("Symbol"); ImGui::SameLine(colW);
        ImGui::SetNextItemWidth(120);
        if (ImGui::BeginCombo("##rSym",_syms[st.selSym])) {
            for (int i=0;i<5;i++) {
                bool sel=(st.selSym==i);
                if (ImGui::Selectable(_syms[i],sel)) { st.selSym=i; st.dataChecked=false; }
                if (sel) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine(colW+132); ImGui::Text("TF"); ImGui::SameLine(colW+150);
        ImGui::SetNextItemWidth(72);
        if (ImGui::BeginCombo("##rTF",_tfs[st.selTF])) {
            for (int i=0;i<7;i++) {
                bool sel=(st.selTF==i);
                if (ImGui::Selectable(_tfs[i],sel)) { st.selTF=i; st.dataChecked=false; }
                if (sel) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

        auto DateRow=[&](const char* lbl,int& y,int& mo,int& d,int& h,int& mi){
            ImGui::Text("%s",lbl); ImGui::SameLine(colW); ImGui::PushID(lbl);
            bool changed=false;
            ImGui::SetNextItemWidth(52); changed|=ImGui::InputInt("##Y",&y,0); y=std::max(2000,std::min(y,2030));
            ImGui::SameLine(0,4); ImGui::TextDisabled("-"); ImGui::SameLine(0,4);
            ImGui::SetNextItemWidth(36); changed|=ImGui::InputInt("##Mo",&mo,0); mo=std::clamp(mo,1,12);
            ImGui::SameLine(0,4); ImGui::TextDisabled("-"); ImGui::SameLine(0,4);
            ImGui::SetNextItemWidth(36); changed|=ImGui::InputInt("##D",&d,0); d=std::clamp(d,1,31);
            ImGui::SameLine(0,8); ImGui::TextDisabled("|"); ImGui::SameLine(0,8);
            ImGui::SetNextItemWidth(36); changed|=ImGui::InputInt("##H",&h,0); h=std::clamp(h,0,23);
            ImGui::SameLine(0,4); ImGui::TextDisabled(":"); ImGui::SameLine(0,4);
            ImGui::SetNextItemWidth(36); changed|=ImGui::InputInt("##Mi",&mi,0); mi=std::clamp(mi,0,59);
            ImGui::PopID();
            if (changed) st.dataChecked=false;
        };
        DateRow("Mulai",st.startYear,st.startMon,st.startDay,st.startHour,st.startMin);
        ImGui::Spacing();
        DateRow("Sampai",st.endYear,st.endMon,st.endDay,st.endHour,st.endMin);
        ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

        // ── Deposit / Balance Awal untuk Replay ──────────────────────────────
        ImGui::TextColored(ImVec4(0.6f,0.7f,1.f,1.f), "Deposit:");
        ImGui::SameLine();
        ImGui::TextDisabled("(balance awal saat replay dimulai)");
        ImGui::Spacing();
        {
            ImGui::PushID("ReplayDeposit");
            float depFloat = (float)TradePanelUI::replayDeposit;

            // Input deposit
            ImGui::Text("  $"); ImGui::SameLine(30);
            ImGui::SetNextItemWidth(140);
            if (ImGui::InputFloat("##replayDeposit", &depFloat, 100.0f, 1000.0f, "%.2f")) {
                if (depFloat > 0.0f) TradePanelUI::replayDeposit = (double)depFloat;
            }

            // Quick presets
            ImGui::SameLine(0, 12);
            ImGui::TextDisabled("Quick:");
            ImGui::SameLine(0, 6);

            double presets[] = { 1000, 5000, 10000, 25000, 50000, 100000 };
            const char* presetLabels[] = { "$1K", "$5K", "$10K", "$25K", "$50K", "$100K" };
            for (int i = 0; i < 6; i++) {
                bool isSel = (fabs(TradePanelUI::replayDeposit - presets[i]) < 0.01);
                if (isSel) {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.5f, 0.8f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.55f, 0.85f, 1.0f));
                } else {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.16f, 0.22f, 0.9f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.28f, 0.4f, 0.95f));
                }
                if (ImGui::SmallButton(presetLabels[i])) {
                    TradePanelUI::replayDeposit = presets[i];
                }
                ImGui::PopStyleColor(2);
                if (i < 5) ImGui::SameLine(0, 3);
            }
            ImGui::PopID();
        }
        ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

        // ── Tambah Layer Chart ───────────────────────────────────────────────
        ImGui::TextColored(ImVec4(0.6f,0.7f,1.f,1.f), "Tambah Layer Chart:");
        ImGui::SameLine();
        ImGui::TextDisabled("(opsional, langsung muncul saat replay jalan)");
        ImGui::Spacing();
        {
            static const char* _tfs2[]={"M1","M5","M15","M30","H1","H4","D1"};
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(12,4));
            for (int i=0;i<7;i++) {
                // Skip TF yang sama dengan TF utama (sudah ada di chart utama)
                bool isMainTF = (i == st.selTF);
                if (isMainTF) {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f,0.4f,0.4f,0.6f));
                    ImGui::PushStyleColor(ImGuiCol_CheckMark, ImVec4(0.4f,0.4f,0.4f,0.6f));
                    bool dummy = false;
                    ImGui::Checkbox(_tfs2[i], &dummy);
                    ImGui::PopStyleColor(2);
                } else {
                    ImGui::Checkbox(_tfs2[i], &st.extraTFs[i]);
                }
                if (i < 6) ImGui::SameLine();
            }
            ImGui::PopStyleVar();
        }
        ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

        ImGui::PushStyleColor(ImGuiCol_Button,ImVec4(0.15f,0.22f,0.38f,1.f));
        if (ImGui::Button("Cek Data",ImVec2(100,0))) ReplaySetupCheckData();
        ImGui::PopStyleColor();
        if (st.dataChecked) {
            ImGui::SameLine(0,10);
            if (st.dataFound)
                ImGui::TextColored(ImVec4(0.3f,0.9f,0.4f,1.f),u8"✓ %s",st.infoMsg.c_str());
            else
                ImGui::TextColored(ImVec4(1.f,0.4f,0.3f,1.f),u8"✗ %s",st.infoMsg.c_str());
        }
        ImGui::Spacing();

        bool canRun=st.dataFound && st.startIndex>=0;
        if (!canRun) ImGui::PushStyleVar(ImGuiStyleVar_Alpha,0.35f);
        ImGui::PushStyleColor(ImGuiCol_Button,       ImVec4(0.18f,0.50f,0.25f,1.f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,ImVec4(0.22f,0.62f,0.30f,1.f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.15f,0.40f,0.20f,1.f));

        if (ImGui::Button(u8"▶  Run Replay",ImVec2(-1,34)) && canRun) {
            // ── 1. Update symbol & TF global ──────────────────────────────
            g_symbol   = _syms[st.selSym];
            g_activeTF = _tfs[st.selTF];
            ChartTab* mainTab=nullptr;
            for (auto* _t:g_chartManager.tabs)
                if (_t->usesGlobalData) { mainTab=_t; break; }
            if (mainTab) { mainTab->symbol=g_symbol; mainTab->timeframe=g_activeTF; mainTab->UpdateLabel(); }

            // ── 2. Reinit replay engine ────────────────────────────────────
            ReinitReplayForSymbol();

            // ── 3. KRITIS: sync g_tfIndices semua TF ke timestamp startIndex
            //    Tanpa ini limitIndex renderer = 0 → chart blank + Y scale gila
            {
                std::lock_guard<std::mutex> lk(g_candlesMutex);
                auto& m1=g_allCandles["M1"];
                if (st.startIndex<(int)m1.size()) {
                    double tStart=m1[st.startIndex].time;
                    g_tfIndices["M1"]=st.startIndex;
                    for (auto& kv:g_allCandles) {
                        if (kv.first=="M1") continue;
                        auto& vec=kv.second;
                        if (vec.empty()) continue;
                        int idx=(int)vec.size()-1; // default: akhir
                        for (int ii=0;ii<(int)vec.size();ii++) {
                            if (vec[ii].time>=tStart) { idx=ii; break; }
                        }
                        g_tfIndices[kv.first]=idx;
                    }
                }
            }

            // ── 4. Set cutoff & posisi replay ─────────────────────────────
            int tfIdx=g_tfIndices[g_activeTF];
            g_replayCutoff.active    =true;
            g_replayCutoff.lineIndex =(double)tfIdx;
            g_replay.SetIndex(st.startIndex);
            g_replay.Pause();

            // ── 5. Sync view ke posisi start ──────────────────────────────
            g_chart.viewCenterIndex=tfIdx;
            g_chart.autoFitY=true;
            if (mainTab) { mainTab->state.viewCenterIndex=tfIdx; mainTab->state.autoFitY=true; }

            // ── 6. Aktifkan UI replay ─────────────────────────────────────
            replayMode    = true;
            g_replayMode  = true;  // sync global mirror
            replayStarted = false;  // false dulu, Play di floating bar = true

            // ── 7. KRITIS: set g_replayActive + g_replayCutoffTime ────────
            // → extra chart tampil data full sampai hari ini, bukan dipotong
            g_replayActive = true;
            g_replay.active = true;

            // ── 7b. RESET replay manager (bersihkan trade & history replay lama) ──
            {
                g_replayManager.trades.clear();
                g_replayManager.balance = TradePanelUI::replayDeposit; // deposit dari user
                g_replayManager.equity  = TradePanelUI::replayDeposit;
                g_replayManager.nextId  = 1;
                if (g_replayManager.history) {
                    g_replayManager.history->closedTrades.clear();
                }
                printf("[REPLAY] Replay trade manager reset (balance=$%.2f).\n",
                       TradePanelUI::replayDeposit);
            }
            {
                std::lock_guard<std::mutex> lk2(g_candlesMutex);
                auto& m1b = g_allCandles["M1"];
                if (st.startIndex < (int)m1b.size())
                    g_replayCutoffTime = m1b[st.startIndex].time;
            }

            // ── 7. Spawn extra chart layers yang dipilih user ─────────────
            // ── 8. Hapus extra chart lama + spawn ChartTab GPU baru ─────
            // Hapus dulu semua tab extra replay sebelumnya
            for (int ri = (int)g_chartManager.tabs.size()-1; ri >= 0; ri--) {
                if (g_chartManager.tabs[ri]->isReplayExtraTab)
                    g_chartManager.RemoveTab(g_chartManager.tabs[ri]->id);
            }
            {
                static const char* _tfs3[]={"M1","M5","M15","M30","H1","H4","D1"};
                for (int i=0;i<7;i++) {
                    if (!st.extraTFs[i]) continue;
                    std::string eTF = _tfs3[i];
                    if (eTF == g_activeTF) continue; // skip TF utama
                    // Spawn sebagai ChartTab GPU — sync otomatis via g_tfIndices
                    ChartTab* et = g_chartManager.AddTab(g_symbol, eTF, /*usesGlobal=*/false);
                    if (et) {
                        et->isReplayExtraTab = true;
                        et->InitOrderFlow(); // symbol → tickSize untuk orderFlowRenderer
                        // Set viewCenter ke posisi replay start
                        if (g_tfIndices.count(eTF))
                            et->state.viewCenterIndex = g_tfIndices[eTF];
                        et->state.autoFitY = true;
                    }
                }
            }

            st.open=false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::PopStyleColor(3);
        if (!canRun) ImGui::PopStyleVar();
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Button,ImVec4(0.18f,0.18f,0.24f,1.f));
        if (ImGui::Button("Batal",ImVec2(-1,0))) { st.open=false; ImGui::CloseCurrentPopup(); }
        ImGui::PopStyleColor();
        ImGui::EndPopup();
    }
    ImGui::PopStyleColor(6);
    ImGui::PopStyleVar(2);
}

void RenderReplayPanel(bool& replayMode) {
    // Nama Window: "ReplayUI" (Bisa digeser terpisah dari Tools)
    if (ImGui::Begin("Replay", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize)) {
        
        // ============================================================
        // 🔄 TOMBOL REPLAY (MENGGUNAKAN GAMBAR PNG CUSTOM)
        // ============================================================
        
        // Atur warna background & tint agar tombol menyala saat aktif
        ImVec4 bgCol = replayMode ? ImVec4(0.1f, 0.4f, 0.8f, 0.6f) : ImVec4(0, 0, 0, 0);
        ImVec4 tintCol = replayMode ? ImVec4(1.0f, 1.0f, 1.0f, 1.0f) : ImVec4(0.8f, 0.8f, 0.8f, 0.9f); // Sedikit diputihkan agar ikon terlihat jelas

        ImGui::PushStyleColor(ImGuiCol_Button, bgCol);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);

        bool isClicked = false;
        if (texReplayBtn != 0) {
            // 🔥 UPDATE DI SINI:
            // Mengubah ukuran menjadi kotak 36x36 agar ikon tidak gepeng
            // karena gambar replay.png kamu bentuknya rasio 1:1 (persegi)
            isClicked = ImGui::ImageButton("##BtnReplay", texReplayBtn, ImVec2(34, 34), ImVec2(0,0), ImVec2(1,1), ImVec4(0,0,0,0), tintCol);
            
            // Opsional: Jika ingin menambahkan tooltip saat mouse di atas ikon
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Mode Replay");
            }
        } else {
            // Fallback teks jika gambar gagal/belum di-load
            isClicked = ImGui::Button("REPLAY", ImVec2(80, 25));
        }

        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
        if (isClicked) {
            if (!replayMode) {
                // Buka popup setup dulu
                g_replaySetup.open        = true;
                g_replaySetup.dataChecked = false;
                g_replaySetup.dataFound   = false;
                // Gate ON: live feed masuk IDB saja, tidak inject WASM
                g_replayGateActive = true;
                printf("[REPLAY] Gate ON — live feed diparkir ke IDB\n");
                ChartTab* _at=g_chartManager.GetActiveTab();
                if (_at) {
                    static const char* _s[]={"XAUUSD","EURUSD","GBPUSD","BTCUSD","ETHUSD"};
                    static const char* _t[]={"M1","M5","M15","M30","H1","H4","D1"};
                    for(int i=0;i<5;i++) if(strcmp(_s[i],_at->symbol.c_str())==0) g_replaySetup.selSym=i;
                    for(int i=0;i<7;i++) if(strcmp(_t[i],_at->timeframe.c_str())==0) g_replaySetup.selTF=i;
                }
            } else {
                // --- KELUAR DARI MODE REPLAY (KEMBALI KE LIVE) ---
                
                // 1. Reset State UI Replay
                g_replayCutoff.active = false;
                g_replayCutoff.showConfirmation = false;
                replayStarted = false;
                g_replay.Pause(); // Pastikan engine replay berhenti
                g_replay.active = false; // Matikan flag internal engine
                
                // 2. 🔥 KUNCI UTAMA: Sync Pointer & Index ke Data Live Terakhir 🔥
                { 
                    std::lock_guard<std::mutex> lock(g_candlesMutex); 
                    
                    // Kembalikan pointer pembacaan ke live candles
                    g_replaySourceTF = &g_allCandles[g_activeTF];
                    g_replayIndexPtr = &g_tfIndices[g_activeTF];

                    // Set index grafik ke candle paling ujung (Live)
                    if(g_allCandles.count(g_activeTF) && !g_allCandles[g_activeTF].empty()) {
                        int lastIdx = (int)g_allCandles[g_activeTF].size() - 1;
                        g_tfIndices[g_activeTF] = lastIdx;
                    }
                }

                // 3. 🔥 RESET HARGA LIVE UNTUK UI 🔥
                // Ini agar Floating Panel & Indikator sadar harga sudah berubah
                if (g_liveTick.price > 0.00001) {
                    currentPrice = g_liveTick.price; // Pakai harga tick live terakhir
                } else {
                    // Fallback kalau belum ada tick baru, ambil close candle terakhir
                     std::lock_guard<std::mutex> lock(g_candlesMutex); 
                     if(!g_allCandles[g_activeTF].empty())
                        currentPrice = g_allCandles[g_activeTF].back().close;
                }

                // 4. TRIGGER ANIMASI LONCAT KE LIVE (Efek tombol >>)
                // Ini akan memaksa chart geser otomatis ke kanan (masa kini)
                isAnimatingToLive = true; 
                animFloatingIndex = (double)g_chart.viewCenterIndex; // Mulai animasi dari posisi saat ini
                g_chart.autoFitY = true; // Reset skala Y agar harga live terlihat
                // 🎯 Trigger per-tab untuk semua primary tabs
                GoToLive::TriggerAllPrimary(g_chartManager,
                                            &g_chart.viewCenterIndex,
                                            &g_chart.autoFitY);
                
                printf("🔄 [SYSTEM] Replay OFF -> Forced Sync to Live Price: %.5f\n", currentPrice);
                replayMode     = false;
                g_replayMode   = false; // sync global mirror
                g_replayActive = false;       // sync ke extra chart
                g_replayCutoffTime = 0.0;     // extra chart kembali tampil live

                // 5. Hapus semua isReplayExtraTab
                for (int ri = (int)g_chartManager.tabs.size()-1; ri >= 0; ri--) {
                    if (g_chartManager.tabs[ri]->isReplayExtraTab)
                        g_chartManager.RemoveTab(g_chartManager.tabs[ri]->id);
                }

                // 6. 🔥 RELOAD DARI IDB
                // Gate tetap ON sampai JS selesai push — tidak ada race condition
                // JS: clear WASM → push IDB → rebuild HTF → wasm_set_replay_mode(0)
                g_replayGateActive = true;
                #ifdef __EMSCRIPTEN__
                emscripten_run_script("if(window.reloadLiveAfterReplay) window.reloadLiveAfterReplay();");
                #endif
            }
        }
        // Tooltip
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Replay Mode");
     
    }
    ImGui::End();
}
#include "UI_ReplayFloatingBar.h"
// -----------------------------------------------------------------
// 🔥 FUNGSI BARU: POPUP SETTINGS (YANG SUDAH JINAK)
// -----------------------------------------------------------------
void RenderIndicatorSettingsPopup() {
    // 1. Cek apakah ada REQUEST untuk buka pop-up? (Hanya dijalankan sekali saat diklik)
    if (g_uiState.requestOpenSettings) {
        ImGui::OpenPopup("IndicatorSettingsPopup"); // Buka pop-up dengan ID tetap
        g_uiState.requestOpenSettings = false;      // Matikan request (biar gak maksa buka terus)
    }

    // 2. Render Pop-up jika sedang terbuka
    //    (Kalau user klik luar, ImGui otomatis bikin fungsi ini return false)
    if (ImGui::BeginPopup("IndicatorSettingsPopup", ImGuiWindowFlags_AlwaysAutoResize)) {

        // Safety Check: Pastikan index valid
        int idx = g_uiState.openIndicatorSettingsIndex;
        if (idx >= 0 && idx < g_activeIndicators.size()) {

            Indicator* ind = g_activeIndicators[idx];

            std::lock_guard<std::mutex> lock(g_candlesMutex);
            auto& candles = g_allCandles[g_activeTF];

            // Panggil render settings, dapatkan status remove
            bool should_remove = ind->RenderSettings(candles);

            // Logika Hapus
            if (should_remove) {
                delete g_activeIndicators[idx];
                g_activeIndicators.erase(g_activeIndicators.begin() + idx);
                g_uiState.openIndicatorSettingsIndex = -1;
                ImGui::CloseCurrentPopup(); // Tutup pop-up setelah hapus
            }
        } else {
            // Kalau index kacau, tutup aja
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}


// ==================================================================================
// 🔥 MODULAR ACTIVE INDICATORS OVERLAY — 3 STATE DESIGN
//   State 1 : Polos  — hanya symbol|TF|harga, tidak ada indikator
//   State 2 : Collapsed — ada indikator, hanya tombol ▼ kecil
//   State 3 : Expanded  — panel list transparan + tombol ▲ pojok kanan bawah
// ==================================================================================
void RenderActiveIndicatorsOverlay(ChartTab* tab, std::vector<Indicator*>& activeInds) {
    if (!tab) return;

    // ── Extern globals ──────────────────────────────────────────────────────────
    extern ImVec4        g_colorText;
    extern double        currentPrice;
    extern std::string   g_symbol;
    extern std::string   g_activeTF;
    extern ImTextureID   texEyeShow;
    extern ImTextureID   texEyeHide;
    extern ImTextureID   texIndSettings;
    extern ImTextureID   texTrash2;

    // ── Konstanta layout ────────────────────────────────────────────────────────
    const float PAD        =  8.0f;
    const float ROW_H      = 26.0f;
    const float ICON_SZ    = 16.0f;
    const float ICON_GAP   =  4.0f;
    const float PANEL_W    = 246.0f;
    const float DOT_R      =  4.0f;
    const float BTN_W      = 30.0f;
    const float BTN_H      = 20.0f;

    ImDrawList* dl      = ImGui::GetWindowDrawList();
    ImVec2      plotPos = ImPlot::GetPlotPos();
    ImVec2      plotSz  = ImPlot::GetPlotSize();

    // ── Style push global ───────────────────────────────────────────────────────
    ImGui::PushStyleVar  (ImGuiStyleVar_FramePadding, ImVec2(3, 2));
    ImGui::PushStyleVar  (ImGuiStyleVar_ItemSpacing,  ImVec2(4, 2));
    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1,1,1,0.10f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(1,1,1,0.18f));

    // ════════════════════════════════════════════════════════════════════════════
    // GROUP 1 — HEADER: "XAUUSD | M15 | 2000.00"  satu baris, font normal
    // ════════════════════════════════════════════════════════════════════════════
    const char* symText = tab->usesGlobalData ? g_symbol.c_str()   : tab->symbol.c_str();
    const char* tfText  = tab->usesGlobalData ? g_activeTF.c_str() : tab->timeframe.c_str();

    ImVec2 hdrPos = ImVec2(plotPos.x + PAD, plotPos.y + PAD);

    // ambil harga dulu
    double livePx = 0, refPx = 0; bool priceOK = false;
    {
        std::lock_guard<std::mutex> lk(g_candlesMutex);
        std::string key = tab->usesGlobalData
            ? g_activeTF
            : (tab->symbol + "_" + tab->timeframe);
        auto& vec = g_allCandles[key];
        if (!vec.empty()) { livePx = vec.back().close; refPx = vec.back().open; priceOK = true; }
        // 🔥 FIX: Semua tab pakai MarketWatch sebagai live price (update setiap tick)
        // MarketWatch.UpdateTick dipanggil untuk SEMUA symbol di wasm_push_tick
        // Lebih reliable dari currentPrice (hanya update untuk g_symbol saja)
        // dan dari vec.back().close (stale sampai candle close berikutnya)
        if (!tab->isReplayExtraTab) {
            extern MarketWatchPanel g_marketWatch;
            // Fallback ke g_symbol kalau tab->symbol belum di-set (primary tab awal)
            std::string lkSym = (!tab->symbol.empty()) ? tab->symbol
                               : (tab->usesGlobalData ? g_symbol : "");
            if (!lkSym.empty()) {
                double mwPx = g_marketWatch.GetLivePrice(lkSym);
                if (mwPx > 1e-5) livePx = mwPx;
            }
        }
        if ((tab->usesGlobalData || tab->isReplayExtraTab) && g_replay.active && g_replay.currentState.price > 1e-5)
            livePx = g_replay.currentState.price;
    }

    // Render satu baris: Symbol(putih) " | "(dim) TF(abu) " | "(dim) Harga(hijau/merah)
    {
        char buf[256];
        float curX = hdrPos.x;
        float curY = hdrPos.y;

        // Symbol — putih terang
        snprintf(buf, sizeof(buf), "%s", symText);
        ImVec2 szSym = ImGui::CalcTextSize(buf);
        dl->AddText(ImVec2(curX, curY), IM_COL32(218, 218, 232, 255), buf);
        curX += szSym.x;

        // separator " | "
        const char* sep = " | ";
        ImVec2 szSep = ImGui::CalcTextSize(sep);
        dl->AddText(ImVec2(curX, curY), IM_COL32(70, 75, 95, 200), sep);
        curX += szSep.x;

        // TF — abu-abu
        snprintf(buf, sizeof(buf), "%s", tfText);
        ImVec2 szTF = ImGui::CalcTextSize(buf);
        dl->AddText(ImVec2(curX, curY), IM_COL32(218, 218, 232, 255), buf);
        curX += szTF.x;

        // separator " | "
        dl->AddText(ImVec2(curX, curY), IM_COL32(70, 75, 95, 200), sep);
        curX += szSep.x;

        // Harga — hijau/merah
        if (priceOK) {
            snprintf(buf, sizeof(buf), (livePx > 500 ? "%.2f" : "%.5f"), livePx);
            ImU32 pxCol = (livePx >= refPx) ? IM_COL32(50,215,100,255) : IM_COL32(255,85,85,255);
            dl->AddText(ImVec2(curX, curY), pxCol, buf);
        }
    }

    // ════════════════════════════════════════════════════════════════════════════
    // Hitung indikator + OF overlay items
    // ════════════════════════════════════════════════════════════════════════════
    int nInds = 0;
    for (auto* ind : activeInds) if (ind) nInds++;

    // Semua OF items dikelola dari UI_IndicatorList.h
    //   CountOrderFlowItems() = VP + FP style + Naked VPOC yang aktif
    int nOFItems = CountOrderFlowItems(tab);
    int nTotal   = nInds + nOFItems;

    // Settings panel OF (VP / FP) — delegasi ke UI_IndicatorList.h
    // Dipanggil setiap frame; internal cek s_vpSettingsOpen / s_fpSettingsOpen
    RenderOrderFlowSettingsPanels(tab);

    // STATE 1 — tidak ada indikator DAN tidak ada OF items → selesai
    if (nTotal == 0) {
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar(2);
        return;
    }

    // ════════════════════════════════════════════════════════════════════════════
    // STATE 2 & 3 — ada indikator
    // ════════════════════════════════════════════════════════════════════════════
    static std::map<int, bool> s_expanded; // false = collapsed (State 2)
    bool& expanded = s_expanded[tab->id];

    // Posisi Y tepat di bawah header text
    float hdrH      = ImGui::GetTextLineHeight();
    float panelTopY = plotPos.y + PAD + hdrH + 4.0f; // 4px gap setelah header

    // ── Posisi tombol ▼/▲ ───────────────────────────────────────────────────
    if (!expanded) {
        // ── STATE 2: hanya tombol ▼ kecil ────────────────────────────────────
        ImVec2 btnPos = ImVec2(plotPos.x + PAD, panelTopY);

        // bg tombol tipis
        dl->AddRectFilled(
            btnPos,
            ImVec2(btnPos.x + BTN_W, btnPos.y + BTN_H),
            IM_COL32(20, 22, 35, 160), 3.0f);
        dl->AddRect(
            btnPos,
            ImVec2(btnPos.x + BTN_W, btnPos.y + BTN_H),
            IM_COL32(60, 65, 88, 180), 3.0f, 0, 1.0f);

        // chevron ▼
        float mx = btnPos.x + BTN_W * 0.5f;
        float my = btnPos.y + BTN_H * 0.5f;
        dl->AddTriangleFilled(
            ImVec2(mx - 5, my - 3), ImVec2(mx + 5, my - 3), ImVec2(mx, my + 3),
            IM_COL32(160, 168, 200, 220));

        // invisible button untuk klik
        ImGui::SetCursorScreenPos(btnPos);
        ImGui::PushID(("##ind_expand_" + std::to_string(tab->id)).c_str());
        if (ImGui::InvisibleButton("##expand", ImVec2(BTN_W, BTN_H)))
            expanded = true;
        ImGui::PopID();

    } else {
        // ── STATE 3: panel expanded ───────────────────────────────────────────
        float panelH  = nTotal * ROW_H + 4.0f;
        ImVec2 panMin = ImVec2(plotPos.x + PAD, panelTopY);
        // clamp agar panel tidak melebihi batas kanan plot
        float  actualW = std::min(PANEL_W, plotPos.x + plotSz.x - PAD - panMin.x);
        ImVec2 panMax  = ImVec2(panMin.x + actualW, panMin.y + panelH);

        // BG hitam semi-transparan (subtle, tidak mencolok)
        dl->AddRectFilled(panMin, panMax, IM_COL32(0, 0, 0, 140), 4.0f);
        dl->AddRect      (panMin, panMax, IM_COL32(50, 55, 75, 120), 4.0f, 0, 1.0f);

        // ── Render tiap baris indikator ──────────────────────────────────────
        float iconGroupW = ICON_SZ * 3 + ICON_GAP * 2;
        float curY = panMin.y + 2.0f;

        for (int i = 0; i < (int)activeInds.size(); i++) {
            Indicator* ind = activeInds[i];
            if (!ind) continue;

            ImGui::PushID(i + tab->id * 1000);

            float rowY = curY;

            // Dot warna indikator
            ImVec4 dotColF = ind->visible ? ind->color : ImVec4(0.22f, 0.22f, 0.28f, 1.f);
            ImU32  dotCol  = ImGui::GetColorU32(dotColF);
            dl->AddCircleFilled(
                ImVec2(panMin.x + PAD + DOT_R, rowY + ROW_H * 0.5f),
                DOT_R, dotCol);

            // Nama indikator
            ImVec4 nameCol = ind->visible ? ind->color : ImVec4(0.32f, 0.32f, 0.38f, 1.f);
            ImGui::SetCursorScreenPos(ImVec2(panMin.x + PAD + DOT_R*2 + 6.f, rowY + (ROW_H - ImGui::GetTextLineHeight()) * 0.5f));
            ImGui::TextColored(nameCol, "%s", ind->name.c_str());

            // Icon group: Eye | Gear | Trash (rata kanan)
            float iconStartX = panMax.x - PAD - iconGroupW;
            float iconY      = rowY + (ROW_H - ICON_SZ) * 0.5f;

            // ── Eye ──
            ImTextureID eyeTex  = ind->visible ? texEyeShow : texEyeHide;
            ImVec4      eyeTint = ind->visible ? ImVec4(0.7f,0.8f,1.0f,0.9f)
                                               : ImVec4(0.4f,0.4f,0.5f,0.6f);
            ImGui::SetCursorScreenPos(ImVec2(iconStartX, iconY));
            if (ImGui::ImageButton("##eye", eyeTex,
                    ImVec2(ICON_SZ,ICON_SZ), ImVec2(0,0), ImVec2(1,1),
                    ImVec4(0,0,0,0), eyeTint))
                ind->visible = !ind->visible;

            // ── Gear ──
            ImGui::SameLine(0, ICON_GAP);
            if (ImGui::ImageButton("##gear", texIndSettings,
                    ImVec2(ICON_SZ,ICON_SZ), ImVec2(0,0), ImVec2(1,1),
                    ImVec4(0,0,0,0), ImVec4(0.65f,0.68f,0.80f,0.85f))) {
                g_uiState.openIndicatorSettingsIndex = i;
                g_uiState.requestOpenSettings = true;
            }

            // ── Trash ──
            ImGui::SameLine(0, ICON_GAP);
            if (ImGui::ImageButton("##del", texTrash2,
                    ImVec2(ICON_SZ,ICON_SZ), ImVec2(0,0), ImVec2(1,1),
                    ImVec4(0,0,0,0), ImVec4(0.80f,0.28f,0.28f,0.85f))) {
                delete activeInds[i];
                activeInds.erase(activeInds.begin() + i);
                i--; nInds--;
                ImGui::PopID();
                continue;
            }

            // separator tipis antar baris
            if (i < (int)activeInds.size() - 1)
                dl->AddLine(
                    ImVec2(panMin.x + 6, rowY + ROW_H),
                    ImVec2(panMax.x - 6, rowY + ROW_H),
                    IM_COL32(40, 42, 60, 100));

            ImGui::PopID();
            curY += ROW_H;
        }

        // ════════════════════════════════════════════════════════════════════
        // BARIS OF OVERLAY — VP, FP Style, Naked VPOC
        // Semua didelegasi ke UI_IndicatorList.h :: RenderOrderFlowIndicatorRows()
        // Gear button → ToggleVPSettings() / ToggleFPSettings() (spesifik)
        // ════════════════════════════════════════════════════════════════════
        curY = RenderOrderFlowIndicatorRows(
            tab, dl, panMin, panMax,
            curY,
            PAD, ROW_H, DOT_R,
            ICON_SZ, ICON_GAP,
            nInds > 0,
            texEyeShow, texEyeHide, texIndSettings, texTrash2);

        // ── Tombol ▲ pojok KANAN BAWAH panel ────────────────────────────────
        ImVec2 btnPos = ImVec2(panMax.x - BTN_W - 2.0f, panMax.y + 2.0f);

        dl->AddRectFilled(
            btnPos,
            ImVec2(btnPos.x + BTN_W, btnPos.y + BTN_H),
            IM_COL32(20, 22, 35, 160), 3.0f);
        dl->AddRect(
            btnPos,
            ImVec2(btnPos.x + BTN_W, btnPos.y + BTN_H),
            IM_COL32(60, 65, 88, 180), 3.0f, 0, 1.0f);

        // chevron ▲
        float mx = btnPos.x + BTN_W * 0.5f;
        float my = btnPos.y + BTN_H * 0.5f;
        dl->AddTriangleFilled(
            ImVec2(mx - 5, my + 3), ImVec2(mx + 5, my + 3), ImVec2(mx, my - 3),
            IM_COL32(160, 168, 200, 220));

        ImGui::SetCursorScreenPos(btnPos);
        ImGui::PushID(("##ind_collapse_" + std::to_string(tab->id)).c_str());
        if (ImGui::InvisibleButton("##collapse", ImVec2(BTN_W, BTN_H)))
            expanded = false;
        ImGui::PopID();
    }

    // ── Cleanup ──────────────────────────────────────────────────────────────
    ImGui::SetWindowFontScale(1.0f);
    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar(2);
}
// -----------------------------------------------------------------
// PANEL NAVIGASI — Style "HIDDEN Button" konsisten
// Background transparan gelap, border tipis, teks putih bold
// Semua warna pakai g_colorText / g_colorHeader → auto ikut theme
// Window bisa digeser (dockable) seperti Market Watch
// -----------------------------------------------------------------
// -----------------------------------------------------------------
// PANEL NAVIGASI — Option A: Segmented Control Navigation
// TradingView-style dengan slider background animasi
// Row 1: [Symbol ▾ | TF ▾ | Candle ▾ | Indicator | + New Chart]
// Row 2: [Chart] [Order Book] [Replay] [Drawing]  ← quick chips
// -----------------------------------------------------------------
void RenderNavigationPanel(bool& replayMode) {

    static float iconSize = 40.0f;

    extern ImVec4 g_colorBg;
    extern ImVec4 g_colorPanel;
    extern ImVec4 g_colorText;
    extern ImVec4 g_colorHeader;
    extern double currentPrice;
    extern bool   isAnimatingToLive;
    extern double animFloatingIndex;

    ImGuiIO& io = ImGui::GetIO();
    float barH = iconSize + 16.0f;

    // Deteksi orientasi dari window ImGui sendiri (sama seperti main6)
    // TIDAK pakai g_isMobile — biarkan ImGui auto-menyesuaikan
    ImVec2 winSzCheck = io.DisplaySize;
    bool isMobile = false; // deteksi mobile di nav dimatikan — window mengalir bebas

    g_navbarHeight = 0.0f; // chart tidak perlu offset, nav mengapung

    // ── Window flags ─────────────────────────────────────────────
    ImGuiWindowFlags window_flags =
        ImGuiWindowFlags_NoTitleBar  |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoBackground;

    ImGui::SetNextWindowSize(ImVec2(60, 400), ImGuiCond_FirstUseEver);

    ImVec4 toolbarBg = g_colorPanel;
    toolbarBg.w = 0.97f;
    ImGui::PushStyleColor(ImGuiCol_WindowBg, toolbarBg);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 6.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,   ImVec2(8.0f, 4.0f));

    if (!ImGui::Begin("Navigasi", nullptr, window_flags)) {
        ImGui::End();
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor();
        return;
    }

    // ── SameLine helper sesuai orientasi window ──────────────────
    // Window bebas di-resize user — tombol selalu SameLine (horizontal)

    // ══════════════════════════════════════════════════════════════
    //  SEGMENTED CONTROL — ROW 1
    //  [Symbol ▾ | TF ▾ | Candle ▾ | Indicator | + New Chart]
    // ══════════════════════════════════════════════════════════════

    // ── Constants ────────────────────────────────────────────────
    const float BTN_H    = iconSize;
    const float SEG_PAD  = 4.0f;
    const float RADIUS   = 10.0f;
    const float SLIDER_R = 7.0f;
    const float ICO_SZ   = BTN_H * 0.55f;
    const float ICO_GAP  = 6.0f;
    const float TXT_PAD  = 10.0f;     // padding kiri-kanan tombol (pas isi)
    const float SEG_GAP  = 0.0f;     // jarak antar segment (0 = rapat)

    // ── Static: slider animation + segment position tracking ─────
    //  1-frame delay: container + slider digambar dari posisi frame
    //  sebelumnya agar selalu di-BELAKANG teks tombol (draw order).
    static float s_segX[5] = {};
    static float s_segW[5] = {};
    static float s_segY = 0.0f, s_segH = BTN_H;
    static float s_sliderX = 0.0f, s_sliderW = 100.0f;
    static int   s_activeSeg = 0;
    static bool  s_inited = false;

    // ── Colors ───────────────────────────────────────────────────
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec4 txtCol4 = g_colorText;
    ImU32 txtW     = IM_COL32(255, 255, 255, 235);
    ImU32 txtD     = IM_COL32(155, 155, 170, 155);

    // Container: gelap, rounded
    ImU32 cBg      = IM_COL32(22, 22, 31, 255);
    ImU32 cBdr     = IM_COL32(34, 34, 51, 200);
    // Slider: biru gelap + glow border
    ImU32 slBg     = IM_COL32(30, 58, 95, 200);
    ImU32 slBdr    = IM_COL32(59, 130, 246, 55);
    // Chip active: teal
    ImU32 chipAct  = IM_COL32(0, 212, 170, 220);

    // Glass look for popups
    ImVec4 glassLook = g_colorPanel;
    glassLook.w = 0.88f;

    // ══════════════════════════════════════════════════════════════
    //  STEP 1: DRAW CONTAINER BG + SLIDER (dari frame sebelumnya)
    //  Digambar duluan → tombol teks di atasnya
    // ══════════════════════════════════════════════════════════════
    if (s_inited) {
        ImVec2 cMin = ImVec2(s_segX[0] - SEG_PAD, s_segY - SEG_PAD);
        ImVec2 cMax = ImVec2(s_segX[4] + s_segW[4] + SEG_PAD, s_segY + s_segH + SEG_PAD);

        // Container background (gelap)
        dl->AddRectFilled(cMin, cMax, cBg, RADIUS);

        // Slider highlight — lerp smooth ke posisi active segment
        int tgt = s_activeSeg;
        s_sliderX += (s_segX[tgt] - s_sliderX) * 0.18f;
        s_sliderW += (s_segW[tgt] - s_sliderW) * 0.18f;

        dl->AddRectFilled(
            ImVec2(s_sliderX - 1, s_segY - 1),
            ImVec2(s_sliderX + s_sliderW + 1, s_segY + s_segH + 1),
            slBg, SLIDER_R);
        dl->AddRect(
            ImVec2(s_sliderX - 1, s_segY - 1),
            ImVec2(s_sliderX + s_sliderW + 1, s_segY + s_segH + 1),
            slBdr, SLIDER_R, 0, 1.0f);
    }

    // ══════════════════════════════════════════════════════════════
    //  STEP 2: PUSH TRANSPARENT STYLES (segmented = tanpa border)
    // ══════════════════════════════════════════════════════════════
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1,1,1,0.05f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(1,1,1,0.10f));
    ImGui::PushStyleColor(ImGuiCol_Border,        ImVec4(0,0,0,0));

    // ══════════════════════════════════════════════════════════════
    //  SEGMENT 0: SYMBOL — icon PNG + displayName + ▾
    // ══════════════════════════════════════════════════════════════
    {
        const ChartTab* _tt = g_chartManager.GetActiveTab();
        const std::string& curSym = (_tt && !_tt->symbol.empty())
                                    ? _tt->symbol : g_symbol;

        const SymbolInfo* si = SymbolRegistry_Find(curSym);
        const char* dispName = si ? si->displayName.c_str()
                                  : (!curSym.empty() ? curSym.c_str() : "---");
        ImTextureID symIcon  = si ? si->icon : 0;

        const float ICO = BTN_H * 0.62f;
        const float GAP = 6.f;
        ImVec2 ts = ImGui::CalcTextSize(dispName);
        ImVec2 ar = ImGui::CalcTextSize(" \xE2\x96\xBE");  // ▾
        float bw = ICO + GAP + ts.x + ar.x + TXT_PAD * 1.5f;
        bw = ImMax(bw, 80.0f);

        ImVec2 bp = ImGui::GetCursorScreenPos();
        float cy = bp.y + BTN_H * 0.5f;

        ImGui::InvisibleButton("##segSym", ImVec2(bw, BTN_H));
        bool hov = ImGui::IsItemHovered();

        // Track posisi untuk container + slider (frame depan)
        s_segX[0] = bp.x; s_segW[0] = bw;
        s_segY = bp.y; s_segH = BTN_H;
        s_inited = true;

        // Gambar icon + teks manual (TANPA bg/border — container handle)
        float contentW = ICO + GAP + ts.x + ar.x;
        float startX   = bp.x + (bw - contentW) * 0.5f;
        float iconY    = cy - ICO * 0.5f;

        if (symIcon) {
            dl->AddCircleFilled(
                ImVec2(startX + ICO * 0.5f, cy), ICO * 0.5f + 1.f,
                IM_COL32(25, 28, 42, 190), 24);
            dl->AddImage(symIcon,
                ImVec2(startX,       iconY),
                ImVec2(startX + ICO, iconY + ICO));
        } else {
            ImU32 cc = IM_COL32(40, 80, 200, 210);
            if (si) {
                switch (si->category) {
                    case SymbolCategory::COMMODITY: cc = IM_COL32(150,110,15,220); break;
                    case SymbolCategory::CRYPTO:    cc = IM_COL32(170, 80,10,220); break;
                    case SymbolCategory::FOREX:     cc = IM_COL32( 20, 80,200,220);break;
                    default: break;
                }
            }
            dl->AddCircleFilled(ImVec2(startX + ICO*0.5f, cy), ICO*0.5f, cc, 24);
            char ini[3] = { curSym.size()>0?curSym[0]:'?',
                            curSym.size()>1?curSym[1]:' ', 0 };
            ImVec2 its = ImGui::CalcTextSize(ini);
            dl->AddText(ImVec2(startX+ICO*0.5f-its.x*0.5f, cy-its.y*0.5f),
                        IM_COL32(255,255,255,230), ini);
        }

        dl->AddText(ImVec2(startX + ICO + GAP, cy - ts.y * 0.5f), txtW, dispName);
        dl->AddText(ImVec2(startX + ICO + GAP + ts.x, cy - ar.y * 0.5f), txtD,
                    " \xE2\x96\xBE");  // ▾

        if (ImGui::IsItemClicked()) {
            s_activeSeg = 0;
            ChartTab* cur2 = g_chartManager.GetActiveTab();
            const char* curTF = (cur2 && !cur2->timeframe.empty())
                                ? cur2->timeframe.c_str() : "M15";
            SymbolPicker::Open("navbar", curTF);
        }
        if (hov)
            ImGui::SetTooltip("Ganti Simbol  |  Aktif: %s", dispName);
    }

    // ── SymbolPicker navbar callback ─────────────────────────────
    SymbolPicker::Render("navbar", [](const std::string& sym, const std::string& tf) {
        ChartTab* activeTab = g_chartManager.GetActiveTab();
        if (!activeTab) return;

        bool symChanged = (activeTab->symbol    != sym);
        bool tfChanged  = (activeTab->timeframe != tf);
        if (!symChanged && !tfChanged) return;

        activeTab->symbol    = sym;
        activeTab->timeframe = tf;
        activeTab->label     = sym + " | " + tf;

        if (activeTab->usesGlobalData) {
            g_symbol   = sym;
            g_activeTF = tf;
            if (symChanged) {
                { std::lock_guard<std::mutex> lock(g_candlesMutex);
                  static const char* pk[] = {"M1","M5","M15","M30","H1","H4","D1","W1","MN"};
                  for (auto* k : pk) { g_allCandles.erase(k); g_tfIndices.erase(k); }
                  g_activeIndicators.clear(); }
                activeTab->state.viewCenterIndex = -1;
                activeTab->state.autoFitY        = true;
                activeTab->state.y_min           = 0.0;
                activeTab->state.y_max           = 0.0;
                activeTab->gpuRenderer.ClearInstances();
                #ifdef __EMSCRIPTEN__
                emscripten_run_script(("SetActiveSymbol('" + sym + "');").c_str());
                #endif
            }
            printf("[NAV] %s -> %s | %s\n",
                   symChanged ? "Symbol+TF" : "TF", sym.c_str(), tf.c_str());
        } else {
            activeTab->state.viewCenterIndex = -1;
            activeTab->state.autoFitY        = true;
            activeTab->lazyPending           = false;
            activeTab->noMoreHistory         = false;
            if (symChanged) {
                #ifdef __EMSCRIPTEN__
                std::string cmd = "LoadTabSymbol(" + std::to_string(activeTab->id)
                                + ",'" + sym + "');";
                emscripten_run_script(cmd.c_str());
                #endif
            }
            printf("[NAV] Tab[%d] %s %s\n",
                   activeTab->id, sym.c_str(), tf.c_str());
        }
    });

    // ══════════════════════════════════════════════════════════════
    //  SEGMENT 1: TIMEFRAME — text + ▾
    // ══════════════════════════════════════════════════════════════
    ImGui::SameLine(0, 0);
    {
        ChartTab*   tipTab = g_chartManager.GetActiveTab();
        const char* tfShow = tipTab ? tipTab->timeframe.c_str() : g_activeTF.c_str();
        if (!tfShow || tfShow[0] == '\0') tfShow = "---";

        ImVec2 ar = ImGui::CalcTextSize(" \xE2\x96\xBE");  // ▾
        float  bw  = ImGui::CalcTextSize(tfShow).x + ar.x + TXT_PAD * 2.0f;
        bw = ImMax(bw, 56.0f);

        ImVec2 bp = ImGui::GetCursorScreenPos();
        float cy = bp.y + BTN_H * 0.5f;

        ImGui::InvisibleButton("##segTF", ImVec2(bw, BTN_H));
        bool hov = ImGui::IsItemHovered();

        s_segX[1] = bp.x; s_segW[1] = bw;

        ImVec2 ts = ImGui::CalcTextSize(tfShow);
        dl->AddText(
            ImVec2(bp.x + (bw - ts.x - ar.x) * 0.5f, cy - ts.y * 0.5f), txtW, tfShow);
        dl->AddText(
            ImVec2(bp.x + (bw + ts.x - ar.x) * 0.5f, cy - ar.y * 0.5f), txtD,
            " \xE2\x96\xBE");

        if (ImGui::IsItemClicked()) { s_activeSeg = 1; ImGui::OpenPopup("PopupTF"); }
        if (hov) ImGui::SetTooltip("Ganti Timeframe");
    }

    // ── Popup TF — LOGIKA TIDAK DIUBAH ──────────────────────────
    ImGui::PushStyleColor(ImGuiCol_PopupBg, glassLook);
    ImGui::PushStyleColor(ImGuiCol_Border,  g_colorHeader);
    if (ImGui::BeginPopup("PopupTF")) {
        static const char* tfs[] = {"M1","M5","M15","M30","H1","H4"};
        ChartTab*          activeTab = g_chartManager.GetActiveTab();
        const std::string& _rawTF    = activeTab ? activeTab->timeframe : g_activeTF;
        const std::string  curTF     = _rawTF.empty() ? "---" : _rawTF;
        for (auto& tf : tfs) {
            ImGui::PushStyleColor(ImGuiCol_Text, g_colorText);
            if (ImGui::Selectable(tf, curTF == tf)) {
                if (curTF != tf && activeTab) {
                    double anchorTime = 0.0;
                    {
                        std::lock_guard<std::mutex> lock(g_candlesMutex);
                        std::string anchorKey = activeTab->usesGlobalData
                            ? activeTab->timeframe
                            : (activeTab->symbol + "_" + activeTab->timeframe);
                        if (g_allCandles.count(anchorKey) && !g_allCandles[anchorKey].empty()) {
                            int vcIdx = activeTab->usesGlobalData
                                ? g_chart.viewCenterIndex
                                : activeTab->state.viewCenterIndex;
                            int idx = std::clamp(vcIdx, 0, (int)g_allCandles[anchorKey].size()-1);
                            anchorTime = g_allCandles[anchorKey][idx].time;
                        }
                    }
                    activeTab->timeframe = tf;
                    activeTab->label     = activeTab->symbol + " | " + std::string(tf);
                    if (activeTab->usesGlobalData) {
                        g_activeTF = tf;
                        { std::lock_guard<std::mutex> lock(g_candlesMutex);
                          g_replaySourceTF = &g_allCandles[g_activeTF];
                          g_replayIndexPtr = &g_tfIndices[g_activeTF];
                          RecalculateAllIndicators(g_allCandles[g_activeTF]);
                          for (auto* ind : g_activeIndicators)
                              ind->Calculate(g_allCandles[g_activeTF]); }
                        needAutoMargin = false;
                        { std::lock_guard<std::mutex> lock(g_candlesMutex);
                          if (!g_allCandles[g_activeTF].empty()) {
                              auto& vec = g_allCandles[g_activeTF];
                              auto it = std::lower_bound(vec.begin(), vec.end(), anchorTime,
                                  [](const Candle& c, double t){ return c.time < t; });
                              g_chart.viewCenterIndex = (it != vec.end())
                                  ? (int)std::distance(vec.begin(), it)
                                  : (int)vec.size() - 1;
                          } }
                        printf("[TF] Tab Utama -> %s\n", tf);
                    } else {
                        activeTab->state.autoFitY = true;
                        if (anchorTime > 0) {
                            std::string newKey = activeTab->symbol + "_" + std::string(tf);
                            std::lock_guard<std::mutex> lock(g_candlesMutex);
                            if (g_allCandles.count(newKey) && !g_allCandles[newKey].empty()) {
                                auto& vec = g_allCandles[newKey];
                                auto it = std::lower_bound(vec.begin(), vec.end(), anchorTime,
                                    [](const Candle& c, double t){ return c.time < t; });
                                activeTab->state.viewCenterIndex = (it != vec.end())
                                    ? (int)std::distance(vec.begin(), it)
                                    : (int)vec.size() - 1;
                            } else {
                                activeTab->state.viewCenterIndex = -1;
                            }
                        } else {
                            activeTab->state.viewCenterIndex = -1;
                        }
                        printf("[TF] Tab[%d] %s %s -> %s\n",
                            activeTab->id, activeTab->symbol.c_str(), curTF.c_str(), tf);
                    }
                }
            }
            ImGui::PopStyleColor();
        }
        ImGui::EndPopup();
    }
    ImGui::PopStyleColor(2);

    // ══════════════════════════════════════════════════════════════
    //  SEGMENT 2: CANDLE STYLE (delegasi ke CandleStyleManager)
    //  segmentedMode=true → skip individual bg/border
    // ══════════════════════════════════════════════════════════════
    g_candleStyleMgr.RenderNavigationUI(true, BTN_H, true /*segmented*/);
    {
        ImVec2 rMin = ImGui::GetItemRectMin();
        ImVec2 rMax = ImGui::GetItemRectMax();
        s_segX[2] = rMin.x;
        s_segW[2] = rMax.x - rMin.x;
        if (ImGui::IsItemClicked()) s_activeSeg = 2;
    }

    // ══════════════════════════════════════════════════════════════
    //  SEGMENT 3: INDICATOR — icon + text
    // ══════════════════════════════════════════════════════════════
    ImGui::SameLine(0, 0);
    {
        const char* lbl = "Indicator";
        float  bw  = TXT_PAD + ICO_SZ + ICO_GAP + ImGui::CalcTextSize(lbl).x + TXT_PAD;
        ImVec2 bp  = ImGui::GetCursorScreenPos();
        float  cy  = bp.y + BTN_H * 0.5f;

        ImGui::InvisibleButton("##segInd", ImVec2(bw, BTN_H));
        bool hov = ImGui::IsItemHovered();

        s_segX[3] = bp.x; s_segW[3] = bw;

        dl->AddImage(
            (ImTextureID)(intptr_t)texIconIndicator,
            ImVec2(bp.x + TXT_PAD,             cy - ICO_SZ * 0.5f),
            ImVec2(bp.x + TXT_PAD + ICO_SZ,     cy + ICO_SZ * 0.5f),
            ImVec2(0,0), ImVec2(1,1),
            ImGui::ColorConvertFloat4ToU32(txtCol4));

        float fh = ImGui::GetTextLineHeight();
        dl->AddText(
            ImVec2(bp.x + TXT_PAD + ICO_SZ + ICO_GAP, cy - fh * 0.5f),
            txtW, lbl);

        if (ImGui::IsItemClicked()) { s_activeSeg = 3; OpenIndicatorModal(); }
        if (hov) ImGui::SetTooltip("Tambah Indikator");
    }

    // ══════════════════════════════════════════════════════════════
    //  SEGMENT 4: NEW CHART — icon + text + dropdown
    // ══════════════════════════════════════════════════════════════
    ImGui::SameLine(0, 0);
    {
        const char* lbl = "New Tab";
        float  bw  = TXT_PAD + ICO_SZ + ICO_GAP + ImGui::CalcTextSize(lbl).x + TXT_PAD;
        ImVec2 bp  = ImGui::GetCursorScreenPos();
        float  cy  = bp.y + BTN_H * 0.5f;

        ImGui::InvisibleButton("##segNewChart", ImVec2(bw, BTN_H));
        bool hov = ImGui::IsItemHovered();

        s_segX[4] = bp.x; s_segW[4] = bw;

        dl->AddImage(
            (ImTextureID)(intptr_t)texAddChart,
            ImVec2(bp.x + TXT_PAD,             cy - ICO_SZ * 0.5f),
            ImVec2(bp.x + TXT_PAD + ICO_SZ,     cy + ICO_SZ * 0.5f),
            ImVec2(0,0), ImVec2(1,1),
            ImGui::ColorConvertFloat4ToU32(txtCol4));

        float fh = ImGui::GetTextLineHeight();
        dl->AddText(
            ImVec2(bp.x + TXT_PAD + ICO_SZ + ICO_GAP, cy - fh * 0.5f),
            txtW, lbl);

        if (ImGui::IsItemClicked()) {
            s_activeSeg = 4;
            ImGui::OpenPopup("##newTabMenu");
        }
        if (hov) ImGui::SetTooltip("Buka Chart atau Order Book Baru");

        // ── Dropdown: pilih tipe tab baru ────────────────────────
        ImGui::SetNextWindowPos(ImVec2(bp.x, bp.y + BTN_H + 2), ImGuiCond_Always);
        ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.07f,0.08f,0.14f,0.97f));
        ImGui::PushStyleColor(ImGuiCol_Border,  ImVec4(0.2f,0.3f,0.5f,0.8f));
        if (ImGui::BeginPopup("##newTabMenu")) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.88f,0.88f,0.92f,1.0f));
            ImGui::TextDisabled("BUKA TAB BARU");
            ImGui::Separator();
            ImGui::Spacing();

            if (ImGui::Selectable("  New Chart", false, 0, ImVec2(200,30))) {
                s_showAddChartModal = true;
                ImGui::CloseCurrentPopup();
            }
            ImGui::TextDisabled("     Pilih symbol + timeframe");

            ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

            if (ImGui::Selectable("  New Order Book", false, 0, ImVec2(200,30))) {
                GetShowOBTabPicker() = true;
                ImGui::CloseCurrentPopup();
            }
            ImGui::TextDisabled("     L2 Order Book real-time");

            ImGui::Spacing();
            ImGui::PopStyleColor();
            ImGui::EndPopup();
        }
        ImGui::PopStyleColor(2);
    }

    // ══════════════════════════════════════════════════════════════
    //  SEGMENT 5: SETTINGS — dipindah ke Row 1, setelah New Chart
    // ══════════════════════════════════════════════════════════════
    ImGui::SameLine(0, 8);
    {
        const char* lbl = "Settings";
        ImVec2 ts  = ImGui::CalcTextSize(lbl);
        float  cw  = TXT_PAD + ICO_SZ + ICO_GAP + ts.x + TXT_PAD;
        ImVec2 bp  = ImGui::GetCursorScreenPos();
        float  cy  = bp.y + BTN_H * 0.5f;

        ImGui::InvisibleButton("##segSettings", ImVec2(cw, BTN_H));
        bool hov      = ImGui::IsItemHovered();
        bool isSetOpen = g_displaySettingsUI.showPopup;

        ImU32 sTxt = isSetOpen ? chipAct : (hov ? txtW : txtD);

        if (texPopupSetting) {
            dl->AddImage(
                (ImTextureID)(intptr_t)texPopupSetting,
                ImVec2(bp.x + TXT_PAD,           cy - ICO_SZ * 0.5f),
                ImVec2(bp.x + TXT_PAD + ICO_SZ,   cy + ICO_SZ * 0.5f),
                ImVec2(0,0), ImVec2(1,1),
                isSetOpen ? IM_COL32(0, 212, 170, 255) : sTxt);
        }

        float fh = ImGui::GetTextLineHeight();
        dl->AddText(
            ImVec2(bp.x + TXT_PAD + ICO_SZ + ICO_GAP, cy - fh * 0.5f),
            sTxt, lbl);

        if (ImGui::IsItemClicked()) g_displaySettingsUI.Toggle();
        if (hov) ImGui::SetTooltip("Pengaturan Tampilan");
    }

    // ══════════════════════════════════════════════════════════════
    //  STEP 3: POP segment styles
    // ══════════════════════════════════════════════════════════════
    ImGui::PopStyleColor(4); // Button, ButtonHovered, ButtonActive, Border
    ImGui::PopStyleVar(2);   // ItemSpacing, FrameBorderSize

    // ══════════════════════════════════════════════════════════════
    //  STEP 4: DRAW CONTAINER BORDER (setelah tombol, di atas)
    //  Menutupi border CandleStyleManager agar rapi
    // ══════════════════════════════════════════════════════════════
    if (s_inited) {
        ImVec2 cMin = ImVec2(s_segX[0] - SEG_PAD, s_segY - SEG_PAD);
        ImVec2 cMax = ImVec2(s_segX[4] + s_segW[4] + SEG_PAD, s_segY + s_segH + SEG_PAD);
        dl->AddRect(cMin, cMax, cBdr, RADIUS, 0, 1.0f);
    }

    // ── Klik kanan: resize tombol (semua platform) ───────────────
    ImGui::PushStyleColor(ImGuiCol_PopupBg, glassLook);
    ImGui::PushStyleColor(ImGuiCol_Border,  g_colorHeader);
    if (ImGui::BeginPopupContextWindow()) {
        ImGui::PushStyleColor(ImGuiCol_Text, g_colorText);
        ImGui::Text("Ukuran Tombol:");
        ImGui::SliderFloat("##sz", &iconSize, 24.0f, 72.0f, "%.0f px");
        ImGui::PopStyleColor();
        ImGui::EndPopup();
    }
    ImGui::PopStyleColor(2);

    ImGui::End();
    ImGui::PopStyleVar(2);  // WindowPadding, ItemSpacing
    ImGui::PopStyleColor(); // WindowBg
}

// ============================================================
// BAGIAN: HELPER — DETEKSI DIGIT DESIMAL DARI SYMBOL
// XAU=2, BTC=2, EUR=5, JPY=3, Default=5
// ============================================================
static int GetDecimalDigits(const std::string& sym) {
    if (sym.find("XAU") != std::string::npos ||
        sym.find("XAG") != std::string::npos ||
        sym.find("BTC") != std::string::npos ||
        sym.find("ETH") != std::string::npos ||
        sym.find("LTC") != std::string::npos ||
        sym.find("BNB") != std::string::npos) {
        return 2;
    }
    if (sym.find("JPY") != std::string::npos) return 3;
    return 5;
}

static void SplitPrice(double price, const std::string& sym,
                       std::string& sSmall, std::string& sBig) {
    int dec = GetDecimalDigits(sym);
    char buf[32];
    snprintf(buf, sizeof(buf), "%.*f", dec, price);
    std::string full = buf;
    if (full.size() >= 2) {
        sBig   = full.substr(full.size() - 2);
        sSmall = full.substr(0, full.size() - 2);
    } else {
        sBig   = full;
        sSmall = "";
    }
}

// ============================================================
// HELPER: Render Harga di dalam tombol BUY / SELL
// Format otomatis berdasarkan symbol (XAU 2 digit, EUR 5 digit, dll)
// ============================================================
static void DrawPriceOnButton(
    ImDrawList*        dl,
    float              btnLeft,
    float              btnTop,
    float              btnW,
    float              btnH,
    double             price,
    const std::string& sym,
    ImVec4             priceColor,
    float              sc,
    const char*        label
) {
    ImFont* font  = ImGui::GetFont();
    float fBase   = ImGui::GetFontSize();
    float fBig    = fBase * (1.30f + 0.12f * sc);

    float cx = btnLeft + btnW * 0.5f;

    // --- Label "BUY" / "SELL" di atas tombol ---
    ImVec2 lsz = ImGui::CalcTextSize(label);
    dl->AddText(ImVec2(cx - lsz.x * 0.5f, btnTop + btnH * 0.10f),
                IM_COL32_WHITE, label);

    // --- Split harga (symbol-aware) ---
    std::string sBig, sSmall;
    SplitPrice(price, sym, sSmall, sBig);

    // --- Hitung lebar total agar bisa di-center ---
    float wSmall = sSmall.empty() ? 0.0f
                 : ImGui::CalcTextSize(sSmall.c_str()).x;
    float wBig   = font->CalcTextSizeA(fBig, FLT_MAX, 0.0f, sBig.c_str()).x;
    float totalW = wSmall + wBig;
    float startX = cx - totalW * 0.5f;

    // Posisi Y: tengah-bawah tombol (sekitar 45% dari atas)
    float baselineY = btnTop + btnH * 0.43f;

    ImU32 col = IM_COL32(
        (int)(priceColor.x * 255),
        (int)(priceColor.y * 255),
        (int)(priceColor.z * 255),
        255
    );

    // Gambar sSmall (ukuran normal, baseline sama)
    if (!sSmall.empty()) {
        // Turunkan sedikit agar baseline sejajar dengan sBig
        float smallY = baselineY + (fBig - fBase);
        dl->AddText(ImVec2(startX, smallY), col, sSmall.c_str());
    }

    // Gambar sBig (ukuran besar/bold)
    dl->AddText(font, fBig, ImVec2(startX + wSmall, baselineY), col, sBig.c_str());
}


// ============================================================
// FUNGSI UTAMA — RenderFloatingTradePanel
// ============================================================
void RenderFloatingTradePanel() {

    // --- Setup Window ---
    ImGui::SetNextWindowBgAlpha(0.85f);
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration
                           | ImGuiWindowFlags_AlwaysAutoResize
                           | ImGuiWindowFlags_NoDocking
                           | ImGuiWindowFlags_NoScrollbar
                           | ImGuiWindowFlags_NoScrollWithMouse;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6.0f, 6.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,   ImVec2(4.0f, 4.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,  ImVec2(4.0f, 2.0f));

    double currentPrice = GetMasterPrice();
    double currentTime  = GetMasterTime();
    // Helper: index candle aktif saat ini (untuk openCandleIdx di zone rendering)
    auto GetCurrentCandleIndex = [&]() -> double {
        auto& c = g_allCandles[g_activeTF];
        if (c.empty()) return 0.0;
        // LIVE MODE: candle aktif = index terakhir
        if (!g_replay.active)
            return (double)(c.size() - 1);
        // REPLAY MODE: cari index candle di active TF yang
        // waktunya <= waktu replay saat ini (binary search)
        double replayTime = GetMasterTime();
        auto it = std::upper_bound(c.begin(), c.end(), replayTime,
            [](double t, const Candle& cd) { return t < cd.time; });
        if (it == c.begin()) return 0.0;
        --it; // candle terakhir yang time-nya <= replayTime
        return (double)(it - c.begin());
    };

    // ── TAB-AWARE TRADE: gunakan symbol + harga dari tab yang aktif ──────────
    // Analoginya: kasir harus cetak struk dengan nama toko yang user sedang
    // buka sekarang — bukan selalu nama toko utama.
    ChartTab* _tradeTab = g_chartManager.GetActiveTab();
    std::string tradeSymbol = (_tradeTab && !_tradeTab->symbol.empty())
                              ? _tradeTab->symbol : g_symbol;

    // Override harga ke MarketWatch per-symbol kalau ini non-primary tab
    if (_tradeTab && !_tradeTab->usesGlobalData) {
        double mwPx = g_marketWatch.GetLivePrice(_tradeTab->symbol);
        if (mwPx > 1e-5) {
            currentPrice = mwPx;
        } else {
            // Fallback ke close candle terakhir di key SYMBOL_TF tab ini
            std::string _key = _tradeTab->symbol + "_" + _tradeTab->timeframe;
            if (g_allCandles.count(_key) && !g_allCandles[_key].empty())
                currentPrice = g_allCandles[_key].back().close;
        }
        // Override currentTime juga — pakai waktu candle terakhir tab ini
        // Supaya openTime akurat untuk cross-TF zone rendering
        std::string _keyTime = _tradeTab->symbol + "_" + _tradeTab->timeframe;
        if (g_allCandles.count(_keyTime) && !g_allCandles[_keyTime].empty())
            currentTime = g_allCandles[_keyTime].back().time;
    }

    // Tab-aware candle index: pakai data tab non-primary kalau bukan tab utama
    auto getTabCandleIndex = [&]() -> double {
        if (!_tradeTab || _tradeTab->usesGlobalData) return GetCurrentCandleIndex();
        std::string _key = _tradeTab->symbol + "_" + _tradeTab->timeframe;
        auto& c = g_allCandles[_key];
        if (c.empty()) return 0.0;
        if (!g_replay.active) return (double)(c.size() - 1);
        double replayTime = GetMasterTime();
        auto it = std::upper_bound(c.begin(), c.end(), replayTime,
            [](double t, const Candle& cd) { return t < cd.time; });
        if (it == c.begin()) return 0.0;
        --it;
        return (double)(it - c.begin());
    };
    // ────────────────────────────────────────────────────────────────────────

    // --- Warna Kedip: bandingkan tick baru vs frame sebelumnya ---
    static double s_lastDisplayPrice = 0.0;
    static ImVec4 priceColor = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
    static float  colorFade  = 0.0f;

    if (currentPrice > s_lastDisplayPrice + 0.000001) {
        priceColor = ImVec4(0.2f, 1.0f, 0.4f, 1.0f); // Hijau naik
        colorFade  = 1.0f;
    } else if (currentPrice < s_lastDisplayPrice - 0.000001) {
        priceColor = ImVec4(1.0f, 0.4f, 0.4f, 1.0f); // Merah turun
        colorFade  = 1.0f;
    }
    s_lastDisplayPrice = currentPrice;

    if (colorFade > 0.0f) {
        colorFade -= 0.008f;
        if (colorFade <= 0.0f) {
            colorFade  = 0.0f;
            priceColor = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
        }
    }

    if (currentPrice <= 0.00001) { ImGui::PopStyleVar(3); return; }

    if (ImGui::Begin("TradePanelFloating", nullptr, flags)) {

        // ============================================================
        // SETTINGS: Klik Kanan di mana saja dalam window
        // ============================================================
        if (ImGui::BeginPopupContextWindow("##TPSettings")) {
            ImGui::TextColored(ImVec4(0.9f,0.7f,0.2f,1.0f), "Trade Panel Settings");
            ImGui::Separator();

            ImGui::Text("Size Scale:");
            bool changed = ImGui::SliderFloat("##scale", &g_tradePanelScale,
                                              0.8f, 2.0f, "%.1fx");
            if (changed && ImGui::IsItemDeactivatedAfterEdit()) SaveSettings();

            ImGui::Separator();
            ImGui::Text("Button Colors:");

            if (ImGui::ColorEdit3("Buy##col",  (float*)&g_colBuy,
                ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_PickerHueBar)) {}
            if (ImGui::IsItemDeactivatedAfterEdit()) SaveSettings();

            if (ImGui::ColorEdit3("Sell##col", (float*)&g_colSell,
                ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_PickerHueBar)) {}
            if (ImGui::IsItemDeactivatedAfterEdit()) SaveSettings();

            ImGui::Separator();
            if (ImGui::Button("Reset Defaults", ImVec2(-1, 0))) {
                g_tradePanelScale = 1.0f;
                g_colBuy  = ImVec4(0.0f, 0.6f, 0.2f, 1.0f);
                g_colSell = ImVec4(0.8f, 0.1f, 0.1f, 1.0f);
                SaveSettings();
            }

            ImGui::EndPopup();
        }

        // ============================================================
        // A. HEADER TOGGLE
        // ============================================================
        static bool isPanelExpanded = true;

        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0,0,0,0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1,1,1,0.1f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(1,1,1,0.2f));
        if (ImGui::ArrowButton("##MainToggle",
                isPanelExpanded ? ImGuiDir_Down : ImGuiDir_Right))
            isPanelExpanded = !isPanelExpanded;
        ImGui::PopStyleColor(3);

        ImGui::SameLine();
        ImGui::AlignTextToFramePadding();
        if (isPanelExpanded) {
            // Tampil symbol aktif di header agar user tahu ini trade untuk symbol apa
            ImGui::TextColored(ImVec4(0.9f,0.9f,0.9f,1.0f), "TRADE  ");
            ImGui::SameLine(0, 0);
            ImGui::TextColored(ImVec4(0.3f, 0.9f, 1.0f, 1.0f), "%s", tradeSymbol.c_str());
        } else {
            ImGui::TextColored(ImVec4(0.9f,0.9f,0.9f,1.0f), "HIDDEN");
        }

        // ============================================================
        // B. AREA BUY | LOT | SELL
        // ============================================================
        if (isPanelExpanded)
        {
            ImGui::Separator();

            float sc    = g_tradePanelScale;
            float btnW  = 80.0f * sc;
            float btnH  = 54.0f * sc;
            float lotW  = 44.0f * sc;
            float inputH = 18.0f;
            float arrowH = ImMax((btnH - inputH) * 0.5f, 10.0f);
            ImVec2 btnSize(btnW, btnH);

            // ---- BUY ----
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
            ImGui::PushStyleColor(ImGuiCol_Button,
                g_colBuy);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                ImVec4(g_colBuy.x*1.15f, g_colBuy.y*1.15f, g_colBuy.z*1.15f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                ImVec4(g_colBuy.x*0.8f,  g_colBuy.y*0.8f,  g_colBuy.z*0.8f,  1.0f));

            ImVec2 buyPos = ImGui::GetCursorScreenPos();
            if (ImGui::Button("##BUY", btnSize)) {
                ActiveTM().OpenTrade(tradeSymbol, TRADE_BUY, currentPrice,
                                         g_lotSize, currentTime, false,
                                         getTabCandleIndex()); // ← pass candle index
            }
            ImGui::PopStyleColor(3);
            ImGui::PopStyleVar();

            if (ImGui::IsItemVisible()) {
                DrawPriceOnButton(ImGui::GetWindowDrawList(),
                    buyPos.x, buyPos.y, btnW, btnH,
                    currentPrice, tradeSymbol, priceColor, sc, "BUY");
            }

            ImGui::SameLine();

            // ---- LOT (arrow up / input / arrow down) ----
            ImGui::BeginGroup();
            {
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(1.0f, 0.0f));
                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,  ImVec2(0.0f, 1.0f));

                // Panah ATAS
                if (ImGui::Button("##LotUp", ImVec2(lotW, arrowH)))
                    g_lotSize += 0.01f;
                {
                    ImVec2 p = ImGui::GetItemRectMin();
                    ImVec2 s = ImGui::GetItemRectSize();
                    ImDrawList* dl = ImGui::GetWindowDrawList();
                    float cx2 = p.x + s.x*0.5f, cy2 = p.y + s.y*0.5f;
                    ImVec2 tp[3]={
                        {cx2,        cy2 - s.y*0.28f},
                        {cx2 - 5.0f, cy2 + s.y*0.22f},
                        {cx2 + 5.0f, cy2 + s.y*0.22f}
                    };
                    dl->AddTriangleFilled(tp[0],tp[1],tp[2], IM_COL32(220,220,220,230));
                }

                // Input
                ImGui::SetNextItemWidth(lotW);
                ImGui::InputFloat("##lot", &g_lotSize, 0,0,"%.2f",
                                  ImGuiInputTextFlags_CharsDecimal);
                if (g_lotSize < 0.01f) g_lotSize = 0.01f;

                // Panah BAWAH
                if (ImGui::Button("##LotDown", ImVec2(lotW, arrowH))) {
                    g_lotSize -= 0.01f;
                    if (g_lotSize < 0.01f) g_lotSize = 0.01f;
                }
                {
                    ImVec2 p = ImGui::GetItemRectMin();
                    ImVec2 s = ImGui::GetItemRectSize();
                    ImDrawList* dl = ImGui::GetWindowDrawList();
                    float cx2 = p.x + s.x*0.5f, cy2 = p.y + s.y*0.5f;
                    ImVec2 tp[3]={
                        {cx2,        cy2 + s.y*0.28f},
                        {cx2 - 5.0f, cy2 - s.y*0.22f},
                        {cx2 + 5.0f, cy2 - s.y*0.22f}
                    };
                    dl->AddTriangleFilled(tp[0],tp[1],tp[2], IM_COL32(220,220,220,230));
                }

                ImGui::PopStyleVar(2);
            }
            ImGui::EndGroup();

            ImGui::SameLine();

            // ---- SELL ----
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
            ImGui::PushStyleColor(ImGuiCol_Button,
                g_colSell);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                ImVec4(g_colSell.x*1.15f, g_colSell.y*1.15f, g_colSell.z*1.15f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                ImVec4(g_colSell.x*0.8f,  g_colSell.y*0.8f,  g_colSell.z*0.8f,  1.0f));

            ImVec2 sellPos = ImGui::GetCursorScreenPos();
            if (ImGui::Button("##SELL", btnSize)) {
                ActiveTM().OpenTrade(tradeSymbol, TRADE_SELL, currentPrice,
                                         g_lotSize, currentTime, false,
                                         getTabCandleIndex()); // ← pass candle index
            }
            ImGui::PopStyleColor(3);
            ImGui::PopStyleVar();

            if (ImGui::IsItemVisible()) {
                double spreadPips = (GetDecimalDigits(tradeSymbol) == 2) ? 0.30 : 0.00030;
                double askPrice   = currentPrice + spreadPips;
                DrawPriceOnButton(ImGui::GetWindowDrawList(),
                    sellPos.x, sellPos.y, btnW, btnH,
                    askPrice, tradeSymbol, priceColor, sc, "SELL");
            }

            // ============================================================
            // C. TOGGLE PENDING
            // ============================================================
            ImGui::Spacing();
            ImGui::Separator();

            float winW = ImGui::GetWindowSize().x;
            ImGui::SetCursorPosX((winW - 24.0f) * 0.5f);

            ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0,0,0,0));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1,1,1,0.1f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(1,1,1,0.2f));
            if (ImGui::ArrowButton("##PendToggle",
                    g_showPendingPanel ? ImGuiDir_Down : ImGuiDir_Right))
                g_showPendingPanel = !g_showPendingPanel;
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Pending Orders");
            ImGui::PopStyleColor(3);

            // ============================================================
            // D. PANEL PENDING 2x2
            // ============================================================
            if (g_showPendingPanel)
            {
                ImGui::Separator();
                ImGui::Spacing();

                float totalW = ImGui::GetContentRegionAvail().x;
                float pBtnW  = (totalW * 0.5f) - 2.0f;
                float pBtnH  = 22.0f * sc;

                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);

                // BUY LIMIT
                ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.10f,0.45f,0.75f,1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.15f,0.55f,0.90f,1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.08f,0.35f,0.60f,1.0f));
                if (ImGui::Button("BUY LIMIT",  ImVec2(pBtnW, pBtnH))) {}
                ImGui::PopStyleColor(3);

                ImGui::SameLine(0,4);

                // SELL LIMIT
                ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.75f,0.35f,0.05f,1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.90f,0.45f,0.08f,1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.60f,0.28f,0.04f,1.0f));
                if (ImGui::Button("SELL LIMIT", ImVec2(pBtnW, pBtnH))) {}
                ImGui::PopStyleColor(3);

                // BUY STOP
                ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.05f,0.45f,0.15f,1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.08f,0.60f,0.22f,1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.04f,0.35f,0.12f,1.0f));
                if (ImGui::Button("BUY STOP",   ImVec2(pBtnW, pBtnH))) {}
                ImGui::PopStyleColor(3);

                ImGui::SameLine(0,4);

                // SELL STOP
                ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.65f,0.08f,0.08f,1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.80f,0.12f,0.12f,1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.50f,0.06f,0.06f,1.0f));
                if (ImGui::Button("SELL STOP",  ImVec2(pBtnW, pBtnH))) {}
                ImGui::PopStyleColor(3);

                ImGui::PopStyleVar();
            }
        }
    }
    ImGui::End();
    ImGui::PopStyleVar(3);
}
// --- 🛠️ HELPER CALLBACK (Tulis ini sebelum fungsi RunLoop/Main) ---
// Kita butuh "amplop" untuk menyimpan data koordinat agar bisa dibaca saat Callback jalan
struct GPURenderData {
    float xMin, xMax, yMin, yMax;
    ImVec2 plotPos, plotSize, screenSize;
    float candleWidth;
};

// ─────────────────────────────────────────────────────────
// 🔥 PER-TAB GPU CALLBACK SYSTEM
// Setiap tab register render data-nya sendiri sebelum flush
// ImGui callback membawa pointer ke tab via cmd->UserCallbackData
// ─────────────────────────────────────────────────────────

// Struct yang dibawa per-callback (1 alokasi per tab per frame)
struct TabGPUCallbackData {
    GPURenderData renderData;
    GPUCandleRenderer* renderer;  // pointer ke renderer milik tab ini
};

// Pool kecil — max 16 chart window terbuka sekaligus
static TabGPUCallbackData s_cbPool[16];
static int s_cbPoolIdx = 0;  // reset tiap frame di awal RenderMainUI

// Callback universal — lihat siapa yang panggil via UserCallbackData
void RenderCandlesCallbackPerTab(const ImDrawList*, const ImDrawCmd* cmd) {
    auto* d = (TabGPUCallbackData*)cmd->UserCallbackData;
    if (!d || !d->renderer) return;
    const auto& rd = d->renderData;
    d->renderer->Render(rd.xMin, rd.xMax, rd.yMin, rd.yMax,
                        rd.plotPos, rd.plotSize, rd.screenSize, rd.candleWidth);
}

// Callback lama (untuk backward compat, tetap ada)
static GPURenderData s_gpuData;
void RenderCandlesCallback(const ImDrawList* parent_list, const ImDrawCmd* cmd) {
    g_gpuRenderer.Render(
        s_gpuData.xMin, s_gpuData.xMax,
        s_gpuData.yMin, s_gpuData.yMax,
        s_gpuData.plotPos, s_gpuData.plotSize,
        s_gpuData.screenSize, s_gpuData.candleWidth
    );
}

// =========================================================
// 🔥 RENDER SATU CHART WINDOW (dipanggil per-tab setiap frame)
// Setiap tab = ImGui::Begin() sendiri → bisa di-dock & resize
// =========================================================
void RenderSingleChartWindow(ChartTab* tab,
                              bool& replayMode, bool& replayStarted,
                              bool& showFPS, bool& g_showVolume)
{
    if (!tab || !tab->isOpen) return;

    // ════════════════════════════════════════════════════════════════
    // 🔥 SYMBOL PICKER — SymbolPickerUI terpusat
    // Kalau tab->symbol kosong → tampil picker, belum render candle
    // ════════════════════════════════════════════════════════════════
    if (tab->symbol.empty()) {
        if (!SymbolPicker::IsOpen())
            SymbolPicker::Open("initial");   // user pilih TF sendiri di picker

        SymbolPicker::Render("initial", [&](const std::string& sym, const std::string& tf) {
            tab->symbol    = sym;
            tab->timeframe = tf;
            tab->UpdateLabel();
            if (tab->usesGlobalData) {
                g_symbol   = sym;
                g_activeTF = tf;
                for (auto* _t : g_chartManager.tabs)
                    if (_t->usesGlobalData) { _t->symbol = sym; break; }
                #ifdef __EMSCRIPTEN__
                std::string cmd = "SetActiveSymbol('" + sym + "');";
                emscripten_run_script(cmd.c_str());
                #endif
                printf("▶ [PICKER] Primary: %s %s\n", sym.c_str(), tf.c_str());
            } else {
                #ifdef __EMSCRIPTEN__
                std::string cmd = "LoadTabSymbol(" + std::to_string(tab->id)
                                + ",'" + sym + "');";
                emscripten_run_script(cmd.c_str());
                #endif
                printf("▶ [PICKER] Tab %d: %s %s\n", tab->id, sym.c_str(), tf.c_str());
            }
        });
        return;
    }


    // ════════════════════════════════════════════════════════════════
    // 📖 ORDER BOOK TAB — render OB panel, bukan candle chart
    // Tab ini tidak punya candle, hanya tampilkan L2 OB untuk symbol
    // ════════════════════════════════════════════════════════════════
    if (tab->isOrderBookTab) {
        std::string obTitle = tab->symbol + " | OB##ob_tab_"
                            + std::to_string(tab->id);
        ImGui::SetNextWindowSize(ImVec2(260, 520), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowBgAlpha(0.96f);

        if (ImGui::Begin(obTitle.c_str(), &tab->isOpen,
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
            RenderOBTabContent(tab->symbol);
        }
        ImGui::End();
        return;
    }

    // ════════════════════════════════════════════════════════════════
    // 🔥 LOADING SPINNER — bulk-load IDB (non-primary saja)
    // ════════════════════════════════════════════════════════════════
    if (tab->isLoading && !tab->usesGlobalData) {
        // FIXED window name agar docking stabil saat symbol/TF berubah
        std::string spTitle = (tab->id == 0) ? "<TAB>"
                           : "<TAB>" + std::to_string(tab->id);
        ImGui::SetNextWindowSize(ImVec2(600, 400), ImGuiCond_FirstUseEver);
        if (ImGui::Begin(spTitle.c_str(), &tab->isOpen,
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
            float   t  = (float)ImGui::GetTime();
            char   sp = "|/-\\"[(int)(t * 4) % 4];
            ImVec2 ws = ImGui::GetWindowSize();
            ImGui::SetCursorPos(ImVec2(ws.x*0.5f-100.f, ws.y*0.5f-12.f));
            ImGui::TextColored(ImVec4(0.2f,0.8f,1.f,1.f),
                "[ %c ] Loading %s...", sp, tab->symbol.c_str());
            ImGui::SetCursorPos(ImVec2(ws.x*0.5f-80.f, ws.y*0.5f+12.f));
            ImGui::TextDisabled("Memuat history candle dari IndexedDB...");
        }
        ImGui::End();
        return;
    }

    // ════════════════════════════════════════════════════════════════
    // 🔥 FIXED WINDOW NAME — docking stabil saat ganti symbol/TF
    // ════════════════════════════════════════════════════════════════
    // Nama window TIDAK berubah saat symbol/TF berganti.
    // ImGui selalu recognize window yang sama → DockId tetap → tidak perlu drag ulang.
    // Simbol/TF sudah ditampilkan di RenderActiveIndicatorsOverlay() di dalam plot.
    std::string winTitle = (tab->id == 0) ? "<TAB>"
                       : "<TAB>" + std::to_string(tab->id);

    // ── WindowPadding=0 agar plot menempel ke tepi title bar (zero gap) ──
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));

    // ── Title bar abu-biru gelap (navy) ──
    ImGui::PushStyleColor(ImGuiCol_TitleBg,
        ImVec4(0.14f, 0.16f, 0.20f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_TitleBgActive,
        ImVec4(0.18f, 0.22f, 0.30f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_TitleBgCollapsed,
        ImVec4(0.10f, 0.12f, 0.16f, 1.00f));

    ImGui::SetNextWindowSize(ImVec2(600, 400), ImGuiCond_FirstUseEver);

    bool windowOpen = tab->isOpen;
    if (!ImGui::Begin(winTitle.c_str(), &windowOpen,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
        tab->isOpen = windowOpen;
        ImGui::PopStyleColor(3); // TitleBg x3
        ImGui::PopStyleVar();    // WindowPadding
        ImGui::End();
        return;
    }
    tab->isOpen = windowOpen;
    // Restore WindowPadding segera setelah Begin() sukses
    // agar header/widget di dalam window tidak zero-padding
    ImGui::PopStyleVar(); // WindowPadding

    // Tandai tab aktif saat window di-fokus
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) {
        g_chartManager.activeTabId = tab->id;
        g_activeChart  = tab;
        // HANYA tab utama (usesGlobalData) yang boleh update global.
        // Tab non-utama punya symbol/TF sendiri — JANGAN overwrite global
        // atau semua tab akan saling "ikut" saat fokus berpindah.
        if (tab->usesGlobalData) {
            g_activeTF = tab->timeframe;
            g_symbol   = tab->symbol;
        }
    }
    // ── AMBIL DATA CANDLE ──
    std::vector<Candle> candles_to_render;
    int global_idx_offset = 0;
    int limitIndex_global = 0; // Dipakai untuk resolve viewCenterIndex setelah block
    {
        std::lock_guard<std::mutex> lk(g_candlesMutex);
        std::vector<Candle>* srcPtr = nullptr;
        if (tab->isReplayExtraTab) {
            // Tab extra replay: langsung pakai g_allCandles[tf] (sama source dengan tab utama)
            // Bukan SYMBOL_TF karena data replay ada di key tf biasa
            srcPtr = &g_allCandles[tab->timeframe];
        } else if (replayMode && g_replaySourceTF && tab->usesGlobalData) {
            srcPtr = g_replaySourceTF;
        } else if (tab->usesGlobalData) {
            // Tab utama → pakai g_allCandles[tf] (data global)
            srcPtr = &g_allCandles[tab->timeframe];
        } else {
            // 🔥 FIX: Tab non-utama SELALU pakai data sendiri di "SYMBOL_TF"
            srcPtr = &g_allCandles[tab->symbol + "_" + tab->timeframe];
        }

        auto& src = *srcPtr;
        if (src.empty()) {
            float  t  = (float)ImGui::GetTime();
            char   sp = "|/-\\"[(int)(t * 4) % 4];
            ImVec2 ws = ImGui::GetWindowSize();
            ImGui::SetCursorPos(ImVec2(ws.x * 0.5f - 80.f, ws.y * 0.5f - 12.f));
            ImGui::TextDisabled("[ %c ] Memuat %s %s...",
                sp, tab->symbol.c_str(), tab->timeframe.c_str());
            ImGui::SetCursorPos(ImVec2(ws.x * 0.5f - 100.f, ws.y * 0.5f + 12.f));
            ImGui::TextDisabled("Menunggu data dari IndexedDB / server");
            ImGui::PopStyleColor(3); // TitleBg x3
            ImGui::End();
            return;
        }

        int limitIndex = (int)src.size() - 1;
        if (tab->isReplayExtraTab && replayMode) {
            // Tab extra replay: potong candle sesuai posisi replay saat ini
            if (g_tfIndices.count(tab->timeframe))
                limitIndex = std::clamp(g_tfIndices[tab->timeframe], 0, (int)src.size()-1);
        } else if (tab->usesGlobalData && (replayMode || g_replayCutoff.active)) {
            limitIndex = std::clamp(g_tfIndices[tab->timeframe], 0, (int)src.size()-1);
        }
        limitIndex_global = limitIndex; // Simpan ke outer scope

        int bufferSize = 10000;
        int sc = std::max(0, limitIndex - bufferSize);
        int ec = limitIndex + 1;
        if (ec > sc) {
            candles_to_render.assign(src.begin()+sc, src.begin()+ec);
            global_idx_offset = sc;
        }
    }

    // V2: Guard dipindah ke JS (rebuildFullFromDB IDB sanitizer).
    // JS filter lebih akurat: pakai median price dari data sendiri,
    // hapus (bukan neutralize) → tidak ada gap artifact.

    if (candles_to_render.empty()) {
        float  t  = (float)ImGui::GetTime();
        char   sp = "|/-\\"[(int)(t * 4) % 4];
        ImVec2 ws = ImGui::GetWindowSize();
        ImGui::SetCursorPos(ImVec2(ws.x * 0.5f - 80.f, ws.y * 0.5f));
        ImGui::TextDisabled("[ %c ] Menyiapkan candle %s %s...",
            sp, tab->symbol.c_str(), tab->timeframe.c_str());
        ImGui::PopStyleColor(3); // TitleBg x3
        ImGui::End();
        return;
    }

    // Replay tick animation (tab utama saja)
    if ((tab->usesGlobalData || tab->isReplayExtraTab) && replayMode && replayStarted) {
        Candle& live = candles_to_render.back();
        double cp = g_replay.currentState.price;
        static double storedTime=0; static double mH=0,mL=0;
        if (live.time!=storedTime){storedTime=live.time;mH=live.open;mL=live.open;}
        if(cp>mH)mH=cp; if(cp<mL)mL=cp;
        live.close=cp; live.high=mH; live.low=mL;
        if(live.high<std::max(live.open,live.close)) live.high=std::max(live.open,live.close);
        if(live.low>std::min(live.open,live.close))  live.low=std::min(live.open,live.close);
    }

    int bufferCount = (int)candles_to_render.size();

    // ── STATE ZOOM/PAN PER-TAB ──
    ChartTabState& cs = tab->state;

    // ── Sinkron dari g_chart ke cs untuk tab utama ──
    if (tab->usesGlobalData) {
        cs.viewCenterIndex = g_chart.viewCenterIndex;
        cs.y_min  = g_chart.y_min;
        cs.y_max  = g_chart.y_max;
        cs.autoFitY = g_chart.autoFitY;
        cs.zoomLevel = g_chart.zoomLevel;
        // 🎯 Bridge legacy globals → tab state (untuk replay exit dll)
        GoToLive::SyncFromGlobals(tab, isAnimatingToLive, animFloatingIndex);
    }

    // Resolve -1 → live end (pertama kali render tab non-utama / ganti symbol)
    // PENTING: pakai limitIndex (GLOBAL index dari src), bukan bufferCount-1 (LOCAL).
    // Contoh: src.size()=70000 M1 candles, bufferSize=10000
    //   → sc=60000, bufferCount=10001 (LOCAL 0..10000)
    //   → limitIndex = 69999 (GLOBAL)
    //   → localViewCenter = 69999 - 60000 = 9999 ✅ (ujung kanan buffer)
    //   Kalau pakai bufferCount-1=10000:
    //   → localViewCenter = 10000 - 60000 = -50000 → clamp ke 0 ❌ (awal history!)
    if (cs.viewCenterIndex < 0) {
        cs.viewCenterIndex = limitIndex_global;  // GLOBAL index (src.size()-1)
        cs.autoFitY = true;
        if (tab->usesGlobalData) {
            g_chart.viewCenterIndex = cs.viewCenterIndex;
            g_chart.autoFitY = true;
        }
    }

    // =========================================================
    // 🔥 PLAY MODE: View otomatis lock & mengikuti candle replay terbaru
    //    Tab utama (usesGlobalData) + Tab extra replay (isReplayExtraTab)
    //    Saat PAUSE → tidak di-lock, user bebas interaksi
    // =========================================================
    if (replayMode && replayStarted && g_replay.running && !cs.isAnimatingToLive) {
        double globalTarget = (double)(global_idx_offset + bufferCount);
        int trackIdx = (int)std::round(globalTarget + g_rightMarginCandles);
        if (tab->usesGlobalData) {
            g_chart.viewCenterIndex = trackIdx;
            g_chart.autoFitY        = true;
            cs.viewCenterIndex      = trackIdx;
            cs.autoFitY             = true;
        } else if (tab->isReplayExtraTab) {
            cs.viewCenterIndex = trackIdx;
            cs.autoFitY        = true;
        }
    }

    // =========================================================
    // 🎯 GO TO LIVE — smooth lerp per-tab (semua tab support)
    //    Tab utama: juga sync ke g_chart (legacy globals bridge)
    //    Tab lain: hanya cs (independen)
    // =========================================================
    {
        int*  pVCI = tab->usesGlobalData ? &g_chart.viewCenterIndex : nullptr;
        bool* pAFY = tab->usesGlobalData ? &g_chart.autoFitY        : nullptr;
        GoToLive::Update(tab, bufferCount, global_idx_offset,
                         g_rightMarginCandles, pVCI, pAFY);
        // Bridge balik ke legacy globals (untuk extern di RenderNavigationPanel)
        GoToLive::SyncToGlobals(tab, isAnimatingToLive, animFloatingIndex);
    }

    int localViewCenter = cs.viewCenterIndex - global_idx_offset;
    int rightBuf = std::max((int)(cs.zoomLevel*5.f),(int)std::ceil(g_rightMarginCandles));
    localViewCenter = std::clamp(localViewCenter, 0, bufferCount-1+rightBuf);
    int view_w    = (int)cs.zoomLevel;
    int end_idx   = localViewCenter + (view_w/3);
    int start_idx = end_idx - view_w;
    if(start_idx<0) start_idx=0;
    if(end_idx<start_idx) end_idx=start_idx;
    int end_idx_with_margin = end_idx + (int)std::round(g_rightMarginCandles);
    float ci_viewSubOffset = 0.f; // sub-pixel X offset dari ChartInteraction (diisi di blok interaction)

    // =========================================================
    // 🔥 LAZY LOAD TRIGGER — scroll kiri mendekati ujung data
    // Hanya untuk primary tab, bukan saat replay / bulk load
    // Throttle: cek setiap ~30 frame (~0.5 detik di 60fps)
    // =========================================================
#ifdef __EMSCRIPTEN__
    // 🔥 PER-TAB LAZY TRIGGER
    // Primary tab  → key "M1" (global data)
    // Non-primary  → key "SYMBOL_M1" (data sendiri)
    // Throttle per-tab agar tidak fire tiap frame
    if (!replayMode && !tab->lazyPending && !tab->noMoreHistory
        && !(tab->usesGlobalData && g_primaryBulkLoading)) {
        static std::map<int,int> s_lazyThrottleMap;
        int& throttle = s_lazyThrottleMap[tab->id];
        if (++throttle >= 30) {
            throttle = 0;
            if (start_idx <= 80) {
                double oldestTime = 0.0;
                {
                    std::lock_guard<std::mutex> lk(g_candlesMutex);
                    std::string m1Key = tab->usesGlobalData
                        ? std::string("M1")
                        : (tab->symbol + "_M1");
                    auto it = g_allCandles.find(m1Key);
                    if (it != g_allCandles.end() && !it->second.empty())
                        oldestTime = it->second.front().time;
                }
                if (oldestTime > 0.0) {
                    tab->lazyPending = true;
                    int tid = tab->id;
                    EM_ASM({
                        if (window.onNearLeftEdgeTab)
                            window.onNearLeftEdgeTab($0, $1);
                    }, tid, oldestTime);
                }
            }
        }
    }
#endif

    // AutoFit Y
    // (candles_to_render sudah di-filter dari corrupt data di atas)
    if (cs.autoFitY) {
        double tMin=1e9,tMax=-1e9; bool hasD=false;
        int s2=std::max(0,start_idx), e2=std::min(bufferCount-1,end_idx);
        for(int i=s2;i<=e2;i++){
            tMin=std::min(tMin,candles_to_render[i].low);
            tMax=std::max(tMax,candles_to_render[i].high);
            hasD=true;
        }
        if(hasD&&tMin<tMax){
            double rng=tMax-tMin,pad=rng*0.1;
            tMin-=pad; tMax+=pad;
            // 🔥 FIX: Saat y_min/y_max == 0 (setelah symbol switch / clear),
            // SNAP langsung ke target. Lerp 0.15/frame dari 0→target butuh
            // ~20+ frame → chart rusak selama itu.
            // Juga snap kalau range lama SANGAT berbeda dari range baru
            // (misal XAUUSD 3000 → EURUSD 1.15 — lerp tidak akan cukup cepat)
            bool needSnap = (cs.y_min == 0.0 && cs.y_max == 0.0);
            if (!needSnap) {
                double oldRange = cs.y_max - cs.y_min;
                double newRange = tMax - tMin;
                // Snap kalau old/new range berbeda > 10x (beda order of magnitude)
                if (oldRange > 0 && (newRange / oldRange > 10.0 || oldRange / newRange > 10.0))
                    needSnap = true;
            }
            if (needSnap) {
                cs.y_min = tMin;
                cs.y_max = tMax;
            } else {
                cs.y_min+=(tMin-cs.y_min)*0.15;
                cs.y_max+=(tMax-cs.y_max)*0.15;
            }
        }
    }
    // Write back cs → g_chart agar tidak di-reset ke 0 tiap frame
    if (tab->usesGlobalData) {
        g_chart.y_min     = cs.y_min;
        g_chart.y_max     = cs.y_max;
        g_chart.autoFitY  = cs.autoFitY;
        g_chart.zoomLevel = cs.zoomLevel;
    }

    // Volume cache
    static std::vector<double> cacheVolXs,cacheVolUp,cacheVolDn;
    size_t cnt=candles_to_render.size();
    if(cacheVolXs.size()<cnt){cacheVolXs.resize(cnt+5000);cacheVolUp.resize(cnt+5000);cacheVolDn.resize(cnt+5000);}
    for(size_t i=0;i<cnt;i++){
        const auto& cc=candles_to_render[i]; cacheVolXs[i]=(double)i;
        if(cc.close>=cc.open){cacheVolUp[i]=cc.volume;cacheVolDn[i]=0;}
        else{cacheVolUp[i]=0;cacheVolDn[i]=cc.volume;}
    }

    // Indikator split overlay vs panel
    std::vector<Indicator*>& activeInds = tab->usesGlobalData ? g_activeIndicators : tab->indicators;
    std::vector<Indicator*> overlayInds, panelInds;
    for(auto* ind:activeInds){
        if(!ind->visible) continue;
        if(ind->type==IND_PANEL) panelInds.push_back(ind);
        else overlayInds.push_back(ind);
    }
    // ── AUTO-RECALC SAAT BAR CLOSE (LIVE MODE) ──────────────────────────────
    // Problem: saat candle baru terbentuk, src.size() naik jadi N+1.
    // UpdateLive punya guard "if(index >= values.size()) return" → bail karena
    // values masih ukuran N lama → indicator beku dari open sampai close berikutnya.
    // Fix: deteksi perubahan jumlah candle per-tab, langsung Calculate() ulang
    // agar values.size() tumbuh mengikuti src → UpdateLive bisa jalan normal.
    if (tab->usesGlobalData && !replayStarted) {
        static std::map<int, int> s_lastCandleCount; // tab_id → last known count
        auto& srcLive = g_allCandles[g_activeTF];
        int curCount = (int)srcLive.size();
        int& lastCount = s_lastCandleCount[tab->id];
        if (curCount != lastCount && curCount > 0) {
            lastCount = curCount;
            for (auto* ind : activeInds)
                if (ind) ind->Calculate(srcLive);
        }
    }

    int totalRows=1+(int)panelInds.size();
    static std::map<int,std::array<float,20>> s_rowRatios;
    auto& rr = s_rowRatios[tab->id];
    if(rr[0]==0.f){ rr.fill(1.f); rr[0]=5.f; }

    // ── IMPLOT ──
    ImPlot::PushStyleColor(ImPlotCol_FrameBg,  g_colorBg);
    ImPlot::PushStyleColor(ImPlotCol_PlotBg,   g_colorBg);
    ImPlot::PushStyleColor(ImPlotCol_AxisText, g_colorText);
    ImVec4 gridCol=g_colorText; gridCol.w=0.2f;
    ImPlot::PushStyleColor(ImPlotCol_AxisGrid, gridCol);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0,0));
    ImPlot::PushStyleVar(ImPlotStyleVar_PlotPadding, ImVec2(0,0));

    std::string subId="##sub_"+std::to_string(tab->id);
    ImPlotSubplotFlags subFlags=ImPlotSubplotFlags_LinkAllX|ImPlotSubplotFlags_NoTitle;

    // Reset crosshair setiap frame — diset ulang oleh plot manapun yang di-hover
    CrosshairReset(tab->crosshair);

    if(ImPlot::BeginSubplots(subId.c_str(),totalRows,1,ImVec2(-1,-1),subFlags,rr.data())) {

        int safe_s=std::max(0,start_idx);
        int safe_e=std::min(end_idx,(int)cnt);
        int vis_cnt=safe_e-safe_s;

        // Volume max p90
        double maxVol=1.0;
        if(vis_cnt>0&&safe_s<(int)cacheVolUp.size()){
            std::vector<double> vs; vs.reserve(vis_cnt);
            for(int i=safe_s;i<safe_e;i++){double v=cacheVolUp[i]+cacheVolDn[i];if(v>0)vs.push_back(v);}
            if(!vs.empty()){std::sort(vs.begin(),vs.end());int p=(int)(vs.size()*0.9f);if(p>=(int)vs.size())p=(int)vs.size()-1;maxVol=vs[p]*1.5f;if(maxVol<1)maxVol=1;}
        }

        // ── CHART UTAMA ──
        std::string mainId="##Main_"+std::to_string(tab->id);
        bool hasPanels=!panelInds.empty();
        if(ImPlot::BeginPlot(mainId.c_str(),ImVec2(-1,-1),
            ImPlotFlags_NoMenus|ImPlotFlags_NoBoxSelect|ImPlotFlags_NoLegend|ImPlotFlags_NoMouseText))
        {
            // TICK AXIS: Apply formatter PALING AWAL, sebelum SetupAxis/SetupAxisLimits
            ImPlotAxisFlags xf=hasPanels?ImPlotAxisFlags_NoTickLabels:ImPlotAxisFlags_None;
            g_axisTicks.ApplyFormatter(candles_to_render, safe_s, safe_e, !hasPanels);
            ImPlot::SetupAxis(ImAxis_X1,nullptr,xf);

            ImPlot::SetupAxis(ImAxis_Y1,nullptr,ImPlotAxisFlags_Opposite|ImPlotAxisFlags_Lock);
            ImPlot::SetupAxis(ImAxis_Y2,nullptr,ImPlotAxisFlags_NoGridLines|ImPlotAxisFlags_NoTickLabels);
            ImPlot::SetupAxisLimits(ImAxis_Y1,cs.y_min,cs.y_max,ImGuiCond_Always);
            ImPlot::SetupAxisLimits(ImAxis_X1, start_idx - ci_viewSubOffset, end_idx_with_margin - ci_viewSubOffset, ImGuiCond_Always);
            ImPlot::SetupAxisLimits(ImAxis_Y2,0,maxVol*5.0,ImGuiCond_Always);

            // Trade visuals:
            //   ALL tabs get full interactive rendering (zones + lines + tags + drag)
            //   Symbol filter ensures only trades matching this tab's symbol are shown
            //   Cross-TF: candle index translated via openTime epoch
            //   BOTH live + replay trade zones render on chart (gabungan, tanpa beda)
            if (safe_s < safe_e) {
                const std::string tabSymbol = (tab->usesGlobalData)
                    ? g_symbol
                    : (tab->symbol.empty() ? g_symbol : tab->symbol);
                // Render live trade zones
                g_liveManager.RenderAllVisuals(false, &candles_to_render, &tabSymbol);
                // Render replay trade zones (gabungan, tanpa filter mode)
                g_replayManager.RenderAllVisuals(false, &candles_to_render, &tabSymbol);
            }
            if(tab->usesGlobalData) {
                g_draw.activePanel  = ""; // chart utama = kosong
                g_draw.hasPanelMode = hasPanels; // ada panel → aktifkan delay touch
                g_draw.Render(candles_to_render);
            }
            else {
                tab->shapes.Render(ImPlot::GetPlotDrawList(),candles_to_render,"");
                g_tabDrawMgrs[tab->id].Render(candles_to_render);
            }

            // 🔥 GPU RENDERER PER-TAB
            // Init GPU renderer tab ini kalau belum
            tab->InitGPU();
            tab->InitOrderFlow();  // Set symbol & tickSize di orderFlowRenderer
            // Sinkron warna tema → semua tab ikut warna yang dipilih user
            tab->SyncColors(g_gpuRenderer.colorBull, g_gpuRenderer.colorBear);

            // Hollow body: aktif saat FP style → candle jadi frame kotak bolong
            // FP data (angka buy/sell) terlihat jelas tanpa ketutupan body GPU
            tab->gpuRenderer.hollowBody = IsFootprintStyle(tab->renderStyle);

            // Upload data candle TAB INI ke VBO milik tab ini
            if (!candles_to_render.empty())
                tab->gpuRenderer.UpdateData(candles_to_render);

            auto limits = ImPlot::GetPlotLimits();
            ImVec2 pPos = ImPlot::GetPlotPos(), pSz = ImPlot::GetPlotSize();

            // 🎯 Simpan bounds plot untuk Go To Live overlay
            GoToLive::SavePlotBounds(tab);

            // Ambil slot pool untuk callback tab ini
            // (pool direset tiap frame di RenderMainUI)
            if (s_cbPoolIdx < 16) {
                auto& cb = s_cbPool[s_cbPoolIdx++];
                cb.renderer   = &tab->gpuRenderer;   // ← renderer MILIK TAB INI
                cb.renderData = {
                    (float)limits.X.Min, (float)limits.X.Max,
                    (float)limits.Y.Min, (float)limits.Y.Max,
                    pPos, pSz, ImGui::GetIO().DisplaySize, 0.8f
                };
                s_gpuData = cb.renderData; // compat

                ImDrawList* dl = ImPlot::GetPlotDrawList();
                dl->AddCallback(ImDrawCallback_ResetRenderState, nullptr);

                if (tab->renderStyle == RENDER_CANDLE
                 || IsFootprintStyle(tab->renderStyle)) {
                    // GPU candle selalu digambar: untuk RENDER_CANDLE biasa
                    // maupun semua mode FP (footprint overlay di atas candle)
                    dl->AddCallback(RenderCandlesCallbackPerTab, &cb);
                } else {
                    static std::vector<double> closeVals;
                    closeVals.resize(candles_to_render.size());
                    for (int ci = 0; ci < (int)candles_to_render.size(); ci++)
                        closeVals[ci] = candles_to_render[ci].close;
                    if (!closeVals.empty()) {
                        if (tab->renderStyle == RENDER_AREA) {
                            ImPlot::SetNextFillStyle(ImVec4(
                                tab->lineColor.x, tab->lineColor.y,
                                tab->lineColor.z, 0.18f));
                            ImPlot::PlotShaded("##area", closeVals.data(), (int)closeVals.size());
                        }
                        ImPlot::SetNextLineStyle(tab->lineColor, 1.8f);
                        ImPlot::PlotLine("##line", closeVals.data(), (int)closeVals.size());
                    }
                }
                dl->AddCallback(ImDrawCallback_ResetRenderState, nullptr);
            }

            // ── ORDER FLOW / FOOTPRINT OVERLAY ──────────────────────────────
            // Per-tab: setiap tab punya renderStyle sendiri.
            // Tab A bisa RENDER_FP_BAR, Tab B tetap RENDER_CANDLE biasa.
            if (!candles_to_render.empty()) {

                // ── REPLAY DRAW PARAMS ───────────────────────────────────────
                // Kalau replay aktif → pass ReplayDrawParams agar footprint
                // hanya tampil untuk price level yang sudah "dikunjungi" harga.
                // Volume di-scale × tickProgress → angka tumbuh live.
                // Kalau bukan replay → rdp_ptr = nullptr → render normal penuh.
                ReplayDrawParams  rdp;
                const ReplayDrawParams* rdp_ptr = nullptr;
                if (g_replayActive && g_replay.active &&
                    (tab->usesGlobalData || tab->isReplayExtraTab))
                {
                    rdp.replayIndex = g_replay.currentIndex;
                    rdp.high        = (float)g_replay.currentState.high;
                    rdp.low         = (float)g_replay.currentState.low;
                    rdp.progress    = g_replay.tickProgress;
                    rdp_ptr         = &rdp;
                }

                // Per-tab instance: tab->orderFlowRenderer sudah tahu
                // symbol, tickSize, dan fpZoom tab ini — tidak akan campur dengan tab lain
                switch (tab->renderStyle) {
                    case RENDER_FP_OVERLAY:
                        tab->orderFlowRenderer.DrawFootprint(candles_to_render, rdp_ptr);
                        break;
                    case RENDER_FP_PROFILE:
                        tab->orderFlowRenderer.DrawFootprintProfile(candles_to_render, rdp_ptr);
                        break;
                    case RENDER_FP_BAR:
                        tab->orderFlowRenderer.DrawFootprintBar(candles_to_render, rdp_ptr);
                        break;
                    default: break;
                }

                // ── VP OVERLAY — layer ke-2, independen dari renderStyle ──────
                // Aktif kalau tab->showVolumeProfile == true.
                // Bisa gabung dengan style apapun:
                //   Candle + VP, FP Bar + VP, FP Profile + VP, dsb.
                // DrawVolumeProfile() render di sisi kanan chart (panel VP)
                // + VPOC dashed line + Value Area shading amber.
                if (tab->showVolumeProfile) {
                    tab->orderFlowRenderer.DrawVolumeProfile(candles_to_render);
                }

                // 🔥 NAKED VPOC — selalu tampil kalau showNakedVPOC=true
                // Independen dari VP toggle — bisa aktif tanpa VP
                // "Lampu sein" tetap nyala meski VP panel disembunyikan
                tab->orderFlowRenderer.DrawNakedVPOCs(candles_to_render);
            }
            // 🔥 isReplayExtraTab: recalculate indicator setiap kali limitIndex berubah
            // Tanpa ini indicator pakai data penuh (future), bukan cutoff replay
            if (tab->isReplayExtraTab && !candles_to_render.empty()) {
                static std::map<int, int> s_lastLimitPerTab;
                int& lastLimit = s_lastLimitPerTab[tab->id];
                if (lastLimit != limitIndex_global) {
                    lastLimit = limitIndex_global;
                    for (auto* ind : activeInds)
                        if (ind) ind->Calculate(candles_to_render);
                }
            }
           // SESUDAH:
                for(auto* ind : activeInds) {
                    if(!ind->visible || ind->type == IND_PANEL) continue;
                    ind->Render(global_idx_offset, bufferCount);
                    // Live mode: update ujung candle aktif dengan harga tick terbaru
                    if(tab->usesGlobalData && !replayStarted && g_liveTick.price > 0.00001) {
                        auto& src = g_allCandles[g_activeTF];
                        if(!src.empty())
                            ind->UpdateLive((int)src.size()-1, g_liveTick.price, 0.0, src);
                    }
                }

            // Volume bars
            if(g_showVolume&&vis_cnt>0){
                ImPlot::SetAxes(ImAxis_X1,ImAxis_Y2);
                ImPlot::SetNextFillStyle(ImVec4(0.2f,0.7f,0.2f,0.4f));
                ImPlot::PlotBars("##VU",cacheVolXs.data()+safe_s,cacheVolUp.data()+safe_s,vis_cnt,0.8);
                ImPlot::SetNextFillStyle(ImVec4(0.8f,0.2f,0.2f,0.4f));
                ImPlot::PlotBars("##VD",cacheVolXs.data()+safe_s,cacheVolDn.data()+safe_s,vis_cnt,0.8);
                ImPlot::SetAxes(ImAxis_X1,ImAxis_Y1);
            }

            // Live price line
            if(!candles_to_render.empty()){
                double lp=candles_to_render.back().close;
                if(tab->usesGlobalData&&replayMode&&replayStarted) lp=g_replay.currentState.price;
                else if(!tab->isReplayExtraTab) {
                    // 🔥 Semua tab pakai MarketWatch (update tiap tick)
                    // Fallback ke g_symbol kalau tab->symbol belum di-set
                    extern MarketWatchPanel g_marketWatch;
                    std::string lkSym2 = (!tab->symbol.empty()) ? tab->symbol
                                       : (tab->usesGlobalData ? g_symbol : "");
                    if (!lkSym2.empty()) {
                        double mwPx = g_marketWatch.GetLivePrice(lkSym2);
                        if (mwPx > 1e-5) lp = mwPx;
                    }
                }
                ImVec4 pc=(lp>=candles_to_render.back().open)?ImVec4(0,1,0,1):ImVec4(1,0.2f,0.2f,1);
                ImPlot::TagY(lp,pc,(lp>500?"%.2f":"%.5f"),lp);
                double dp=lp;
                ImPlot::DragLineY(9000+tab->id,&dp,pc,0.7f,
                    ImPlotDragToolFlags_NoFit|ImPlotDragToolFlags_NoInputs);
            }

            // Drag & Zoom — tab utama pakai g_chart, tab lain state sendiri
            if(ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)){
                bool isTrdBusy=false;
                for(const auto& t:ActiveTM().trades) if(t.IsDragging()){isTrdBusy=true;break;}

                // isDrwBusy: HANYA block saat mid-stroke / editing / popup.
                // anyDrawToolActive dihapus — itu yang bikin chart macet setelah drawing selesai.
                CDrawingManager& curDraw = tab->usesGlobalData ? g_draw : g_tabDrawMgrs[tab->id];
                bool isDrwBusy = curDraw.isDrawing || curDraw.IsEditing() || curDraw.isPopupOpen;

                // 🔄 AUTO-RESET KE CURSOR setelah stroke selesai (seperti TradingView).
                if (!curDraw.isDrawing && !ImGui::IsMouseDown(0)
                    && g_uiState.activeTool != TOOL_CURSOR
                    && g_uiState.activeTool != TOOL_NONE) {
                    g_uiState.activeTool = TOOL_CURSOR;
                    curDraw.StopDrawing();
                }

                bool isCutBusy=tab->usesGlobalData&&isCutoffDragging;
                // 🔥 PLAY: blokir semua interaksi chart saat replay berjalan
                //    PAUSE: user bebas geser/zoom, autoFitY dimatikan otomatis saat drag
                bool replayIsPlaying = replayMode && replayStarted && g_replay.running;
                bool blockIt=isTrdBusy||isDrwBusy||isCutBusy||replayIsPlaying;

                if(tab->usesGlobalData){
                    // Tab utama: ChartInteraction (pull g_chart → update → push balik)
                    static ChartInteraction ci_main;
                    ci_main.PullFrom(g_chart);
                    ci_main.Update(
                        g_allCandles.count(g_activeTF)
                            ? (int)g_allCandles[g_activeTF].size()-1 : 0,
                        blockIt);
                    ci_main.PushTo(g_chart);
                    ci_viewSubOffset   = ci_main.GetViewOffset();
                    cs.viewCenterIndex = g_chart.viewCenterIndex;
                    cs.y_min    = g_chart.y_min; cs.y_max  = g_chart.y_max;
                    cs.autoFitY = g_chart.autoFitY;
                    cs.zoomLevel= g_chart.zoomLevel;
                } else {
                    // Tab non-utama: ChartInteraction per-tab
                    static std::map<int, ChartInteraction> s_ci;
                    ChartInteraction& ci = s_ci[tab->id];
                    ci.PullFrom(cs);
                    {
                        std::string k = tab->symbol + "_" + tab->timeframe;
                        int maxIdx = g_allCandles.count(k)
                            ? (int)g_allCandles[k].size()-1 : 0;
                        ci.Update(maxIdx, blockIt);
                    }
                    ci.PushTo(cs);
                    ci_viewSubOffset = ci.GetViewOffset();
                }

                // ── CTRL + SCROLL → fpZoom per-tab (text density footprint) ──
                // Disimpan di tab->orderFlowRenderer.fpZoom — per-tab instance
                if (IsFootprintStyle(tab->renderStyle)) {
                    ImGuiIO& io2 = ImGui::GetIO();
                    if (io2.KeyCtrl && fabs(io2.MouseWheel) > 0.01f) {
                        tab->orderFlowRenderer.fpZoom += io2.MouseWheel * 0.15f;
                        tab->orderFlowRenderer.fpZoom  = std::clamp(tab->orderFlowRenderer.fpZoom, 0.2f, 5.0f);
                    }
                }
            }

            {
                // Overlay indikator: semua tab, per-tab aware
                std::vector<Indicator*>& tabInds = tab->usesGlobalData
                    ? g_activeIndicators : tab->indicators;

                // ✅ CROSSHAIR: semua tab (primary + non-primary)
                {
                    // Tentukan candle key untuk lookup waktu
                    std::string crossKey;
                    if (tab->isReplayExtraTab || tab->usesGlobalData)
                        crossKey = tab->timeframe;  // "M5", "H1", dll
                    else
                        crossKey = tab->symbol + "_" + tab->timeframe;  // "BTCUSDT_M5"

                    CDrawingManager& _cd = tab->usesGlobalData ? g_draw : g_tabDrawMgrs[tab->id];
                    if (!_cd.isDrawing)
                        CrosshairDrawMain(tab->crosshair, crossKey, hasPanels, global_idx_offset);
                    else
                        tab->crosshair.active = false;
                }

                RenderActiveIndicatorsOverlay(tab, tabInds);
            }

            ImPlot::EndPlot();
        }

        // ── PANEL INDICATORS ──
        // PanelInteraction — satu instance per panel (key = tab*100 + panel index)
        static std::map<int,PanelInteraction> s_panelCI;

        for(int i=0;i<(int)panelInds.size();i++){
            Indicator* ind=panelInds[i];
            std::string pid="##Pan_"+ind->name+std::to_string(i)+"_t"+std::to_string(tab->id);
            int piKey = tab->id * 100 + i;
            PanelInteraction& pi = s_panelCI[piKey];

            if(ImPlot::BeginPlot(pid.c_str(),ImVec2(-1,-1),
                ImPlotFlags_NoMenus|ImPlotFlags_NoBoxSelect|ImPlotFlags_NoLegend)){
                bool isLast=(i==(int)panelInds.size()-1);
                // TICK AXIS: ApplyToPlot PALING AWAL, sebelum SetupAxis/SetupAxisLimits
                g_axisTicks.ApplyToPlot(isLast);

                ImPlot::SetupAxis(ImAxis_X1,nullptr,isLast?ImPlotAxisFlags_None:ImPlotAxisFlags_NoTickLabels);
                ImPlot::SetupAxisLimits(ImAxis_X1, start_idx - ci_viewSubOffset, end_idx_with_margin - ci_viewSubOffset, ImGuiCond_Always);

                // ── Y axis: PanelInteraction handle pan/scale/zoom ──────────
                bool isRSI = (ind->name=="RSI"||ind->name=="Stochastic");
                double defYMin = isRSI ? 0.0   : pi.y_min;
                double defYMax = isRSI ? 100.0  : pi.y_max;

                // pi.Update() hanya dipanggil setelah setup — aman
                // Tapi kita tentukan flag Y SEBELUM PlotX (locking)
                if (pi.autoFit && !isRSI) {
                    // Non-RSI belum di-interact → biarkan ImPlot autofit
                    ImPlot::SetupAxis(ImAxis_Y1, ind->name.c_str(),
                        ImPlotAxisFlags_Opposite | ImPlotAxisFlags_AutoFit);
                } else if (isRSI) {
                    // RSI: range tetap 0–100, bisa di-scroll oleh PanelInteraction
                    if (pi.autoFit) { pi.y_min = 0.0; pi.y_max = 100.0; }
                    ImPlot::SetupAxis(ImAxis_Y1, ind->name.c_str(), ImPlotAxisFlags_Opposite);
                    ImPlot::SetupAxisLimits(ImAxis_Y1, pi.y_min, pi.y_max, ImGuiCond_Always);
                } else {
                    // Sudah di-interact → pakai range dari PanelInteraction
                    ImPlot::SetupAxis(ImAxis_Y1, ind->name.c_str(), ImPlotAxisFlags_Opposite);
                    ImPlot::SetupAxisLimits(ImAxis_Y1, pi.y_min, pi.y_max, ImGuiCond_Always);
                }
                // Update PanelInteraction state (pan/scale/zoom) — setelah semua setup selesai
                // blockIt panel: sama seperti chart utama — stop pan saat drawing aktif
                {
                    CDrawingManager& curDrawP = tab->usesGlobalData ? g_draw : g_tabDrawMgrs[tab->id];
                    bool panelDrwBusy = curDrawP.isDrawing || curDrawP.IsEditing() || curDrawP.isPopupOpen;
                    if (panelDrwBusy) {
                        // Override: jangan pan saat drawing
                        pi.isPanningX = pi.isPanningY = pi.isScalingY = false;
                        pi.panConfirmed = false;
                    }
                }
                pi.Update(defYMin, defYMax, cs.zoomLevel);

                // ── Pan X dari panel → push ke chart state (viewCenterIndex) ──
                if (pi.xDeltaCandles != 0.f) {
                    int step = (int)pi.xDeltaCandles;
                    cs.viewCenterIndex = std::clamp(
                        cs.viewCenterIndex + step,
                        0,
                        (int)cnt + (int)(cs.zoomLevel * 5.f));
                    if (tab->usesGlobalData) {
                        g_chart.viewCenterIndex = cs.viewCenterIndex;
                    }
                }

                // ── PANEL HEADER: nama indikator + tombol ✕ hapus ────────────
                {
                    ImDrawList* hdl  = ImPlot::GetPlotDrawList();
                    ImVec2 hPos      = ImPlot::GetPlotPos();
                    ImVec2 hSz       = ImPlot::GetPlotSize();

                    // Background label nama — pojok kiri atas
                    const char* indName = ind->name.c_str();
                    ImVec2 txtSz = ImGui::CalcTextSize(indName);
                    float padX = 6.f, padY = 3.f;
                    ImVec2 lblMin = ImVec2(hPos.x + 4, hPos.y + 4);
                    ImVec2 lblMax = ImVec2(lblMin.x + txtSz.x + padX*2, lblMin.y + txtSz.y + padY*2);
                    hdl->AddRectFilled(lblMin, lblMax, IM_COL32(20,20,30,160), 3.f);
                    hdl->AddText(ImVec2(lblMin.x + padX, lblMin.y + padY),
                                 IM_COL32(180,220,255,200), indName);

                    // Tombol ✕ — pojok kanan atas panel
                    float btnSz = 18.f;
                    ImVec2 btnMin = ImVec2(hPos.x + hSz.x - btnSz - 6, hPos.y + 5);
                    ImVec2 btnMax = ImVec2(btnMin.x + btnSz, btnMin.y + btnSz);
                    ImVec2 mp    = ImGui::GetMousePos();
                    bool   hov   = (mp.x >= btnMin.x && mp.x <= btnMax.x &&
                                    mp.y >= btnMin.y && mp.y <= btnMax.y);
                    ImU32  btnC  = hov ? IM_COL32(220,60,60,230) : IM_COL32(80,80,100,180);
                    hdl->AddRectFilled(btnMin, btnMax, btnC, 3.f);
                    // tanda ✕
                    float m = 4.f;
                    ImU32 xC = IM_COL32(255,255,255,220);
                    hdl->AddLine(ImVec2(btnMin.x+m, btnMin.y+m), ImVec2(btnMax.x-m, btnMax.y-m), xC, 1.5f);
                    hdl->AddLine(ImVec2(btnMax.x-m, btnMin.y+m), ImVec2(btnMin.x+m, btnMax.y-m), xC, 1.5f);

                    if (hov && ImGui::IsMouseClicked(0)) {
                        // Tandai untuk dihapus setelah loop selesai
                        ind->markedForRemoval = true;
                        // Reset PanelInteraction instance-nya
                        s_panelCI.erase(piKey);
                    }
                }

                // Render full buffer (bukan vis_cnt) agar X align dengan X-axis local range.
             // SESUDAH:
                    ind->Render(global_idx_offset, bufferCount);
                    if((tab->usesGlobalData||tab->isReplayExtraTab) && replayMode && replayStarted)
                        ind->UpdateLive(g_replay.currentIndex, g_replay.currentState.price,
                                        g_replay.currentState.volume, g_allCandles["M1"]);
                    else if(tab->usesGlobalData && !replayStarted && g_liveTick.price > 0.00001) {
                        auto& src = g_allCandles[g_activeTF];
                        if(!src.empty())
                            ind->UpdateLive((int)src.size()-1, g_liveTick.price, 0.0, src);
                    }

                // ── DRAWING di panel — set activePanel dulu, lalu render ──────
                {
                    CDrawingManager& curDrawP = tab->usesGlobalData ? g_draw : g_tabDrawMgrs[tab->id];

                    // 1. SELALU set activePanel ke panel ini saat BeginPlot aktif.
                    //    Tidak boleh kondisional — agar HandleEditing tahu konteks
                    //    panel yang sedang di-render meski user belum hover/klik.
                    curDrawP.activePanel = ind->name; // "RSI", "Volume", dll

                    // 2. SELALU panggil Render saat plot ini di-hover ATAU ada state aktif.
                    //    Kasus kritis: klik PERTAMA untuk select shape → tidak ada
                    //    isDrawing/IsEditing/isPopupOpen sebelumnya, tapi HandleEditing
                    //    harus tetap jalan agar klik bisa men-select shape.
                    bool panelNeedsRender = ImPlot::IsPlotHovered()   // hover → bisa select
                                        || curDrawP.isDrawing          // sedang gambar
                                        || curDrawP.IsEditing()        // sedang drag handle
                                        || curDrawP.isPopupOpen        // popup terbuka
                                        || !curDrawP.selectedShapeId.empty(); // ada shape selected
                    if (panelNeedsRender)
                        curDrawP.Render(candles_to_render, /*isPanel=*/true);

                    // 3. Render shapes milik panel ini — dengan ClipRect agar tidak tembus batas.
                    //    Dipanggil terpisah dari Render() agar shapes selalu tampil
                    //    bahkan saat panelNeedsRender=false (misal cursor di chart utama).
                    {
                        ImDrawList* pdlS = ImPlot::GetPlotDrawList();
                        ImVec2 clipMin = ImPlot::GetPlotPos();
                        ImVec2 clipSz  = ImPlot::GetPlotSize();
                        ImVec2 clipMax = ImVec2(clipMin.x + clipSz.x, clipMin.y + clipSz.y);
                        pdlS->PushClipRect(clipMin, clipMax, true);
                        g_shapes.Render(pdlS, candles_to_render, curDrawP.selectedShapeId, ind->name);
                        pdlS->PopClipRect();
                    }
                }

                // ── CROSSHAIR di panel indicator ─────────────────────────────
                {
                    CDrawingManager& _cd = tab->usesGlobalData ? g_draw : g_tabDrawMgrs[tab->id];

                    // Tentukan candle key untuk lookup waktu
                    std::string crossKey;
                    if (tab->isReplayExtraTab || tab->usesGlobalData)
                        crossKey = tab->timeframe;
                    else
                        crossKey = tab->symbol + "_" + tab->timeframe;

                    CrosshairDrawPanel(tab->crosshair, crossKey,
                                       global_idx_offset, isLast, _cd.isDrawing);
                }

                ImPlot::EndPlot();
            }
        }

        // ── CLEANUP: hapus panel yang di-klik ✕ ────────────────────────
        {
            std::vector<Indicator*>& indList = tab->usesGlobalData
                ? g_activeIndicators : tab->indicators;
            for (auto it = indList.begin(); it != indList.end(); ) {
                if ((*it)->markedForRemoval) {
                    delete *it;
                    it = indList.erase(it);
                } else {
                    ++it;
                }
            }
        }

        ImPlot::EndSubplots();

        // ── POST-RENDER: Gambar garis V di chart utama jika cursor di panel ──
        // Saat cursor di panel RSI, chart utama sudah EndPlot() duluan.
        // Kita gambar ulang di window draw list menggunakan batas yang disimpan.
        if (hasPanels) CrosshairDrawPost(tab->crosshair);
    }

    ImPlot::PopStyleVar();
    ImGui::PopStyleVar();
    ImPlot::PopStyleColor(4);

    // ═════════════════════════════════════════════════════════
    // 🎯 GO TO LIVE BUTTON — semua tab (primary & non-primary)
    // ═════════════════════════════════════════════════════════
    {
        // Ambil harga live tab ini (MarketWatch lebih akurat dari close candle)
        double livePrice = candles_to_render.empty() ? 0.0
                         : candles_to_render.back().close;
        {
            extern MarketWatchPanel g_marketWatch;
            std::string lkSym = (!tab->symbol.empty()) ? tab->symbol
                               : (tab->usesGlobalData  ? g_symbol : "");
            if (!lkSym.empty()) {
                double mwPx = g_marketWatch.GetLivePrice(lkSym);
                if (mwPx > 1e-5) livePrice = mwPx;
            }
        }
        int*  pVCI = tab->usesGlobalData ? &g_chart.viewCenterIndex : nullptr;
        bool* pAFY = tab->usesGlobalData ? &g_chart.autoFitY        : nullptr;
        GoToLive::RenderButton(tab, bufferCount, global_idx_offset,
                               replayMode, livePrice, pVCI, pAFY);
    }

    // FPS (tab utama saja)
    if(showFPS && tab->usesGlobalData){
        ImVec2 wp=ImGui::GetWindowPos();
        ImGui::SetNextWindowPos(ImVec2(wp.x+10,wp.y+60));
        ImGui::SetNextWindowBgAlpha(0.6f);
        ImGuiWindowFlags ff=ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_AlwaysAutoResize|
            ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoFocusOnAppearing|
            ImGuiWindowFlags_NoNav|ImGuiWindowFlags_NoMove;
        if(ImGui::Begin("##FPSOverlay",&showFPS,ff)){
            float fps=ImGui::GetIO().Framerate;
            ImVec4 fc=ImVec4(0,1,0,1);
            if(fps<45)fc=ImVec4(1,1,0,1); if(fps<30)fc=ImVec4(1,0,0,1);
            ImGui::TextColored(fc,"FPS: %.1f",fps);
            ImGui::TextColored(ImVec4(0.8f,0.8f,0.8f,1),"%.3f ms",1000.f/fps);
            ImGui::Separator();
            ImGui::TextDisabled("Candles: %d",bufferCount);
        }
        ImGui::End();
    }

    ImGui::PopStyleColor(3); // TitleBg x3
    ImGui::End();
}

// Forward declaration — LoadWebLayout didefinisi setelah RenderMainUI
void LoadWebLayout(bool isMobileLayout = false);


// ================================================================
// 📌 RIGHT ICON BAR — Semua tombol icon vertikal di kanan layar.
//    Tambah tombol baru di sini, panggil fungsi dari file UI-nya.
//    Dipanggil dari RenderMainUI() tiap frame.
// ================================================================
void RenderRightBar() {
    ImGuiIO& io = ImGui::GetIO();

    // ImGuiCond_FirstUseEver → posisi/ukuran default hanya saat pertama kali
    // (setelah itu ImGui ingat dari imgui.ini / DockSpace layout)
    ImGui::SetNextWindowSize(ImVec2(48.0f, 300.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(
        ImVec2(io.DisplaySize.x - 56.0f, 36.0f),
        ImGuiCond_FirstUseEver);

    ImGuiWindowFlags barFlags =
        ImGuiWindowFlags_NoTitleBar  |
        ImGuiWindowFlags_NoScrollbar;
        // ✅ HAPUS NoSavedSettings → posisi & dock state tersimpan ke imgui.ini

    ImGui::PushStyleColor(ImGuiCol_WindowBg,  ImVec4(0.07f, 0.07f, 0.09f, 0.97f));
    ImGui::PushStyleColor(ImGuiCol_Border,    ImVec4(0.20f, 0.20f, 0.25f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,  ImVec2(4, 8));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,    ImVec2(0, 6));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 6.0f);

    // ✅ Nama window tanpa ### prefix → title bar tampil "Right Bar" saat di-dock
    //    (kalau NoTitleBar aktif, nama tidak terlihat saat float — tapi tetap
    //    dipakai ImGui sebagai KEY untuk menyimpan posisi di imgui.ini)
    ImGui::Begin("Right Bar", nullptr, barFlags);

    // ── Helper: satu tombol icon ──────────────────────────────────────
    auto RBBtn = [&](const char* id, ImTextureID tex, bool isActive,
                     const char* tooltip) -> bool {
        ImVec4 bg    = isActive ? ImVec4(0.20f,0.40f,0.80f,0.70f) : ImVec4(0,0,0,0);
        ImVec4 tint  = isActive ? ImVec4(0.8f,0.9f,1.0f,1.0f)     : ImVec4(0.7f,0.7f,0.8f,0.85f);
        ImGui::PushStyleColor(ImGuiCol_Button,        bg);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1,1,1,0.10f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(1,1,1,0.18f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,  ImVec2(6,6));
        bool pressed = ImGui::ImageButton(id, tex, ImVec2(24,24),
                                          ImVec2(0,0), ImVec2(1,1), bg, tint);
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(3);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", tooltip);
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        }
        return pressed;
    };

    auto RBSep = [&]() {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 p = ImGui::GetCursorScreenPos();
        float w = ImGui::GetContentRegionAvail().x;
        dl->AddLine(ImVec2(p.x+2, p.y+2), ImVec2(p.x+w-2, p.y+2),
                    IM_COL32(255,255,255,25));
        ImGui::Dummy(ImVec2(0,8));
    };

    // ── TOMBOL: Market Watch ──────────────────────────────────────────
    if (RBBtn("##RB_MktWatch", texMarketWatch,
              g_marketWatch.isOpen, "Market Watch"))
        g_marketWatch.isOpen = !g_marketWatch.isOpen;

    RBSep();

    // ── TOMBOL: Pohon Objek (UI_ObjectTree.h) ────────────────────────
    if (RBBtn("##RB_ObjTree", texTreeObj,
              g_objectTree.isOpen, "Pohon Objek"))
        g_objectTree.Toggle();

    RBSep();

    // ── TOMBOL BARU: Tambah di sini ──────────────────────────────────
    // Contoh:
    // if (RBBtn("##RB_Alert", texAlert, g_alertPanel.isOpen, "Alert & Notifikasi"))
    //     g_alertPanel.Toggle();
    // RBSep();

    ImGui::End();
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(2);
}

void RenderMainUI() {
    // =============================================================
    // 1. SETUP THEME (WARNA UI)
    // =============================================================
    ImGui::PushStyleColor(ImGuiCol_WindowBg, g_colorBg);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, g_colorPanel);
    ImGui::PushStyleColor(ImGuiCol_Text, g_colorText);
    
    ImGui::PushStyleColor(ImGuiCol_TitleBg,       g_colorHeader);
    ImGui::PushStyleColor(ImGuiCol_TitleBgActive, g_colorHeader);
    ImGui::PushStyleColor(ImGuiCol_Tab,           g_colorHeader);
    ImGui::PushStyleColor(ImGuiCol_TabHovered,    ImVec4(g_colorHeader.x + 0.1f, g_colorHeader.y + 0.1f, g_colorHeader.z + 0.1f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_TabActive,     g_colorPanel);
    ImGui::PushStyleColor(ImGuiCol_TabUnfocused,  g_colorHeader);
    ImGui::PushStyleColor(ImGuiCol_TabUnfocusedActive, g_colorPanel);
    ImGui::PushStyleColor(ImGuiCol_PopupBg,       g_colorPanel);

    // 🔥 Reset pool callback GPU per-tab (setiap frame)
    s_cbPoolIdx = 0;

    // DOCKING — Mobile: offset bawah navbar agar chart tidak tertimpa
    {
        ImGuiViewport* vp = ImGui::GetMainViewport();
        if (g_isMobile && g_navbarHeight > 0.0f) {
            // Geser DockSpace mulai di bawah navbar
            ImVec2 dsPos  = ImVec2(vp->WorkPos.x,
                                   vp->WorkPos.y + g_navbarHeight);
            ImVec2 dsSize = ImVec2(vp->WorkSize.x,
                                   vp->WorkSize.y - g_navbarHeight);
            ImGui::SetNextWindowPos (dsPos);
            ImGui::SetNextWindowSize(dsSize);
            ImGui::SetNextWindowViewport(vp->ID);

            ImGuiWindowFlags dsFlags =
                ImGuiWindowFlags_NoTitleBar    | ImGuiWindowFlags_NoCollapse  |
                ImGuiWindowFlags_NoResize      | ImGuiWindowFlags_NoMove      |
                ImGuiWindowFlags_NoBringToFrontOnFocus |
                ImGuiWindowFlags_NoBackground  | ImGuiWindowFlags_NoScrollbar |
                ImGuiWindowFlags_NoDocking;

            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0,0));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
            ImGui::Begin("##MobileDockHost", nullptr, dsFlags);
            ImGuiID dsId = ImGui::GetID("##MobileDockSpace");
            ImGui::DockSpace(dsId, ImVec2(0,0),
                ImGuiDockNodeFlags_PassthruCentralNode);
            ImGui::End();
            ImGui::PopStyleVar(3);
        } else {
            // Desktop: DockSpace penuh seperti biasa
            ImGui::DockSpaceOverViewport(0, vp);
        }
    }

    // --- STATE VARIABLES (STATIC) ---
    
    // Default true biar langsung kelihatan performanya
    static bool showFPS = false;
    static bool g_showVolume = false;
    static bool replayMode = false;

    // 🔥 Sync dari global: kalau wasm_cancel_replay() jalan → g_replayMode sudah false
    // Ini agar RenderReplayPanel/RenderNavigationPanel ikut tahu replay sudah off
    if (!g_replayMode && replayMode) replayMode = false;
    
    // Variabel Capture Posisi Grid
    static ImVec2 savedPlotPos;  
    static ImVec2 savedPlotSize; 
    static bool isPlotDrawn = false; 
  
   

    // =============================================================
    // 2. RENDER PANEL LAIN
    // =============================================================


    // 🔥 SYNC: g_replayCutoffTime → timestamp candle replay saat ini.
    if (replayMode) {
        std::lock_guard<std::mutex> lock(g_candlesMutex);
        if (g_allCandles.count("M1") && !g_allCandles["M1"].empty()) {
            int idx = std::clamp(g_replay.currentIndex, 0,
                                 (int)g_allCandles["M1"].size() - 1);
            g_replayCutoffTime = g_allCandles["M1"][idx].time;
        }
    } else {
        g_replayCutoffTime = 0.0; // Reset saat tidak replay
    }

    RenderTopToolbar();  
    RenderJarvisWindow();             
    RenderReplayPanel(replayMode);
    RenderReplaySetupPopup(replayMode, replayStarted);   
#ifdef __EMSCRIPTEN__
    // Layout swap saat device berganti (Mobile ↔ Desktop)
    if (g_layoutJustSwitched) {
        g_layoutJustSwitched = false;
        LoadWebLayout(g_isMobile);
        printf("[LAYOUT] Loaded %s layout after device switch\n",
               g_isMobile ? "Mobile" : "Desktop");
    }
#endif
    RenderNavigationPanel(replayMode);
    // Indikator per-tab: kirim candles dan list milik tab aktif
    {
        ChartTab* activeTab = g_chartManager.GetActiveTab();
        std::vector<Indicator*>& targetInds = (activeTab && !activeTab->usesGlobalData)
            ? activeTab->indicators : g_activeIndicators;
        const std::vector<Candle>& indCandles = [&]() -> const std::vector<Candle>& {
            if (activeTab && !activeTab->usesGlobalData) {
                std::string key = activeTab->symbol + "_" + activeTab->timeframe;
                if (g_allCandles.count(key) && !g_allCandles[key].empty())
                    return g_allCandles[key];
            }
            return g_allCandles[g_activeTF];
        }();
        RenderIndicatorModal(indCandles, targetInds);
    }
    RenderIndicatorSettingsPopup();
    g_draw.RenderShapePopup();
    for(auto& [tabId, dm] : g_tabDrawMgrs) dm.RenderShapePopup();
   g_marketWatch.Render(nullptr, g_symbol);
    // RENDER TRADE PANEL
    double currentPriceForPanel = 0;
    {
        std::lock_guard<std::mutex> lock(g_candlesMutex);
        if (!g_allCandles[g_activeTF].empty()) {
            currentPriceForPanel = g_allCandles[g_activeTF].back().close;
        }
        if (replayMode && replayStarted) {
             currentPriceForPanel = g_replay.currentState.price;
        }
    }
    TradePanelUI::Render(ActiveTM(), currentPriceForPanel, g_replay.active);
    g_tradeSettingsUI.Render();
    g_displaySettingsUI.Render();  // Popup Pengaturan Tampilan
    // ====== TRADE HISTORY: Render History Panel (live + replay) ======
    HistoryPanelUI::Render(g_liveHistory, g_replayHistory,
                           TradePanelUI::liveDeposit, TradePanelUI::replayDeposit);

    RenderFloatingTradePanel();
   

    // =============================================================
    // 🗂️ RIGHT ICON BAR + OBJECT TREE PANEL
    // =============================================================
    RenderRightBar();
    g_objectTree.Render();
    // =============================================================
    // 3. 🔥 MULTI-CHART: Render semua tab sebagai window terpisah
    //    Setiap tab = ImGui::Begin() sendiri → bisa di-dock & resize
    //    Persis seperti MT5 multi-chart layout
    // =============================================================

    // Modal "Tambah Chart Baru" (dibuka dari toolbar atau tombol +)
    RenderAddChartModal();
    RenderAddOrderBookModal();   // OB tab picker — symbol only, no TF

    // Loop semua tab, render masing-masing
    for (auto* tab : g_chartManager.tabs) {
        RenderSingleChartWindow(tab, replayMode, replayStarted, showFPS, g_showVolume);
    }

    // Bersihkan tab yang ditutup user (isOpen = false)
    for (int i = (int)g_chartManager.tabs.size()-1; i >= 0; i--) {
        if (!g_chartManager.tabs[i]->isOpen && g_chartManager.tabs.size() > 1) {
            // Jangan hapus tab terakhir
            g_chartManager.RemoveTab(g_chartManager.tabs[i]->id);
        } else if (!g_chartManager.tabs[i]->isOpen) {
            // Kalau cuma 1, buka lagi (jangan sampai kosong)
            g_chartManager.tabs[i]->isOpen = true;
        }
    }

    // POP WARNA UI UTAMA
    ImGui::PopStyleColor(11);
    if (replayMode) {
        // ── Tick engine replay setiap frame ────────────────────────────────
        // cutoffBlocking=false saat replayStarted (user sudah pencet Play)
        // cutoffBlocking=true  saat masih di pause/setup (engine diam)
        if (replayStarted && g_replay.active)
            g_replay.Update(!replayStarted); // Update(false) = jalan bebas

        RenderReplayFloatingBar(replayMode, replayStarted);
    }
}
// ── AKHIR RenderMainUI ──
// ==================================================================================
// FUNGSI RESIZE KALIBRASI BARU (DPI AWARE: MOBILE VS DESKTOP)
// ==================================================================================
#ifdef __EMSCRIPTEN__
#include <emscripten/html5.h> 

EM_BOOL OnWebResize(int eventType, const EmscriptenUiEvent* uiEvent, void* userData) {
    // 🔥 PENGAMAN: Kalau ImGui belum siap, return
    if (ImGui::GetCurrentContext() == NULL) return EM_FALSE; 

    // 1. Ambil Ukuran CSS (Ukuran Logis Layar)
    double w = uiEvent->windowInnerWidth;
    double h = uiEvent->windowInnerHeight;
    
    // 2. Update Canvas & OpenGL
    emscripten_set_canvas_element_size("#canvas", w, h);
    GLFWwindow* window = (GLFWwindow*)userData;
    glfwSetWindowSize(window, (int)w, (int)h);
    glViewport(0, 0, (int)w, (int)h);

    // 3. Update ImGui Display Size
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2((float)w, (float)h);

    // --- RESET STYLE KE DEFAULT DULU ---
    ImGui::GetStyle() = ImGuiStyle();
    ImGui::StyleColorsDark(); 
    ImGuiStyle& style = ImGui::GetStyle();

    // --- LOGIKA DETEKSI TIPE DEVICE ---
    // HP biasanya lebarnya di bawah 900px (Portrait/Landscape kecil)
    bool isMobile = (w < 900.0);

    // ── Deteksi device switch (Mobile ↔ Desktop) ──────────────────
    static bool s_firstResize   = true;
    static bool s_prevMobile    = false;
    if (s_firstResize) { s_prevMobile = isMobile; s_firstResize = false; }

    bool deviceSwitched = (isMobile != s_prevMobile);
    if (deviceSwitched) {
        // Simpan layout lama dulu
        size_t szOld;
        const char* oldIni = ImGui::SaveIniSettingsToMemory(&szOld);
        const char* oldKey = s_prevMobile
            ? "MyTradingApp_Layout_Mobile"
            : "MyTradingApp_Layout_Desktop";
        EM_ASM({
            localStorage.setItem(UTF8ToString($0), UTF8ToString($1));
        }, oldKey, oldIni);
        g_layoutJustSwitched = true;
        s_prevMobile = isMobile;
        printf("[LAYOUT] Device switched → load %s layout\n",
               isMobile ? "Mobile" : "Desktop");
    }

    g_isMobile = isMobile;   // simpan global → dipakai RenderNavigationPanel

    if (isMobile) {
        printf("📱 MOBILE UI DETECTED: %dx%d\n", (int)w, (int)h);
        
        // --- MOBILE CONFIG (BESAR & SENTUH) ---
        
        // 1. Scale Ukuran Widget (Padding, Spacing) biar jempol masuk
        float widgetScale = 1.15f; 
        style.ScaleAllSizes(widgetScale); 

        // 2. FONT SCALE (Normal / Sedikit Besar)
        // Jika base font 20px, scale 1.0 tetap 20px (Enak dibaca di HP)
        io.FontGlobalScale = 1.0f; 

        // 3. OPTIMASI SENTUH
        style.ScrollbarSize = 18.0f; // Scrollbar tebal
        style.GrabMinSize = 20.0f;   // Pentol scrollbar besar
        style.TouchExtraPadding = ImVec2(12.0f, 12.0f); // Area sentuh ghoib
        style.ItemSpacing = ImVec2(8.0f, 8.0f); // Jarak antar item lega
        style.WindowRounding = 8.0f;
    } 
    else {
        // --- DESKTOP CONFIG (PADAT & RAPI) ---
        printf("💻 DESKTOP UI DETECTED\n");

        // 1. FONT SCALE (DIPERKECIL)
        // Logika Hacker: Di Desktop, kita mau lihat banyak data chart.
        // Kita kecilkan font jadi 0.85x (Sekitar 16-17px realnya).
        io.FontGlobalScale = 0.85f; 

        // 2. STYLE RAPI (Compact)
        style.WindowPadding = ImVec2(8, 8);
        style.FramePadding  = ImVec2(5, 3);
        style.CellPadding   = ImVec2(4, 2);
        style.ItemSpacing   = ImVec2(6, 6); // Rapat tapi rapi
        
        style.ScrollbarSize = 12.0f; // Scrollbar tipis elegan
        style.TouchExtraPadding = ImVec2(0, 0); // Mouse presisi, gak butuh padding hantu
        style.WindowRounding = 4.0f; // Sudut lebih tajam (profesional)
    }

    return EM_TRUE;
}
#endif
// =========================================================
// FUNGSI PENGAMAN: LOAD FONT TANPA CRASH
// =========================================================
ImFont* LoadFontSafe(ImGuiIO& io, const char* path, float size, const ImFontConfig* config, const ImWchar* ranges) {
    // 1. Cek secara fisik apakah file ada di Virtual File System
    FILE* f = fopen(path, "rb");
    if (f) {
        fclose(f);
        printf("✅ [FONT] Berhasil memuat: %s\n", path);
        return io.Fonts->AddFontFromFileTTF(path, size, config, ranges);
    } 
    
    // 2. Jika tidak ada, jangan panggil AddFont (biar gak ASSERT/CRASH)
    printf("⚠️ [WARNING] File %s TIDAK DITEMUKAN! Melewati...\n", path);
    return nullptr; 
}
// =============================================================
// 1. WEB UI PERSISTENCE (LocalStorage Helper)
// =============================================================
// ================================================================
// DEFAULT LAYOUT — Di-embed saat compile, dipakai jika belum ada
// layout tersimpan di localStorage.
//
// CARA UPDATE:
//   1. Atur posisi window di browser
//   2. Buka Console → ketik:
//        copy(localStorage.getItem("MyTradingApp_Layout_Desktop"))
//        copy(localStorage.getItem("MyTradingApp_Layout_Mobile"))
//   3. Paste hasilnya ke dalam string di bawah
//   4. Compile ulang → semua user baru langsung dapat layout bagus
// ================================================================

static const char* k_DefaultLayoutDesktop = R"INI(
[Window][Debug##Default]
Pos=60,6
Size=400,400
Collapsed=0

[Window][Tools]
Pos=0,58
Size=51,718
Collapsed=0
DockId=0x00000002,0

[Window][Navigasi]
Pos=0,0
Size=1392,56
Collapsed=0
DockId=0x00000001,0

[Window][TradePanelFloating]
Pos=644,10
Size=89,33
Collapsed=0

[Window][Chart]
Pos=54,17
Size=1482,6
Collapsed=0
DockId=0x00000015,0

[Window][ReplayConfirm]
Pos=915,121
Size=73,38
Collapsed=0

[Window][Replay]
Pos=1394,0
Size=51,54
Collapsed=0
DockId=0x0000000B,0

[Window][Terminal / Trade Panel]
Pos=701,25
Size=835,26
Collapsed=0

[Window][Library Indikator]
Pos=-7,96
Size=500,600
Collapsed=0

[Window][Konfirmasi Simpan]
Pos=848,495
Size=224,90
Collapsed=0

[Window][##ReplayFloatingBar]
Pos=467,667
Size=320,85
Collapsed=0

[Window][Konfirmasi]
Pos=457,179
Size=136,58
Collapsed=0

[Window][Market Watch]
Size=246,928
Collapsed=0

[Window][Market Watch]
Pos=1161,58
Size=322,547
Collapsed=0
DockId=0x0000000A,0

[Window][Trade]
Pos=0,778
Size=1445,32
Collapsed=0
DockId=0x00000008,0

[Window][Market Watch Emoji]
Size=246,928
Collapsed=0
DockId=0x00000005,0

[Window][WindowOverViewport_11111111]
Pos=0,0
Size=1445,810
Collapsed=0

[Window][Right Bar]
Pos=1394,56
Size=51,720
Collapsed=0
DockId=0x0000000F,0

[Window][###ObjectTree]
Pos=1161,58
Size=322,547
Collapsed=0
DockId=0x0000000A,0

[Window][##SymPickerV2]
Pos=488,186
Size=560,437
Collapsed=0

[Window][<TAB>]
Pos=53,58
Size=641,363
Collapsed=0
DockId=0x00000015,0

[Window][<TAB>1]
Pos=696,58
Size=696,363
Collapsed=0
DockId=0x0000000C,0

[Window][##ReplaySetup]
Pos=341,232
Size=471,370
Collapsed=0

[Window][<TAB>2]
Pos=696,423
Size=696,353
Collapsed=0
DockId=0x0000000E,0

[Window][Trade History & Stats]
Pos=0,778
Size=1445,32
Collapsed=0
DockId=0x00000008,1

[Window][<TAB>3]
Pos=53,423
Size=641,353
Collapsed=0
DockId=0x00000014,0

[Window][<TAB>4]
Pos=53,335
Size=560,441
Collapsed=0
DockId=0x00000016,0

[Table][0x7E352B0B,11]
RefScale=17
Column 0  Width=37
Column 1  Weight=1.0000
Column 2  Weight=1.0000
Column 3  Weight=1.0000
Column 4  Weight=1.0000
Column 5  Weight=1.0000
Column 6  Weight=1.0000
Column 7  Weight=1.0000
Column 8  Weight=1.0000
Column 9  Weight=1.0000
Column 10 Width=75

[Table][0x3A629C60,4]
RefScale=18
Column 0  Width=60
Column 1  Weight=1.0000
Column 2  Width=24
Column 3  Width=60

[Table][0xFB29C657,4]
RefScale=17

[Table][0x06FC48A0,12]
RefScale=17
Column 0  Width=35
Column 1  Width=70
Column 2  Width=42
Column 3  Width=120
Column 4  Width=65
Column 5  Width=65
Column 6  Width=55
Column 7  Width=55
Column 8  Width=70
Column 9  Weight=1.0000
Column 10 Width=120
Column 11 Width=65

[Table][0x45F4181A,11]
RefScale=17
Column 0  Width=40
Column 1  Weight=1.0000
Column 2  Weight=1.0000
Column 3  Weight=1.0000
Column 4  Weight=1.0000
Column 5  Weight=1.0000
Column 6  Weight=1.0000
Column 7  Weight=1.0000
Column 8  Weight=1.0000
Column 9  Weight=1.0000
Column 10 Width=80

[Docking][Data]
DockSpace                   ID=0x08BD597D Window=0x1BBC0F80 Pos=0,0 Size=1445,810 Split=Y Selected=0x8FB8965C
  DockNode                  ID=0x00000003 Parent=0x08BD597D SizeRef=1920,776 Split=X
    DockNode                ID=0x00000005 Parent=0x00000003 SizeRef=246,831 Selected=0x662EAB37
    DockNode                ID=0x00000006 Parent=0x00000003 SizeRef=1597,831 Split=X Selected=0x0A5975BB
      DockNode              ID=0x00000004 Parent=0x00000006 SizeRef=1483,770 Split=Y Selected=0x6FC69BA3
        DockNode            ID=0x00000001 Parent=0x00000004 SizeRef=1483,56 HiddenTabBar=1 Selected=0xEC657A34
        DockNode            ID=0x00000010 Parent=0x00000004 SizeRef=1483,718 Split=X Selected=0x6FC69BA3
          DockNode          ID=0x00000002 Parent=0x00000010 SizeRef=51,686 HiddenTabBar=1 Selected=0x18A5FDB9
          DockNode          ID=0x00000011 Parent=0x00000010 SizeRef=1430,686 Split=X Selected=0x6FC69BA3
            DockNode        ID=0x00000007 Parent=0x00000011 SizeRef=723,743 Split=X Selected=0x6FC69BA3
              DockNode      ID=0x0000000D Parent=0x00000007 SizeRef=732,547 Split=Y Selected=0x2B733529
                DockNode    ID=0x00000012 Parent=0x0000000D SizeRef=556,362 Split=Y Selected=0x2B733529
                  DockNode  ID=0x00000015 Parent=0x00000012 SizeRef=556,275 CentralNode=1 Selected=0x2B733529
                  DockNode  ID=0x00000016 Parent=0x00000012 SizeRef=556,441 Selected=0x7E70D646
                DockNode    ID=0x00000014 Parent=0x0000000D SizeRef=556,353 Selected=0xD606D802
              DockNode      ID=0x00000013 Parent=0x00000007 SizeRef=696,547 Split=Y Selected=0xA624075A
                DockNode    ID=0x0000000C Parent=0x00000013 SizeRef=548,362 Selected=0xA624075A
                DockNode    ID=0x0000000E Parent=0x00000013 SizeRef=548,353 Selected=0xEE17B7AE
            DockNode        ID=0x0000000A Parent=0x00000011 SizeRef=322,743 Selected=0x83176BF0
      DockNode              ID=0x00000009 Parent=0x00000006 SizeRef=51,770 Split=Y Selected=0x2C9D8BAE
        DockNode            ID=0x0000000B Parent=0x00000009 SizeRef=51,54 HiddenTabBar=1 Selected=0x04D208E7
        DockNode            ID=0x0000000F Parent=0x00000009 SizeRef=51,720 HiddenTabBar=1 Selected=0x2C9D8BAE
  DockNode                  ID=0x00000008 Parent=0x08BD597D SizeRef=1920,32 Selected=0x3A4AC1F2

)INI";


// ── DEFAULT MOBILE LAYOUT ─────────────────────────────────────
// TODO: Atur layout di HP → copy dari localStorage → paste di sini
static const char* k_DefaultLayoutMobile =
    // PASTE MOBILE LAYOUT DI SINI setelah kamu atur di HP
    // Contoh format sama seperti desktop di atas
    nullptr;  // nullptr = pakai ImGui default dulu

// ─────────────────────────────────────────────────────────────
// LoadWebLayout(isMobileLayout)
//   true  → load key Mobile, fallback k_DefaultLayoutMobile
//   false → load key Desktop, fallback k_DefaultLayoutDesktop
// ─────────────────────────────────────────────────────────────
void LoadWebLayout(bool isMobileLayout) {
#ifdef __EMSCRIPTEN__
    // A. Load Layout ImGui — key berbeda per device
    const char* layoutKey = isMobileLayout
        ? "MyTradingApp_Layout_Mobile"
        : "MyTradingApp_Layout_Desktop";

    char* savedIni = (char*)EM_ASM_INT({
        var key  = UTF8ToString($0);
        var data = localStorage.getItem(key);
        if (!data) return 0;
        var buffer = Module._malloc(lengthBytesUTF8(data) + 1);
        stringToUTF8(data, buffer, lengthBytesUTF8(data) + 1);
        return buffer;
    }, layoutKey);

    if (savedIni) {
        // Ada layout tersimpan → pakai itu
        ImGui::LoadIniSettingsFromMemory(savedIni);
        free(savedIni);
        printf("[WEB] Layout Loaded (%s) from LocalStorage\n",
               isMobileLayout ? "Mobile" : "Desktop");
    } else {
        // Belum ada → pakai default yang di-compile ke binary
        const char* defaultIni = isMobileLayout
            ? k_DefaultLayoutMobile
            : k_DefaultLayoutDesktop;
        if (defaultIni) {
            ImGui::LoadIniSettingsFromMemory(defaultIni);
            printf("[WEB] Default Layout Applied (%s)\n",
                   isMobileLayout ? "Mobile" : "Desktop");
        } else {
            printf("[WEB] No default layout for %s, using ImGui default\n",
                   isMobileLayout ? "Mobile" : "Desktop");
        }
    }

    // B. Load Chart State (Zoom, Posisi, TF)
    char* savedState = (char*)EM_ASM_INT({
        var data = localStorage.getItem("MyTradingApp_ChartState");
        if (!data) return 0;
        var buffer = Module._malloc(lengthBytesUTF8(data) + 1);
        stringToUTF8(data, buffer, lengthBytesUTF8(data) + 1);
        return buffer;
    });

    if (savedState) {
        try {
            auto j = json::parse(savedState);
            if (j.contains("zoom")) g_chart.zoomLevel = j["zoom"].get<float>();
            if (j.contains("center")) g_chart.viewCenterIndex = j["center"].get<int>();
            if (j.contains("tf")) {
                std::string t = j["tf"].get<std::string>();
                if (g_allCandles.count(t)) g_activeTF = t;
            }
            printf("[WEB] Chart State Restored: Zoom=%.2f TF=%s\n", g_chart.zoomLevel, g_activeTF.c_str());
        } catch(...) {
            printf("[WEB] Warning: Corrupt ChartState JSON\n");
        }
        free(savedState);
    }

    // C. Load UI Panel State (isOpen tiap panel di Right Bar)
    char* savedUI = (char*)EM_ASM_INT({
        var data = localStorage.getItem("MyTradingApp_UIState");
        if (!data) return 0;
        var buffer = Module._malloc(lengthBytesUTF8(data) + 1);
        stringToUTF8(data, buffer, lengthBytesUTF8(data) + 1);
        return buffer;
    });
    if (savedUI) {
        try {
            auto uj = json::parse(savedUI);
            if (uj.contains("objTree")) g_objectTree.isOpen = uj["objTree"].get<bool>();
            // Tambah panel baru di sini:
            // if (uj.contains("alertPanel")) g_alertPanel.isOpen = uj["alertPanel"].get<bool>();
            if (uj.contains("mktWatch")) g_marketWatch.isOpen = uj["mktWatch"].get<bool>();
            printf("[WEB] UI Panel State Restored\n");
        } catch(...) {
            printf("[WEB] Warning: Corrupt UIState JSON\n");
        }
        free(savedUI);
    }
#endif
}
void SaveWebLayout() {
#ifdef __EMSCRIPTEN__
    // A. Simpan Layout ImGui — key berbeda per device
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantSaveIniSettings) {
        size_t size;
        const char* ini_data = ImGui::SaveIniSettingsToMemory(&size);
        const char* layoutKey = g_isMobile
            ? "MyTradingApp_Layout_Mobile"
            : "MyTradingApp_Layout_Desktop";
        EM_ASM({
            localStorage.setItem(UTF8ToString($0), UTF8ToString($1));
        }, layoutKey, ini_data);
        io.WantSaveIniSettings = false;
    }

    // B. Simpan Chart State (Setiap 1 Detik agar tidak lag)
    static double lastSave = 0;
    double now = emscripten_get_now();
    if (now - lastSave > 1000) { 
        lastSave = now;
        json j;
        j["zoom"]   = g_chart.zoomLevel;
        j["center"] = g_chart.viewCenterIndex;
        j["tf"]     = g_activeTF;
        std::string s = j.dump();
        EM_ASM({ localStorage.setItem("MyTradingApp_ChartState", UTF8ToString($0)); }, s.c_str());
    }

    // C. Simpan UI Panel State (isOpen tiap panel di Right Bar)
    //    Key terpisah agar ringan — tidak perlu setiap frame, cukup tiap 2 detik
    static double lastUISave = 0;
    if (now - lastUISave > 2000) {
        lastUISave = now;
        json uj;
        uj["objTree"] = g_objectTree.isOpen;
        // Tambah panel baru di sini:
        // uj["alertPanel"]  = g_alertPanel.isOpen;
        uj["mktWatch"] = g_marketWatch.isOpen;
        std::string us = uj.dump();
        EM_ASM({ localStorage.setItem("MyTradingApp_UIState", UTF8ToString($0)); }, us.c_str());
    }
#endif
}

// Wrapper untuk Main Loop Emscripten
std::function<void()> LoopCallback;
void RunLoop(void* arg) {
    LoopCallback();
}

// ===============================================================
// 🚀 MAIN ENTRY POINT
// ===============================================================
// ===========================================================
// Timeframe Target List (GLOBAL untuk WEB & DESKTOP)
// ===========================================================
std::vector<std::pair<std::string,int>> targets = {
    {"M5", 300},
    {"M15",900},
    {"M30",1800},
    {"H1",3600},
    {"H4",14400}
};


int main(int, char**) {
    TA_Initialize();
    std::string m1_cache_file = "candles_M1.cache";
// ===========================================================
// 1️⃣ MODE WEB: LOAD PRELOAD TRAD CACHE
// ===========================================================
#ifdef __EMSCRIPTEN__
printf("🌐 Web Mode: Loading Preloaded TRAD Cache...\n");
#ifdef __EMSCRIPTEN__
    EM_ASM(
        FS.mkdir('/data');
        FS.mount(IDBFS, {}, '/data');
        FS.syncfs(true, function(err) {
            console.log("IDBFS mounted & loaded:", err);
        });
    );
#endif


auto loadWebCacheTRAD = [&](const char* tfName, const char* filename){
    printf("[WEB] Loading %s\n", filename);

    FILE* f = fopen(filename, "rb");
    if (!f) {
        printf("❌ Missing TRAD file: %s\n", filename);
        return;
    }

    // Read magic
    char magic[4];
    fread(magic, 1, 4, f);
    if (memcmp(magic, "TRAD", 4) != 0) {
        printf("❌ Invalid TRAD header in %s\n", filename);
        fclose(f);
        return;
    }

    // Read count
    uint64_t count = 0;
    fread(&count, sizeof(uint64_t), 1, f);
    printf("   → %llu candles\n", (unsigned long long)count);

    std::vector<Candle>& dst = g_allCandles[tfName];
    dst.reserve(count);

    for (uint64_t i = 0; i < count; i++) {
        Candle c;

        fread(&c.open,   sizeof(double), 1, f);
        fread(&c.high,   sizeof(double), 1, f);
        fread(&c.low,    sizeof(double), 1, f);
        fread(&c.close,  sizeof(double), 1, f);
        fread(&c.time,   sizeof(double), 1, f);
        fread(&c.volume, sizeof(double), 1, f);

        uint64_t len = 0;
        fread(&len, sizeof(uint64_t), 1, f);

        char tmp[64];
        if (len > 63) len = 63;
        fread(tmp, 1, len, f);
        tmp[len] = 0;
        c.datetime = tmp;

        dst.push_back(c);
    }
    fclose(f);
};

// Load all TF
loadWebCacheTRAD("M1",  "candles_M1.cache");
loadWebCacheTRAD("M5",  "candles_M5.cache");
loadWebCacheTRAD("M15", "candles_M15.cache");
loadWebCacheTRAD("M30", "candles_M30.cache");
loadWebCacheTRAD("H1",  "candles_H1.cache");
loadWebCacheTRAD("H4",  "candles_H4.cache");

for (auto& kv : g_allCandles) {
    g_tfIndices[kv.first] = kv.second.empty() ? 0 : (int)kv.second.size() - 1;
}


#else
// ===========================================================
// 2️⃣ BUILD HIGHER TIMEFRAMES (WEB & PC)
// ===========================================================
printf("⚙️ Building Timeframes...\n");

std::vector<std::pair<std::string,int>> targets = {
    {"M5", 300}, {"M15",900}, {"M30",1800}, {"H1",3600}, {"H4",14400}
};

{
    const auto& m1 = g_allCandles["M1"];
    for (auto& t : targets){
        printf("   -> %s from M1...\n", t.first.c_str());
#ifdef __EMSCRIPTEN__
        // Web sudah punya cache → jangan generate ulang TF
        if (g_allCandles[t.first].empty()) {
            g_allCandles[t.first] = BuildTimeframeFromM1(m1, t.second);
        }
#else
        g_allCandles[t.first] = BuildTimeframeFromM1(m1, t.second);
#endif
        g_tfIndices[t.first] = g_allCandles[t.first].size() - 1;
    }
}
#endif  // <--- baru ini penutup global
    // -----------------------------------------------------------
    // 3️⃣ TAHAP 3: SETUP REPLAY & UI
    // -----------------------------------------------------------

    g_replay.Init(&g_allCandles["M1"], 5.0f);

    for (const auto& t : targets) {
    g_replay.LinkTF(&g_allCandles[t.first], &g_tfIndices[t.first]);
    }
// ... (kode sebelumnya) ...

    g_replay.OnCandleChange = [&](int idx){
        std::lock_guard<std::mutex> lock(g_candlesMutex);
        
        // Update index global
        g_tfIndices["M1"] = idx;

        // 🔥 FIX ERROR g_tradeModule UNDECLARED DISINI 🔥
        // Kita gunakan g_replayManager untuk mengecek semua trade saat replay digeser
        if (!g_allCandles["M1"].empty() && idx < g_allCandles["M1"].size()) {
            
            // Ambil candle pada posisi replay saat ini
            const Candle& cd = g_allCandles["M1"][idx];
            double cidx = GetActiveTFCandleIndex(cd.time); // ← index di active TF
            
            // Cek apakah ada trade yang kena SL/TP di candle ini (filter per simbol)
            g_replayManager.CheckAllHits(cd, cidx, &g_symbol);
            
            // Opsional: Update profit floating juga biar angka di tabel update pas digeser slider
            g_replayManager.UpdateAllLogic(cd.close, cd.time, cidx, &g_symbol);
        }
    };

    // ... (kode setelahnya) ...
    InitWebSocket();


    {
        std::lock_guard<std::mutex> lock(g_candlesMutex);
        if (!g_allCandles["M1"].empty()) {
            int lastIdx = (int)g_allCandles["M1"].size() - 1;
            currentIndex = lastIdx;
            
            // HANYA set ke lastIdx jika viewCenterIndex masih 0 (artinya belum di-load dari LocalStorage)
            if (g_chart.viewCenterIndex == 0) { 
                g_chart.viewCenterIndex = lastIdx;
            }
        }
    }
    // ===========================================================
    // SETUP WINDOW & CONTEXT (UPDATED FOR RESIZE CALLBACK)
    // ===========================================================
    if (!glfwInit()) return 1;
    
    // Setup Window Hint untuk WebGL
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
    glfwWindowHint(GLFW_CONTEXT_CREATION_API, GLFW_EGL_CONTEXT_API);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
// -----------------------------------------------------------
    // 🔥 CONFIG: DYNAMIC RESOLUTION (HP & PC AMAN)
    // -----------------------------------------------------------
    int width = 1280; // Default fallback
    int height = 720;

    #ifdef __EMSCRIPTEN__
        // Di Web, kita tanya browser dulu ukurannya berapa
        double w, h;
        emscripten_get_element_css_size("#canvas", &w, &h);
        width = (int)w;
        height = (int)h;
        printf("🚀 Start Resolution: %dx%d\n", width, height);
    #endif

    GLFWwindow* window = glfwCreateWindow(width, height, "YATA TRADER PRO", NULL, NULL); 
    if (!window) return 1;
    glfwMakeContextCurrent(window);

  
    // 2. SETUP IMGUI CONTEXT
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();
    
    // Load Layout & Config
    ImGui::LoadIniSettingsFromDisk("imgui.ini");
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable; 
    io.ConfigFlags |= ImGuiConfigFlags_IsTouchScreen;
    io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
    io.IniFilename = NULL; 

    // Set Ukuran Display Awal untuk ImGui
    io.DisplaySize = ImVec2((float)width, (float)height);

    ImGui::StyleColorsDark();

// ---------------------------------------------------------
    // 🔥 FONT SETUP: MULTI-STYLE ARSENAL (SAFE MODE)
    // ---------------------------------------------------------
    ImFontConfig fontCfg;
    fontCfg.OversampleH = 3; 
    fontCfg.OversampleV = 3;
    fontCfg.PixelSnapH = true;

    ImFontConfig iconCfg;
    iconCfg.MergeMode = true;
    iconCfg.PixelSnapH = true;
    iconCfg.GlyphOffset.y = 3.0f; 

    static const ImWchar icon_ranges[] = { 0x0020, 0x00FF, 0x2000, 0x2BFF, 0x1F000, 0x1F9FF, 0 };

    // 1. LOAD MODERN (Roboto.ttf) - WAJIB ADA
    g_fonts[0] = io.Fonts->AddFontFromFileTTF("Roboto.ttf", 20.0f, &fontCfg);
    io.Fonts->AddFontFromFileTTF("seguisym.ttf", 24.0f, &iconCfg, icon_ranges);

    // 2. LOAD CLASSIC (times.ttf)
    g_fonts[1] = LoadFontSafe(io, "times.ttf", 20.0f, &fontCfg, NULL);
    if (g_fonts[1]) {
        io.Fonts->AddFontFromFileTTF("seguisym.ttf", 24.0f, &iconCfg, icon_ranges);
    } else {
        g_fonts[1] = g_fonts[0]; // Pakai Roboto kalau Times gak ada
    }

    // 3. LOAD HACKER (code.ttf)
    g_fonts[2] = LoadFontSafe(io, "code.ttf", 19.0f, &fontCfg, NULL);
    if (g_fonts[2]) {
        io.Fonts->AddFontFromFileTTF("seguisym.ttf", 24.0f, &iconCfg, icon_ranges);
    } else {
        g_fonts[2] = g_fonts[0]; // Pakai Roboto kalau Code gak ada
    }
  // -----------------------------------------------------------
    // 🔥🔥🔥 REGISTRASI CALLBACK RESIZE (WAJIB) 🔥🔥🔥
    // -----------------------------------------------------------
    #ifdef __EMSCRIPTEN__
        // PENTING: Panggil fungsi resize sekali di awal untuk menerapkan Mode HP/PC
        EmscriptenUiEvent initEvent;
        initEvent.windowInnerWidth = width;
        initEvent.windowInnerHeight = height;
        OnWebResize(0, &initEvent, window);

        // Pasang "Telinga" agar kalau HP diputar, UI menyesuaikan
        emscripten_set_resize_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, window, EM_FALSE, OnWebResize);
    #endif

// Init Backend
ImGui_ImplGlfw_InitForOpenGL(window, true);
ImGui_ImplOpenGL3_Init("#version 300 es");

// Load Layout Web — pakai key sesuai device
LoadWebLayout(g_isMobile);

// 🔥 MULTI-CHART: Init tab pertama — symbol kosong, tampil picker dulu
{
    ChartTab* t = g_chartManager.AddTab("", "", true);
    (void)t;
    printf("✅ Chart tab utama siap (menunggu picker)\n");
}
    // -----------------------------------------------------------
    // 🔁 MAIN LOOP DEFINITION
    // -----------------------------------------------------------
    // ====== TRADE HISTORY: Auto-save timer ======
    static double g_lastHistorySave = 0;
    static const double HISTORY_SAVE_INTERVAL = 30.0; // save setiap 30 detik

    LoopCallback = [&]() {
       
        glfwPollEvents();

        // ── 🔥 TRADE HISTORY: Auto-save berkala ─────────────────────────
        {
            double now = std::chrono::system_clock::now().time_since_epoch().count() / 1000000000.0;
            if (now - g_lastHistorySave > HISTORY_SAVE_INTERVAL) {
                g_lastHistorySave = now;
                #ifdef __EMSCRIPTEN__
                    g_liveHistory.SaveToFile("/data/trade_history_live.json");
                    g_replayHistory.SaveToFile("/data/trade_history_replay.json");
                    EM_ASM(FS.syncfs(false, function(err) {
                        if(err) console.log("History auto-save failed:", err);
                    }););
                #else
                    g_liveHistory.SaveToFile("trade_history_live.json");
                    g_replayHistory.SaveToFile("trade_history_replay.json");
                #endif
            }
        }

        // ── 🔥 CANCEL REPLAY REQUEST ─────────────────────────────────────
        // Dipicu dari wasm_cancel_replay() (via data_check.js).
        // Semua local var (replayMode, replayStarted, g_chart, dll) accessible di sini.
        if (g_cancelReplayRequested) {
            g_cancelReplayRequested = false;

            // Reset state replay — pakai g_replayMode (global mirror) bukan local replayMode
            g_replayMode   = false;
            replayStarted  = false;
            g_replayActive = false;

            // Stop engine
            g_replay.Pause();
            g_replay.active = false;

            // Reset cutoff
            g_replayCutoff.active          = false;
            g_replayCutoff.showConfirmation = false;
            g_replayCutoffTime             = 0.0;

            // Kembalikan pointer ke live data
            {
                std::lock_guard<std::mutex> lock(g_candlesMutex);
                g_replaySourceTF = &g_allCandles[g_activeTF];
                g_replayIndexPtr = &g_tfIndices[g_activeTF];
                if (g_allCandles.count(g_activeTF) && !g_allCandles[g_activeTF].empty())
                    g_tfIndices[g_activeTF] = (int)g_allCandles[g_activeTF].size() - 1;
            }

            // Hapus tab extra replay
            for (int ri = (int)g_chartManager.tabs.size()-1; ri >= 0; ri--) {
                if (g_chartManager.tabs[ri]->isReplayExtraTab)
                    g_chartManager.RemoveTab(g_chartManager.tabs[ri]->id);
            }

            // GoToLive: chart loncat ke candle terbaru
            GoToLive::TriggerAllPrimary(g_chartManager,
                                        &g_chart.viewCenterIndex,
                                        &g_chart.autoFitY);
            g_chart.autoFitY = true;

            // Aktifkan gate — JS (reloadLiveAfterReplay) menutupnya setelah rebuild
            g_replayGateActive = true;

            printf("[DATA_CHECK] Cancel replay handled in main loop — kembali ke live\n");
        }
        // ────────────────────────────────────────────────────────────────

        // Start Frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::GetIO().FontDefault = g_fonts[g_selectedFontIdx];
        ImGui::NewFrame();

        
        // Hanya muncul jika g_showCreatorMode = true (diaktifkan lewat JS)
        if (g_showCreatorMode) {
             ShowCreatorPanel(&g_showCreatorMode);
        }
        
        // 1. Render UI Utama (Chart, Toolbar, dll)
        RenderMainUI();
        
        // Cek dan simpan layout jika berubah (Khusus Web)
        #ifdef __EMSCRIPTEN__
        SaveWebLayout(); 
        #endif
        // --- LOGIC UTAMA: MULTI-TRADE UPDATE (DUAL CORE) ---
        // Kita cek M1 karena itu base terkecil untuk pergerakan harga
        if (!g_allCandles["M1"].empty()) {
            
            double priceNow = 0.0;
            double timeNow = 0.0;

            // 🔀 CABANG 1: MODUS REPLAY
            if (g_replay.active) {
                priceNow = g_replay.currentState.price;
                // Safety: Jika harga replay 0, ambil close dari index saat ini
                if (priceNow == 0.0 && g_replay.currentIndex < g_allCandles["M1"].size()) {
                    priceNow = g_allCandles["M1"][g_replay.currentIndex].close;
                }
                
                if (g_replay.currentIndex < g_allCandles["M1"].size()) {
                    timeNow = g_allCandles["M1"][g_replay.currentIndex].time;
                }
            }
            // 🔀 CABANG 2: MODUS LIVE (REAL-TIME)
            else {
                // Ambil Candle PALING UJUNG (Terbaru)
                const Candle& liveCandle = g_allCandles["M1"].back();
                priceNow = liveCandle.close; // Harga running saat ini
                timeNow = liveCandle.time;
            }

            // 🔥 EKSEKUSI LOGIKA TRADE — PER-SYMBOL (live + replay terpisah)
            // Saat live: update g_liveManager. Saat replay: update g_replayManager.
            {
                // Pilih manager yang benar berdasarkan mode
                TradeManager& tm = g_replay.active ? g_replayManager : g_liveManager;

                // Kumpulkan unique simbol dari semua open trade di manager ini
                std::set<std::string> openSymbols;
                for (const auto& t : tm.trades)
                    if (t.isOpen) openSymbols.insert(t.symbol);

                // Untuk setiap simbol, ambil harga & waktu, lalu update trade
                for (const auto& sym : openSymbols) {
                    double symPrice = 0.0;
                    double symTime  = 0.0;

                    if (g_replay.active) {
                        // REPLAY: cari candle terakhir simbol ini
                        std::string m1key = sym + "_M1";
                        if (g_allCandles.count(m1key) && !g_allCandles[m1key].empty() &&
                            g_replay.currentIndex < (int)g_allCandles[m1key].size()) {
                            const Candle& rcd = g_allCandles[m1key][g_replay.currentIndex];
                            symPrice = rcd.close;
                            symTime  = rcd.time;
                        } else {
                            for (auto& [key, cnd] : g_allCandles) {
                                if (key.find(sym + "_") == 0 && !cnd.empty()) {
                                    symPrice = cnd.back().close;
                                    symTime  = cnd.back().time;
                                    break;
                                }
                            }
                        }
                    } else {
                        // LIVE: ambil dari market watch dulu, fallback ke candle terakhir
                        symPrice = g_marketWatch.GetLivePrice(sym);
                        if (symPrice < 1e-5) {
                            for (auto& [key, cnd] : g_allCandles) {
                                if (key.find(sym + "_") == 0 && !cnd.empty()) {
                                    symPrice = cnd.back().close;
                                    symTime  = cnd.back().time;
                                    break;
                                }
                            }
                        } else {
                            for (auto& [key, cnd] : g_allCandles) {
                                if (key.find(sym + "_") == 0 && !cnd.empty()) {
                                    symTime = cnd.back().time;
                                    break;
                                }
                            }
                        }
                    }

                    if (symPrice < 0.00001) continue;

                    // Hitung candle index di active TF untuk simbol ini
                    double symCidx = 0.0;
                    {
                        std::string tfKey = sym + "_" + g_activeTF;
                        if (g_allCandles.count(tfKey) && !g_allCandles[tfKey].empty()) {
                            auto& c = g_allCandles[tfKey];
                            auto it = std::upper_bound(c.begin(), c.end(), symTime,
                                [](double t, const Candle& cd) { return t < cd.time; });
                            if (it != c.begin()) { --it; symCidx = (double)(it - c.begin()); }
                        }
                    }

                    // Update floating P/L + cek SL/TP untuk trade simbol ini saja
                    tm.UpdateAllLogic(symPrice, symTime, symCidx, &sym);

                    // Buat tick candle untuk hit testing
                    Candle tickCandle;
                    tickCandle.open = tickCandle.high = tickCandle.low = tickCandle.close = symPrice;
                    tickCandle.time = symTime;
                    tm.CheckAllHits(tickCandle, symCidx, &sym);
                }
            }
        }
        #ifdef __EMSCRIPTEN__
            // Selalu paksa update kursor web berdasarkan apa yang ImGui butuhkan saat ini
            UpdateWebCursor(ImGui::GetMouseCursor());
        #endif
        // --- ORDER BOOK PANEL ---
        // Update symbol tracking dan render panel OB kalau aktif
        if (g_showOrderBook) {
            // Deteksi symbol berubah → request OB baru dari server
            static std::string s_lastOBSym = "";
            if (g_symbol != s_lastOBSym) {
                s_lastOBSym = g_symbol;
                #ifdef __EMSCRIPTEN__
                std::string _obReqSym = g_symbol;
                EM_ASM({
                    var s = UTF8ToString($0);
                    if (window.requestOrderBook) {
                        window.requestOrderBook(s);
                        console.log('[OB] Symbol switch → request OB: ' + s);
                    }
                }, _obReqSym.c_str());
                #endif
            }
            g_obSymbol = g_symbol; // ikut active symbol
            RenderOrderBookPanel();
        }

        // --- RENDERING ---
        ImGui::Render();
        int w, h; 
        glfwGetFramebufferSize(window, &w, &h);
        glViewport(0, 0, w, h);
        
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);

        // --- SYNC FILE SYSTEM ---
        #ifdef __EMSCRIPTEN__
        static double lastSync = 0;
        double now = emscripten_get_now();
        if (now - lastSync > 1500) { 
            EM_ASM(
                FS.syncfs(false, function(err) {
                    if (err) console.log("sync error", err);
                });
            );
            lastSync = now;
        }
        #endif
    };
    //panggilan gambar ikon
    InitIcons();
    printf("✅ Gambar berhasil diload ke GPU!\n");
    g_tradeSettingsUI.settingIconTex = texPopupSetting;

    // ── Set icon PNG ke registry simbol ──────────────────────────────────
    // Yang sudah ada:
    SymbolRegistry_SetIcon("XAUUSD",  texIconGold);
    SymbolRegistry_SetIcon("EURUSD",  texIconEuro);
    SymbolRegistry_SetIcon("GBPUSD",  texIconPound);
    SymbolRegistry_SetIcon("BTCUSDT", texIconBTC);
    SymbolRegistry_SetIcon("ETHUSDT", texIconETH);
    // TODO: uncomment setelah PNG dikumpulkan di folder assets/
    // SymbolRegistry_SetIcon("USDJPY",  texIconJPY);    // assets/jpy.png
    // SymbolRegistry_SetIcon("AUDUSD",  texIconAUD);    // assets/aud.png
    // SymbolRegistry_SetIcon("USDCAD",  texIconCAD);    // assets/cad.png
    // SymbolRegistry_SetIcon("NZDUSD",  texIconNZD);    // assets/nzd.png
    // SymbolRegistry_SetIcon("USDCHF",  texIconCHF);    // assets/chf.png
    // SymbolRegistry_SetIcon("XAGUSD",  texIconSilver); // assets/silver.png
    // SymbolRegistry_SetIcon("WTIUSD",  texIconOil);    // assets/oil.png
    // SymbolRegistry_SetIcon("SOLUSDT", texIconSOL);    // assets/sol.png
    // SymbolRegistry_SetIcon("BNBUSDT", texIconBNB);    // assets/bnb.png
    // SymbolRegistry_SetIcon("XRPUSDT", texIconXRP);    // assets/xrp.png
    // ─── Jangan lupa: declare ImTextureID baru di TextureHelper.h ────────
    // ============================================================
    // 🔥 SETUP MEMORI PERMANEN (IDBFS) - VERSI ANTI CRASH 🔥
    // ============================================================
    #ifdef __EMSCRIPTEN__
        // Kita pakai Javascript langsung (EM_ASM) agar bisa Try-Catch
        EM_ASM(
            // 1. Cek apakah folder /data sudah ada? Kalau belum, baru buat.
            try {
                var stat = FS.analyzePath('/data');
                if (!stat.exists) {
                    FS.mkdir('/data');
                    console.log("📁 Folder /data dibuat.");
                } else {
                    console.log("📁 Folder /data sudah ada.");
                }
            } catch(e) { console.log("Info Folder:", e); }

            // 2. Coba MOUNT. Kalau Resource Busy, kita abaikan (berarti sudah siap)
            try {
                FS.mount(IDBFS, {}, '/data');
                console.log("💾 IDBFS Mounted sukses.");
            } catch(e) {
                // INI RAHASIANYA: Kalau error "Busy", kita anggap sukses saja
                console.warn("⚠️ Mount Warning (Aman, diabaikan):", e.message);
            }

            // 3. SYNC (Load dari Harddisk ke RAM)
            FS.syncfs(true, function(err) {
                if (err) {
                    console.log("❌ Error Sync:", err);
                } else {
                    console.log("✅ IDBFS Siap! Melakukan Load Settings...");
                    // Panggil fungsi C++ LoadSettings() dengan aman
                    if (Module._LoadSettings) {
                        Module._LoadSettings(); 
                    } else {
                        console.log("⚠️ Fungsi _LoadSettings belum siap, skip.");
                    }
                }
            });
        );
    #else
        // Desktop Mode
        LoadSettings();
    #endif

    // ====== TRADE HISTORY: Init (live + replay terpisah) ======
    g_liveManager.history = &g_liveHistory;
    g_replayManager.history = &g_replayHistory;
    #ifdef __EMSCRIPTEN__
        g_liveHistory.LoadFromFile("/data/trade_history_live.json");
        g_replayHistory.LoadFromFile("/data/trade_history_replay.json");
    #else
        g_liveHistory.LoadFromFile("trade_history_live.json");
        g_replayHistory.LoadFromFile("trade_history_replay.json");
    #endif
    printf("[HISTORY] Trade History module initialized (live + replay).\n");

    // -----------------------------------------------------------
    // 🚀 START LOOP
    // -----------------------------------------------------------
#ifdef __EMSCRIPTEN__
    // 🌐 WEB MODE
    emscripten_set_main_loop_arg(RunLoop, NULL, 0, 1);
#else
    // 🖥️ DESKTOP MODE
    while (!glfwWindowShouldClose(window)) {
        LoopCallback();
    }
#endif

// ====== TRADE HISTORY: Save on exit (live + replay terpisah) ======
#ifdef __EMSCRIPTEN__
    g_liveHistory.SaveToFile("/data/trade_history_live.json");
    g_replayHistory.SaveToFile("/data/trade_history_replay.json");
    EM_ASM(FS.syncfs(false, function(err) {
        if(err) console.log("History save failed:", err);
        else console.log("History saved on exit!");
    }););
#else
    g_liveHistory.SaveToFile("trade_history_live.json");
    g_replayHistory.SaveToFile("trade_history_replay.json");
#endif

 // SAVE CACHE (Hanya akan efektif di Desktop, atau perlu sync IDBFS di Web)
    printf("💾 Menyimpan Cache ke Disk...\n");
    SaveCandlesToFile("M1", "candles_M1.cache");
    for(const auto& t : targets) {
      SaveCandlesToFile(t.first, "candles_" + t.first + ".cache");

    }
    TA_Shutdown();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}