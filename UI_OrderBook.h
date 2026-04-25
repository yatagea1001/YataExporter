#pragma once
#include "imgui.h"
#include <vector>
#include <string>
#include <map>
#include <algorithm>
#include <cmath>

// =============================================================================
// 📊 ORDER BOOK UI PANEL
//
// Menampilkan real-time L2 order book dari Hyperliquid.
// Data masuk via wasm_push_orderbook_level() → di-render tiap frame.
//
// Analogi "lampu sein":
//   Bid besar = support kuat   → bar hijau panjang
//   Ask besar = resistance kuat → bar merah panjang
//   Wall terdeteksi             → highlight khusus
//   Trader langsung tau tanpa perlu dijelaskan ✅
//
// Cara pakai:
//   1. Aktifkan dari Indicator List → g_showOrderBook = true
//   2. Data masuk dari JS via wasm_push_orderbook_level
//   3. Panggil RenderOrderBookPanel() di render loop
// =============================================================================

// ── Global state Order Book ─────────────────────────────────────────────────
struct OBLevel {
    float price;
    float size;
};

// Storage per symbol — update tiap kali data baru masuk dari JS
static std::map<std::string, std::vector<OBLevel>> g_obBids;  // sorted high→low
static std::map<std::string, std::vector<OBLevel>> g_obAsks;  // sorted low→high

// Show/hide flag — diset dari indicator list (global, satu panel OB)
bool g_showOrderBook = false;

// Symbol aktif untuk OB — diupdate dari main.cpp tiap frame
std::string g_obSymbol = "";

// Grouping step per symbol (seperti selector 0.1/1/10/100 di Binance)
// Aggregasi harga ke kelipatan step → level lebih sedikit, lebih mudah dibaca
static std::map<std::string, float> g_obGroupStep; // sym → step (default 1.0)

// ── OB Analytics (Imbalance + Signals mode) ──────────────────────────────────
struct OBTickSnapshot {
    double timestamp = 0;        // epoch ms (dari JS Date.now())
    float  imbalance = 0.0f;     // -1.0 (100% ask) s/d +1.0 (100% bid)
    float  rise_ratio_60 = 0.0f; // perubahan harga 60 snapshot terakhir
};

// Tick snapshot history per symbol (max ~1 jam)
static std::map<std::string, std::vector<OBTickSnapshot>> g_obTickHistory;
static constexpr int    OB_MAX_SNAP_HISTORY  = 3600;
static constexpr float OB_IMB_BUY_THRESHOLD  =  0.30f;  // > 0.30 = BUY PRESSURE
static constexpr float OB_IMB_SELL_THRESHOLD = -0.30f;  // < -0.30 = SELL PRESSURE
static constexpr float OB_RISE_THRESHOLD     =  0.001f; // > 0.001 = harga naik

// View mode state
enum class OBMode { Book, Imb, Sig };
static OBMode g_obMode = OBMode::Book;

// Helper: push snapshot (called from WASM bridge in main.cpp)
inline void OB_PushSnapshot(const std::string& sym, double ts,
    float imbalance, float rise60) {
    auto& hist = g_obTickHistory[sym];
    hist.push_back({ts, imbalance, rise60});
    if ((int)hist.size() > OB_MAX_SNAP_HISTORY)
        hist.erase(hist.begin(), hist.begin() + (int)hist.size() - OB_MAX_SNAP_HISTORY);
}

inline void OB_ClearSnapshot(const std::string& sym) {
    g_obTickHistory[sym].clear();
}

// Helper: snap harga ke kelipatan step
static inline float OB_SnapPrice(float price, float step) {
    if (step <= 0.0f) return price;
    return std::floor(price / step) * step;
}

// =============================================================================
// WASM BRIDGE — implementasi ada di main.cpp (extern "C" tidak boleh di .h)
// Deklarasi di sini agar RenderOrderBookPanel bisa dipanggil dari main.cpp
// =============================================================================
// Fungsi-fungsi ini diimplementasikan di main.cpp:
//   void wasm_clear_orderbook(const char* symbol)
//   void wasm_push_orderbook_level(const char* symbol, float price, float size, int isBid)
//
// Helper inline untuk clear/push dari dalam file ini kalau dibutuhkan
inline void OB_Clear(const std::string& sym) {
    g_obBids[sym].clear();
    g_obAsks[sym].clear();
}

inline void OB_PushLevel(const std::string& sym, float price, float size, bool isBid) {
    OBLevel lvl{ price, size };
    if (isBid) g_obBids[sym].push_back(lvl);
    else       g_obAsks[sym].push_back(lvl);
}

// =============================================================================
// RENDER PANEL
// Dipanggil dari main.cpp di render loop, di luar BeginPlot block
// =============================================================================
inline void RenderOrderBookPanel() {
    if (!g_showOrderBook) return;
    if (g_obSymbol.empty()) return;

    auto& bids = g_obBids[g_obSymbol];
    auto& asks = g_obAsks[g_obSymbol];
    bool hasData = !bids.empty() || !asks.empty();

    // ── Hitung total size untuk normalisasi bar ──────────────────────────────
    float maxSize = 0.0f;
    for (auto& b : bids) maxSize = std::max(maxSize, b.size);
    for (auto& a : asks) maxSize = std::max(maxSize, a.size);
    float wallThreshold_temp = maxSize * 0.85f; // pre-calc untuk avoid redef

    // ── Wall detection threshold (top 15% size = wall) ──────────────────────
    float wallThreshold = wallThreshold_temp;

    // ── Layout ───────────────────────────────────────────────────────────────
    const float PANEL_W  = 200.0f;
    const float ROW_H    = 18.0f;
    const float MAX_ROWS = 15;     // tampilkan max 15 bid + 15 ask

    // Hitung tinggi panel
    int nBids    = (int)std::min((float)bids.size(), MAX_ROWS);
    int nAsks    = (int)std::min((float)asks.size(), MAX_ROWS);
    float panelH = (nBids + nAsks + 2) * ROW_H + 50.0f; // +spread row +header

    // ── Window Order Book — posisi pertama kali di kanan, bebas dipindah ──────
    ImGuiIO& io = ImGui::GetIO();
    // Paksa posisi kanan atas — trader bisa pindah manual setelah itu
    ImGui::SetNextWindowPos(
        ImVec2(io.DisplaySize.x - PANEL_W - 10.0f, 60.0f),
        ImGuiCond_Appearing);  // set posisi setiap kali window muncul pertama kali
    ImGui::SetNextWindowSize(ImVec2(PANEL_W, std::max(panelH, 80.0f)), ImGuiCond_Appearing);
    ImGui::SetNextWindowBgAlpha(0.88f);

    // 🔥 Window bisa di-dock dan dipindah bebas
    // Tidak ada NoMove/NoResize → trader bisa resize sesuai kebutuhan
    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoScrollbar      |
        ImGuiWindowFlags_NoBringToFrontOnFocus;

    char winTitle[64];
    snprintf(winTitle, sizeof(winTitle), "Order Book##ob_%s", g_obSymbol.c_str());

    ImGui::PushStyleColor(ImGuiCol_WindowBg,    ImVec4(0.06f, 0.06f, 0.10f, 0.92f));
    ImGui::PushStyleColor(ImGuiCol_TitleBg,     ImVec4(0.10f, 0.10f, 0.18f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_TitleBgActive, ImVec4(0.12f, 0.12f, 0.22f, 1.00f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 5.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,  ImVec2(0, 0));

    if (ImGui::Begin(winTitle, &g_showOrderBook, flags)) {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 wPos   = ImGui::GetWindowPos();
        ImVec2 wSize  = ImGui::GetWindowSize();

        float fh   = ImGui::GetTextLineHeight();
        float barW = PANEL_W - 4.0f;   // lebar maksimal bar
        float curY = wPos.y + 26.0f;   // mulai di bawah title bar

        // ── Symbol badge + header kolom ─────────────────────────────────────
        ImGui::SetCursorScreenPos(ImVec2(wPos.x + 4, curY));
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "%s", g_obSymbol.c_str());
        ImGui::SameLine();
        ImGui::TextDisabled("| Price");
        ImGui::SameLine(PANEL_W * 0.6f);
        ImGui::TextDisabled("Size");
        curY += fh + 4.0f;

        // Kalau data belum ada → tampilkan status waiting
        if (!hasData) {
            dl->AddLine(ImVec2(wPos.x, curY), ImVec2(wPos.x + PANEL_W, curY),
                        IM_COL32(60, 60, 80, 200), 1.0f);
            curY += 4.0f;
            const char* waitMsg = "Waiting for data...";
            ImVec2 sz = ImGui::CalcTextSize(waitMsg);
            dl->AddText(ImVec2(wPos.x + (PANEL_W - sz.x) * 0.5f, curY + 8.0f),
                        IM_COL32(120, 120, 150, 200), waitMsg);
            ImGui::End();
            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor(3);
            return;
        }

        // separator
        dl->AddLine(ImVec2(wPos.x, curY), ImVec2(wPos.x + PANEL_W, curY),
                    IM_COL32(60, 60, 80, 200), 1.0f);
        curY += 2.0f;

        // ── ASK ROWS (dari atas) — sorted low→high, tampilkan terbalik ──────
        // Ask terdekat ke mid price di paling bawah section ask
        std::vector<OBLevel> topAsks(asks.begin(),
            asks.begin() + std::min((int)asks.size(), nAsks));
        // Render terbalik: yang terdekat mid di bawah
        for (int i = (int)topAsks.size() - 1; i >= 0; i--) {
            auto& a = topAsks[i];
            float ratio = a.size / maxSize;

            bool isWall = (a.size >= wallThreshold);

            // Background bar merah (ask = penjual)
            ImU32 barCol = isWall
                ? IM_COL32(180, 30, 30, 200)    // wall = merah terang
                : IM_COL32(120, 20, 20, 120);    // normal = merah redup

            float barLen = barW * ratio;
            dl->AddRectFilled(
                ImVec2(wPos.x + PANEL_W - barLen, curY),
                ImVec2(wPos.x + PANEL_W, curY + ROW_H - 1),
                barCol);

            // Wall highlight — border kuning
            if (isWall) {
                dl->AddRect(
                    ImVec2(wPos.x + PANEL_W - barLen, curY),
                    ImVec2(wPos.x + PANEL_W, curY + ROW_H - 1),
                    IM_COL32(255, 200, 50, 180), 0, 0, 1.0f);
            }

            // Teks harga
            char buf[32];
            if (a.price > 100.0f)
                snprintf(buf, sizeof(buf), "%.1f", a.price);
            else
                snprintf(buf, sizeof(buf), "%.5f", a.price);

            ImU32 txtCol = isWall
                ? IM_COL32(255, 150, 150, 255)
                : IM_COL32(220, 100, 100, 255);
            dl->AddText(ImVec2(wPos.x + 4, curY + (ROW_H - fh) * 0.5f), txtCol, buf);

            // Teks size
            char szBuf[24];
            if (a.size >= 1000.0f)
                snprintf(szBuf, sizeof(szBuf), "%.0fK", a.size / 1000.0f);
            else
                snprintf(szBuf, sizeof(szBuf), "%.2f", a.size);

            ImVec2 szText = ImGui::CalcTextSize(szBuf);
            dl->AddText(
                ImVec2(wPos.x + PANEL_W - szText.x - 4, curY + (ROW_H - fh) * 0.5f),
                txtCol, szBuf);

            curY += ROW_H;
        }

        // ── SPREAD ROW ───────────────────────────────────────────────────────
        if (!bids.empty() && !asks.empty()) {
            float bestBid = bids.front().price;
            float bestAsk = asks.front().price;
            float spread  = bestAsk - bestBid;

            dl->AddRectFilled(
                ImVec2(wPos.x, curY),
                ImVec2(wPos.x + PANEL_W, curY + ROW_H),
                IM_COL32(20, 20, 35, 220));

            char spreadBuf[48];
            if (bestBid > 100.0f)
                snprintf(spreadBuf, sizeof(spreadBuf), "Spread: %.1f", spread);
            else
                snprintf(spreadBuf, sizeof(spreadBuf), "Spread: %.5f", spread);

            ImVec2 spSz = ImGui::CalcTextSize(spreadBuf);
            dl->AddText(
                ImVec2(wPos.x + (PANEL_W - spSz.x) * 0.5f, curY + (ROW_H - fh) * 0.5f),
                IM_COL32(180, 180, 120, 255), spreadBuf);
            curY += ROW_H;
        }

        // ── BID ROWS — sorted high→low, bid terbaik di atas ─────────────────
        std::vector<OBLevel> topBids(bids.begin(),
            bids.begin() + std::min((int)bids.size(), nBids));

        for (auto& b : topBids) {
            float ratio = b.size / maxSize;
            bool isWall = (b.size >= wallThreshold);

            // Background bar hijau (bid = pembeli)
            ImU32 barCol = isWall
                ? IM_COL32(30, 160, 60, 200)
                : IM_COL32(20, 100, 40, 120);

            float barLen = barW * ratio;
            dl->AddRectFilled(
                ImVec2(wPos.x, curY),
                ImVec2(wPos.x + barLen, curY + ROW_H - 1),
                barCol);

            if (isWall) {
                dl->AddRect(
                    ImVec2(wPos.x, curY),
                    ImVec2(wPos.x + barLen, curY + ROW_H - 1),
                    IM_COL32(255, 200, 50, 180), 0, 0, 1.0f);
            }

            // Teks harga
            char buf[32];
            if (b.price > 100.0f)
                snprintf(buf, sizeof(buf), "%.1f", b.price);
            else
                snprintf(buf, sizeof(buf), "%.5f", b.price);

            ImU32 txtCol = isWall
                ? IM_COL32(100, 255, 140, 255)
                : IM_COL32(60, 200, 100, 255);
            dl->AddText(ImVec2(wPos.x + 4, curY + (ROW_H - fh) * 0.5f), txtCol, buf);

            // Teks size
            char szBuf[24];
            if (b.size >= 1000.0f)
                snprintf(szBuf, sizeof(szBuf), "%.0fK", b.size / 1000.0f);
            else
                snprintf(szBuf, sizeof(szBuf), "%.2f", b.size);

            ImVec2 szText = ImGui::CalcTextSize(szBuf);
            dl->AddText(
                ImVec2(wPos.x + PANEL_W - szText.x - 4, curY + (ROW_H - fh) * 0.5f),
                txtCol, szBuf);

            if (ImGui::IsMouseHoveringRect(
                ImVec2(wPos.x, curY),
                ImVec2(wPos.x + PANEL_W, curY + ROW_H))) {
                if (isWall)
                    ImGui::SetTooltip("⚠️ WALL: %.2f size — support kuat", b.size);
            }
            curY += ROW_H;
        }
    }
    ImGui::End();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(3);
}

// =============================================================================
// RENDER OB TAB CONTENT
// Dipanggil dari RenderSingleChartWindow saat tab->isOrderBookTab == true
// Render OB panel yang MENGISI seluruh tab window — bukan floating panel
// =============================================================================
// Forward declarations — didefinisi di bawah RenderOBTabContent
static inline void FormatTimeHMMS(char* buf, size_t sz, double timestamp_ms);
inline void RenderIMBList(const std::string& symbol, float startY, ImVec2 wPos, ImVec2 wSize);
inline void RenderSIGList(const std::string& symbol, float startY, ImVec2 wPos, ImVec2 wSize);

inline void RenderOBTabContent(const std::string& symbol) {
    if (symbol.empty()) return;

    auto& bids = g_obBids[symbol];
    auto& asks = g_obAsks[symbol];
    bool hasData = !bids.empty() || !asks.empty();

    ImDrawList* dl  = ImGui::GetWindowDrawList();
    ImVec2 wPos     = ImGui::GetWindowPos();
    ImVec2 wSize    = ImGui::GetWindowSize();

    const float ROW_H   = 22.0f;
    const float PAD     = 8.0f;
    float fh            = ImGui::GetTextLineHeight();
    float colW          = wSize.x - PAD * 2;

    float& groupStep = g_obGroupStep[symbol];
    if (groupStep <= 0.0f) groupStep = 1.0f;

    float curY = wPos.y + 26.0f;
    const ImU32 BG_HEADER = IM_COL32(14, 18, 30, 255);
    const ImU32 BG_SUBHDR = IM_COL32(10, 14, 24, 255);
    const ImU32 SEP_COL   = IM_COL32(32, 48, 72, 255);

    // ══════════════════════════════════════════════════════════════════════
    // ROW 0 — Mode Buttons [Book] [Imb] [Sig] — baris terpisah di atas
    // ══════════════════════════════════════════════════════════════════════
    {
        const float H     = 28.0f;
        const float BTN_H = 20.0f;
        dl->AddRectFilled(ImVec2(wPos.x,curY), ImVec2(wPos.x+wSize.x,curY+H), BG_SUBHDR);

        const char* modeLabels[] = {"Book", "Imb", "Sig"};
        const OBMode  modeVals[]  = {OBMode::Book, OBMode::Imb, OBMode::Sig};
        const float MBW = 44.0f; // button width
        const float MG  = 3.0f;  // gap
        const float totalModeW = 3 * MBW + 2 * MG;
        float modeStartX = wPos.x + (wSize.x - totalModeW) * 0.5f; // centered
        float modeY = curY + (H - BTN_H) * 0.5f;

        for (int m = 0; m < 3; m++) {
            bool active = (g_obMode == modeVals[m]);
            float bx = modeStartX + m * (MBW + MG);

            ImGui::PushStyleColor(ImGuiCol_Button,
                active ? ImVec4(0.08f, 0.38f, 0.72f, 0.95f)
                       : ImVec4(0.10f, 0.15f, 0.25f, 0.85f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                active ? ImVec4(0.10f, 0.42f, 0.78f, 1.0f)
                       : ImVec4(0.16f, 0.26f, 0.42f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                ImVec4(0.06f, 0.12f, 0.22f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Text,
                active ? ImVec4(1.0f, 1.0f, 1.0f, 1.0f)
                       : ImVec4(0.55f, 0.65f, 0.80f, 1.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));

            char mid[16];
            snprintf(mid, sizeof(mid), "%s##obm%d", modeLabels[m], m);
            ImGui::SetCursorScreenPos(ImVec2(bx, modeY));
            if (ImGui::Button(mid, ImVec2(MBW, BTN_H))) {
                g_obMode = modeVals[m];
            }
            if (ImGui::IsItemHovered()) {
                if (m == 0) ImGui::SetTooltip("Order book standar (bid/ask)");
                if (m == 1) ImGui::SetTooltip("Imbalance — tekanan beli/jual tiap detik");
                if (m == 2) ImGui::SetTooltip("Signals — kombinasi imbalance + momentum");
            }
            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor(4);
        }

        // Garis bawah mode bar
        dl->AddLine(ImVec2(wPos.x,curY+H), ImVec2(wPos.x+wSize.x,curY+H), SEP_COL);
        curY += H;
    }

    // ══════════════════════════════════════════════════════════════════════
    // ROW 1 — [Symbol]  [Lot Size popup]  [?]
    // ══════════════════════════════════════════════════════════════════════
    {
        const float H     = 28.0f;
        const float BTN_H = 20.0f;
        dl->AddRectFilled(ImVec2(wPos.x,curY), ImVec2(wPos.x+wSize.x,curY+H), BG_SUBHDR);

        // Symbol badge (kiri)
        {
            const char* sym = symbol.c_str();
            ImVec2 symSz = ImGui::CalcTextSize(sym);
            float badgeW = symSz.x + 12.0f;
            float badgeY = curY + (H - BTN_H) * 0.5f;
            dl->AddRectFilled(ImVec2(wPos.x+PAD, badgeY),
                              ImVec2(wPos.x+PAD+badgeW, badgeY+BTN_H),
                              IM_COL32(16,28,52,220), 3.0f);
            dl->AddRect(ImVec2(wPos.x+PAD, badgeY),
                        ImVec2(wPos.x+PAD+badgeW, badgeY+BTN_H),
                        IM_COL32(35,65,120,160), 3.0f, 0, 1.0f);
            dl->AddText(ImVec2(wPos.x+PAD+6, badgeY+(BTN_H-fh)*0.5f),
                        IM_COL32(150,195,255,255), sym);
        }

        // LOT button (tengah) — popup pilih grouping (Book mode only)
        if (g_obMode == OBMode::Book) {
            char lotLabel[32];
            if (groupStep < 1.0f)
                snprintf(lotLabel, sizeof(lotLabel), "Lot Size: 0.1");
            else
                snprintf(lotLabel, sizeof(lotLabel), "Lot Size: %.0f", groupStep);

            ImVec2 lotSz = ImGui::CalcTextSize(lotLabel);
            float lotW   = lotSz.x + 16.0f;
            float lotX   = wPos.x + (wSize.x - lotW) * 0.5f;
            float lotY   = curY + (H - BTN_H) * 0.5f;

            ImGui::SetCursorScreenPos(ImVec2(lotX, lotY));
            ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.10f,0.18f,0.32f,0.90f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.16f,0.30f,0.55f,1.00f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.08f,0.15f,0.28f,1.00f));
            ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(0.80f,0.92f,1.0f,1.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8,0));

            char lotBtnId[32]; snprintf(lotBtnId, sizeof(lotBtnId), "%s##lot_%s", lotLabel, symbol.c_str());
            if (ImGui::Button(lotBtnId, ImVec2(lotW, BTN_H)))
                ImGui::OpenPopup("##lot_popup");
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Grouping / aggregasi harga per level");

            ImGui::PopStyleVar(2); ImGui::PopStyleColor(4);

            // Popup lot selector
            ImGui::SetNextWindowPos(ImVec2(lotX, lotY + BTN_H + 2), ImGuiCond_Always);
            ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.07f,0.10f,0.17f,0.98f));
            ImGui::PushStyleColor(ImGuiCol_Border,  ImVec4(0.15f,0.25f,0.45f,0.9f));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 5.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,  ImVec2(6,6));
            if (ImGui::BeginPopup("##lot_popup")) {
                ImGui::Text("Kelompokkan per level:");
                ImGui::Separator();
                static const float  kSteps[]  = {0.1f, 1.0f, 10.0f, 100.0f};
                static const char*  kLabels[] = {"0.1", "1",  "10",  "100"};
                for (int i = 0; i < 4; i++) {
                    bool sel = (groupStep == kSteps[i]);
                    if (sel) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f,0.75f,1.0f,1.0f));
                    char sid[16]; snprintf(sid, sizeof(sid), "  %s##ls%d", kLabels[i], i);
                    if (ImGui::Selectable(sid, sel, 0, ImVec2(80,22))) {
                        groupStep = kSteps[i];
                        ImGui::CloseCurrentPopup();
                    }
                    if (sel) { ImGui::SameLine(); ImGui::TextDisabled(" ✓"); ImGui::PopStyleColor(); }
                }
                ImGui::EndPopup();
            }
            ImGui::PopStyleVar(2); ImGui::PopStyleColor(2);
        }

        // Tombol "?" (kanan)
        {
            const float BTN_SZ = 20.0f;
            float btnX = wPos.x + wSize.x - BTN_SZ - PAD;
            float btnY = curY + (H - BTN_SZ) * 0.5f;
            ImGui::SetCursorScreenPos(ImVec2(btnX, btnY));
            ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.10f,0.18f,0.35f,0.85f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.18f,0.36f,0.68f,1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.08f,0.14f,0.28f,1.0f));
            ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(0.55f,0.78f,1.0f,1.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, BTN_SZ * 0.5f);
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0,0));
            char qid[32]; snprintf(qid, sizeof(qid), "?##q_%s", symbol.c_str());
            if (ImGui::Button(qid, ImVec2(BTN_SZ, BTN_SZ))) {
                #ifdef __EMSCRIPTEN__
                float wx=wPos.x,wy=wPos.y,ww=wSize.x,wh=wSize.y;
                EM_ASM({ if(window.toggleOBTutorial) window.toggleOBTutorial($0,$1,$2,$3);
                },(int)wx,(int)wy,(int)ww,(int)wh);
                #endif
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Cara membaca Order Book");
            ImGui::PopStyleVar(2); ImGui::PopStyleColor(4);
        }

        dl->AddLine(ImVec2(wPos.x,curY+H), ImVec2(wPos.x+wSize.x,curY+H), SEP_COL);
        curY += H;
    }

    // ══════════════════════════════════════════════════════════════════════
    // ROW 3 — Kolom header: Price | Size | Sum
    // ══════════════════════════════════════════════════════════════════════
    {
        const float H = 19.0f;
        dl->AddRectFilled(ImVec2(wPos.x,curY), ImVec2(wPos.x+wSize.x,curY+H), IM_COL32(8,11,18,255));
        ImU32 CHC = IM_COL32(100, 130, 165, 230);

        // Price rata kiri
        dl->AddText(ImVec2(wPos.x + PAD, curY+(H-fh)*0.5f), CHC, "Price");
        // Size rata tengah
        const char* szLbl = "Size";
        ImVec2 szSzH = ImGui::CalcTextSize(szLbl);
        dl->AddText(ImVec2(wPos.x + wSize.x*0.60f - szSzH.x*0.5f, curY+(H-fh)*0.5f), CHC, szLbl);
        // Sum rata kanan
        const char* sumLbl = "Sum";
        ImVec2 sumSzH = ImGui::CalcTextSize(sumLbl);
        dl->AddText(ImVec2(wPos.x + wSize.x - sumSzH.x - PAD, curY+(H-fh)*0.5f), CHC, sumLbl);

        dl->AddLine(ImVec2(wPos.x,curY+H), ImVec2(wPos.x+wSize.x,curY+H), SEP_COL);
        curY += H;
    }

    if (!hasData) {
        const char* msg = "Waiting for Order Book data...";
        ImVec2 msz = ImGui::CalcTextSize(msg);
        dl->AddText(ImVec2(wPos.x + (wSize.x - msz.x)*0.5f,
                           wPos.y + wSize.y * 0.5f),
                    IM_COL32(100,100,130,200), msg);

        // Request lagi via JS kalau data masih kosong
        ImGui::SetCursorScreenPos(ImVec2(wPos.x + wSize.x*0.5f - 60, wPos.y + wSize.y*0.5f + 30));
        if (ImGui::Button("Request OB Data", ImVec2(120, 28))) {
            #ifdef __EMSCRIPTEN__
            std::string _sym = symbol;
            EM_ASM({
                var s = UTF8ToString($0);
                if (window.requestOrderBook) window.requestOrderBook(s);
            }, _sym.c_str());
            #endif
        }
        return;
    }

    // ── MODE DISPATCH ──────────────────────────────────────────────────
    // Imb & Sig mode punya render sendiri, Book mode lanjut kode di bawah
    if (g_obMode == OBMode::Imb) { RenderIMBList(symbol, curY, wPos, wSize); return; }
    if (g_obMode == OBMode::Sig) { RenderSIGList(symbol, curY, wPos, wSize); return; }

    // ── IMBALANCE BAR (Book mode only) ──────────────────────────────────
    // Hitung total volume semua bid vs semua ask
    // Imbalance = seberapa dominan satu sisi vs sisi lain
    // > 60% bid  → tekanan beli kuat  → harga cenderung naik
    // > 60% ask  → tekanan jual kuat  → harga cenderung turun
    // ─────────────────────────────────────────────────────────────────────
    {
        float totalBid = 0.0f, totalAsk = 0.0f;
        for (auto& b : bids) totalBid += b.size;
        for (auto& a : asks) totalAsk += a.size;
        float totalAll = totalBid + totalAsk;

        if (totalAll > 0.0f) {
            float bidPct = totalBid / totalAll; // 0.0 – 1.0
            float askPct = totalAsk / totalAll;

            const float IB_H    = 24.0f;  // tinggi imbalance bar
            const float IB_PAD  = 6.0f;
            float barW  = wSize.x - IB_PAD * 2;

            // Background row
            dl->AddRectFilled(
                ImVec2(wPos.x,          curY),
                ImVec2(wPos.x+wSize.x,  curY + IB_H),
                IM_COL32(12, 14, 20, 230));

            // Bar BID (hijau, dari kiri)
            float bidBarW = barW * bidPct;
            ImU32 bidBarCol = (bidPct > 0.60f)
                ? IM_COL32(30, 180, 70, 220)   // dominan → terang
                : IM_COL32(20, 110, 50, 160);   // normal
            dl->AddRectFilled(
                ImVec2(wPos.x + IB_PAD, curY + 4),
                ImVec2(wPos.x + IB_PAD + bidBarW, curY + IB_H - 4),
                bidBarCol, 3.0f);

            // Bar ASK (merah, dari kanan)
            float askBarW = barW * askPct;
            ImU32 askBarCol = (askPct > 0.60f)
                ? IM_COL32(200, 40, 40, 220)
                : IM_COL32(130, 25, 25, 160);
            dl->AddRectFilled(
                ImVec2(wPos.x + IB_PAD + bidBarW, curY + 4),
                ImVec2(wPos.x + IB_PAD + bidBarW + askBarW, curY + IB_H - 4),
                askBarCol, 3.0f);

            // Garis tengah (50/50)
            float midX = wPos.x + IB_PAD + barW * 0.5f;
            dl->AddLine(
                ImVec2(midX, curY + 2),
                ImVec2(midX, curY + IB_H - 2),
                IM_COL32(60, 60, 80, 200), 1.0f);

            // Label persentase kiri (BID)
            char bidPctBuf[16];
            snprintf(bidPctBuf, sizeof(bidPctBuf), "B %.0f%%", bidPct * 100.f);
            ImVec2 bidTSz = ImGui::CalcTextSize(bidPctBuf);
            ImU32  bidTCol = (bidPct > 0.60f)
                ? IM_COL32(80, 255, 130, 255)
                : IM_COL32(60, 180, 100, 200);
            dl->AddText(
                ImVec2(wPos.x + IB_PAD + 3, curY + (IB_H - fh) * 0.5f),
                bidTCol, bidPctBuf);

            // Label persentase kanan (ASK)
            char askPctBuf[16];
            snprintf(askPctBuf, sizeof(askPctBuf), "A %.0f%%", askPct * 100.f);
            ImVec2 askTSz = ImGui::CalcTextSize(askPctBuf);
            ImU32  askTCol = (askPct > 0.60f)
                ? IM_COL32(255, 100, 100, 255)
                : IM_COL32(200, 80, 80, 200);
            dl->AddText(
                ImVec2(wPos.x + wSize.x - askTSz.x - IB_PAD - 3,
                       curY + (IB_H - fh) * 0.5f),
                askTCol, askPctBuf);

            // Label dominasi di tengah
            const char* domLabel = nullptr;
            ImU32       domColor = 0;
            if (bidPct > 0.65f)      { domLabel = "BUY PRESSURE";   domColor = IM_COL32(50,220,100,255); }
            else if (askPct > 0.65f) { domLabel = "SELL PRESSURE";  domColor = IM_COL32(255,80,80,255);  }
            else if (bidPct > 0.55f) { domLabel = "BID DOMINAN";    domColor = IM_COL32(50,180,80,200);  }
            else if (askPct > 0.55f) { domLabel = "ASK DOMINAN";    domColor = IM_COL32(200,70,70,200);  }
            else                     { domLabel = "BALANCED";        domColor = IM_COL32(180,180,80,200); }

            if (domLabel) {
                ImVec2 domSz = ImGui::CalcTextSize(domLabel);
                dl->AddText(
                    ImVec2(wPos.x + (wSize.x - domSz.x) * 0.5f,
                           curY + (IB_H - fh) * 0.5f),
                    domColor, domLabel);
            }

            curY += IB_H + 2.0f;

            // Separator tipis setelah imbalance bar
            dl->AddLine(
                ImVec2(wPos.x, curY),
                ImVec2(wPos.x + wSize.x, curY),
                IM_COL32(30, 40, 55, 200), 1.0f);
            curY += 2.0f;

            // Reserve ruang ImGui agar tidak overlap dengan cursor
            ImGui::SetCursorScreenPos(ImVec2(wPos.x, curY));
            ImGui::Dummy(ImVec2(wSize.x, 0));
        }
    }

    // ── GROUPING — aggregate levels ke kelipatan groupStep ──────────────
    // contoh step=10: harga 66753..66762 semua digabung ke 66750
    auto aggAsks = [&]() -> std::vector<OBLevel> {
        std::map<float, float> m;
        for (auto& lvl : asks) { float k = OB_SnapPrice(lvl.price, groupStep); m[k] += lvl.size; }
        std::vector<OBLevel> r;
        for (auto& p : m) r.push_back({p.first, p.second});
        std::sort(r.begin(), r.end(), [](auto& a, auto& b){ return a.price < b.price; });
        return r;
    }();

    auto aggBids = [&]() -> std::vector<OBLevel> {
        std::map<float, float> m;
        for (auto& lvl : bids) { float k = OB_SnapPrice(lvl.price, groupStep); m[k] += lvl.size; }
        std::vector<OBLevel> r;
        for (auto& p : m) r.push_back({p.first, p.second});
        std::sort(r.begin(), r.end(), [](auto& a, auto& b){ return a.price > b.price; });
        return r;
    }();

    // ── Normalisasi CUMULATIF — bar naik progresif, rapi tidak ngacak ──────
    float maxSize = 0.0f;
    for (auto& b : aggBids) maxSize = std::max(maxSize, b.size);
    for (auto& a : aggAsks) maxSize = std::max(maxSize, a.size);
    float wallThreshold = maxSize * 0.85f;

    // Total size untuk SUM (cumulative)
    float totalBidSize = 0.0f, totalAskSize = 0.0f;
    for (auto& b : aggBids) totalBidSize += b.size;
    for (auto& a : aggAsks) totalAskSize += a.size;

    // Cumulative max untuk normalisasi bar (sisi terbesar = 100%)
    float cumMax = std::max(totalBidSize, totalAskSize);
    if (cumMax <= 0.0f) cumMax = 1.0f;

    // Hitung rows yang muat
    float available = wSize.y - (curY - wPos.y) - 22.0f - 10.0f;
    int maxRows = (int)(available / (ROW_H * 2.0f));
    maxRows = std::max(maxRows, 3);
    maxRows = std::min(maxRows, 20);

    int nAsks = (int)std::min((int)aggAsks.size(), maxRows);
    int nBids = (int)std::min((int)aggBids.size(), maxRows);

    // Pre-compute top levels (dipakai untuk cumulative ratio dan render)
    std::vector<OBLevel> topAsks(aggAsks.begin(), aggAsks.begin() + nAsks);
    std::vector<OBLevel> topBids(aggBids.begin(), aggBids.begin() + nBids);

    // Pre-compute cumulative ratios per level — rapi dan smooth
    std::vector<float> askCumRatio(nAsks, 0.0f);
    {
        float cum = 0.0f;
        for (int i = (int)topAsks.size() - 1; i >= 0; i--) {
            cum += topAsks[i].size;
            int ri = (int)topAsks.size() - 1 - i;
            if (ri < nAsks) askCumRatio[ri] = cum / cumMax;
        }
    }
    std::vector<float> bidCumRatio(nBids, 0.0f);
    {
        float cum = 0.0f;
        for (int i = 0; i < (int)topBids.size(); i++) {
            cum += topBids[i].size;
            bidCumRatio[i] = cum / cumMax;
        }
    }

    // Layout kolom:  [bar] | [Price 30%] | [Size 25%] | [Sum 20%]
    const float PRICE_X  = PAD;
    const float SIZE_CTR = wSize.x * 0.60f;
    const float SUM_R    = wSize.x - PAD;

    // ── ASK ROWS (merah, di atas, terdekat mid di bawah) ─────────────────
    // Bar pakai CUMULATIF ratio — semakin dekat mid, semakin lebar
    float askCumSum = totalAskSize;
    for (int i = (int)topAsks.size()-1; i >= 0; i--) {
        auto& a = topAsks[i];
        int ri = (int)topAsks.size() - 1 - i; // 0 = terdekat mid (display row index)
        float ratio  = (ri < nAsks) ? askCumRatio[ri] : 0.0f; // cumulatif!
        bool  isWall = (a.size >= wallThreshold);
        bool  hov    = ImGui::IsMouseHoveringRect(
            ImVec2(wPos.x, curY), ImVec2(wPos.x+wSize.x, curY+ROW_H));

        // Highlight hover row
        if (hov) dl->AddRectFilled(ImVec2(wPos.x,curY),
            ImVec2(wPos.x+wSize.x, curY+ROW_H), IM_COL32(255,80,80,15));

        // Bar cumulatif dari kanan — semakin dekat spread, semakin lebar
        float barLen = (wSize.x - PAD*2) * ratio;
        ImU32 barCol = isWall ? IM_COL32(160,25,25,200) : IM_COL32(100,15,15,90);
        dl->AddRectFilled(
            ImVec2(wPos.x + wSize.x - barLen, curY + 2),
            ImVec2(wPos.x + wSize.x,           curY + ROW_H - 2),
            barCol);
        // Bright edge di ujung bar (efek gradient halus)
        if (barLen > 6.0f) {
            ImU32 edgeCol = isWall ? IM_COL32(200,45,45,140) : IM_COL32(140,25,25,60);
            dl->AddRectFilled(
                ImVec2(wPos.x + wSize.x - barLen, curY + 2),
                ImVec2(wPos.x + wSize.x - barLen + 5.0f, curY + ROW_H - 2),
                edgeCol);
        }
        if (isWall)
            dl->AddRect(ImVec2(wPos.x+wSize.x-barLen, curY+2),
                        ImVec2(wPos.x+wSize.x, curY+ROW_H-2),
                        IM_COL32(255,200,50,160), 0, 0, 1.0f);

        ImU32 tc   = isWall ? IM_COL32(255,140,140,255) : IM_COL32(205,90,90,255);
        ImU32 tcDim= IM_COL32(160,70,70,200);

        // Price
        char pBuf[32];
        snprintf(pBuf, sizeof(pBuf), a.price > 100.f ? "%.1f" : "%.5f", a.price);
        // Price rata kiri
        dl->AddText(ImVec2(wPos.x + PRICE_X, curY+(ROW_H-fh)*0.5f), tc, pBuf);

        // Size rata tengah
        char sBuf[24];
        if (a.size >= 1e6f) snprintf(sBuf, sizeof(sBuf), "%.2fM", a.size/1e6f);
        else if (a.size >= 1000.f) snprintf(sBuf, sizeof(sBuf), "%.1fK", a.size/1000.f);
        else snprintf(sBuf, sizeof(sBuf), "%.3f", a.size);
        ImVec2 szSz = ImGui::CalcTextSize(sBuf);
        dl->AddText(ImVec2(wPos.x + SIZE_CTR - szSz.x*0.5f, curY+(ROW_H-fh)*0.5f), tc, sBuf);

        // SUM (cumulative dari atas)
        char sumBuf[24];
        if (askCumSum >= 1e6f) snprintf(sumBuf, sizeof(sumBuf), "%.2fM", askCumSum/1e6f);
        else if (askCumSum >= 1000.f) snprintf(sumBuf, sizeof(sumBuf), "%.1fK", askCumSum/1000.f);
        else snprintf(sumBuf, sizeof(sumBuf), "%.2f", askCumSum);
        ImVec2 sumSz = ImGui::CalcTextSize(sumBuf);
        // Sum rata kanan
        dl->AddText(ImVec2(wPos.x + SUM_R - sumSz.x, curY+(ROW_H-fh)*0.5f), tcDim, sumBuf);
        askCumSum -= a.size;

        if (hov && isWall) ImGui::SetTooltip("WALL ASK: %.3f  — resistance kuat", a.size);
        curY += ROW_H;
    }

    // ── SPREAD ROW ────────────────────────────────────────────────────────
    if (!aggBids.empty() && !aggAsks.empty()) {
        float bestBid   = aggBids.front().price;
        float bestAsk   = aggAsks.front().price;
        float spread    = bestAsk - bestBid;
        float spreadPct = (bestBid > 0) ? (spread / bestBid * 100.0f) : 0.0f;

        const float SP_H = 22.0f;
        dl->AddRectFilled(ImVec2(wPos.x, curY),
                          ImVec2(wPos.x+wSize.x, curY+SP_H),
                          IM_COL32(10, 12, 22, 250));
        dl->AddLine(ImVec2(wPos.x, curY),     ImVec2(wPos.x+wSize.x, curY),     IM_COL32(28,35,55,255));
        dl->AddLine(ImVec2(wPos.x, curY+SP_H),ImVec2(wPos.x+wSize.x, curY+SP_H),IM_COL32(28,35,55,255));

        char spBuf[48];
        if (bestBid > 100.0f)
            snprintf(spBuf, sizeof(spBuf), "Spread  %.1f  (%.4f%%)", spread, spreadPct);
        else
            snprintf(spBuf, sizeof(spBuf), "Spread  %.5f", spread);

        ImVec2 spSz = ImGui::CalcTextSize(spBuf);
        dl->AddText(ImVec2(wPos.x+(wSize.x-spSz.x)*0.5f, curY+(SP_H-fh)*0.5f),
                    IM_COL32(160,155,80,230), spBuf);
        curY += SP_H;
    }

    // ── BID ROWS (hijau, di bawah) ────────────────────────────────────────
    // Bar pakai CUMULATIF ratio — semakin dekat spread, semakin lebar
    float bidCumSum = 0.0f;
    for (int bi = 0; bi < (int)topBids.size(); bi++) {
        auto& b = topBids[bi];
        float ratio  = (bi < nBids) ? bidCumRatio[bi] : 0.0f; // cumulatif!
        bool  isWall = (b.size >= wallThreshold);
        bool  hov    = ImGui::IsMouseHoveringRect(
            ImVec2(wPos.x, curY), ImVec2(wPos.x+wSize.x, curY+ROW_H));

        if (hov) dl->AddRectFilled(ImVec2(wPos.x,curY),
            ImVec2(wPos.x+wSize.x, curY+ROW_H), IM_COL32(50,255,100,12));

        float barLen = (wSize.x - PAD*2) * ratio;
        ImU32 barCol = isWall ? IM_COL32(18,140,55,200) : IM_COL32(12,90,35,90);
        dl->AddRectFilled(
            ImVec2(wPos.x + wSize.x - barLen, curY + 2),
            ImVec2(wPos.x + wSize.x,           curY + ROW_H - 2),
            barCol);
        // Bright edge di ujung bar
        if (barLen > 6.0f) {
            ImU32 edgeCol = isWall ? IM_COL32(30,170,70,140) : IM_COL32(20,110,45,60);
            dl->AddRectFilled(
                ImVec2(wPos.x + wSize.x - barLen, curY + 2),
                ImVec2(wPos.x + wSize.x - barLen + 5.0f, curY + ROW_H - 2),
                edgeCol);
        }
        if (isWall)
            dl->AddRect(ImVec2(wPos.x+wSize.x-barLen, curY+2),
                        ImVec2(wPos.x+wSize.x,         curY+ROW_H-2),
                        IM_COL32(255,200,50,160), 0, 0, 1.0f);

        ImU32 tc    = isWall ? IM_COL32(90,255,130,255) : IM_COL32(50,185,90,255);
        ImU32 tcDim = IM_COL32(40,140,70,200);

        // Price
        char pBuf[32];
        snprintf(pBuf, sizeof(pBuf), b.price > 100.f ? "%.1f" : "%.5f", b.price);
        // Price rata kiri
        dl->AddText(ImVec2(wPos.x + PRICE_X, curY+(ROW_H-fh)*0.5f), tc, pBuf);

        // Size rata tengah
        char sBuf[24];
        if (b.size >= 1e6f) snprintf(sBuf, sizeof(sBuf), "%.2fM", b.size/1e6f);
        else if (b.size >= 1000.f) snprintf(sBuf, sizeof(sBuf), "%.1fK", b.size/1000.f);
        else snprintf(sBuf, sizeof(sBuf), "%.3f", b.size);
        ImVec2 szSz = ImGui::CalcTextSize(sBuf);
        dl->AddText(ImVec2(wPos.x + SIZE_CTR - szSz.x*0.5f, curY+(ROW_H-fh)*0.5f), tc, sBuf);

        // SUM (cumulative dari atas ke bawah)
        bidCumSum += b.size;
        char sumBuf[24];
        if (bidCumSum >= 1e6f) snprintf(sumBuf, sizeof(sumBuf), "%.2fM", bidCumSum/1e6f);
        else if (bidCumSum >= 1000.f) snprintf(sumBuf, sizeof(sumBuf), "%.1fK", bidCumSum/1000.f);
        else snprintf(sumBuf, sizeof(sumBuf), "%.2f", bidCumSum);
        ImVec2 sumSz = ImGui::CalcTextSize(sumBuf);
        // Sum rata kanan
        dl->AddText(ImVec2(wPos.x + SUM_R - sumSz.x, curY+(ROW_H-fh)*0.5f), tcDim, sumBuf);

        if (hov && isWall) ImGui::SetTooltip("WALL BID: %.3f  — support kuat", b.size);
        curY += ROW_H;
    }
}

// =============================================================================
// RENDER IMB LIST — Mode Imbalance
// Menampilkan riwayat snapshot: TIME | IMBALANCE | SIGNAL
// =============================================================================
static inline void FormatTimeHMMS(char* buf, size_t sz, double timestamp_ms) {
    // WASM-safe: hitung detik sejak tengah malam UTC tanpa gmtime
    long long ms = (long long)timestamp_ms;
    long long total_sec = ms / 1000;
    long long day_sec = ((total_sec % 86400) + 86400) % 86400;
    int h = (int)(day_sec / 3600);
    int m = (int)((day_sec % 3600) / 60);
    int s = (int)(day_sec % 60);
    snprintf(buf, sz, "%02d:%02d:%02d", h, m, s);
}

inline void RenderIMBList(const std::string& symbol, float startY,
                          ImVec2 wPos, ImVec2 wSize) {
    auto& hist = g_obTickHistory[symbol];
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float fh = ImGui::GetTextLineHeight();
    const float ROW_H = 24.0f;
    const float PAD   = 8.0f;
    float curY = startY;

    // ── Current imbalance gauge (mini bar) ──────────────────────────────
    {
        const float GH = 40.0f;
        dl->AddRectFilled(ImVec2(wPos.x, curY), ImVec2(wPos.x+wSize.x, curY+GH),
                          IM_COL32(10, 14, 24, 250));

        if (!hist.empty()) {
            float imb = hist.back().imbalance;
            float absImb = std::fabs(imb);
            float barW = (wSize.x - PAD*2) * 0.5f * std::min(absImb, 1.0f);

            // Background track
            float trackY = curY + 22.0f;
            float trackH = 8.0f;
            float midX = wPos.x + (wSize.x) * 0.5f;
            dl->AddRectFilled(ImVec2(wPos.x + PAD, trackY),
                              ImVec2(wPos.x + wSize.x - PAD, trackY + trackH),
                              IM_COL32(25, 28, 38, 200), 4.0f);

            // Colored bar
            if (imb >= 0) {
                dl->AddRectFilled(ImVec2(midX - barW, trackY),
                                  ImVec2(midX, trackY + trackH),
                                  IM_COL32(30, 160, 70, 200), 4.0f);
                // Bright tip
                dl->AddRectFilled(ImVec2(midX - barW * 0.3f, trackY),
                                  ImVec2(midX, trackY + trackH),
                                  absImb > 0.3f ? IM_COL32(50, 220, 100, 240)
                                               : IM_COL32(40, 180, 80, 200), 4.0f);
            } else {
                dl->AddRectFilled(ImVec2(midX, trackY),
                                  ImVec2(midX + barW, trackY + trackH),
                                  IM_COL32(160, 40, 40, 200), 4.0f);
                dl->AddRectFilled(ImVec2(midX, trackY),
                                  ImVec2(midX + barW * 0.3f, trackY + trackH),
                                  absImb > 0.3f ? IM_COL32(220, 60, 60, 240)
                                               : IM_COL32(180, 50, 50, 200), 4.0f);
            }

            // Center line (50/50)
            dl->AddLine(ImVec2(midX, trackY - 2), ImVec2(midX, trackY + trackH + 2),
                        IM_COL32(80, 80, 100, 200), 1.0f);

            // Labels
            char imbBuf[32];
            snprintf(imbBuf, sizeof(imbBuf), "IMB: %+.3f", imb);
            ImU32 imbCol = imb > 0.3f ? IM_COL32(50, 230, 110, 255)
                          : imb < -0.3f ? IM_COL32(240, 75, 75, 255)
                          : IM_COL32(160, 165, 180, 255);
            ImVec2 imbTSz = ImGui::CalcTextSize(imbBuf);
            dl->AddText(ImVec2(wPos.x + (wSize.x - imbTSz.x) * 0.5f, curY + 4), imbCol, imbBuf);

            // Side labels
            dl->AddText(ImVec2(wPos.x + PAD, trackY + trackH + 2),
                        IM_COL32(60, 180, 100, 180), "BID");
            const char* askLbl = "ASK";
            ImVec2 askTSz = ImGui::CalcTextSize(askLbl);
            dl->AddText(ImVec2(wPos.x + wSize.x - askTSz.x - PAD, trackY + trackH + 2),
                        IM_COL32(180, 60, 60, 180), askLbl);
        } else {
            const char* msg = "Accumulating data...";
            ImVec2 msz = ImGui::CalcTextSize(msg);
            dl->AddText(ImVec2(wPos.x + (wSize.x - msz.x)*0.5f, curY + 12),
                        IM_COL32(100,100,130,200), msg);
        }

        dl->AddLine(ImVec2(wPos.x, curY+GH), ImVec2(wPos.x+wSize.x, curY+GH),
                    IM_COL32(32, 48, 72, 255));
        curY += GH;
    }

    // ── Column header ───────────────────────────────────────────────────
    {
        const float HDR_H = 26.0f;
        dl->AddRectFilled(ImVec2(wPos.x, curY), ImVec2(wPos.x+wSize.x, curY+HDR_H),
                          IM_COL32(8, 11, 18, 255));
        ImU32 HC = IM_COL32(100, 130, 165, 230);

        dl->AddText(ImVec2(wPos.x + PAD, curY + (HDR_H-fh)*0.5f), HC, "TIME");

        const char* imbLbl = "IMBALANCE";
        ImVec2 imbSz = ImGui::CalcTextSize(imbLbl);
        dl->AddText(ImVec2(wPos.x + wSize.x*0.5f - imbSz.x*0.5f, curY + (HDR_H-fh)*0.5f), HC, imbLbl);

        const char* sigLbl = "SIGNAL";
        ImVec2 sigSz = ImGui::CalcTextSize(sigLbl);
        dl->AddText(ImVec2(wPos.x + wSize.x - sigSz.x - PAD, curY + (HDR_H-fh)*0.5f), HC, sigLbl);

        dl->AddLine(ImVec2(wPos.x, curY+HDR_H), ImVec2(wPos.x+wSize.x, curY+HDR_H),
                    IM_COL32(32, 48, 72, 255));
        curY += HDR_H;
    }

    // ── Data rows ───────────────────────────────────────────────────────
    if (hist.empty()) {
        const char* msg = "Waiting for analysis data...";
        ImVec2 msz = ImGui::CalcTextSize(msg);
        dl->AddText(ImVec2(wPos.x + (wSize.x - msz.x)*0.5f, curY + 30),
                    IM_COL32(100,100,130,200), msg);
        ImGui::SetCursorScreenPos(ImVec2(wPos.x, curY));
        ImGui::Dummy(ImVec2(wSize.x, 60));
        return;
    }

    int count = (int)std::min(hist.size(), (size_t)30);
    float totalListH = (float)count * ROW_H;

    for (int i = 0; i < count; i++) {
        int idx = (int)hist.size() - 1 - i;
        const auto& snap = hist[idx];
        float y = curY + (float)i * ROW_H;

        // Bounds check
        if (y + ROW_H > wPos.y + wSize.y - 10.0f) break;

        // Alternating row background
        ImU32 rowBg = (i % 2 == 0) ? IM_COL32(12, 14, 22, 255) : IM_COL32(16, 19, 30, 255);
        dl->AddRectFilled(ImVec2(wPos.x, y), ImVec2(wPos.x+wSize.x, y+ROW_H), rowBg);

        // Hover highlight
        if (ImGui::IsMouseHoveringRect(ImVec2(wPos.x, y), ImVec2(wPos.x+wSize.x, y+ROW_H)))
            dl->AddRectFilled(ImVec2(wPos.x, y), ImVec2(wPos.x+wSize.x, y+ROW_H),
                              IM_COL32(40, 50, 70, 60));

        // ── TIME ──
        char timeBuf[16];
        FormatTimeHMMS(timeBuf, sizeof(timeBuf), snap.timestamp);
        dl->AddText(ImVec2(wPos.x + PAD, y + (ROW_H-fh)*0.5f),
                    IM_COL32(120, 130, 155, 230), timeBuf);

        // ── IMBALANCE value ──
        char imbBuf[16];
        snprintf(imbBuf, sizeof(imbBuf), "%+.3f", snap.imbalance);
        ImU32 imbCol = snap.imbalance > OB_IMB_BUY_THRESHOLD  ? IM_COL32(50, 220, 110, 255)
                      : snap.imbalance < OB_IMB_SELL_THRESHOLD ? IM_COL32(230, 80, 80, 255)
                      : IM_COL32(140, 145, 160, 230);
        ImVec2 imbTSz = ImGui::CalcTextSize(imbBuf);
        dl->AddText(ImVec2(wPos.x + wSize.x*0.5f - imbTSz.x*0.5f, y + (ROW_H-fh)*0.5f), imbCol, imbBuf);

        // ── SIGNAL ──
        const char* signal = "NEUTRAL";
        ImU32 sigCol = IM_COL32(130, 135, 155, 200);
        if (snap.imbalance > OB_IMB_BUY_THRESHOLD)  { signal = "BUY \xe2\x96\xb2";  sigCol = IM_COL32(50, 220, 110, 255); }
        else if (snap.imbalance < OB_IMB_SELL_THRESHOLD) { signal = "SELL \xe2\x96\xbc"; sigCol = IM_COL32(230, 80, 80, 255); }

        ImVec2 sigTSz = ImGui::CalcTextSize(signal);
        dl->AddText(ImVec2(wPos.x + wSize.x - sigTSz.x - PAD, y + (ROW_H-fh)*0.5f), sigCol, signal);
    }

    ImGui::SetCursorScreenPos(ImVec2(wPos.x, curY + totalListH));
    ImGui::Dummy(ImVec2(wSize.x, totalListH));
}

// =============================================================================
// RENDER SIG LIST — Mode Signals
// Menampilkan riwayat snapshot: TIME | RISE% | ACTION
// Logika: kombinasi imbalance (tekanan order) + momentum harga
// =============================================================================
inline void RenderSIGList(const std::string& symbol, float startY,
                          ImVec2 wPos, ImVec2 wSize) {
    auto& hist = g_obTickHistory[symbol];
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float fh = ImGui::GetTextLineHeight();
    const float ROW_H = 24.0f;
    const float PAD   = 8.0f;
    float curY = startY;

    // ── Latest signal summary card ──────────────────────────────────────
    {
        const float SH = 44.0f;
        dl->AddRectFilled(ImVec2(wPos.x, curY), ImVec2(wPos.x+wSize.x, curY+SH),
                          IM_COL32(10, 14, 24, 250));

        if (!hist.empty()) {
            const auto& snap = hist.back();
            // Determine latest action
            const char* action = "HOLD";
            ImU32 actCol = IM_COL32(140, 145, 160, 230);
            if (snap.imbalance > OB_IMB_BUY_THRESHOLD && snap.rise_ratio_60 > OB_RISE_THRESHOLD) {
                action = "STRONG BUY";  actCol = IM_COL32(40, 235, 120, 255);
            } else if (snap.imbalance < OB_IMB_SELL_THRESHOLD && snap.rise_ratio_60 < -OB_RISE_THRESHOLD) {
                action = "STRONG SELL"; actCol = IM_COL32(240, 55, 55, 255);
            } else if (snap.imbalance > OB_IMB_BUY_THRESHOLD) {
                action = "BUY";         actCol = IM_COL32(50, 200, 100, 255);
            } else if (snap.imbalance < OB_IMB_SELL_THRESHOLD) {
                action = "SELL";        actCol = IM_COL32(220, 75, 75, 255);
            }

            // Latest signal label
            char riseBuf[24];
            snprintf(riseBuf, sizeof(riseBuf), "60s: %+.2f%%", snap.rise_ratio_60 * 100.0f);
            ImU32 riseCol = snap.rise_ratio_60 > 0 ? IM_COL32(50, 200, 100, 200)
                           : snap.rise_ratio_60 < 0 ? IM_COL32(200, 70, 70, 200)
                           : IM_COL32(130, 130, 150, 200);
            ImVec2 riseSz = ImGui::CalcTextSize(riseBuf);
            dl->AddText(ImVec2(wPos.x + PAD, curY + 4), riseCol, riseBuf);

            ImVec2 actSz = ImGui::CalcTextSize(action);
            dl->AddText(ImVec2(wPos.x + wSize.x - actSz.x - PAD, curY + 4), actCol, action);

            // Latest action big text
            dl->AddText(ImVec2(wPos.x + (wSize.x - actSz.x)*0.5f, curY + 22), actCol, action);
        } else {
            const char* msg = "Accumulating data...";
            ImVec2 msz = ImGui::CalcTextSize(msg);
            dl->AddText(ImVec2(wPos.x + (wSize.x - msz.x)*0.5f, curY + 12),
                        IM_COL32(100,100,130,200), msg);
        }

        dl->AddLine(ImVec2(wPos.x, curY+SH), ImVec2(wPos.x+wSize.x, curY+SH),
                    IM_COL32(32, 48, 72, 255));
        curY += SH;
    }

    // ── Column header ───────────────────────────────────────────────────
    {
        const float HDR_H = 26.0f;
        dl->AddRectFilled(ImVec2(wPos.x, curY), ImVec2(wPos.x+wSize.x, curY+HDR_H),
                          IM_COL32(8, 11, 18, 255));
        ImU32 HC = IM_COL32(100, 130, 165, 230);

        dl->AddText(ImVec2(wPos.x + PAD, curY + (HDR_H-fh)*0.5f), HC, "TIME");

        const char* riseLbl = "RISE%";
        ImVec2 riseSz = ImGui::CalcTextSize(riseLbl);
        dl->AddText(ImVec2(wPos.x + wSize.x*0.5f - riseSz.x*0.5f, curY + (HDR_H-fh)*0.5f), HC, riseLbl);

        const char* actLbl = "ACTION";
        ImVec2 actTSz = ImGui::CalcTextSize(actLbl);
        dl->AddText(ImVec2(wPos.x + wSize.x - actTSz.x - PAD, curY + (HDR_H-fh)*0.5f), HC, actLbl);

        dl->AddLine(ImVec2(wPos.x, curY+HDR_H), ImVec2(wPos.x+wSize.x, curY+HDR_H),
                    IM_COL32(32, 48, 72, 255));
        curY += HDR_H;
    }

    // ── Data rows ───────────────────────────────────────────────────────
    if (hist.empty()) {
        const char* msg = "Waiting for analysis data...";
        ImVec2 msz = ImGui::CalcTextSize(msg);
        dl->AddText(ImVec2(wPos.x + (wSize.x - msz.x)*0.5f, curY + 30),
                    IM_COL32(100,100,130,200), msg);
        ImGui::SetCursorScreenPos(ImVec2(wPos.x, curY));
        ImGui::Dummy(ImVec2(wSize.x, 60));
        return;
    }

    int count = (int)std::min(hist.size(), (size_t)30);
    float totalListH = (float)count * ROW_H;

    for (int i = 0; i < count; i++) {
        int idx = (int)hist.size() - 1 - i;
        const auto& snap = hist[idx];
        float y = curY + (float)i * ROW_H;

        if (y + ROW_H > wPos.y + wSize.y - 10.0f) break;

        // Alternating row background
        ImU32 rowBg = (i % 2 == 0) ? IM_COL32(12, 14, 22, 255) : IM_COL32(16, 19, 30, 255);
        dl->AddRectFilled(ImVec2(wPos.x, y), ImVec2(wPos.x+wSize.x, y+ROW_H), rowBg);

        // Hover highlight
        if (ImGui::IsMouseHoveringRect(ImVec2(wPos.x, y), ImVec2(wPos.x+wSize.x, y+ROW_H)))
            dl->AddRectFilled(ImVec2(wPos.x, y), ImVec2(wPos.x+wSize.x, y+ROW_H),
                              IM_COL32(40, 50, 70, 60));

        // ── TIME ──
        char timeBuf[16];
        FormatTimeHMMS(timeBuf, sizeof(timeBuf), snap.timestamp);
        dl->AddText(ImVec2(wPos.x + PAD, y + (ROW_H-fh)*0.5f),
                    IM_COL32(120, 130, 155, 230), timeBuf);

        // ── RISE% ──
        char riseBuf[20];
        snprintf(riseBuf, sizeof(riseBuf), "%+.2f%%", snap.rise_ratio_60 * 100.0f);
        ImU32 riseCol = snap.rise_ratio_60 > OB_RISE_THRESHOLD  ? IM_COL32(50, 210, 100, 255)
                      : snap.rise_ratio_60 < -OB_RISE_THRESHOLD ? IM_COL32(220, 75, 75, 255)
                      : IM_COL32(130, 135, 150, 220);
        ImVec2 riseSz = ImGui::CalcTextSize(riseBuf);
        dl->AddText(ImVec2(wPos.x + wSize.x*0.5f - riseSz.x*0.5f, y + (ROW_H-fh)*0.5f), riseCol, riseBuf);

        // ── ACTION (combined: imbalance + momentum) ──
        const char* action = "HOLD";
        ImU32 actCol = IM_COL32(130, 135, 155, 200);
        if (snap.imbalance > OB_IMB_BUY_THRESHOLD && snap.rise_ratio_60 > OB_RISE_THRESHOLD) {
            action = "STRONG BUY";  actCol = IM_COL32(40, 235, 120, 255);
        } else if (snap.imbalance < OB_IMB_SELL_THRESHOLD && snap.rise_ratio_60 < -OB_RISE_THRESHOLD) {
            action = "STRONG SELL"; actCol = IM_COL32(240, 55, 55, 255);
        } else if (snap.imbalance > OB_IMB_BUY_THRESHOLD) {
            action = "BUY";         actCol = IM_COL32(50, 200, 100, 255);
        } else if (snap.imbalance < OB_IMB_SELL_THRESHOLD) {
            action = "SELL";        actCol = IM_COL32(220, 75, 75, 255);
        }

        ImVec2 actSz = ImGui::CalcTextSize(action);
        dl->AddText(ImVec2(wPos.x + wSize.x - actSz.x - PAD, y + (ROW_H-fh)*0.5f), actCol, action);
    }

    ImGui::SetCursorScreenPos(ImVec2(wPos.x, curY + totalListH));
    ImGui::Dummy(ImVec2(wSize.x, totalListH));
}
