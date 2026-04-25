#pragma once
// ================================================================
// SymbolPickerUI.h  —  Symbol + Timeframe Picker Terpusat  (V2)
// ================================================================
//
// V2 PERUBAHAN UTAMA:
//   • Slide 1 : Pilih Simbol  (logo per simbol, search, filter tab)
//   • Slide 2 : Pilih Timeframe + Ringkasan + Konfirmasi / Batal
//   • Callback sekarang: void(const std::string& sym, const std::string& tf)
//     *** BREAKING CHANGE dari V1 — lihat bagian MIGRASI di bawah ***
//
// ────────────────────────────────────────────────────────────────
// CARA PAKAI  (sama seperti dulu, callback ditambah parameter tf)
// ────────────────────────────────────────────────────────────────
//
//   // Buka picker, opsional pre-select TF dari tab aktif
//   SymbolPicker::Open("ctx_navbar", activeTab->timeframe.c_str());
//
//   // Di render loop  (tiap frame)
//   SymbolPicker::Render([](const std::string& sym, const std::string& tf) {
//       SwitchSymbolAndTF(sym, tf);
//   });
//
// ────────────────────────────────────────────────────────────────
// MIGRASI DARI V1
// ────────────────────────────────────────────────────────────────
//
//   V1 callback:  [](const std::string& sym)           { ... }
//   V2 callback:  [](const std::string& sym, const std::string& tf) { ... }
//
//   Cari semua SymbolPicker::Render( di main.cpp dan tambahkan
//   parameter tf ke lambda, lalu ganti hardcoded "M15" dengan tf.
//   Lihat komentar di main.cpp untuk patch per call-site.
//
// ────────────────────────────────────────────────────────────────
// CARA TAMBAH SIMBOL BARU
// ────────────────────────────────────────────────────────────────
//   Tambah entry di g_symbolRegistry di bawah.
//   Setelah assets PNG siap, set icon di main.cpp setelah InitIcons():
//     SymbolRegistry_SetIcon("BTCUSDT", texIconBTC);
//
// CARA TAMBAH TIMEFRAME BARU
//   Edit kSP_TFs[] di bawah.
//   Lakukan hal yang sama di kTFs[] dalam UI_ChartTabs.h agar sync.
// ================================================================

#include "imgui.h"
#include <string>
#include <vector>
#include <functional>
#include <algorithm>
#include <cstring>
#include <cstdio>

// ── Live price dari MarketWatch ──────────────────────────────────
#include "MarketWatchPanel.h"
extern MarketWatchPanel g_marketWatch;

// ── Logo YATA di header picker ───────────────────────────────────
// texIconSymbol diload dari assets/simbol.png via InitIcons() di main.cpp.
// Kalau nanti mau pakai logo khusus YATA, ganti ke texYataLogo di sini.
// Kalau 0 (belum diload) → fallback kotak biru bertulisan "Y".
extern ImTextureID texIconSymbol;

// ─────────────────────────────────────────────────────────────────
//  SYMBOL REGISTRY
// ─────────────────────────────────────────────────────────────────
enum class SymbolCategory { ALL, FOREX, CRYPTO, COMMODITY, INDEX };

struct SymbolInfo {
    std::string    id;           // Key internal: "XAUUSD"
    std::string    displayName;  // Tampilan: "XAU/USD"
    std::string    description;  // "Gold vs US Dollar"
    SymbolCategory category;
    const char*    emoji;        // Fallback sementara sampai PNG siap
    ImTextureID    icon = 0;     // Set via SymbolRegistry_SetIcon() di main.cpp
    float          tickSize  = 0.01f;
    bool           isCrypto  = false;
    bool           isFavorite= false;
};

// ─────────────────────────────────────────────────────────────────
//  🔥 TAMBAH SIMBOL BARU DI SINI
//  Urutan = urutan tampil di UI
// ─────────────────────────────────────────────────────────────────
inline std::vector<SymbolInfo> g_symbolRegistry = {

    // ── COMMODITY ────────────────────────────────────────────────
    // TODO: SymbolRegistry_SetIcon("XAUUSD",  texIconGold);   // assets/gold.png
    // TODO: SymbolRegistry_SetIcon("XAGUSD",  texIconSilver); // assets/silver.png  ← buat file ini
    { "XAUUSD", "XAU/USD", "Gold vs US Dollar",   SymbolCategory::COMMODITY, "🥇", 0, 0.01f,   false },
    { "XAGUSD", "XAG/USD", "Silver vs US Dollar", SymbolCategory::COMMODITY, "🥈", 0, 0.001f,  false },
    { "WTIUSD", "WTI/USD", "Crude Oil vs USD",    SymbolCategory::COMMODITY, "🛢",  0, 0.01f,   false },

    // ── FOREX ────────────────────────────────────────────────────
    // TODO: SymbolRegistry_SetIcon("EURUSD",  texIconEuro);    // assets/euro.png
    // TODO: SymbolRegistry_SetIcon("GBPUSD",  texIconPound);   // assets/pound.png
    // TODO: SymbolRegistry_SetIcon("USDJPY",  texIconJPY);     // assets/jpy.png   ← buat file ini
    // TODO: SymbolRegistry_SetIcon("AUDUSD",  texIconAUD);     // assets/aud.png   ← buat file ini
    // TODO: SymbolRegistry_SetIcon("USDCAD",  texIconCAD);     // assets/cad.png   ← buat file ini
    { "EURUSD", "EUR/USD", "Euro vs US Dollar",    SymbolCategory::FOREX, "🇪🇺", 0, 0.00001f, false },
    { "GBPUSD", "GBP/USD", "Pound vs US Dollar",   SymbolCategory::FOREX, "🇬🇧", 0, 0.00001f, false },
    { "USDJPY", "USD/JPY", "US Dollar vs Yen",     SymbolCategory::FOREX, "🇯🇵", 0, 0.01f,    false },
    { "AUDUSD", "AUD/USD", "Aussie vs US Dollar",  SymbolCategory::FOREX, "🇦🇺", 0, 0.00001f, false },
    { "USDCAD", "USD/CAD", "US Dollar vs CAD",     SymbolCategory::FOREX, "🇨🇦", 0, 0.00001f, false },
    { "NZDUSD", "NZD/USD", "NZD vs US Dollar",     SymbolCategory::FOREX, "🇳🇿", 0, 0.00001f, false },
    { "USDCHF", "USD/CHF", "US Dollar vs CHF",     SymbolCategory::FOREX, "🇨🇭", 0, 0.00001f, false },
    { "EURGBP", "EUR/GBP", "Euro vs Pound",        SymbolCategory::FOREX, "🇪🇺", 0, 0.00001f, false },
    { "EURJPY", "EUR/JPY", "Euro vs Yen",          SymbolCategory::FOREX, "🇯🇵", 0, 0.01f,    false },
    { "GBPJPY", "GBP/JPY", "Pound vs Yen",         SymbolCategory::FOREX, "🇬🇧", 0, 0.01f,    false },

    // ── CRYPTO ───────────────────────────────────────────────────
    // TODO: SymbolRegistry_SetIcon("BTCUSDT", texIconBTC);     // assets/btc.png
    // TODO: SymbolRegistry_SetIcon("ETHUSDT", texIconETH);     // assets/eth.png
    // TODO: SymbolRegistry_SetIcon("SOLUSDT", texIconSOL);     // assets/sol.png   ← buat file ini
    // TODO: SymbolRegistry_SetIcon("BNBUSDT", texIconBNB);     // assets/bnb.png   ← buat file ini
    // TODO: SymbolRegistry_SetIcon("XRPUSDT", texIconXRP);     // assets/xrp.png   ← buat file ini
    { "BTCUSDT", "BTC/USDT", "Bitcoin vs Tether",   SymbolCategory::CRYPTO, "₿",  0, 0.1f,    true  },
    { "ETHUSDT", "ETH/USDT", "Ethereum vs Tether",  SymbolCategory::CRYPTO, "Ξ",  0, 0.01f,   true  },
    { "SOLUSDT", "SOL/USDT", "Solana vs Tether",    SymbolCategory::CRYPTO, "◎",  0, 0.01f,   true  },
    { "BNBUSDT", "BNB/USDT", "BNB vs Tether",       SymbolCategory::CRYPTO, "🔶", 0, 0.01f,   true  },
    { "XRPUSDT", "XRP/USDT", "XRP vs Tether",       SymbolCategory::CRYPTO, "✦",  0, 0.0001f, true  },
};

// ─────────────────────────────────────────────────────────────────
//  Registry helpers
// ─────────────────────────────────────────────────────────────────
inline void SymbolRegistry_SetIcon(const std::string& symId, ImTextureID tex) {
    for (auto& s : g_symbolRegistry)
        if (s.id == symId) { s.icon = tex; return; }
}

inline const SymbolInfo* SymbolRegistry_Find(const std::string& id) {
    for (const auto& s : g_symbolRegistry)
        if (s.id == id) return &s;
    return nullptr;
}

// ─────────────────────────────────────────────────────────────────
//  TIMEFRAME DATA
//  !! Sinkronisasikan dengan kTFs[] di UI_ChartTabs.h !!
// ─────────────────────────────────────────────────────────────────
struct SP_TFInfo { const char* id; const char* label; const char* group; };
static const SP_TFInfo kSP_TFs[] = {
    { "M1",  "1m",  "Menit"  },
    { "M5",  "5m",  "Menit"  },
    { "M15", "15m", "Menit"  },   // index 2 = default
    { "M30", "30m", "Menit"  },
    { "H1",  "1H",  "Jam"    },
    { "H4",  "4H",  "Jam"    },
    { "D1",  "1D",  "Hari"   },
    { "W1",  "1W",  "Minggu" },
    { "MN",  "MN",  "Bulan"  },
};
static const int kSP_TFCount = (int)(sizeof(kSP_TFs) / sizeof(kSP_TFs[0]));

// Cari index TF dari id string, fallback ke 2 (M15)
static inline int SP_FindTFIdx(const char* tfId) {
    if (!tfId || tfId[0] == '\0') return 2;
    for (int i = 0; i < kSP_TFCount; i++)
        if (strcmp(kSP_TFs[i].id, tfId) == 0) return i;
    return 2;
}

// =================================================================
//  SYMBOL PICKER — State & Render
// =================================================================
namespace SymbolPicker {

struct State {
    bool           isOpen        = false;
    int            slideIdx      = 0;       // 0 = Simbol, 1 = Timeframe
    char           searchBuf[64] = {};
    SymbolCategory activeFilter  = SymbolCategory::ALL;
    std::string    callerCtx;
    float          openAnim      = 0.f;
    int            hoveredIdx    = -1;
    // Pending — set di slide 1, dikonfirmasi di slide 2
    std::string    pendingSymId;
    int            pendingTFIdx  = 2;       // default M15
};

static State s_sp;

// ─────────────────────────────────────────────────────────────────
//  Open / Close / IsOpen
// ─────────────────────────────────────────────────────────────────

// defaultTFId : pre-select TF yang sudah aktif  (misal dari tab aktif navbar)
inline void Open(const char* ctx = "default", const char* defaultTFId = "M15") {
    s_sp.isOpen       = true;
    s_sp.callerCtx    = ctx ? ctx : "default";
    s_sp.slideIdx     = 0;
    s_sp.openAnim     = 0.f;
    s_sp.hoveredIdx   = -1;
    s_sp.pendingSymId = "";
    s_sp.pendingTFIdx = SP_FindTFIdx(defaultTFId);
    memset(s_sp.searchBuf, 0, sizeof(s_sp.searchBuf));
    s_sp.activeFilter = SymbolCategory::ALL;
}

inline void Close() { s_sp.isOpen = false; }
inline bool IsOpen(){ return s_sp.isOpen; }


// ─────────────────────────────────────────────────────────────────
//  Internal: Render logo bulat per simbol
//  • Jika sym.icon terisi → tampilkan PNG
//  • Jika belum → circle berwarna + emoji (sementara)
// ─────────────────────────────────────────────────────────────────
static void SP_Logo(const SymbolInfo& sym, float sz, float alpha) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p  = ImGui::GetCursorScreenPos();
    ImVec2 cx(p.x + sz * 0.5f, p.y + sz * 0.5f);

    // Warna latar per kategori
    ImU32 bg;
    switch (sym.category) {
        case SymbolCategory::FOREX:     bg = IM_COL32( 10,  28, 80, 200); break;
        case SymbolCategory::CRYPTO:    bg = IM_COL32( 60,  36,  4, 200); break;
        case SymbolCategory::COMMODITY: bg = IM_COL32( 58,  42,  3, 200); break;
        default:                        bg = IM_COL32( 20,  26, 50, 200); break;
    }
    dl->AddCircleFilled(cx, sz * 0.5f, bg, 32);
    dl->AddCircle      (cx, sz * 0.5f, IM_COL32(255,255,255,28), 32, 1.0f);

    if (sym.icon) {
        // ─────────────────────────────────────────────────────────
        // PNG SIAP → render image
        // ─────────────────────────────────────────────────────────
        ImGui::Image(sym.icon, ImVec2(sz, sz));
    } else {
        // ─────────────────────────────────────────────────────────
        // EMOJI FALLBACK (sementara sampai PNG dikumpulkan)
        // Ganti blok ini dengan ImGui::Image(sym.icon, ...) setelah
        // semua PNG di-load dan SymbolRegistry_SetIcon() dipanggil.
        // ─────────────────────────────────────────────────────────
        ImVec2 ts = ImGui::CalcTextSize(sym.emoji);
        dl->AddText(
            ImVec2(cx.x - ts.x * 0.5f, cx.y - ts.y * 0.5f),
            IM_COL32(255, 255, 255, (int)(alpha * 225)),
            sym.emoji
        );
        ImGui::Dummy(ImVec2(sz, sz));
    }
}

// ─────────────────────────────────────────────────────────────────
//  Internal: Badge kategori kecil
// ─────────────────────────────────────────────────────────────────
static void SP_CatBadge(SymbolCategory cat, float alpha) {
    ImVec4 col;
    const char* lbl;
    switch (cat) {
        case SymbolCategory::FOREX:
            col={0.20f,0.55f,0.92f,0.90f}; lbl="FX";  break;
        case SymbolCategory::CRYPTO:
            col={0.92f,0.50f,0.08f,0.90f}; lbl="CR";  break;
        case SymbolCategory::COMMODITY:
            col={0.88f,0.66f,0.08f,0.90f}; lbl="COM"; break;
        default:
            col={0.48f,0.32f,0.80f,0.90f}; lbl="IDX"; break;
    }
    ImGui::PushStyleColor(ImGuiCol_Text,
        ImVec4(col.x, col.y, col.z, alpha));
    ImGui::PushStyleColor(ImGuiCol_Button,
        ImVec4(col.x*0.16f, col.y*0.16f, col.z*0.16f, 0.65f));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6.f, 2.f));
    ImGui::SmallButton(lbl);
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(2);
}

// ─────────────────────────────────────────────────────────────────
//  SLIDE 1 — Pilih Simbol
// ─────────────────────────────────────────────────────────────────
static void SP_DrawSlide1(float alpha) {
    const float LOGO_SZ = 28.f;
    const float ROW_H   = 40.f;

    // ── Search bar ───────────────────────────────────────────────
    ImGui::SetNextItemWidth(-1.f);
    if (s_sp.openAnim < 0.25f) ImGui::SetKeyboardFocusHere();
    ImGui::PushStyleColor(ImGuiCol_FrameBg,
        ImVec4(0.09f, 0.11f, 0.17f, 1.f));
    ImGui::PushStyleColor(ImGuiCol_Border,
        ImVec4(0.18f, 0.28f, 0.50f, 1.f));
    ImGui::InputTextWithHint(
        "##sp_search",
        "  🔍  Cari simbol...  XAUUSD · BTC · Gold · EUR",
        s_sp.searchBuf, sizeof(s_sp.searchBuf));
    ImGui::PopStyleColor(2);
    ImGui::Spacing();

    // ── Filter tabs ──────────────────────────────────────────────
    struct TabDef { const char* lbl; SymbolCategory cat; };
    const TabDef tabs[] = {
        {"  Semua  ",    SymbolCategory::ALL      },
        {"  Forex  ",    SymbolCategory::FOREX    },
        {"  Crypto  ",   SymbolCategory::CRYPTO   },
        {"  Komoditas ", SymbolCategory::COMMODITY },
    };
    for (int i = 0; i < 4; i++) {
        bool on = (s_sp.activeFilter == tabs[i].cat);
        ImGui::PushStyleColor(ImGuiCol_Button,
            on ? ImVec4(0.11f,0.30f,0.70f,1.f) : ImVec4(0.07f,0.09f,0.15f,0.80f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
            on ? ImVec4(0.15f,0.40f,0.88f,1.f) : ImVec4(0.11f,0.15f,0.25f,1.f));
        ImGui::PushStyleColor(ImGuiCol_Text,
            on ? ImVec4(1.f,1.f,1.f,alpha)
               : ImVec4(0.52f,0.62f,0.80f,alpha));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,  ImVec2(12.f, 5.f));
        if (ImGui::Button(tabs[i].lbl))
            s_sp.activeFilter = tabs[i].cat;
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(3);
        if (i < 3) ImGui::SameLine(0, 5.f);
    }
    ImGui::Spacing();

    // ── Symbol table (scrollable, kolom bisa digeser) ───────────
    // Reserve ruang untuk footer
    float listH = ImGui::GetContentRegionAvail().y - 56.f;

    ImGui::PushStyleColor(ImGuiCol_ChildBg,           ImVec4(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_TableHeaderBg,     ImVec4(0.08f,0.10f,0.18f,1.f));
    ImGui::PushStyleColor(ImGuiCol_TableBorderLight,  ImVec4(0.14f,0.20f,0.34f,0.60f));
    ImGui::PushStyleColor(ImGuiCol_TableRowBg,        ImVec4(0.07f,0.09f,0.13f,0.48f));
    ImGui::PushStyleColor(ImGuiCol_TableRowBgAlt,     ImVec4(0.05f,0.06f,0.10f,0.32f));

    ImGui::BeginChild("##sp_list", ImVec2(0.f, listH), false,
        ImGuiWindowFlags_HorizontalScrollbar);

    // Lowercase search query
    std::string q(s_sp.searchBuf);
    std::transform(q.begin(), q.end(), q.begin(), ::tolower);

    ImGuiTableFlags tflags =
        ImGuiTableFlags_RowBg             |  // zebra stripe
        ImGuiTableFlags_BordersInnerV     |  // garis vertikal antar kolom
        ImGuiTableFlags_SizingStretchProp |  // kolom stretch proporsional
        ImGuiTableFlags_Resizable         |  // ← bisa digeser user!
        ImGuiTableFlags_ScrollY           |  // scroll vertikal dalam table
        ImGuiTableFlags_NoHostExtendX;       // tidak melebar melebihi parent

    if (ImGui::BeginTable("##sp_table", 4, tflags,
        ImVec2(0.f, listH)))
    {
        // Setup kolom — lebar default, user bisa resize
        ImGui::TableSetupScrollFreeze(0, 1); // freeze header row
        ImGui::TableSetupColumn("SIMBOL",  ImGuiTableColumnFlags_WidthStretch,  2.4f);
        ImGui::TableSetupColumn("HARGA",   ImGuiTableColumnFlags_WidthStretch,  1.2f);
        ImGui::TableSetupColumn("CHG%",    ImGuiTableColumnFlags_WidthStretch,  0.8f);
        ImGui::TableSetupColumn("KAT.",    ImGuiTableColumnFlags_WidthFixed,   52.f);

        // Header row dengan styling custom
        ImGui::PushStyleColor(ImGuiCol_Text,
            ImVec4(0.32f, 0.42f, 0.58f, alpha));
        ImGui::TableHeadersRow();
        ImGui::PopStyleColor();

        int rowIdx = 0;
        for (auto& sym : g_symbolRegistry) {
            // Filter kategori
            if (s_sp.activeFilter != SymbolCategory::ALL &&
                sym.category != s_sp.activeFilter) continue;

            // Filter search
            if (!q.empty()) {
                std::string a = sym.id, b = sym.displayName, c2 = sym.description;
                auto lo = [](std::string& s2) {
                    std::transform(s2.begin(), s2.end(), s2.begin(), ::tolower);
                };
                lo(a); lo(b); lo(c2);
                if (a.find(q)==std::string::npos &&
                    b.find(q)==std::string::npos &&
                    c2.find(q)==std::string::npos) continue;
            }

            bool isSel = (sym.id == s_sp.pendingSymId);

            ImGui::TableNextRow(ImGuiTableRowFlags_None, ROW_H);

            // ── COL 0: Logo + Nama + Deskripsi ───────────────────
            ImGui::TableSetColumnIndex(0);

            // Warna row terpilih override
            if (isSel)
                ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0,
                    IM_COL32(20, 58, 140, 180));

            // Selectable span all columns sebagai hit-area
            ImGui::PushStyleColor(ImGuiCol_Header,
                ImVec4(0.f,0.f,0.f,0.f)); // transparan — warna dari TableRowBg
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered,
                ImVec4(0.14f,0.24f,0.48f,0.90f));
            ImGui::PushStyleColor(ImGuiCol_HeaderActive,
                ImVec4(0.17f,0.30f,0.65f,1.00f));

            char sel_id[64];
            snprintf(sel_id, sizeof(sel_id), "##sp_row_%s", sym.id.c_str());
            bool clicked = ImGui::Selectable(
                sel_id, isSel,
                ImGuiSelectableFlags_SpanAllColumns |
                ImGuiSelectableFlags_AllowOverlap,
                ImVec2(0.f, ROW_H));
            ImGui::PopStyleColor(3);

            if (ImGui::IsItemHovered()) s_sp.hoveredIdx = rowIdx;
            if (clicked) {
                s_sp.pendingSymId = sym.id;
                s_sp.slideIdx     = 1; // → slide TF
            }

            // Bar biru kiri row terpilih
            if (isSel) {
                ImDrawList* dl  = ImGui::GetWindowDrawList();
                ImVec2 rMin     = ImGui::GetItemRectMin();
                ImVec2 rMax     = ImGui::GetItemRectMax();
                dl->AddRectFilled(
                    ImVec2(rMin.x,      rMin.y + ROW_H*0.10f),
                    ImVec2(rMin.x+2.5f, rMax.y - ROW_H*0.10f),
                    IM_COL32(76,141,255,230), 2.f);
            }

            // Logo (overlay di atas Selectable)
            ImGui::SameLine(4.f);
            float rowTopY = ImGui::GetCursorPosY();
            ImGui::SetCursorPosY(rowTopY + (ROW_H - LOGO_SZ) * 0.5f);
            SP_Logo(sym, LOGO_SZ, alpha);

            // Nama + deskripsi
            ImGui::SameLine(0.f, 8.f);
            float nameX = ImGui::GetCursorPosX();
            ImGui::SetCursorPosY(rowTopY + 3.f);
            ImGui::PushStyleColor(ImGuiCol_Text,
                ImVec4(0.92f, 0.95f, 1.0f, alpha));
            ImGui::TextUnformatted(sym.displayName.c_str());
            ImGui::PopStyleColor();

            ImGui::SetCursorPosX(nameX);
            ImGui::SetCursorPosY(rowTopY + 20.f);
            ImGui::PushStyleColor(ImGuiCol_Text,
                ImVec4(0.38f, 0.50f, 0.66f, alpha * 0.80f));
            ImGui::TextUnformatted(sym.description.c_str());
            ImGui::PopStyleColor();

            // ── COL 1: Harga live ─────────────────────────────────
            ImGui::TableSetColumnIndex(1);
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (ROW_H - ImGui::GetTextLineHeight()) * 0.5f);
            double px = g_marketWatch.GetLivePrice(sym.id);
            if (px > 0.0) {
                ImGui::PushStyleColor(ImGuiCol_Text,
                    ImVec4(0.76f, 0.92f, 0.76f, alpha));
                ImGui::Text(px > 500.0 ? "%.2f" : "%.5f", px);
                ImGui::PopStyleColor();
            } else {
                ImGui::PushStyleColor(ImGuiCol_Text,
                    ImVec4(0.30f, 0.36f, 0.50f, alpha));
                ImGui::TextUnformatted("—");
                ImGui::PopStyleColor();
            }

            // ── COL 2: CHG% ──────────────────────────────────────
            // (placeholder — isi dari data live bila sudah ada di MarketWatch)
            ImGui::TableSetColumnIndex(2);
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (ROW_H - ImGui::GetTextLineHeight()) * 0.5f);
            ImGui::PushStyleColor(ImGuiCol_Text,
                ImVec4(0.30f, 0.36f, 0.50f, alpha * 0.70f));
            ImGui::TextUnformatted("—");
            ImGui::PopStyleColor();

            // ── COL 3: Badge kategori ─────────────────────────────
            ImGui::TableSetColumnIndex(3);
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (ROW_H - ImGui::GetTextLineHeight()) * 0.5f - 1.f);
            SP_CatBadge(sym.category, alpha);

            rowIdx++;
        }

        // Kosong — tidak ada hasil filter/search
        if (rowIdx == 0) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Text,
                ImVec4(0.36f, 0.42f, 0.55f, alpha));
            ImGui::Text("  Tidak ada simbol untuk \"%s\"", s_sp.searchBuf);
            ImGui::PopStyleColor();
        }

        ImGui::EndTable();
    }

    ImGui::EndChild();
    ImGui::PopStyleColor(5); // ChildBg + table colors

    // ── Footer Slide 1 — [✕ Batal]  [Pilih TF →] ───────────────────
    ImGui::Separator();
    ImGui::Spacing();

    bool hasSym = !s_sp.pendingSymId.empty();

    // Info simbol terpilih (satu baris kecil di atas tombol)
    if (hasSym) {
        const SymbolInfo* si = SymbolRegistry_Find(s_sp.pendingSymId);
        ImGui::PushStyleColor(ImGuiCol_Text,
            ImVec4(0.48f, 0.76f, 1.0f, alpha));
        ImGui::Text("  ✓  %s",
            si ? si->displayName.c_str() : s_sp.pendingSymId.c_str());
        ImGui::PopStyleColor();
    }
    ImGui::Spacing();

    // Lebar tombol sama persis dengan slide 2
    float totalW  = ImGui::GetContentRegionAvail().x;
    float cancelW = totalW * 0.28f;
    float nextW   = totalW - cancelW - 8.f;

    // ── Tombol Batal (gaya identik slide 2) ──────────────────────
    ImGui::PushStyleColor(ImGuiCol_Button,
        ImVec4(0.06f, 0.08f, 0.12f, 0.80f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
        ImVec4(0.22f, 0.04f, 0.07f, 0.75f));
    ImGui::PushStyleColor(ImGuiCol_Text,
        ImVec4(0.48f, 0.54f, 0.66f, alpha));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.f, 10.f));
    if (ImGui::Button("✕  Batal##sp_cancel_s1", ImVec2(cancelW, 0.f)))
        Close();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);

    ImGui::SameLine(0.f, 8.f);

    // ── Tombol Pilih TF → ─────────────────────────────────────────
    if (!hasSym) ImGui::BeginDisabled();
    ImGui::PushStyleColor(ImGuiCol_Button,
        hasSym ? ImVec4(0.10f, 0.34f, 0.72f, 1.f)
               : ImVec4(0.07f, 0.09f, 0.14f, 0.60f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
        ImVec4(0.14f, 0.44f, 0.90f, 1.f));
    ImGui::PushStyleColor(ImGuiCol_Text,
        ImVec4(1.f, 1.f, 1.f, alpha));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.f, 10.f));
    if (ImGui::Button("  Pilih TF  →##sp_next", ImVec2(nextW, 0.f)))
        s_sp.slideIdx = 1;
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);
    if (!hasSym) ImGui::EndDisabled();
}

// ─────────────────────────────────────────────────────────────────
//  SLIDE 2 — Pilih Timeframe + Konfirmasi
// ─────────────────────────────────────────────────────────────────
static void SP_DrawSlide2(
    float alpha,
    const std::function<void(const std::string&, const std::string&)>& onConfirm)
{
    const SymbolInfo* si = SymbolRegistry_Find(s_sp.pendingSymId);

    // ── Baris atas: tombol back + badge simbol ────────────────────
    ImGui::PushStyleColor(ImGuiCol_Button,
        ImVec4(0.07f, 0.09f, 0.17f, 0.80f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
        ImVec4(0.12f, 0.17f, 0.30f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_Text,
        ImVec4(0.56f, 0.68f, 0.88f, alpha));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,  ImVec2(10.f, 6.f));
    if (ImGui::Button("← Kembali##sp_back"))
        s_sp.slideIdx = 0;
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(3);

    // Badge simbol terpilih di sisi kanan
    if (si) {
        float remW = ImGui::GetContentRegionAvail().x;
        ImGui::SameLine(0, 10.f);
        ImGui::PushStyleColor(ImGuiCol_ChildBg,
            ImVec4(0.09f, 0.13f, 0.22f, 0.80f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 6.f);
        if (ImGui::BeginChild("##sp_badge", ImVec2(remW, 30.f), true)) {
            ImGui::SetCursorPosX(8.f);
            ImGui::SetCursorPosY(4.f);
            SP_Logo(*si, 20.f, alpha);
            ImGui::SameLine(0, 8.f);
            ImGui::SetCursorPosY(6.f);
            ImGui::PushStyleColor(ImGuiCol_Text,
                ImVec4(0.44f, 0.76f, 1.0f, alpha));
            ImGui::TextUnformatted(si->displayName.c_str());
            ImGui::PopStyleColor();
            ImGui::SameLine(0, 6.f);
            ImGui::SetCursorPosY(8.f);
            ImGui::PushStyleColor(ImGuiCol_Text,
                ImVec4(0.36f, 0.48f, 0.64f, alpha * 0.75f));
            ImGui::Text("— %s", si->description.c_str());
            ImGui::PopStyleColor();
        }
        ImGui::EndChild();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
    }

    ImGui::Spacing();

    // ── Step indicator ───────────────────────────────────────────
    // ✓ Simbol  ──  ● Timeframe  ──  ○ Konfirmasi
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.18f,0.82f,0.58f,alpha));
    ImGui::TextUnformatted("✓ Simbol");
    ImGui::PopStyleColor();
    ImGui::SameLine(0, 4.f);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.22f,0.30f,0.48f,alpha));
    ImGui::TextUnformatted("──");
    ImGui::PopStyleColor();
    ImGui::SameLine(0, 4.f);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.44f,0.76f,1.00f,alpha));
    ImGui::TextUnformatted("● Timeframe");
    ImGui::PopStyleColor();
    ImGui::SameLine(0, 4.f);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.22f,0.30f,0.48f,alpha));
    ImGui::TextUnformatted("──");
    ImGui::PopStyleColor();
    ImGui::SameLine(0, 4.f);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.28f,0.34f,0.50f,alpha));
    ImGui::TextUnformatted("○ Konfirmasi");
    ImGui::PopStyleColor();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // ── Label TF ─────────────────────────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.36f,0.48f,0.66f,alpha));
    ImGui::TextUnformatted("PILIH TIMEFRAME");
    ImGui::PopStyleColor();
    ImGui::Spacing();

    // ── TF Chips  (5 per baris) ───────────────────────────────────
    {
        const int   PER_ROW = 5;
        const float GAP     = 6.f;
        const float CHIP_H  = 34.f;
        float availW = ImGui::GetContentRegionAvail().x;
        float chipW  = ImMax((availW - GAP * (PER_ROW - 1)) / PER_ROW, 40.f);

        int col = 0;
        for (int i = 0; i < kSP_TFCount; i++) {
            bool sel = (s_sp.pendingTFIdx == i);

            ImGui::PushStyleColor(ImGuiCol_Button,
                sel ? ImVec4(0.10f,0.34f,0.72f,1.f)
                    : ImVec4(0.07f,0.10f,0.18f,0.88f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                sel ? ImVec4(0.14f,0.44f,0.90f,1.f)
                    : ImVec4(0.11f,0.16f,0.28f,1.f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                ImVec4(0.07f, 0.26f, 0.60f, 1.f));
            ImGui::PushStyleColor(ImGuiCol_Text,
                sel ? ImVec4(1.f,1.f,1.f,alpha)
                    : ImVec4(0.50f,0.60f,0.80f,alpha));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 7.f);
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,  ImVec2(0.f,0.f));

            char btn_id[32];
            snprintf(btn_id, sizeof(btn_id), "%s##sp_tf%d",
                     kSP_TFs[i].label, i);
            if (ImGui::Button(btn_id, ImVec2(chipW, CHIP_H)))
                s_sp.pendingTFIdx = i;

            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor(4);

            col++;
            if (col < PER_ROW && i < kSP_TFCount - 1)
                ImGui::SameLine(0.f, GAP);
            else
                col = 0;
        }
    }

    ImGui::Spacing();
    ImGui::Spacing();

    // ── Summary card ─────────────────────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_ChildBg,
        ImVec4(0.07f, 0.09f, 0.15f, 0.80f));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.f);

    if (ImGui::BeginChild("##sp_sum", ImVec2(-1.f, 78.f), true)) {
        // 3-kolom via manual SameLine
        float cw = ImGui::GetWindowWidth() / 3.f;

        // ── Labels row ───────────────────────────────────────────
        ImGui::SetCursorPos(ImVec2(12.f, 8.f));
        ImGui::PushStyleColor(ImGuiCol_Text,
            ImVec4(0.30f,0.40f,0.55f,alpha));
        ImGui::TextUnformatted("SIMBOL");

        ImGui::SetCursorPos(ImVec2(12.f + cw, 8.f));
        ImGui::TextUnformatted("TIMEFRAME");

        ImGui::SetCursorPos(ImVec2(12.f + cw*2.f, 8.f));
        ImGui::TextUnformatted("HARGA TERAKHIR");
        ImGui::PopStyleColor();

        // ── Values row ───────────────────────────────────────────
        ImGui::SetCursorPos(ImVec2(12.f, 28.f));
        ImGui::PushStyleColor(ImGuiCol_Text,
            ImVec4(0.44f, 0.76f, 1.0f, alpha));
        ImGui::TextUnformatted(
            si ? si->displayName.c_str()
               : s_sp.pendingSymId.c_str());
        ImGui::PopStyleColor();

        ImGui::SetCursorPos(ImVec2(12.f + cw, 28.f));
        ImGui::PushStyleColor(ImGuiCol_Text,
            ImVec4(0.20f, 0.88f, 0.60f, alpha));
        ImGui::Text("%s  (%s)",
            kSP_TFs[s_sp.pendingTFIdx].label,
            kSP_TFs[s_sp.pendingTFIdx].group);
        ImGui::PopStyleColor();

        ImGui::SetCursorPos(ImVec2(12.f + cw*2.f, 28.f));
        double px2 = g_marketWatch.GetLivePrice(s_sp.pendingSymId);
        if (px2 > 0.0) {
            ImGui::PushStyleColor(ImGuiCol_Text,
                ImVec4(0.76f, 0.92f, 0.76f, alpha));
            ImGui::Text(px2 > 500.0 ? "%.2f" : "%.5f", px2);
            ImGui::PopStyleColor();
        } else {
            ImGui::PushStyleColor(ImGuiCol_Text,
                ImVec4(0.28f, 0.34f, 0.50f, alpha));
            ImGui::TextUnformatted("—");
            ImGui::PopStyleColor();
        }

        // ── Kategori bawah kiri ───────────────────────────────────
        ImGui::SetCursorPos(ImVec2(12.f, 52.f));
        if (si) {
            ImVec4 cc;
            const char* cl;
            switch(si->category){
                case SymbolCategory::FOREX:
                    cc={0.20f,0.55f,0.90f,alpha}; cl="FOREX"; break;
                case SymbolCategory::CRYPTO:
                    cc={0.90f,0.50f,0.08f,alpha}; cl="CRYPTO"; break;
                case SymbolCategory::COMMODITY:
                    cc={0.85f,0.65f,0.08f,alpha}; cl="KOMODITAS"; break;
                default:
                    cc={0.48f,0.32f,0.80f,alpha}; cl="INDEX"; break;
            }
            ImGui::PushStyleColor(ImGuiCol_Text, cc);
            ImGui::TextUnformatted(cl);
            ImGui::PopStyleColor();
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // ── Action buttons: [Batal]  [✔ Buka Chart] ──────────────────
    float totalW  = ImGui::GetContentRegionAvail().x;
    float cancelW = totalW * 0.28f;
    float confirmW= totalW - cancelW - 8.f;

    // Batal
    ImGui::PushStyleColor(ImGuiCol_Button,
        ImVec4(0.06f, 0.08f, 0.12f, 0.80f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
        ImVec4(0.22f, 0.04f, 0.07f, 0.75f));
    ImGui::PushStyleColor(ImGuiCol_Text,
        ImVec4(0.48f, 0.54f, 0.66f, alpha));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.f, 10.f));
    if (ImGui::Button("✕  Batal##sp_cancel", ImVec2(cancelW, 0.f)))
        Close();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);

    ImGui::SameLine(0.f, 8.f);

    // Konfirmasi / Buka Chart
    ImGui::PushStyleColor(ImGuiCol_Button,
        ImVec4(0.07f, 0.36f, 0.16f, 1.f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
        ImVec4(0.10f, 0.48f, 0.22f, 1.f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,
        ImVec4(0.05f, 0.26f, 0.11f, 1.f));
    ImGui::PushStyleColor(ImGuiCol_Text,
        ImVec4(0.82f, 1.0f, 0.86f, alpha));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.f, 10.f));

    if (ImGui::Button("  ✔  Buka Chart##sp_confirm",
                      ImVec2(confirmW, 0.f)))
    {
        if (onConfirm && !s_sp.pendingSymId.empty())
            onConfirm(s_sp.pendingSymId, kSP_TFs[s_sp.pendingTFIdx].id);
        Close();
    }

    ImGui::PopStyleVar();
    ImGui::PopStyleColor(4);
}

// =================================================================
//  RENDER — panggil tiap frame di render loop
//
//  Callback signature:
//    void(const std::string& sym, const std::string& tf)
//
//  Contoh:
//    SymbolPicker::Render([](const std::string& sym, const std::string& tf) {
//        SwitchSymbol(sym);
//        SetActiveTF(tf);
//    });
// =================================================================
// ctx harus sama dengan yang dipakai di Open("ctx").
// Ini mencegah 2+ caller merender picker bersamaan (duplicate window / ID conflict).
inline void Render(
    const char* ctx,
    std::function<void(const std::string&, const std::string&)> onConfirm)
{
    if (!s_sp.isOpen) return;
    // Guard: hanya render untuk caller yang membuka picker ini
    if (!ctx || s_sp.callerCtx != ctx) return;

    // Animasi fade-in
    s_sp.openAnim = std::min(1.f, s_sp.openAnim + ImGui::GetIO().DeltaTime * 6.f);
    float alpha   = s_sp.openAnim;

    // Ukuran: slide 0 lebih tinggi (ada scroll list), slide 1 auto
    ImVec2 winSz = (s_sp.slideIdx == 0)
                 ? ImVec2(560.f, 530.f)
                 : ImVec2(560.f, 0.f);
    ImGuiWindowFlags autoR = (s_sp.slideIdx == 1)
                           ? ImGuiWindowFlags_AlwaysAutoResize : 0;

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(winSz, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(alpha * 0.97f);

    ImGui::PushStyleColor(ImGuiCol_WindowBg,
        ImVec4(0.06f, 0.07f, 0.11f, 1.f));
    ImGui::PushStyleColor(ImGuiCol_Border,
        ImVec4(0.16f, 0.22f, 0.36f, 1.f));
    ImGui::PushStyleColor(ImGuiCol_Text,
        ImVec4(0.90f, 0.93f, 0.98f, alpha));
    ImGui::PushStyleColor(ImGuiCol_FrameBg,
        ImVec4(0.09f, 0.11f, 0.17f, 1.f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered,
        ImVec4(0.12f, 0.16f, 0.25f, 1.f));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarBg,
        ImVec4(0.f, 0.f, 0.f, 0.f));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab,
        ImVec4(0.18f, 0.26f, 0.44f, 0.80f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,   12.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,    ImVec2(16.f, 14.f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,      ImVec2(8.f, 7.f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding,    7.f);
    ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarRounding,4.f);

    bool windowOpen = true;
    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar          |
        ImGuiWindowFlags_NoResize            |
        ImGuiWindowFlags_NoMove              |
        ImGuiWindowFlags_NoScrollbar         |
        ImGuiWindowFlags_NoScrollWithMouse   |
        autoR;

    if (ImGui::Begin("##SymPickerV2", &windowOpen, flags)) {

        // ── Header: logo YATA + judul + tombol ✕ ─────────────────
        {
            const float LOGO_W = 28.f;
            const float LOGO_H = 28.f;
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 1.f);

            if (texIconSymbol) {
                // ─────────────────────────────────────────────────
                // PNG logo YATA — assets/simbol.png
                // Ganti texIconSymbol → texYataLogo kalau sudah punya
                // file logo platform YATA yang lebih proper.
                // ─────────────────────────────────────────────────
                ImGui::Image(texIconSymbol, ImVec2(LOGO_W, LOGO_H));
            } else {
                // Fallback: kotak biru + huruf "Y" sampai PNG diload
                ImDrawList* dl = ImGui::GetWindowDrawList();
                ImVec2 hp = ImGui::GetCursorScreenPos();
                dl->AddRectFilled(hp,
                    ImVec2(hp.x + LOGO_W, hp.y + LOGO_H),
                    IM_COL32(30, 80, 210, 235), 6.f);
                ImVec2 ts = ImGui::CalcTextSize("Y");
                dl->AddText(
                    ImVec2(hp.x + (LOGO_W - ts.x) * 0.5f,
                           hp.y + (LOGO_H - ts.y) * 0.5f),
                    IM_COL32(255, 255, 255, 240), "Y");
                ImGui::Dummy(ImVec2(LOGO_W, LOGO_H));
            }
        }
        ImGui::SameLine(0, 9.f);

        const char* hdrTitle =
            (s_sp.slideIdx == 0) ? "  PILIH SIMBOL"
                                 : "  PILIH TIMEFRAME";
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4.f);
        ImGui::PushStyleColor(ImGuiCol_Text,
            ImVec4(0.44f, 0.74f, 1.0f, alpha));
        ImGui::TextUnformatted(hdrTitle);
        ImGui::PopStyleColor();
        // Tombol × di header DIHAPUS — diganti tombol Batal di footer slide 1
        // (mobile-friendly: tombol besar lebih mudah dipencet daripada × kecil)

        ImGui::Separator();
        ImGui::Spacing();

        // ── Konten slide ─────────────────────────────────────────
        if (s_sp.slideIdx == 0)
            SP_DrawSlide1(alpha);
        else
            SP_DrawSlide2(alpha, onConfirm);
    }
    ImGui::End();

    ImGui::PopStyleColor(7);
    ImGui::PopStyleVar(5);

    if (!windowOpen) Close();
}

} // namespace SymbolPicker
