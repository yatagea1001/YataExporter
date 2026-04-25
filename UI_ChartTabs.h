#pragma once
// ================================================================
// UI_ChartTabs.h — Add Chart  (V3 — delegate ke SymbolPickerUI)
// ================================================================
//
// V3 PERUBAHAN:
//   • RenderAddChartModal() sekarang cuma buka SymbolPicker::Open()
//     dan menunggu callback (sym, tf).
//   • Semua logika pilih simbol + TF sudah ada di SymbolPickerUI.h.
//   • Modal popup internal sudah dihapus.
//   • kTFs[] + RenderTFChips() TETAP ada di sini untuk dipakai
//     komponen lain (misal TF switcher di navbar).
//
// ────────────────────────────────────────────────────────────────
// CARA TAMBAH TF BARU:
//   Edit kTFs[] di bawah DAN kSP_TFs[] di SymbolPickerUI.h
//   (harus sama supaya konsisten di seluruh UI)
//
// CARA TAMBAH SIMBOL BARU:
//   Edit g_symbolRegistry di SymbolPickerUI.h → semua UI ikut.
// ================================================================

#include "imgui.h"
#include "MultiChart.h"
#include "SymbolPickerUI.h"
#include <string>

extern ImVec4 g_colorBg, g_colorPanel, g_colorText, g_colorHeader;
extern std::string g_activeTF, g_symbol;

// ─────────────────────────────────────────────────────────────────
//  DAFTAR TIMEFRAME
//  !! Sinkronisasikan dengan kSP_TFs[] di SymbolPickerUI.h !!
// ─────────────────────────────────────────────────────────────────
struct TFInfo {
    const char* id;     // key internal: "M15"
    const char* label;  // tampilan: "15m"
    const char* group;  // grup: "Menit" / "Jam" / "Hari"
};

static const TFInfo kTFs[] = {
    { "M1",  "1m",  "Menit"  },
    { "M5",  "5m",  "Menit"  },
    { "M15", "15m", "Menit"  },
    { "M30", "30m", "Menit"  },
    { "H1",  "1H",  "Jam"    },
    { "H4",  "4H",  "Jam"    },
    { "D1",  "1D",  "Hari"   },
    { "W1",  "1W",  "Minggu" },
    { "MN",  "MN",  "Bulan"  },
    // Tambah di sini → tambah juga di kSP_TFs[] SymbolPickerUI.h
};
static const int kTFCount = (int)(sizeof(kTFs) / sizeof(kTFs[0]));

// ─── State internal ──────────────────────────────────────────────
static bool s_showAddChartModal    = false; // flag dari tombol ADD CHART
static bool s_showOBTabPicker      = false; // flag dari tombol New Order Book

// ─────────────────────────────────────────────────────────────────
//  SyncActiveChartContext
// ─────────────────────────────────────────────────────────────────
static inline void SyncActiveChartContext() {
    ChartTab* tab = g_chartManager.GetActiveTab();
    if (!tab) return;
    g_activeChart = tab;
    if (tab->usesGlobalData) {
        g_activeTF = tab->timeframe;
        g_symbol   = tab->symbol;
    }
}

// ─────────────────────────────────────────────────────────────────
//  RenderTFChips — reusable chip row (dipakai di navbar atau tempat lain)
//  selIdx = index ke kTFs[], diubah in-place
// ─────────────────────────────────────────────────────────────────
static void RenderTFChips(int& selIdx) {
    const float CHIP_H = 30.f;
    const float GAP    = 5.f;
    float availW = ImGui::GetContentRegionAvail().x;

    int perRow = kTFCount;
    float chipW = (availW - GAP * (perRow - 1)) / perRow;
    if (chipW < 38.f) {
        perRow = (kTFCount + 1) / 2;
        chipW  = (availW - GAP * (perRow - 1)) / perRow;
    }
    chipW = ImMax(chipW, 38.f);

    int col = 0;
    for (int i = 0; i < kTFCount; i++) {
        bool sel = (selIdx == i);

        ImGui::PushStyleColor(ImGuiCol_Button,
            sel ? ImVec4(0.14f,0.40f,0.82f,1.f)
                : ImVec4(0.09f,0.11f,0.18f,0.90f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
            sel ? ImVec4(0.18f,0.50f,1.00f,1.f)
                : ImVec4(0.14f,0.18f,0.30f,1.f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,
            ImVec4(0.10f,0.32f,0.72f,1.f));
        ImGui::PushStyleColor(ImGuiCol_Text,
            sel ? ImVec4(1.f,1.f,1.f,1.f)
                : ImVec4(0.55f,0.65f,0.82f,1.f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,  ImVec2(0,0));

        char id[32];
        snprintf(id, sizeof(id), "%s##tf%d", kTFs[i].label, i);
        if (ImGui::Button(id, ImVec2(chipW, CHIP_H)))
            selIdx = i;

        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(4);

        col++;
        if (col < perRow && i < kTFCount - 1)
            ImGui::SameLine(0.f, GAP);
        else
            col = 0;
    }
}

// ─────────────────────────────────────────────────────────────────
//  Getter state s_showOBTabPicker (untuk dipanggil dari main.cpp)
// ─────────────────────────────────────────────────────────────────
static inline bool& GetShowOBTabPicker() { return s_showOBTabPicker; }

// ─────────────────────────────────────────────────────────────────
//  RenderAddChartModal
//  Panggil tiap frame di RenderMainUI.
//
//  Flow:
//    Tombol ADD CHART di navbar → s_showAddChartModal = true
//    ↓
//    RenderAddChartModal() membuka SymbolPicker::Open("addchart")
//    ↓
//    Pengguna pilih Simbol + TF di SymbolPickerUI → Konfirmasi
//    ↓
//    Callback menerima (sym, tf) → AddTab + LoadTabSymbol
// ─────────────────────────────────────────────────────────────────
static inline void RenderAddChartModal() {

    // Buka picker saat flag nyala (set dari tombol ADD CHART di navbar)
    if (s_showAddChartModal) {
        s_showAddChartModal = false; // reset langsung — picker menjaga state-nya sendiri
        SymbolPicker::Open("addchart", "M15");
    }

    // Render picker tiap frame — hanya aktif kalau ctx "addchart" yang open
    SymbolPicker::Render("addchart", [](const std::string& sym, const std::string& tf) {
        // ── Buat tab chart baru (usesGlobal=false → tab independen) ──
        ChartTab* t = g_chartManager.AddTab(sym.c_str(), tf.c_str(), false);
        if (!t) return;
        t->renderStyle = RENDER_CANDLE;

        printf("✅ [AddChart] Tab baru: %s  %s  (id=%d)\n",
               sym.c_str(), tf.c_str(), t->id);

        #ifdef __EMSCRIPTEN__
        {
            char cmd[160];
            // Kirim sym DAN tf ke JS agar LoadTabSymbol bisa set TF langsung
            snprintf(cmd, sizeof(cmd),
                "LoadTabSymbol(%d, '%s', '%s')", t->id, sym.c_str(), tf.c_str());
            emscripten_run_script(cmd);
            printf("[TAB] LoadTabSymbol(%d, %s, %s)\n",
                   t->id, sym.c_str(), tf.c_str());
        }
        #endif
    });
}

// ─────────────────────────────────────────────────────────────────
//  RenderAddOrderBookModal
//  Symbol picker khusus OB — TIDAK ada TF selector
//  Simple popup: list crypto symbols → langsung buat OB tab
// ─────────────────────────────────────────────────────────────────
static inline void RenderAddOrderBookModal() {
    if (!s_showOBTabPicker) return;

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f,0.5f));
    ImGui::SetNextWindowSize(ImVec2(340, 320), ImGuiCond_Appearing);

    // 🔥 OpenPopup HARUS dipanggil SEBELUM BeginPopupModal — aturan ImGui
    ImGui::OpenPopup("New Order Book Tab##obpicker");

    ImGui::PushStyleColor(ImGuiCol_PopupBg,  ImVec4(0.07f,0.08f,0.12f,0.97f));
    ImGui::PushStyleColor(ImGuiCol_Border,   ImVec4(0.2f,0.3f,0.5f,0.8f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);

    if (ImGui::BeginPopupModal("New Order Book Tab##obpicker",
            &s_showOBTabPicker,
            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar))
    {
        ImDrawList* dl  = ImGui::GetWindowDrawList();
        ImVec2 wPos     = ImGui::GetWindowPos();
        float  wW       = ImGui::GetWindowWidth();

        // Header
        dl->AddRectFilled(wPos, ImVec2(wPos.x+wW, wPos.y+32),
                          IM_COL32(20,28,50,255));
        dl->AddText(ImVec2(wPos.x+12, wPos.y+8),
                    IM_COL32(100,180,255,255), "New Order Book Tab");

        ImGui::Dummy(ImVec2(0,34));
        ImGui::TextDisabled("Pilih symbol untuk Order Book:");
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // List dari g_symbolRegistry — filter CRYPTO saja
        for (auto& sym : g_symbolRegistry) {
            if (sym.category != SymbolCategory::CRYPTO) continue;

            ImGui::PushStyleColor(ImGuiCol_Header,
                ImVec4(0.15f,0.35f,0.65f,0.8f));
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered,
                ImVec4(0.20f,0.45f,0.80f,0.9f));

            char lbl[64];
            snprintf(lbl, sizeof(lbl), "  %s##ob_%s",
                     sym.id.c_str(), sym.id.c_str());

            if (ImGui::Selectable(lbl, false, 0, ImVec2(0,32))) {
                // Buat OB tab
                ChartTab* t    = g_chartManager.AddTab(sym.id.c_str(), "OB", false);
                t->isOrderBookTab = true;
                t->UpdateLabel();

                printf("[OB TAB] Baru: %s (id=%d)\n", sym.id.c_str(), t->id);

                #ifdef __EMSCRIPTEN__
                {
                    int tid = t->id;
                    std::string _sym = sym.id;
                    EM_ASM({
                        var s = UTF8ToString($0);
                        if (window.requestOrderBook) {
                            window.requestOrderBook(s);
                            console.log('[OB TAB] Created, request OB: ' + s);
                        }
                    }, _sym.c_str());
                }
                #endif

                s_showOBTabPicker = false;
                ImGui::PopStyleColor(2);
                ImGui::CloseCurrentPopup();
                break;
            }
            ImGui::PopStyleColor(2);
        }

        ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
        if (ImGui::Button("Batal", ImVec2(-1,0))) {
            s_showOBTabPicker = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(2);

}
