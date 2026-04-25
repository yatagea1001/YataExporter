#pragma once
// ================================================================
// CandleStyleManager.h
// ================================================================
// Popup: Grid kartu 2-kolom, tiap kartu punya mini preview chart
// yang di-draw manual via ImDrawList — konsisten dgn style navigasi.
// ================================================================

#include "imgui.h"
#include "MultiChart.h"
#include "OrderFlowSettingsUI.h"
#include <string>
#include <vector>
#include <cmath>
#include <algorithm>   // std::max, std::min, std::clamp

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

extern ImVec4 g_colorBg, g_colorPanel, g_colorText, g_colorHeader;
extern bool   g_replayActive; // dari main.cpp — true saat replay mode aktif

// ----------------------------------------------------------------
// DEFINISI STYLE
// ----------------------------------------------------------------
struct RenderStyleDef {
    CandleRenderStyle id;
    const char* name;
    const char* icon;
    const char* tooltip;
    const char* desc;
};

static const RenderStyleDef k_StyleDefs[] = {
    { RENDER_CANDLE,     "Candle",      "Candlestick klasik",                             "OHLC classic"      },
    { RENDER_LINE,       "Line",        "Garis harga close",                              "Close price line"  },
    { RENDER_AREA,       "Area",        "Area fill bawah close",                          "Filled area"       },
    { RENDER_FP_OVERLAY, "FootPrint Overlay", "Footprint Overlay — bid/ask box di atas candle", "Bid/Ask overlay"   },
    { RENDER_FP_PROFILE, "FootPrint Profile", "Footprint Profile — candle tipis + grid bid/ask","Bid/Ask profile"   },
    { RENDER_FP_BAR,     "FootPrint Bar",     "Footprint Bar — proportional bar buy/sell",      "Buy/sell bars"     },
};
static const int k_StyleCount = (int)RENDER_STYLE_COUNT;

// ----------------------------------------------------------------
// HELPER: Draw mini chart preview per style
// ----------------------------------------------------------------
static void DrawMiniPreview(ImDrawList* dl, ImVec2 pMin, ImVec2 pMax,
                            CandleRenderStyle style, bool isActive)
{
    const float W  = pMax.x - pMin.x;
    const float H  = pMax.y - pMin.y;

    // Warna preview
    ImU32 colBg    = IM_COL32( 15,  18,  26, 255);
    ImU32 colGrid  = IM_COL32( 50,  55,  70, 100);
    ImU32 colBull  = isActive ? IM_COL32( 80,220,130,255) : IM_COL32( 60,180,110,200);
    ImU32 colBear  = isActive ? IM_COL32(230, 80, 80,255) : IM_COL32(200, 70, 70,200);
    ImU32 colLine  = isActive ? IM_COL32( 80,200,255,255) : IM_COL32( 60,160,220,200);
    ImU32 colAreaF = isActive ? IM_COL32( 80,200,255,110) : IM_COL32( 60,160,220, 70);
    ImU32 colBuy   = isActive ? IM_COL32( 80,220,130,190) : IM_COL32( 60,180,110,140);
    ImU32 colSell  = isActive ? IM_COL32(230, 80, 80,190) : IM_COL32(200, 70, 70,140);

    dl->AddRectFilled(pMin, pMax, colBg, 6.0f);
    // Grid horizontal
    for (int i = 1; i < 3; i++) {
        float gy = pMin.y + H * (i / 3.0f);
        dl->AddLine(ImVec2(pMin.x, gy), ImVec2(pMax.x, gy), colGrid, 0.5f);
    }

    struct Bar { float c, o, h, l; };
    static const Bar bars[] = {
        {0.42f,0.35f,0.52f,0.28f},
        {0.55f,0.42f,0.62f,0.38f},
        {0.50f,0.56f,0.65f,0.44f},
        {0.65f,0.50f,0.72f,0.46f},
        {0.60f,0.66f,0.74f,0.54f},
        {0.72f,0.60f,0.78f,0.56f},
        {0.68f,0.73f,0.80f,0.62f},
    };
    const int  nBars = 7;
    const float padX = W * 0.06f;
    const float padY = H * 0.08f;
    const float barW = (W - padX * 2.0f) / nBars;
    const float iW   = barW * 0.55f;   // inner body width

    auto toY   = [&](float v) { return (pMax.y - padY) - v * (H - padY * 2.0f); };
    auto barCx = [&](int i)   { return pMin.x + padX + (i + 0.5f) * barW; };

    switch (style) {

    case RENDER_CANDLE:
        for (int i = 0; i < nBars; i++) {
            const Bar& b = bars[i];
            float x = barCx(i);
            ImU32 c = (b.c >= b.o) ? colBull : colBear;
            dl->AddLine(ImVec2(x, toY(b.h)), ImVec2(x, toY(b.l)), c, 1.0f);
            float yT = toY(std::max(b.c,b.o)), yB = toY(std::min(b.c,b.o));
            if (yB - yT < 1.5f) yB = yT + 1.5f;
            dl->AddRectFilled(ImVec2(x-iW*.5f,yT), ImVec2(x+iW*.5f,yB), c, 1.0f);
        }
        break;

    case RENDER_LINE:
        for (int i = 0; i < nBars-1; i++)
            dl->AddLine(ImVec2(barCx(i),toY(bars[i].c)),
                        ImVec2(barCx(i+1),toY(bars[i+1].c)), colLine, 1.8f);
        for (int i = 0; i < nBars; i++)
            dl->AddCircleFilled(ImVec2(barCx(i),toY(bars[i].c)), 2.0f, colLine);
        break;

    case RENDER_AREA: {
        ImVec2 pts[9];
        pts[0] = ImVec2(barCx(0), pMax.y - padY);
        for (int i = 0; i < nBars; i++) pts[i+1] = ImVec2(barCx(i), toY(bars[i].c));
        pts[nBars+1] = ImVec2(barCx(nBars-1), pMax.y - padY);
        dl->AddConvexPolyFilled(pts, nBars+2, colAreaF);
        for (int i = 0; i < nBars-1; i++)
            dl->AddLine(ImVec2(barCx(i),toY(bars[i].c)),
                        ImVec2(barCx(i+1),toY(bars[i+1].c)), colLine, 1.5f);
        break;
    }

    case RENDER_FP_OVERLAY:
        for (int i = 0; i < nBars; i++) {
            const Bar& b = bars[i];
            float x = barCx(i);
            ImU32 c = (b.c>=b.o)?colBull:colBear;
            dl->AddLine(ImVec2(x,toY(b.h)),ImVec2(x,toY(b.l)),c,0.8f);
            float yT=toY(std::max(b.c,b.o)),yB=toY(std::min(b.c,b.o));
            if (yB-yT<1.5f) yB=yT+1.5f;
            dl->AddRect(ImVec2(x-iW*.5f,yT),ImVec2(x+iW*.5f,yB),c,0.8f,0,0.8f);
            float bH = yB-yT;
            if (bH>4.f) {
                float rH=bH/3.f;
                for (int r=0;r<3;r++) {
                    float ry=yT+r*rH, fw=iW*(r%2==0?.6f:.9f);
                    ImU32 fc=(r%2==0)?colBuy:colSell;
                    dl->AddRectFilled(ImVec2(x-fw*.5f,ry+.5f),ImVec2(x+fw*.5f,ry+rH-.5f),fc,.5f);
                }
            }
        }
        break;

    case RENDER_FP_PROFILE:
        for (int i = 0; i < nBars; i++) {
            const Bar& b = bars[i];
            float x=barCx(i), yT=toY(b.h), yB=toY(b.l);
            ImU32 c=(b.c>=b.o)?colBull:colBear;
            dl->AddLine(ImVec2(x,yT),ImVec2(x,yB),c,0.8f);
            float rng=yB-yT;
            if (rng>4.f) {
                float step=rng/4.f;
                for (int r=0;r<4;r++) {
                    float ry=yT+r*step, frac=.4f+.4f*fabsf(sinf((float)(i*3+r)));
                    float bL=iW*1.2f*frac;
                    ImU32 cr=(r%2==0)?colBuy:colSell;
                    dl->AddRectFilled(ImVec2(x,ry+.5f),ImVec2(x+bL,ry+step-1.f),cr,.5f);
                }
            }
        }
        break;

    case RENDER_FP_BAR:
        for (int i = 0; i < nBars; i++) {
            const Bar& b=bars[i];
            float x=barCx(i), yT=toY(b.h), yB=toY(b.l), hw=iW*.5f;
            float bf=.3f+.5f*fabsf(sinf((float)(i*2.1f))), sf=1.f-bf;
            float bH=yB-yT;
            dl->AddRectFilled(ImVec2(x-hw,yT),ImVec2(x,yT+bH*bf),colBuy,.5f);
            dl->AddRectFilled(ImVec2(x,yT),ImVec2(x+hw,yT+bH*sf),colSell,.5f);
            dl->AddRect(ImVec2(x-hw,yT),ImVec2(x+hw,yB),IM_COL32(80,80,80,80),.5f,0,.5f);
        }
        break;

    default: break;
    }
}

// ================================================================
// MANAGER
// ================================================================
class CandleStyleManager {
public:

    void ApplyToTab(int tabId, CandleRenderStyle style) {
        ChartTab* tab = g_chartManager.GetById(tabId);
        if (!tab) return;
        tab->renderStyle = style;
    }

    void ApplyToActive(CandleRenderStyle style) {
        ChartTab* tab = g_chartManager.GetActiveTab();
        if (!tab) return;
        tab->renderStyle = style;
    }

    void CycleActive() {
        ChartTab* tab = g_chartManager.GetActiveTab();
        if (!tab) return;
        tab->renderStyle = (CandleRenderStyle)(((int)tab->renderStyle + 1) % k_StyleCount);
    }

    const RenderStyleDef* GetActiveDef() {
        ChartTab* tab = g_chartManager.GetActiveTab();
        if (!tab) return &k_StyleDefs[0];
        int idx = (int)tab->renderStyle;
        if (idx < 0 || idx >= k_StyleCount) return &k_StyleDefs[0];
        return &k_StyleDefs[idx];
    }

    // ─────────────────────────────────────────────────────────────
    // RenderNavigationUI — tombol (konsisten dgn Navigasi lain) + popup
    // ─────────────────────────────────────────────────────────────
    void RenderNavigationUI(bool isHorizontal, float iconSize, bool segmentedMode = false) {

        ChartTab* tab = g_chartManager.GetActiveTab();
        if (isHorizontal) { ImGui::SameLine(); }
        else               { ImGui::Dummy(ImVec2(0,4)); }

        const RenderStyleDef* cur = GetActiveDef();

        // Konstanta sama dengan RenderNavigationPanel
        const float BTN_H   = iconSize;
        const float RADIUS  = 8.0f;
        const float PAD_X   = 18.0f;
        const float ICO_SZ  = iconSize * 0.62f;
        const float ICO_OY  = -2.0f;
        const float ICO_GAP = 10.0f;

        // Warna outline-only style
        ImVec4 btnBg     = ImVec4(0.f, 0.f, 0.f, 0.f);
        ImVec4 btnHover  = ImVec4(1.f, 1.f, 1.f, 0.08f);
        ImVec4 btnActive = ImVec4(1.f, 1.f, 1.f, 0.16f);
        ImVec4 btnBorder = ImVec4(1.f, 1.f, 1.f, 0.60f);
        ImVec4 btnText   = ImVec4(1.f, 1.f, 1.f, 1.0f);

        const char* lbl = cur->name;
        float bw  = PAD_X + ICO_SZ + ICO_GAP + ImGui::CalcTextSize(lbl).x + PAD_X;
        ImVec2 bp = ImGui::GetCursorScreenPos();
        float  cy = bp.y + BTN_H * 0.5f;

        ImGui::InvisibleButton("##btnCandleStyle", ImVec2(bw, BTN_H));
        bool hov = ImGui::IsItemHovered();
        bool act = ImGui::IsItemActive();

        ImDrawList* dlB = ImGui::GetWindowDrawList();
        ImVec2 bpMx = ImVec2(bp.x + bw, bp.y + BTN_H);
        ImVec4 bgV  = act ? btnActive : (hov ? btnHover : btnBg);

        // Segmented mode: skip individual bg/border (handled by container)
        if (!segmentedMode) {
            dlB->AddRectFilled(bp, bpMx, ImGui::ColorConvertFloat4ToU32(bgV), RADIUS);
            dlB->AddRect      (bp, bpMx, ImGui::ColorConvertFloat4ToU32(btnBorder), RADIUS, 0, 1.5f);
        }

        // Mini preview sebagai icon di dalam tombol
        {
            float  pw = ICO_SZ, ph = ICO_SZ * 0.80f;
            ImVec2 pm = ImVec2(bp.x + PAD_X, cy - ph * 0.5f + ICO_OY);
            ImVec2 px = ImVec2(pm.x + pw,    pm.y + ph);
            dlB->PushClipRect(bp, bpMx, true);
            DrawMiniPreview(dlB, pm, px, tab ? tab->renderStyle : RENDER_CANDLE, true);
            dlB->PopClipRect();
        }

        float fh = ImGui::GetTextLineHeight();
        dlB->AddText(
            ImVec2(bp.x + PAD_X + ICO_SZ + ICO_GAP, cy - fh * 0.5f),
            ImGui::ColorConvertFloat4ToU32(btnText), lbl
        );

        if (ImGui::IsItemClicked())
            ImGui::OpenPopup("##CandleStylePopup");
        if (hov) ImGui::SetTooltip("Ganti Chart Style");

        // ── POPUP ─────────────────────────────────────────────────
        ImGui::PushStyleColor(ImGuiCol_PopupBg,  ImVec4(0.09f, 0.10f, 0.14f, 0.97f));
        ImGui::PushStyleColor(ImGuiCol_Border,   ImVec4(1.0f,  1.0f,  1.0f,  0.10f));
        ImGui::PushStyleVar  (ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 14.0f));
        ImGui::PushStyleVar  (ImGuiStyleVar_ItemSpacing,   ImVec2(10.0f, 10.0f));
        ImGui::PushStyleVar  (ImGuiStyleVar_PopupRounding, 12.0f);

        if (ImGui::BeginPopup("##CandleStylePopup")) {

            ImDrawList* dl = ImGui::GetWindowDrawList();

            // ── Header ────────────────────────────────────────────
            ImGui::TextColored(ImVec4(1,1,1,0.40f), "CHART STYLE");
            if (tab) {
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(0.40f,0.80f,1.0f,0.80f),
                    " — %s", tab->label.c_str());
            }
            ImGui::Spacing();

            // ── Grid 2-kolom ──────────────────────────────────────
            const float CARD_W  = 126.0f;
            const float CARD_H  = 104.0f;
            const float PREV_H  = 62.0f;    // zona preview
            const float LABEL_H = CARD_H - PREV_H;
            const float CR      = 8.0f;
            const int   COLS    = 2;

            int curStyle = tab ? (int)tab->renderStyle : 0;

            for (int i = 0; i < k_StyleCount; i++) {
                const auto& def  = k_StyleDefs[i];
                bool isAct       = (curStyle == i);
                bool isLastInRow = ((i % COLS) == COLS-1) || (i == k_StyleCount-1);

                ImVec2 cMin = ImGui::GetCursorScreenPos();
                ImVec2 cMax = ImVec2(cMin.x + CARD_W, cMin.y + CARD_H);

                ImGui::PushID(i);
                ImGui::InvisibleButton("##card", ImVec2(CARD_W, CARD_H));
                bool chov = ImGui::IsItemHovered();
                bool cclk = ImGui::IsItemClicked();
                ImGui::PopID();

                // Background kartu
                ImVec4 cbg = isAct
                    ? ImVec4(0.10f,0.25f,0.50f,0.85f)
                    : (chov ? ImVec4(1,1,1,0.07f) : ImVec4(1,1,1,0.03f));
                dl->AddRectFilled(cMin, cMax, ImGui::ColorConvertFloat4ToU32(cbg), CR);

                // Border kartu
                ImVec4 cbrd = isAct
                    ? ImVec4(0.30f,0.65f,1.0f,0.85f)
                    : (chov ? ImVec4(1,1,1,0.28f) : ImVec4(1,1,1,0.08f));
                dl->AddRect(cMin, cMax,
                    ImGui::ColorConvertFloat4ToU32(cbrd), CR, 0, isAct ? 1.5f : 1.0f);

                // Preview area (clip ke dalam kartu)
                ImVec2 pvMin = ImVec2(cMin.x + 6.f, cMin.y + 6.f);
                ImVec2 pvMax = ImVec2(cMax.x - 6.f, cMin.y + PREV_H - 4.f);
                dl->PushClipRect(pvMin, pvMax, true);
                DrawMiniPreview(dl, pvMin, pvMax, def.id, isAct);
                dl->PopClipRect();

                // Divider
                float ly = cMin.y + PREV_H;
                dl->AddLine(ImVec2(cMin.x+8.f, ly), ImVec2(cMax.x-8.f, ly),
                    isAct ? IM_COL32(80,160,255,70) : IM_COL32(255,255,255,12), 0.8f);

                // Nama style — center
                ImVec2 ns  = ImGui::CalcTextSize(def.name);
                float  nx  = cMin.x + (CARD_W - ns.x) * 0.5f;
                float  ny  = ly + (LABEL_H - ns.y) * 0.5f - 1.f;
                ImU32  nc  = isAct ? IM_COL32(120,200,255,255) : IM_COL32(220,220,220,200);
                dl->AddText(ImVec2(nx, ny), nc, def.name);

                // Badge ✓ pojok kanan atas
                if (isAct) {
                    dl->AddCircleFilled(ImVec2(cMax.x-9.f, cMin.y+9.f),
                        5.5f, IM_COL32(50,160,255,220));
                    dl->AddText(ImVec2(cMax.x-13.5f, cMin.y+2.5f),
                        IM_COL32(255,255,255,255), u8"\u2713");
                }

                // Tooltip
                if (chov) {
                    ImGui::BeginTooltip();
                    ImGui::TextColored(ImVec4(0.4f,0.8f,1.f,1.f),
                        "%s  %s", def.icon, def.name);
                    ImGui::Separator();
                    ImGui::TextUnformatted(def.tooltip);
                    ImGui::EndTooltip();
                }

                if (cclk) {
                    ApplyToActive(def.id);

                    // 🔥 V2: Request footprint data HANYA saat user pilih FP style
                    // Sebelumnya auto di setiap switch symbol → data FP (volume USD)
                    // bisa tercampur dengan OHLC saat timing race condition.
                    // Sekarang FP hanya diminta on-demand di sini.
                    #ifdef __EMSCRIPTEN__
                    if (IsFootprintStyle(def.id)) {
                        ChartTab* _ft = g_chartManager.GetActiveTab();
                        if (_ft) {
                            std::string sym = _ft->symbol;
                            // 🔥 Kirim bypassGate=1 saat replay aktif
                            // → requestFootprint akan load dari IDB langsung, skip server
                            // → notifyWASM_footprint bypass g_replayGateActive di C++
                            int bypass = g_replayActive ? 1 : 0;
                            EM_ASM({
                                var s = UTF8ToString($0);
                                var bypassGate = $1;
                                if (window.requestFootprint) {
                                    window.requestFootprint(s, 500, bypassGate);
                                    console.log('[FP] User pilih FP style, symbol: ' + s + ', replayBypass: ' + bypassGate);
                                }
                            }, sym.c_str(), bypass);
                        }
                    }
                    #endif

                    ImGui::CloseCurrentPopup();
                }

                if (!isLastInRow) ImGui::SameLine();
            }

            ImGui::Spacing();

            // ── VP OVERLAY TOGGLE ──────────────────────────────────
            // Bukan bagian dari renderStyle cycling — flag independen.
            // Aktif di atas style apapun: Candle + VP, FP Bar + VP, dsb.
            {
                ImVec2 la2 = ImGui::GetCursorScreenPos();
                la2.y += 1.f;
                dl->AddLine(la2, ImVec2(la2.x + 270.f, la2.y),
                    IM_COL32(255,255,255,18), 1.f);
                ImGui::Spacing();
                ImGui::TextColored(ImVec4(1,1,1,0.28f), "OVERLAY TAMBAHAN");
                ImGui::Spacing();

                bool vpOn = tab && tab->showVolumeProfile;

                // Kartu full-width untuk VP toggle
                const float VP_CARD_W = CARD_W * 2.0f + 10.0f; // span 2 kolom
                const float VP_CARD_H = 36.0f;
                ImVec2 vpMin = ImGui::GetCursorScreenPos();
                ImVec2 vpMax = ImVec2(vpMin.x + VP_CARD_W, vpMin.y + VP_CARD_H);

                ImGui::InvisibleButton("##vpToggle", ImVec2(VP_CARD_W, VP_CARD_H));
                bool vpHov = ImGui::IsItemHovered();
                bool vpClk = ImGui::IsItemClicked();

                // Background kartu — amber kalau aktif
                ImVec4 vpBg = vpOn
                    ? ImVec4(0.37f, 0.24f, 0.03f, 0.80f)
                    : (vpHov ? ImVec4(1,1,1,0.07f) : ImVec4(1,1,1,0.03f));
                dl->AddRectFilled(vpMin, vpMax,
                    ImGui::ColorConvertFloat4ToU32(vpBg), 8.0f);

                // Border
                ImVec4 vpBrd = vpOn
                    ? ImVec4(0.94f, 0.62f, 0.11f, 0.85f)
                    : (vpHov ? ImVec4(1,1,1,0.28f) : ImVec4(1,1,1,0.08f));
                dl->AddRect(vpMin, vpMax,
                    ImGui::ColorConvertFloat4ToU32(vpBrd), 8.0f, 0,
                    vpOn ? 1.5f : 1.0f);

                // Dot indikator kiri
                float cy2 = (vpMin.y + vpMax.y) * 0.5f;
                ImU32 dotCol = vpOn
                    ? IM_COL32(239,159,39,240)
                    : IM_COL32(120,120,120,120);
                dl->AddCircleFilled(ImVec2(vpMin.x + 14.f, cy2), 4.5f, dotCol);

                // Mini preview bars VP di kanan kartu
                {
                    float bx = vpMax.x - 52.f, bw2 = 48.f, bh2 = VP_CARD_H - 10.f;
                    float by2 = vpMin.y + 5.f;
                    // 5 bar VP mini
                    static const float vpBars[] = {0.3f, 0.9f, 0.6f, 0.75f, 0.45f};
                    ImU32 cVpBuy  = vpOn ? IM_COL32(40,185,90,200) : IM_COL32(40,140,70,100);
                    ImU32 cVpSell = vpOn ? IM_COL32(200,60,60,200) : IM_COL32(160,50,50,100);
                    ImU32 cVpPoc  = vpOn ? IM_COL32(255,215,0,220) : IM_COL32(180,150,30,100);
                    for (int b = 0; b < 5; b++) {
                        float barLen = vpBars[b] * bw2;
                        float by3 = by2 + b * (bh2 / 5.f) + 0.5f;
                        float bh3 = bh2 / 5.f - 1.f;
                        float splitX2 = bx + barLen * 0.55f;
                        ImU32 bc = (b == 1) ? cVpPoc : (b % 2 == 0 ? cVpSell : cVpBuy);
                        if (b == 1) {
                            // VPOC row
                            dl->AddRectFilled(ImVec2(bx, by3),
                                ImVec2(bx + barLen, by3 + bh3), cVpPoc, 1.f);
                            dl->AddRect(ImVec2(bx-0.5f, by3-0.5f),
                                ImVec2(bx + barLen + 0.5f, by3 + bh3 + 0.5f),
                                IM_COL32(255,215,0,vpOn?180:60), 0.5f, 0, 0.8f);
                        } else {
                            float sw2 = barLen * 0.45f, bw3 = barLen * 0.55f;
                            dl->AddRectFilled(ImVec2(bx, by3), ImVec2(bx+sw2, by3+bh3), cVpSell, 0.5f);
                            dl->AddRectFilled(ImVec2(bx+sw2, by3), ImVec2(bx+barLen, by3+bh3), cVpBuy, 0.5f);
                        }
                    }
                    // Garis VPOC putus-putus
                    if (vpOn) {
                        float vpocY2 = by2 + 1.f * (bh2 / 5.f) + bh2/10.f;
                        for (float dx2 = bx - 20.f; dx2 > vpMin.x + 28.f; dx2 -= 6.f)
                            dl->AddLine(ImVec2(dx2, vpocY2), ImVec2(dx2 - 4.f, vpocY2),
                                IM_COL32(255,215,0,120), 1.0f);
                    }
                }

                // Label teks
                const char* vpLbl = vpOn ? "Volume Profile Overlay  ON" : "Volume Profile Overlay";
                ImVec2 vpTs = ImGui::CalcTextSize(vpLbl);
                ImU32  vpTc = vpOn
                    ? IM_COL32(239, 180, 50, 255)
                    : IM_COL32(180, 180, 180, 180);
                dl->AddText(ImVec2(vpMin.x + 26.f, cy2 - vpTs.y * 0.5f), vpTc, vpLbl);
                if (vpClk && tab) {
                    tab->showVolumeProfile = !tab->showVolumeProfile;
                    // Request footprint kalau baru ON dan data mungkin belum ada
                    #ifdef __EMSCRIPTEN__
                    if (tab->showVolumeProfile) {
                        std::string sym2 = tab->symbol;
                        int bypass2 = g_replayActive ? 1 : 0;
                        EM_ASM({
                            var s = UTF8ToString($0);
                            var bypassGate = $1;
                            if (window.requestFootprint) {
                                window.requestFootprint(s, 500, bypassGate);
                                console.log('[VP] Toggle ON, symbol: ' + s + ', replayBypass: ' + bypassGate);
                            }
                        }, sym2.c_str(), bypass2);
                    }
                    #endif
                }

                if (vpHov) {
                    ImGui::BeginTooltip();
                    ImGui::TextColored(ImVec4(0.94f,0.72f,0.20f,1.f),
                        "Volume Profile Overlay");
                    ImGui::Separator();
                    ImGui::TextUnformatted(
                        vpOn
                        ? "Klik untuk nonaktifkan VP layer"
                        : "Tambah VP bars di atas chart style aktif\n"
                          "VPOC + Value Area 70% + buy/sell histogram");
                    ImGui::EndTooltip();
                }
            }

            ImGui::Spacing();
            ImVec2 la = ImGui::GetCursorScreenPos();
            la.y += 2.f;
            dl->AddLine(la, ImVec2(la.x + 270.f, la.y),
                IM_COL32(255,255,255,18), 1.f);
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(1,1,1,0.28f), "SETTINGS");
            ImGui::Spacing();

            // ── SETTINGS BUTTONS — per-style ─────────────────────
            // Tampilkan settings button sesuai style/overlay aktif
            {
                const float SET_BTN_W = CARD_W * 2.0f + 10.0f; // sama lebar dengan VP card
                const float SET_BTN_H = 32.0f;

                // VP Settings button — tampil saat VP overlay aktif
                if (tab && tab->showVolumeProfile) {
                    ImVec2 sMin = ImGui::GetCursorScreenPos();
                    ImVec2 sMax = ImVec2(sMin.x + SET_BTN_W, sMin.y + SET_BTN_H);

                    ImGui::InvisibleButton("##vpSettingsBtn", ImVec2(SET_BTN_W, SET_BTN_H));
                    bool sHov = ImGui::IsItemHovered();
                    bool sClk = ImGui::IsItemClicked();

                    // Background amber gelap
                    ImVec4 sbg = sHov
                        ? ImVec4(0.35f,0.22f,0.05f,0.90f)
                        : ImVec4(0.20f,0.14f,0.04f,0.70f);
                    dl->AddRectFilled(sMin, sMax, ImGui::ColorConvertFloat4ToU32(sbg), 6.0f);
                    dl->AddRect(sMin, sMax,
                        ImGui::ColorConvertFloat4ToU32(ImVec4(0.94f,0.62f,0.15f,0.60f)),
                        6.0f, 0, 1.0f);

                    // Gear icon + label
                    float scy = (sMin.y + sMax.y) * 0.5f;
                    dl->AddText(ImVec2(sMin.x + 12.f, scy - 7.f),
                        IM_COL32(239,180,50,220), "Settings");
                    dl->AddText(ImVec2(sMin.x + 12.f + ImGui::CalcTextSize("Settings").x + 8.f, scy - 7.f),
                        IM_COL32(239,159,39,160), "Volume Profile");

                    // Chevron kanan
                    dl->AddText(ImVec2(sMax.x - 18.f, scy - 7.f),
                        IM_COL32(239,159,39,120), ">");

                    if (sClk) {
                        ImGui::CloseCurrentPopup();
                        ToggleVPSettings();
                    }
                    if (sHov) {
                        ImGui::BeginTooltip();
                        ImGui::TextColored(ImVec4(0.94f,0.72f,0.20f,1.f),
                            "Volume Profile Settings");
                        ImGui::Separator();
                        ImGui::TextUnformatted("Toggle VP labels, VA shading, VPOC line");
                        ImGui::EndTooltip();
                    }
                    ImGui::Spacing();
                }

                // FP Settings button — tampil saat FP style aktif
                if (tab && IsFootprintStyle(tab->renderStyle)) {
                    ImVec2 sMin = ImGui::GetCursorScreenPos();
                    ImVec2 sMax = ImVec2(sMin.x + SET_BTN_W, sMin.y + SET_BTN_H);

                    ImGui::InvisibleButton("##fpSettingsBtn", ImVec2(SET_BTN_W, SET_BTN_H));
                    bool sHov = ImGui::IsItemHovered();
                    bool sClk = ImGui::IsItemClicked();

                    // Background biru gelap
                    ImVec4 sbg = sHov
                        ? ImVec4(0.08f,0.18f,0.35f,0.90f)
                        : ImVec4(0.06f,0.12f,0.22f,0.70f);
                    dl->AddRectFilled(sMin, sMax, ImGui::ColorConvertFloat4ToU32(sbg), 6.0f);
                    dl->AddRect(sMin, sMax,
                        ImGui::ColorConvertFloat4ToU32(ImVec4(0.30f,0.60f,1.0f,0.60f)),
                        6.0f, 0, 1.0f);

                    // Label
                    float scy = (sMin.y + sMax.y) * 0.5f;
                    dl->AddText(ImVec2(sMin.x + 12.f, scy - 7.f),
                        IM_COL32(100,180,255,220), "Settings");
                    dl->AddText(ImVec2(sMin.x + 12.f + ImGui::CalcTextSize("Settings").x + 8.f, scy - 7.f),
                        IM_COL32(100,180,255,160), "Footprint");

                    // Chevron kanan
                    dl->AddText(ImVec2(sMax.x - 18.f, scy - 7.f),
                        IM_COL32(100,180,255,120), ">");

                    if (sClk) {
                        ImGui::CloseCurrentPopup();
                        ToggleFPSettings();
                    }
                    if (sHov) {
                        ImGui::BeginTooltip();
                        ImGui::TextColored(ImVec4(0.4f,0.8f,1.f,1.f),
                            "Footprint Settings");
                        ImGui::Separator();
                        ImGui::TextUnformatted("Display mode, colors, font, detection");
                        ImGui::EndTooltip();
                    }
                    ImGui::Spacing();
                }
            }

            // ── ALL CHARTS ───────────────────────────────────────
            la = ImGui::GetCursorScreenPos();
            la.y += 2.f;
            dl->AddLine(la, ImVec2(la.x + 270.f, la.y),
                IM_COL32(255,255,255,18), 1.f);
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(1,1,1,0.28f), "ALL CHARTS");
            ImGui::Spacing();

            for (auto* t : g_chartManager.tabs) {
                if (!t) continue;
                const RenderStyleDef* tDef = &k_StyleDefs[
                    std::clamp((int)t->renderStyle, 0, k_StyleCount-1)];
                bool isThis = (t->id == g_chartManager.activeTabId);
                ImGui::TextColored(
                    isThis ? ImVec4(0.4f,0.9f,1.f,1.f) : ImVec4(1,1,1,0.45f),
                    "  %s %s  %s",
                    isThis ? u8"\u25B6" : " ",
                    t->label.c_str(), tDef->name);
                if (ImGui::IsItemClicked())
                    g_chartManager.activeTabId = t->id;
            }

            ImGui::EndPopup();
        }

        ImGui::PopStyleVar(3);
        ImGui::PopStyleColor(2);
    }
};

// ─────────────────────────────────────────────────────────────────
// GLOBAL INSTANCE
// ─────────────────────────────────────────────────────────────────
extern CandleStyleManager g_candleStyleMgr;
