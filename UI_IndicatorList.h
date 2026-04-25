#pragma once
#include "imgui.h"
#include "Indicators.h"
#include <string>
#include <vector>
#include <map>
#include <algorithm>

// =========================================================
// ORDER FLOW INTEGRATION
// ChartTab, IsFootprintStyle, CandleStyleName — dari MultiChart.h
// ToggleVPSettings, ToggleFPSettings, RenderOrderFlowSettings — dari OrderFlowSettingsUI.h
// =========================================================
#include "MultiChart.h"
#include "OrderFlowSettingsUI.h"

// =========================================================
// WARNA TEMA (dari main.cpp)
// =========================================================
extern ImVec4 g_colorBg;
extern ImVec4 g_colorPanel;
extern ImVec4 g_colorText;
extern ImVec4 g_colorHeader;

// =========================================================
// LAYER ID
// Layer 0          = Tab utama live (via legacy vector<Indicator*>)
// Layer LAYER_REPLAY = Chart replay (main_Chart.cpp)
// Layer 100+N      = Extra chart live (RenderExtraLiveChart)
// Layer 300+N      = Extra chart replay (g_extraCharts)
// =========================================================
static constexpr int LAYER_REPLAY = 999;

// =========================================================
// LAYER STORAGE
// =========================================================
static std::map<int, std::vector<Indicator*>> s_layerInds;

static inline std::vector<Indicator*>& GetLayerIndicators(int layerID) {
    return s_layerInds[layerID];
}

static inline void ClearLayerIndicators(int layerID) {
    auto it = s_layerInds.find(layerID);
    if (it == s_layerInds.end()) return;
    for (auto* ind : it->second) delete ind;
    it->second.clear();
}

static inline void RecalculateLayerIndicators(
    int layerID,
    const std::vector<Candle>& candles)
{
    for (auto* ind : GetLayerIndicators(layerID))
        if (ind) ind->Calculate(candles);
}

static inline void UpdateLiveLayerIndicators(
    int layerID,
    int lastIdx,
    double currentPrice,
    double currentVolume,
    const std::vector<Candle>& candles)
{
    (void)lastIdx; (void)currentPrice; (void)currentVolume;
    RecalculateLayerIndicators(layerID, candles);
}

// =========================================================
// STATE MODAL PER LAYER
// =========================================================
static std::map<int, bool> s_layerModalOpen;
static std::map<int, char[128]> s_layerSearch;

static inline void OpenIndicatorModalForLayer(int layerID) {
    s_layerModalOpen[layerID] = true;
}

// =========================================================
// HELPERS
// =========================================================
static inline void AddIndicatorToLayer(
    Indicator* ind,
    const std::vector<Candle>& candles,
    int layerID)
{
    if (!ind) return;
    ind->Calculate(candles);
    GetLayerIndicators(layerID).push_back(ind);
}

static inline void AddIndicatorToTab(
    Indicator* ind,
    const std::vector<Candle>& candles,
    std::vector<Indicator*>& targetInds)
{
    if (!ind) return;
    ind->Calculate(candles);
    targetInds.push_back(ind);
}

static inline bool IsSearchMatch(const char* itemName, const char* searchStr) {
    if (searchStr[0] == 0) return true;
    std::string a = itemName, b = searchStr;
    std::transform(a.begin(), a.end(), a.begin(), ::tolower);
    std::transform(b.begin(), b.end(), b.begin(), ::tolower);
    return a.find(b) != std::string::npos;
}

// =========================================================
// RENDER MODAL — VERSI LAYER (int layerID)
// Dipanggil dari main_Chart.cpp
// =========================================================
static inline void RenderIndicatorModal(
    const std::vector<Candle>& currentCandles,
    int layerID)
{
    bool& isOpen = s_layerModalOpen[layerID];
    if (!isOpen) return;

    char popupID[64];
    snprintf(popupID, sizeof(popupID), "Library Indikator##L%d", layerID);
    ImGui::OpenPopup(popupID);

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(500, 600));

    ImGui::PushStyleColor(ImGuiCol_PopupBg, g_colorPanel);
    ImGui::PushStyleColor(ImGuiCol_Border,  g_colorHeader);
    ImGui::PushStyleColor(ImGuiCol_Text,    g_colorText);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);

    if (s_layerSearch.find(layerID) == s_layerSearch.end())
        memset(s_layerSearch[layerID], 0, 128);
    char* searchBuf = s_layerSearch[layerID];

    if (ImGui::BeginPopupModal(popupID, &isOpen,
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar))
    {
        ImGui::TextColored(
            ImVec4(g_colorText.x, g_colorText.y, g_colorText.z, 0.7f),
            "PENCARIAN INDIKATOR");
        ImGui::SetNextItemWidth(-1);
        ImGui::InputTextWithHint("##SearchL", "Contoh: SMA, Bollinger, RSI...",
            searchBuf, 128);
        ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0,0,0,0));
        ImVec4 hc = g_colorHeader; hc.w = 0.3f;
        ImVec4 ac = g_colorHeader; ac.w = 0.5f;
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hc);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ac);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.8f, 1.0f, 1.0f));
        if (ImGui::Button(u8"\u2795 Create New Script (Custom)", ImVec2(-1, 35)))
            printf("Buka Script Editor (Layer %d)...\n", layerID);
        ImGui::PopStyleColor(4);

        ImGui::Spacing(); ImGui::Separator();
        ImGui::PushStyleColor(ImGuiCol_ChildBg, g_colorBg);
        ImGui::TextDisabled("DAFTAR TEKNIKAL");
        ImGui::BeginChild("ListAreaL", ImVec2(0, -40), true);

#define LSEL(label, expr) \
    if (IsSearchMatch(label, searchBuf)) { \
        if (ImGui::Selectable(label, false, 0, ImVec2(0, 30))) { \
            AddIndicatorToLayer(expr, currentCandles, layerID); \
            isOpen = false; ImGui::CloseCurrentPopup(); } }

        LSEL("Moving Average (SMA)",          new SMAIndicator(14, ImVec4(1,0.8f,0.2f,1)))
        LSEL("Moving Average (EMA)",          new EMAIndicator(14, ImVec4(1,0.5f,0.1f,1)))
        LSEL("Relative Strength Index (RSI)", new RSIIndicator(14, ImVec4(0,1,1,1)))
        LSEL("Bollinger Indicator",           new BollingerIndicator(20, 2.0))
        LSEL("Volume",                        new VolumeIndicator(0, ImVec4(0.2f,0.8f,0.2f,0.6f)))
        LSEL("MACD",                              new MACDIndicator())
        LSEL("Stochastic",                        new StochIndicator())
        LSEL("Average True Range (ATR)",          new ATRIndicator())
        LSEL("Average Directional Index (ADX)",   new ADXIndicator())
        LSEL("Commodity Channel Index (CCI)",     new CCIIndicator())
        LSEL("Williams Percent Range",            new WilliamsRIndicator())
        LSEL("Money Flow Index (MFI)",            new MFIIndicator())
        LSEL("Rate of Change (ROC)",              new ROCIndicator())
        LSEL("On Balance Volume (OBV)",           new OBVIndicator())
        LSEL("Supertrend",                        new SupertrendIndicator())
#undef LSEL

        ImGui::EndChild();
        ImGui::PopStyleColor();
        ImGui::Separator(); ImGui::Dummy(ImVec2(0,2));
        if (ImGui::Button("Tutup", ImVec2(-1,0))) {
            isOpen = false; ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar();
}

// =========================================================
// RENDER MODAL — VERSI LEGACY (vector<Indicator*>&)
// Dipanggil dari main.cpp (tab utama)
// =========================================================
static bool s_legacyModalOpen  = false;
static char s_legacySearch[128] = {};

static inline void RenderIndicatorModal(
    const std::vector<Candle>& currentCandles,
    std::vector<Indicator*>&   targetInds)
{
    if (!s_legacyModalOpen) return;

    ImGui::OpenPopup("Library Indikator");
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(500, 600));

    ImGui::PushStyleColor(ImGuiCol_PopupBg, g_colorPanel);
    ImGui::PushStyleColor(ImGuiCol_Border,  g_colorHeader);
    ImGui::PushStyleColor(ImGuiCol_Text,    g_colorText);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);

    if (ImGui::BeginPopupModal("Library Indikator", &s_legacyModalOpen,
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar))
    {
        ImGui::TextColored(
            ImVec4(g_colorText.x, g_colorText.y, g_colorText.z, 0.7f),
            "PENCARIAN INDIKATOR");
        ImGui::SetNextItemWidth(-1);
        ImGui::InputTextWithHint("##Search", "Contoh: SMA, Bollinger, RSI...",
            s_legacySearch, IM_ARRAYSIZE(s_legacySearch));
        ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0,0,0,0));
        ImVec4 hc = g_colorHeader; hc.w = 0.3f;
        ImVec4 ac = g_colorHeader; ac.w = 0.5f;
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hc);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ac);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.8f, 1.0f, 1.0f));
        if (ImGui::Button(u8"\u2795 Create New Script (Custom)", ImVec2(-1,35)))
            printf("Buka Script Editor...\n");
        ImGui::PopStyleColor(4);

        ImGui::Spacing(); ImGui::Separator();
        ImGui::PushStyleColor(ImGuiCol_ChildBg, g_colorBg);
        ImGui::TextDisabled("DAFTAR TEKNIKAL");
        ImGui::BeginChild("ListArea", ImVec2(0,-40), true);

#define LLEG(label, expr) \
    if (IsSearchMatch(label, s_legacySearch)) { \
        if (ImGui::Selectable(label, false, 0, ImVec2(0,30))) { \
            AddIndicatorToTab(expr, currentCandles, targetInds); \
            s_legacyModalOpen = false; ImGui::CloseCurrentPopup(); } }

        LLEG("Moving Average (SMA)",          new SMAIndicator(14, ImVec4(1,0.8f,0.2f,1)))
        LLEG("Moving Average (EMA)",          new EMAIndicator(14, ImVec4(1,0.5f,0.1f,1)))
        LLEG("Relative Strength Index (RSI)", new RSIIndicator(14, ImVec4(0,1,1,1)))
        LLEG("Bollinger Indicator",           new BollingerIndicator(20, 2.0))
        LLEG("Volume",                        new VolumeIndicator(0, ImVec4(0.2f,0.8f,0.2f,0.6f)))
        LLEG("MACD",                              new MACDIndicator())
        LLEG("Stochastic",                        new StochIndicator())
        LLEG("Average True Range (ATR)",          new ATRIndicator())
        LLEG("Average Directional Index (ADX)",   new ADXIndicator())
        LLEG("Commodity Channel Index (CCI)",     new CCIIndicator())
        LLEG("Williams Percent Range",            new WilliamsRIndicator())
        LLEG("Money Flow Index (MFI)",            new MFIIndicator())
        LLEG("Rate of Change (ROC)",              new ROCIndicator())
        LLEG("On Balance Volume (OBV)",           new OBVIndicator())
        LLEG("Supertrend",                        new SupertrendIndicator())
#undef LLEG

        ImGui::EndChild();
        ImGui::PopStyleColor();
        ImGui::Separator(); ImGui::Dummy(ImVec2(0,2));
        if (ImGui::Button("Tutup", ImVec2(-1,0))) {
            s_legacyModalOpen = false; ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar();
}

// Alias untuk kode lama yang pakai showIndicatorModal langsung
static inline void OpenIndicatorModal() { s_legacyModalOpen = true; }


// #############################################################################
// #############################################################################
//
//  ORDER FLOW OVERLAY SECTION
//
//  Semua yang berhubungan dengan OrderFlow di indicator list
//  (Volume Profile, Footprint, Naked VPOC) dikendalikan dari sini.
//
//  Arsitektur:
//
//    User buka indicator list (expanded)
//        |
//        +-- Row: "Volume Profile"     [amber dot] [eye] [gear] [trash]
//        |       gear -> ToggleVPSettings() -> buka panel VP
//        |       trash -> showVolumeProfile = false
//        |
//        +-- Row: "Footprint (Bar)"     [blue dot]  [eye] [gear] [trash]
//        |       gear -> ToggleFPSettings() -> buka panel FP
//        |       trash -> renderStyle = RENDER_CANDLE (kembali ke candle)
//        |       MUNCUL HANYA saat IsFootprintStyle() == true
//        |
//        +-- Row: "Naked VPOC"          [red dot]   [eye] [gear] [trash]
//                gear -> ToggleFPSettings() -> buka panel FP
//                trash -> showNakedVPOC = false
//
//  Rule mutual exclusion:
//    - VP Settings panel dan FP Settings panel TIDAK bisa buka bersamaan.
//    - ToggleVPSettings() otomatis tutup FP panel, dan sebaliknya.
//    - Tapi VP overlay DAN FP style BISA aktif bersamaan di chart.
//
// #############################################################################
// #############################################################################

// =========================================================
// COUNT — berapa OF items yang aktif untuk tab ini?
// Dipakai untuk menghitung tinggi panel indicator list.
//
// Item yang dihitung:
//   1. Volume Profile    (tab->showVolumeProfile)
//   2. Footprint style   (IsFootprintStyle(tab->renderStyle))
//   3. Naked VPOC        (tab->orderFlowRenderer.showNakedVPOC)
// =========================================================
static inline int CountOrderFlowItems(const ChartTab* tab) {
    if (!tab) return 0;
    int n = 0;
    if (tab->showVolumeProfile)                n++;   // VP overlay
    if (IsFootprintStyle(tab->renderStyle))     n++;   // FP Overlay/Profile/Bar
    if (tab->orderFlowRenderer.showNakedVPOC)   n++;   // Naked VPOC
    return n;
}

// =========================================================
// RENDER SETTINGS PANELS
// Delegasi ke OrderFlowSettingsUI.h
// Dipanggil 1x per frame dari RenderActiveIndicatorsOverlay
// TIDAK perlu conditional — di dalam, masing2 cek open state sendiri.
// =========================================================
static inline void RenderOrderFlowSettingsPanels(ChartTab* tab) {
    if (!tab) return;
    // OrderFlowSettingsUI.h punya VP panel dan FP panel,
    // masing2 punya s_vpSettingsOpen / s_fpSettingsOpen sendiri.
    // Hanya render yang open saja (internal check).
    RenderOrderFlowSettings(tab);
}

// =========================================================
// RENDER ORDER FLOW INDICATOR ROWS
//
// Dipanggil dari dalam expanded panel di RenderActiveIndicatorsOverlay
// SETELAH semua regular indicator rows selesai di-render.
//
// Parameter:
//   tab         -> ChartTab aktif
//   dl          -> ImDrawList (dari ImGui::GetWindowDrawList())
//   panMin      -> pojok kiri atas panel expanded
//   panMax      -> pojok kanan bawah panel expanded
//   startRowY   -> Y position baris pertama OF item
//   PAD         -> padding dalam panel
//   ROW_H       -> tinggi tiap baris (sama dengan indicator row)
//   DOT_R       -> radius dot warna
//   ICON_SZ     -> ukuran icon eye/gear/trash
//   ICON_GAP    -> jarak antar icon
//   hasSeparator -> true = render separator tipis sebelum baris pertama
//
// Return: jumlah baris yang di-render
// =========================================================
static inline int RenderOrderFlowIndicatorRows(
    ChartTab* tab,
    ImDrawList* dl,
    ImVec2 panMin,
    ImVec2 panMax,
    float startRowY,
    float PAD,
    float ROW_H,
    float DOT_R,
    float ICON_SZ,
    float ICON_GAP,
    bool hasSeparator,
    ImTextureID texEyeShow,
    ImTextureID texEyeHide,
    ImTextureID texIndSettings,
    ImTextureID texTrash2)
{
    if (!tab) return 0;

    int count = CountOrderFlowItems(tab);
    if (count == 0) return 0;

    // ── Icon textures (dari extern di caller) ────────────────────
    float iconGroupW = ICON_SZ * 3 + ICON_GAP * 2;
    float curY = startRowY;

    // ════════════════════════════════════════════════════════════
    // ROW 1 — VOLUME PROFILE (amber)
    // Muncul saat tab->showVolumeProfile == true
    // ════════════════════════════════════════════════════════════
    if (tab->showVolumeProfile) {
        ImGui::PushID("ofl_vp_row");
        float rowY = curY;

        // Separator
        if (hasSeparator)
            dl->AddLine(
                ImVec2(panMin.x + 6, rowY),
                ImVec2(panMax.x - 6, rowY),
                IM_COL32(40, 42, 60, 100));

        // Dot amber
        dl->AddCircleFilled(
            ImVec2(panMin.x + PAD + DOT_R, rowY + ROW_H * 0.5f),
            DOT_R, IM_COL32(239, 159, 39, 220));

        // Nama + badge status
        ImGui::SetCursorScreenPos(ImVec2(panMin.x + PAD + DOT_R * 2 + 6.f,
            rowY + (ROW_H - ImGui::GetTextLineHeight()) * 0.5f));
        ImGui::TextColored(ImVec4(0.94f, 0.62f, 0.15f, 1.0f), "Volume Profile");

        // Badge "ON" kecil di sebelah nama
        ImVec2 nameEnd = ImGui::GetCursorScreenPos();
        const char* badgeVP = "ON";
        ImVec2 badgeSz = ImGui::CalcTextSize(badgeVP);
        float badgeX = nameEnd.x + 6.0f;
        float badgeY = rowY + (ROW_H - badgeSz.y) * 0.5f;
        dl->AddRectFilled(
            ImVec2(badgeX - 2, badgeY - 1),
            ImVec2(badgeX + badgeSz.x + 4, badgeY + badgeSz.y + 1),
            IM_COL32(239, 159, 39, 40), 2.0f);
        dl->AddText(ImVec2(badgeX, badgeY),
            IM_COL32(239, 159, 39, 180), badgeVP);

        // Icon group: Eye | Gear | Trash
        float iconStartX = panMax.x - PAD - iconGroupW;
        float iconY      = rowY + (ROW_H - ICON_SZ) * 0.5f;

        // Eye — VP selalu visible kalau aktif (eye dekoratif)
        ImGui::SetCursorScreenPos(ImVec2(iconStartX, iconY));
        ImGui::ImageButton("##ofl_vp_eye", texEyeShow,
            ImVec2(ICON_SZ, ICON_SZ), ImVec2(0, 0), ImVec2(1, 1),
            ImVec4(0, 0, 0, 0), ImVec4(0.9f, 0.6f, 0.1f, 0.9f));

        // Gear -> VP Settings
        ImGui::SameLine(0, ICON_GAP);
        if (ImGui::ImageButton("##ofl_vp_gear", texIndSettings,
                ImVec2(ICON_SZ, ICON_SZ), ImVec2(0, 0), ImVec2(1, 1),
                ImVec4(0, 0, 0, 0), ImVec4(0.94f, 0.62f, 0.15f, 0.85f)))
            ToggleVPSettings();

        // Trash -> matikan VP
        ImGui::SameLine(0, ICON_GAP);
        if (ImGui::ImageButton("##ofl_vp_del", texTrash2,
                ImVec2(ICON_SZ, ICON_SZ), ImVec2(0, 0), ImVec2(1, 1),
                ImVec4(0, 0, 0, 0), ImVec4(0.80f, 0.28f, 0.28f, 0.85f))) {
            tab->showVolumeProfile = false;
        }

        ImGui::PopID();
        curY += ROW_H;
    }

    // ════════════════════════════════════════════════════════════
    // ROW 2 — FOOTPRINT STYLE (biru)
    // Muncul HANYA saat IsFootprintStyle(tab->renderStyle) == true
    // Ini menandakan chart style sedang dalam mode FP.
    // Gear -> FP Settings panel
    // Trash -> kembali ke Candle style (bukan matikan data)
    // ════════════════════════════════════════════════════════════
    if (IsFootprintStyle(tab->renderStyle)) {
        ImGui::PushID("ofl_fp_row");
        float rowY = curY;

        // Separator
        dl->AddLine(
            ImVec2(panMin.x + 6, rowY),
            ImVec2(panMax.x - 6, rowY),
            IM_COL32(40, 42, 60, 100));

        // Dot biru
        dl->AddCircleFilled(
            ImVec2(panMin.x + PAD + DOT_R, rowY + ROW_H * 0.5f),
            DOT_R, IM_COL32(100, 180, 255, 220));

        // Nama — pakai nama style aktif (FP Overlay / FP Profile / FP Bar)
        const char* fpName = CandleStyleName(tab->renderStyle);
        ImGui::SetCursorScreenPos(ImVec2(panMin.x + PAD + DOT_R * 2 + 6.f,
            rowY + (ROW_H - ImGui::GetTextLineHeight()) * 0.5f));
        ImGui::TextColored(ImVec4(0.40f, 0.70f, 1.0f, 1.0f), "Footprint");

        // Badge nama style di sebelah
        ImVec2 nameEnd = ImGui::GetCursorScreenPos();
        ImVec2 fpSz = ImGui::CalcTextSize(fpName);
        float badgeX = nameEnd.x + 6.0f;
        float badgeY = rowY + (ROW_H - fpSz.y) * 0.5f;
        dl->AddRectFilled(
            ImVec2(badgeX - 2, badgeY - 1),
            ImVec2(badgeX + fpSz.x + 4, badgeY + fpSz.y + 1),
            IM_COL32(100, 180, 255, 40), 2.0f);
        dl->AddText(ImVec2(badgeX, badgeY),
            IM_COL32(100, 180, 255, 180), fpName);

        // Icon group: Eye | Gear | Trash
        float iconStartX = panMax.x - PAD - iconGroupW;
        float iconY      = rowY + (ROW_H - ICON_SZ) * 0.5f;

        // Eye — FP style selalu visible
        ImGui::SetCursorScreenPos(ImVec2(iconStartX, iconY));
        ImGui::ImageButton("##ofl_fp_eye", texEyeShow,
            ImVec2(ICON_SZ, ICON_SZ), ImVec2(0, 0), ImVec2(1, 1),
            ImVec4(0, 0, 0, 0), ImVec4(0.4f, 0.7f, 1.0f, 0.9f));

        // Gear -> FP Settings
        ImGui::SameLine(0, ICON_GAP);
        if (ImGui::ImageButton("##ofl_fp_gear", texIndSettings,
                ImVec2(ICON_SZ, ICON_SZ), ImVec2(0, 0), ImVec2(1, 1),
                ImVec4(0, 0, 0, 0), ImVec4(0.40f, 0.70f, 1.0f, 0.85f)))
            ToggleFPSettings();

        // Trash -> kembali ke Candle style (BUKAN matikan overlay)
        ImGui::SameLine(0, ICON_GAP);
        if (ImGui::ImageButton("##ofl_fp_del", texTrash2,
                ImVec2(ICON_SZ, ICON_SZ), ImVec2(0, 0), ImVec2(1, 1),
                ImVec4(0, 0, 0, 0), ImVec4(0.80f, 0.28f, 0.28f, 0.85f))) {
            tab->renderStyle = RENDER_CANDLE;
        }

        ImGui::PopID();
        curY += ROW_H;
    }

    // ════════════════════════════════════════════════════════════
    // ROW 3 — NAKED VPOC (merah)
    // Muncul saat tab->orderFlowRenderer.showNakedVPOC == true
    // Gear -> FP Settings (nVPOC bagian dari FP detection settings)
    // ════════════════════════════════════════════════════════════
    if (tab->orderFlowRenderer.showNakedVPOC) {
        ImGui::PushID("ofl_nvpoc_row");
        float rowY = curY;

        // Separator
        dl->AddLine(
            ImVec2(panMin.x + 6, rowY),
            ImVec2(panMax.x - 6, rowY),
            IM_COL32(40, 42, 60, 100));

        // Dot merah
        dl->AddCircleFilled(
            ImVec2(panMin.x + PAD + DOT_R, rowY + ROW_H * 0.5f),
            DOT_R, IM_COL32(230, 70, 70, 220));

        // Nama
        ImGui::SetCursorScreenPos(ImVec2(panMin.x + PAD + DOT_R * 2 + 6.f,
            rowY + (ROW_H - ImGui::GetTextLineHeight()) * 0.5f));
        ImGui::TextColored(ImVec4(1.0f, 0.40f, 0.40f, 1.0f), "Naked VPOC");

        // Badge "ON"
        ImVec2 nameEnd = ImGui::GetCursorScreenPos();
        const char* badgeNV = "ON";
        ImVec2 badgeSz = ImGui::CalcTextSize(badgeNV);
        float badgeX = nameEnd.x + 6.0f;
        float badgeY = rowY + (ROW_H - badgeSz.y) * 0.5f;
        dl->AddRectFilled(
            ImVec2(badgeX - 2, badgeY - 1),
            ImVec2(badgeX + badgeSz.x + 4, badgeY + badgeSz.y + 1),
            IM_COL32(230, 70, 70, 40), 2.0f);
        dl->AddText(ImVec2(badgeX, badgeY),
            IM_COL32(230, 70, 70, 180), badgeNV);

        // Icon group: Eye | Gear | Trash
        float iconStartX = panMax.x - PAD - iconGroupW;
        float iconY      = rowY + (ROW_H - ICON_SZ) * 0.5f;

        // Eye — dekoratif
        ImGui::SetCursorScreenPos(ImVec2(iconStartX, iconY));
        ImGui::ImageButton("##ofl_nv_eye", texEyeShow,
            ImVec2(ICON_SZ, ICON_SZ), ImVec2(0, 0), ImVec2(1, 1),
            ImVec4(0, 0, 0, 0), ImVec4(1.0f, 0.4f, 0.4f, 0.9f));

        // Gear -> FP Settings (nVPOC = FP detection feature)
        ImGui::SameLine(0, ICON_GAP);
        if (ImGui::ImageButton("##ofl_nv_gear", texIndSettings,
                ImVec2(ICON_SZ, ICON_SZ), ImVec2(0, 0), ImVec2(1, 1),
                ImVec4(0, 0, 0, 0), ImVec4(1.0f, 0.4f, 0.4f, 0.85f)))
            ToggleFPSettings();

        // Trash -> matikan nVPOC
        ImGui::SameLine(0, ICON_GAP);
        if (ImGui::ImageButton("##ofl_nv_del", texTrash2,
                ImVec2(ICON_SZ, ICON_SZ), ImVec2(0, 0), ImVec2(1, 1),
                ImVec4(0, 0, 0, 0), ImVec4(0.80f, 0.28f, 0.28f, 0.85f))) {
            tab->orderFlowRenderer.showNakedVPOC = false;
        }

        ImGui::PopID();
        curY += ROW_H;
    }

    return count;
}
