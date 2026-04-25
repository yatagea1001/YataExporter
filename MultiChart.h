#pragma once
#include <vector>
#include <string>
#include <array>
#include <map>
#include "imgui.h"
#include "Candle.h"
#include "Indicators.h"
#include "GlobalShapeManager.h"
#include "GPUCandleRenderer.h"   // ← tiap tab punya renderer sendiri
#include "OrderFlowRenderer.h"  // ← tiap tab punya OF renderer sendiri
#include "CrosshairRenderer.h"  // ← tiap tab punya crosshair state sendiri

enum CandleRenderStyle {
    RENDER_CANDLE,
    RENDER_LINE,
    RENDER_AREA,
    // ── ORDER FLOW / FOOTPRINT ──────────────────────────────────
    // Candle + footprint overlay (bid/ask boxes)
    RENDER_FP_OVERLAY,
    // Footprint profile (candle tipis + grid bid/ask per level)
    RENDER_FP_PROFILE,
    // Footprint bar (proportional bar kiri=sell, kanan=buy)
    // Paling informatif, default utama untuk order flow
    RENDER_FP_BAR,

    RENDER_STYLE_COUNT  // sentinel untuk cycling
};

static inline const char* CandleStyleName(CandleRenderStyle s) {
    switch(s) {
        case RENDER_CANDLE:     return "Candle";
        case RENDER_LINE:       return "Line";
        case RENDER_AREA:       return "Area";
        case RENDER_FP_OVERLAY: return "FP Overlay";
        case RENDER_FP_PROFILE: return "FP Profile";
        case RENDER_FP_BAR:     return "FP Bar";
        default:                return "Candle";
    }
}

// Apakah style ini mode order flow?
static inline bool IsFootprintStyle(CandleRenderStyle s) {
    return s == RENDER_FP_OVERLAY
        || s == RENDER_FP_PROFILE
        || s == RENDER_FP_BAR;
}

// ─────────────────────────────────────────────
// State zoom/pan independen per-tab
// ─────────────────────────────────────────────
struct ChartTabState {
    double y_min=0, y_max=0;
    bool   autoFitY=true;
    float  zoomLevel=150.f, targetZoom=150.f;
    int    viewCenterIndex=0;
    double dragAccumulator=0;
    float  inertiaVelocity=0;
    bool   isInertiaActive=false;
    bool   isResizingY=false, isPanConfirmed=false, lastTouchState=false;
    ImVec2 panStartPos={0,0};

    // ── GoToLive per-tab ──────────────────────────────────────
    // Setiap tab punya animasi sendiri → multi-tab semua bisa go live
    bool   isAnimatingToLive = false; // sedang smooth-lerp ke candle live
    double animFloatingIndex = 0.0;   // posisi floating animasi (sub-candle)
    ImVec2 savedPlotPos      = {0.f, 0.f}; // disimpan saat BeginPlot
    ImVec2 savedPlotSize     = {0.f, 0.f}; // disimpan saat BeginPlot
    bool   plotBoundsReady   = false;       // true setelah frame pertama render
};

// ─────────────────────────────────────────────
// Satu tab chart
// ─────────────────────────────────────────────
struct ChartTab {
    int         id        = -1;
    std::string symbol    = "XAUUSD";
    std::string timeframe = "M1";
    std::string label     = "XAUUSD M1";

    // true  = pakai g_allCandles (tab utama / default)
    // false = punya data sendiri (tab tambahan)
    bool usesGlobalData   = true;
    // true = tab extra yang dibuat saat replay (dihapus otomatis saat exit replay)
    // Data source: g_allCandles[tf] langsung (bukan SYMBOL_TF)
    // limitIndex: g_tfIndices[tf] — ikut sync replay engine
    bool isReplayExtraTab  = false;

    // 📖 ORDER BOOK TAB — tab khusus tampilkan L2 OB
    // Tidak ada candle, tidak ada chart — hanya OB panel full-window
    // Symbol: ikut tab->symbol (bisa berbeda tiap tab)
    // TF: "OB" (tidak relevan)
    bool isOrderBookTab    = false;

    std::vector<Indicator*> indicators;
    GlobalShapeManager      shapes;
    ChartTabState           state;       // zoom/pan sendiri
    CrosshairState          crosshair;   // crosshair state per-tab

    // 🔥 GPU RENDERER PER-TAB
    // Setiap tab punya VBO sendiri → candle M15 tidak tumpuk H4
    GPUCandleRenderer gpuRenderer;
    bool gpuInitialized = false;

    // 🔥 ORDER FLOW RENDERER PER-TAB (analogi GPUCandleRenderer)
    // Setiap tab punya instance sendiri dengan symbol & tickSize yang benar.
    // Tab BTCUSDT M15 tidak akan pakai tickSize XAUUSD, dan sebaliknya.
    // fpZoom per-tab (Ctrl+Scroll) juga disimpan di sini, bukan di ChartTab.
    OrderFlowRenderer orderFlowRenderer;

    CandleRenderStyle renderStyle = RENDER_CANDLE;
    ImVec4 lineColor = {0.2f, 0.7f, 1.f, 1.f};

    // 🔥 VP OVERLAY — layer tambahan, independen dari renderStyle
    // Bisa aktif di atas SEMUA style: Candle, FP Bar, FP Profile, dll.
    // Toggle via tombol "VP" di toolbar — amber kalau ON.
    // Render: DrawVolumeProfile() dipanggil SETELAH switch(renderStyle)
    bool showVolumeProfile = false;

    // ── SMART SYNC (Zero Copy Per Frame) ──────────────────────
    // Buffer candle milik tab ini — tidak di-copy ulang tiap frame
    // Hanya update selisih yang berubah (lihat RenderSingleChartWindow)
    std::vector<Candle> localCandles;
    std::string         lastTF = "";   // deteksi TF berubah → copy full

    // ── REPLAY ANIMASI PER-TAB ─────────────────────────────────
    // Masing-masing tab track high/low candle live-nya sendiri
    // Semua tab animate saat replay (bukan hanya tab utama)
    double replayMemHigh  = 0;
    double replayMemLow   = 0;
    double replayLastTime = 0;

    bool isOpen    = true;
    bool isLoading = false;

    // 🔥 PER-TAB LAZY LOAD STATE
    // lazyPending  = true saat C++ sudah trigger JS, tunggu JS selesai rebuild
    // noMoreHistory= true saat server konfirmasi sudah tidak ada data lebih lama
    //                → tidak perlu trigger lazy lagi untuk symbol ini
    bool lazyPending    = false;
    bool noMoreHistory  = false;

    // ── DOCKING MEMORY ─────────────────────────────────────
    // Saat symbol/TF berubah → judul window berubah → ImGui anggap window baru.
    // Kita simpan posisi & ukuran lama, lalu restore di frame berikutnya.
    ImVec2 savedPos          = {-1.f, -1.f}; // -1 = belum ada
    ImVec2 savedSize         = {600.f, 400.f};
    bool   pendingRestore    = false;         // true → SetNextWindowPos dengan Always

    // ── DOCK ID MEMORY ────────────────────────────────────
    // Saat symbol/TF berubah → nama window beda → ImGui kehilangan DockId.
    // Kita simpan DockId sebelum perubahan, lalu SetNextWindowDockID() restore.
    unsigned int savedDockId = 0;             // 0 = belum ada / tidak dock
    bool         pendingDockRestore = false;   // true → SetNextWindowDockID di frame berikutnya

    void UpdateLabel() {
        if (isOrderBookTab)
            label = symbol + " | OB";
        else
            label = symbol + " | " + timeframe;
    }

    void ClearIndicators() {
        for (auto* i : indicators) delete i;
        indicators.clear();
    }

    // Init GPU renderer tab ini (panggil 1x)
    void InitGPU() {
        if (!gpuInitialized) {
            gpuRenderer.Init();
            gpuInitialized = true;
        }
    }

    // Init OrderFlow renderer dengan symbol tab ini
    // Panggil setelah symbol diset (di AddTab atau LoadTabSymbol)
    void InitOrderFlow() {
        orderFlowRenderer.SetSymbol(symbol);
    }

    // Sinkron warna dari global (biar tema berlaku ke semua tab)
    void SyncColors(ImVec4 bull, ImVec4 bear) {
        gpuRenderer.colorBull     = bull;
        gpuRenderer.colorBear     = bear;
        gpuRenderer.wickColorBull = bull;
        gpuRenderer.wickColorBear = bear;
    }

    ~ChartTab() {
        ClearIndicators();
        gpuRenderer.Shutdown();
    }
};

// ─────────────────────────────────────────────
// Manager
// ─────────────────────────────────────────────
class MultiChartManager {
public:
    std::vector<ChartTab*> tabs;
    int activeTabId = -1, nextId = 0;

    ChartTab* AddTab(const std::string& sym, const std::string& tf,
                     bool usesGlobal = true) {
        ChartTab* t   = new ChartTab();
        t->id         = nextId++;
        t->symbol     = sym;
        t->timeframe  = tf;
        t->usesGlobalData = usesGlobal;
        t->UpdateLabel();
        tabs.push_back(t);
        activeTabId = t->id;
        return t;
    }

    void RemoveTab(int id) {
        for (int i = 0; i < (int)tabs.size(); i++) {
            if (tabs[i]->id == id) {
                delete tabs[i];
                tabs.erase(tabs.begin() + i);
                if (activeTabId == id)
                    activeTabId = tabs.empty() ? -1 : tabs[std::max(0,i-1)]->id;
                return;
            }
        }
    }

    ChartTab* GetActiveTab() {
        for (auto* t : tabs) if (t->id == activeTabId) return t;
        return nullptr;
    }

    ChartTab* GetById(int id) {
        for (auto* t : tabs) if (t->id == id) return t;
        return nullptr;
    }

    bool IsEmpty() const { return tabs.empty(); }

    ~MultiChartManager() { for (auto* t : tabs) delete t; }
};

extern MultiChartManager g_chartManager;
extern ChartTab*         g_activeChart;
