#pragma once
// ================================================================
// ChartInteraction.h — Reusable chart interaction engine
// ================================================================
// Handle semua interaksi plot ImPlot:
//   - Pan X (drag kiri-kanan) + inertia + anti-shake + sub-pixel smooth
//   - Pan Y (drag tengah chart atas-bawah) — right mouse / ctrl+drag
//   - Zoom horizontal (scroll wheel, 2-finger pinch WASM)
//   - Y-axis scale drag (drag area kanan plot)
//   - Touch / mobile support (WASM)
//   - Double-click → autoFitY reset
//
// CARA PAKAI:
//   ChartInteraction ci;                    // per-tab instance
//   ci.Update(maxCandles, blocked);         // di dalam BeginPlot()..EndPlot()
//   float visualOffset = ci.GetViewOffset();// tambahkan ke start_idx / end_idx
// ================================================================

#include "imgui.h"
#include "implot.h"
#include <algorithm>
#include <cmath>
#include <map>

extern bool  g_isTouchActive;
extern float g_js_touch_start_x;
extern float g_js_touch_start_y;
extern float g_js_pan_delta_x;
extern float g_js_pan_delta_y;
extern float g_js_zoom_delta;
extern bool  IsCursorOnBorder();

// ================================================================
struct ChartInteraction {

    // ── View state ────────────────────────────────────────────────
    int    viewCenterIndex = 0;
    float  zoomLevel       = 150.f;
    float  targetZoom      = 150.f;
    double y_min           = 0.0;
    double y_max           = 0.0;
    bool   autoFitY        = true;

    // ── Sub-pixel X offset (smooth drag saat zoom dekat) ─────────
    // Nilai 0..1: geser tambahan dari viewCenterIndex ke kiri/kanan
    // Dipakai di luar: start_idx -= ci.viewSubOffset; end_idx -= ci.viewSubOffset
    float  viewSubOffset   = 0.f;

    // ── Physics ───────────────────────────────────────────────────
    double dragAccumulator  = 0.0;
    float  inertiaVelocity  = 0.0f;
    bool   isInertiaActive  = false;

    // ── State machine ─────────────────────────────────────────────
    bool   isResizingY    = false;
    bool   isPanningY     = false;   // pan Y (geser harga naik/turun)
    bool   isPanConfirmed = false;
    bool   lastTouchState = false;
    ImVec2 panStartPos    = {0.f, 0.f};

    // ── Config (bisa diubah per-tab) ──────────────────────────────
    float  yAxisWidth   = 70.f;
    float  zoomMin      = 5.f;
    float  zoomMax      = 5000.f;
    float  panThreshold = 5.f;

    // Konstanta (tidak perlu diubah)
    static constexpr float FRICTION        = 0.88f;  // inertia rem
    static constexpr float ZOOM_FACTOR     = 0.05f;  // kecepatan zoom (lebih kecil=lebih pelan)
    static constexpr float ZOOM_SMOOTH     = 0.12f;  // lerp zoom (lebih kecil=lebih smooth)

    // ================================================================
    void ResetTouchState() {
        isResizingY = isPanningY = isPanConfirmed = false;
        lastTouchState  = false;
        isInertiaActive = false;
        g_js_pan_delta_x = g_js_pan_delta_y = 0.f;
        g_js_touch_start_x = g_js_touch_start_y = -1.f;
    }

    // ================================================================
    // Update — panggil 1x per frame DI DALAM BeginPlot()..EndPlot()
    //   maxIndex  = candles.size()-1 (untuk clamp inertia)
    //   blocked   = true jika drawing/trade/cutoff aktif
    // ================================================================
    void Update(int maxIndex, bool blocked) {
        ImGuiIO& io     = ImGui::GetIO();
        ImVec2 plotPos  = ImPlot::GetPlotPos();
        ImVec2 plotSize = ImPlot::GetPlotSize();

        // ── Zona Y-axis (kanan plot) ──────────────────────────────
        float rightEdge  = plotPos.x + plotSize.x;
        bool  inYZone    = (io.MousePos.x > rightEdge &&
                            io.MousePos.x < rightEdge + yAxisWidth);
        bool  touchYZone = (g_isTouchActive &&
                            g_js_touch_start_x >= rightEdge &&
                            g_js_touch_start_x <= rightEdge + yAxisWidth);
        bool  hoverYAxis = inYZone || touchYZone;
        if (inYZone && !ImGui::IsMouseDown(0))
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);

        // ── Block check ──────────────────────────────────────────
        bool isBorder  = IsCursorOnBorder();
        bool isUIBusy  = io.WantCaptureMouse
                      && !ImPlot::IsPlotHovered()
                      && !hoverYAxis;
        bool shouldBlock = blocked || isUIBusy || isBorder;

        // ── inputLeft = LEFT mouse atau touch (satu-satunya trigger pan) ──
        bool inputLeft = ImGui::IsMouseDown(0) || g_isTouchActive;

        if (shouldBlock && !hoverYAxis) {
            isInertiaActive = false; inertiaVelocity = 0.f;
            // 🔥 REMOVED: g_js_pan_delta consumption di sini
            // Dulu: g_js_pan_delta_x = g_js_pan_delta_y = 0.f;
            // Bug: saat user touch panel indicator, shouldBlock=true (karena
            //   WantCaptureMouse=true + IsPlotHovered()=false untuk chart utama).
            //   Akibatnya pan delta dimakan chart utama sebelum PanelInteraction
            //   sempat baca. Zoom delta selamat karena _handleZoom() return early.
            // Fix: biarkan delta hidup — akan dikonsumsi oleh PanelInteraction,
            //   atau di-clear oleh wasm_notify_touch_end() saat jari dilepas.
            _handleZoom();
            return;
        }

        // ── Frame 1 klik: tentukan mode ──────────────────────────
        bool clickStartL = io.MouseClicked[0];
        #ifdef __EMSCRIPTEN__
        if (g_isTouchActive && !lastTouchState) clickStartL = true;
        lastTouchState = g_isTouchActive;
        #endif

        if (inputLeft && clickStartL && !blocked) {
            inertiaVelocity = 0.f;
            if (hoverYAxis) {
                // Mode: scale Y (drag zona kanan plot)
                isResizingY = true; isPanningY = false;
                autoFitY = false;
                #ifdef __EMSCRIPTEN__
                g_js_pan_delta_x = g_js_pan_delta_y = 0.f;
                #endif
            } else if (ImPlot::IsPlotHovered()) {
                // Mode: pan X + Y bersamaan (left drag di dalam chart)
                isResizingY = false; isPanningY = false;
                panStartPos    = io.MousePos;
                isPanConfirmed = false;
                autoFitY       = false; // matikan autofit saat user geser Y
            }
        }
        if (!inputLeft) {
            isResizingY = isPanningY = false;
            isPanConfirmed = false;
        }

        // ── MODE A: Scale Y (drag zona kanan plot) ────────────────
        if (isResizingY && inputLeft) {
            _handleYResize(io, plotPos, plotSize);
            _handleZoom();
            return;
        }

        // ── MODE C: Inertia (autoplay saat dilepas) ───────────────
        if (isInertiaActive && !inputLeft) {
            // Smooth sub-pixel inertia
            dragAccumulator += inertiaVelocity;
            int step = (int)dragAccumulator;
            viewSubOffset = (float)(dragAccumulator - (int)dragAccumulator);
            if (step != 0) {
                viewCenterIndex += step;
                dragAccumulator -= step;
            }
            inertiaVelocity *= FRICTION;
            if (fabs(inertiaVelocity) < 0.008f) {
                isInertiaActive = false; inertiaVelocity = 0.f;
                viewSubOffset = 0.f;
            }
            viewCenterIndex = std::clamp(
                viewCenterIndex, 0,
                maxIndex + (int)(zoomLevel * 5.f));
        }
        if (inputLeft) { isInertiaActive = false; }

        // ── MODE D: Pan X ─────────────────────────────────────────
        if (!isResizingY && !isPanningY && !blocked
            && inputLeft && ImPlot::IsPlotHovered()) {

            // Anti-shake
            if (!isPanConfirmed) {
                float dx = io.MousePos.x - panStartPos.x;
                float dy = io.MousePos.y - panStartPos.y;
                if (dx*dx + dy*dy >= panThreshold * panThreshold)
                    isPanConfirmed = true;
            }

            if (isPanConfirmed) {
                float rawX = 0.f;
                float rawY = 0.f;
                #ifdef __EMSCRIPTEN__
                if (g_isTouchActive) {
                    rawX = -g_js_pan_delta_x;
                    rawY =  g_js_pan_delta_y;
                    g_js_pan_delta_x = g_js_pan_delta_y = 0.f;
                } else
                #endif
                {
                    rawX = -io.MouseDelta.x;
                    rawY =  io.MouseDelta.y;
                }

                // ── Pan X (horizontal) ───────────────────────────
                float pxPerCandle = (plotSize.x > 0.f)
                    ? (plotSize.x / zoomLevel) : 1.f;
                float dd = rawX / pxPerCandle;
                dragAccumulator += dd;
                int step = (int)dragAccumulator;
                viewSubOffset = (float)(dragAccumulator - (double)step);
                if (step != 0) {
                    viewCenterIndex  += step;
                    dragAccumulator  -= step;
                    float v = (float)step * 0.55f;
                    float maxV = std::max(1.5f, zoomLevel * 0.015f);
                    inertiaVelocity  = std::clamp(v, -maxV, maxV);
                    isInertiaActive  = (fabs(inertiaVelocity) > 0.01f);
                }

                // ── Pan Y (vertikal, geser harga naik/turun) ─────
                if (rawY != 0.f && plotSize.y > 0.f) {
                    double range      = y_max - y_min;
                    double pricePerPx = range / (double)plotSize.y;
                    double shift      = rawY * pricePerPx;
                    y_min += shift;
                    y_max += shift;
                }
            }
        } else if (inputLeft) {
            isInertiaActive = false; inertiaVelocity = 0.f;
        }

        if (ImGui::IsMouseDoubleClicked(0)) { autoFitY = true; viewSubOffset = 0.f; }

        _handleZoom();
    }

    // ── Helper: ambil offset sub-pixel untuk X-axis ───────────────
    // Pakai ini di luar: SetupAxisLimits(start_idx - GetViewOffset(), ...)
    float GetViewOffset() const { return viewSubOffset; }

    // ── Sync helpers ─────────────────────────────────────────────
    template<typename T>
    void PullFrom(const T& s) {
        viewCenterIndex = s.viewCenterIndex;
        zoomLevel       = s.zoomLevel;
        targetZoom      = s.targetZoom;
        y_min           = s.y_min;
        y_max           = s.y_max;
        autoFitY        = s.autoFitY;
        dragAccumulator = s.dragAccumulator;
        inertiaVelocity = s.inertiaVelocity;
        isInertiaActive = s.isInertiaActive;
        isResizingY     = s.isResizingY;
        isPanConfirmed  = s.isPanConfirmed;
        lastTouchState  = s.lastTouchState;
        panStartPos     = s.panStartPos;
    }

    template<typename T>
    void PushTo(T& s) const {
        s.viewCenterIndex = viewCenterIndex;
        s.zoomLevel       = zoomLevel;
        s.targetZoom      = targetZoom;
        s.y_min           = y_min;
        s.y_max           = y_max;
        s.autoFitY        = autoFitY;
        s.dragAccumulator = dragAccumulator;
        s.inertiaVelocity = inertiaVelocity;
        s.isInertiaActive = isInertiaActive;
        s.isResizingY     = isResizingY;
        s.isPanConfirmed  = isPanConfirmed;
        s.lastTouchState  = lastTouchState;
        s.panStartPos     = panStartPos;
    }

private:
    void _handleZoom() {
        if (!ImPlot::IsPlotHovered() || IsCursorOnBorder()) return;
        ImGuiIO& io = ImGui::GetIO();
        float scroll = 0.f;
        if (fabs(io.MouseWheel) > 0.f) {
            scroll = io.MouseWheel;
            #ifdef __EMSCRIPTEN__
            scroll *= 2.f;   // touch device amplify sedikit
            #endif
        }
        #ifdef __EMSCRIPTEN__
        if (g_js_zoom_delta != 0.f) {
            scroll = g_js_zoom_delta * 0.08f;
            g_js_zoom_delta = 0.f;
        }
        #endif
        if (fabs(scroll) > 0.f) {
            // Zoom speed proporsional ke zoomLevel tapi dibatasi agar
            // tidak terasa "licin" saat level besar
            float speedMult = std::min(zoomLevel * ZOOM_FACTOR, 15.f);
            targetZoom -= scroll * speedMult;
            targetZoom  = std::clamp(targetZoom, zoomMin, zoomMax);
        }
        zoomLevel += (targetZoom - zoomLevel) * ZOOM_SMOOTH;
    }

    void _handleYResize(const ImGuiIO& io, ImVec2 plotPos, ImVec2 plotSize) {
        float dY = 0.f;
        #ifdef __EMSCRIPTEN__
        if (g_isTouchActive) {
            dY = g_js_pan_delta_y;
            g_js_pan_delta_y = g_js_pan_delta_x = 0.f;
        } else
        #endif
        { dY = io.MouseDelta.y; }
        if (dY == 0.f) return;

        double range = y_max - y_min;
        if (range <= 0.0) return;
        double scale = std::clamp(1.0 + dY * 0.012, 0.75, 1.3);
        double pivot = y_min + range * 0.5;
        if (plotSize.y > 0.f) {
            float r = std::clamp((io.MousePos.y - plotPos.y) / plotSize.y, 0.f, 1.f);
            pivot = y_max - r * range;
        }
        double pr = (pivot - y_min) / range;
        double nr = range * scale;
        y_min = pivot - pr * nr;
        y_max = y_min + nr;
    }
};
// ================================================================
// PanelInteraction — Y-only interaction untuk subplot indicator
// ================================================================
// X axis TIDAK disentuh (sudah di-link ke chart utama via LinkAllX).
// Fitur:
//   - Pan Y   : drag kiri-kanan di dalam panel → geser range Y
//   - Scale Y : drag zona kanan (Y-axis area) → kecilkan/besarkan range
//   - Zoom Y  : scroll wheel di dalam panel → scale Y simetris
//   - Touch   : WASM touch support (pan & pinch Y)
//   - Reset   : double-click → kembali ke default range
//
// CARA PAKAI (di dalam BeginPlot..EndPlot tiap panel):
//   static std::map<int,PanelInteraction> s_pi;
//   PanelInteraction& pi = s_pi[panelKey];
//   pi.Update(defaultYMin, defaultYMax, isRSI);
//   ImPlot::SetupAxisLimits(ImAxis_Y1, pi.y_min, pi.y_max, ImGuiCond_Always);
// ================================================================
struct PanelInteraction {

    double y_min        = 0.0;
    double y_max        = 100.0;
    bool   autoFit      = true;
    bool   isPanningY   = false;
    bool   isScalingY   = false;
    bool   isPanningX   = false;   // pan horizontal (X) — push ke chart utama
    bool   panConfirmed = false;
    bool   lastTouch    = false;   // per-instance (bukan static) agar multi-panel aman
    ImVec2 panStart     = {0,0};

    // Pan X output — baca dari luar setiap frame
    // Tambahkan ke viewCenterIndex chart utama
    float  xDeltaCandles = 0.f;  // candle yang perlu digeser
    double xDragAcc      = 0.0;  // sub-pixel accumulator

    static constexpr float Y_AXIS_ZONE = 55.f;
    static constexpr float PAN_THRESH  = 3.f;

    // ──────────────────────────────────────────────────────────────
    // Update — panggil di dalam BeginPlot..EndPlot
    //   defMin/defMax : range default indikator (0/100 untuk RSI)
    //   zoomLevel     : dari ChartInteraction (untuk hitung px/candle)
    // ──────────────────────────────────────────────────────────────
    void Update(double defMin, double defMax, float zoomLevel = 150.f) {
        if (autoFit) { y_min = defMin; y_max = defMax; }

        xDeltaCandles = 0.f; // reset tiap frame

        ImGuiIO& io    = ImGui::GetIO();
        ImVec2 pPos    = ImPlot::GetPlotPos();
        ImVec2 pSz     = ImPlot::GetPlotSize();
        ImVec2 pMax    = ImVec2(pPos.x + pSz.x, pPos.y + pSz.y);

        // ── Deteksi zona Y-axis (kanan plot) ────────────────────
        float rightEdge = pMax.x;
        bool  inYZone   = (io.MousePos.x > rightEdge &&
                           io.MousePos.x < rightEdge + Y_AXIS_ZONE &&
                           io.MousePos.y >= pPos.y &&
                           io.MousePos.y <= pMax.y);
        #ifdef __EMSCRIPTEN__
        bool touchYZone = (g_isTouchActive &&
                           g_js_touch_start_x >= rightEdge &&
                           g_js_touch_start_x <= rightEdge + Y_AXIS_ZONE);
        inYZone = inYZone || touchYZone;
        #endif

        if (inYZone && !ImGui::IsMouseDown(0))
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
        #ifdef __EMSCRIPTEN__
        if ((isPanningX || isPanningY) && g_isTouchActive)
            ImGui::SetMouseCursor(ImGuiMouseCursor_None);
        #endif

        bool inputLeft = ImGui::IsMouseDown(0);
        #ifdef __EMSCRIPTEN__
        inputLeft = inputLeft || g_isTouchActive;
        #endif

        // ── Frame pertama klik: tentukan mode ───────────────────
        bool clicked = ImGui::IsMouseClicked(0);
        #ifdef __EMSCRIPTEN__
        // lastTouch adalah member → aman untuk multi-panel
        if (g_isTouchActive && !lastTouch) clicked = true;
        lastTouch = g_isTouchActive;
        #endif

        // Touch: cek apakah jari dalam batas plot (IsPlotHovered() kadang false saat touch)
        bool inPlotArea = ImPlot::IsPlotHovered();
        #ifdef __EMSCRIPTEN__
        if (g_isTouchActive && !inYZone) {
            // Fallback: cek posisi jari vs batas plot secara manual
            float tx = g_js_touch_start_x, ty = g_js_touch_start_y;
            inPlotArea = inPlotArea ||
                (tx >= pPos.x && tx <= pMax.x && ty >= pPos.y && ty <= pMax.y);
        }
        #endif

        if (clicked && inputLeft) {
            if (inYZone) {
                isScalingY = true; isPanningY = false; isPanningX = false;
                autoFit = false;
                xDragAcc = 0.0;
            } else if (inPlotArea) {
                // Pan X + Y sekaligus — mouse & touch
                isPanningY = true; isPanningX = true; isScalingY = false;
                panStart = io.MousePos;
                #ifdef __EMSCRIPTEN__
                if (g_isTouchActive)
                    panStart = ImVec2(g_js_touch_start_x, g_js_touch_start_y);
                #endif
                panConfirmed = false;
                autoFit = false;
                xDragAcc = 0.0;
            }
        }
        if (!inputLeft) {
            isScalingY = isPanningY = isPanningX = panConfirmed = false;
        }

        // ── MODE A: Scale Y (drag zona kanan) ───────────────────
        if (isScalingY && inputLeft) {
            float dY = 0.f;
            #ifdef __EMSCRIPTEN__
            if (g_isTouchActive) {
                dY = g_js_pan_delta_y;
                g_js_pan_delta_y = g_js_pan_delta_x = 0.f;
            } else
            #endif
            { dY = io.MouseDelta.y; }

            if (dY != 0.f) {
                double range = y_max - y_min;
                if (range > 0.0) {
                    double scale = std::clamp(1.0 + dY * 0.015, 0.7, 1.4);
                    // Pivot: posisi Y di bawah kursor
                    float r = std::clamp((io.MousePos.y - pPos.y) / pSz.y, 0.f, 1.f);
                    double pivot = y_max - r * range;
                    double pr = (pivot - y_min) / range;
                    double nr = range * scale;
                    y_min = pivot - pr * nr;
                    y_max = y_min + nr;
                }
            }
        }

        // ── MODE B: Pan X + Y (drag dalam panel) ─────────────────
        if ((isPanningY || isPanningX) && inputLeft && inPlotArea) {
            if (!panConfirmed) {
                float dx = io.MousePos.x - panStart.x;
                float dy = io.MousePos.y - panStart.y;
                #ifdef __EMSCRIPTEN__
                // Touch: konfirmasi lebih cepat karena tidak ada hover noise
                float thresh = g_isTouchActive ? 1.5f : PAN_THRESH;
                if (dx*dx + dy*dy >= thresh * thresh) panConfirmed = true;
                #else
                if (dx*dx + dy*dy >= PAN_THRESH * PAN_THRESH) panConfirmed = true;
                #endif
            }
            if (panConfirmed) {
                float rawX = 0.f, rawY = 0.f;
                #ifdef __EMSCRIPTEN__
                if (g_isTouchActive) {
                    rawX = -g_js_pan_delta_x;
                    rawY =  g_js_pan_delta_y;
                    g_js_pan_delta_x = g_js_pan_delta_y = 0.f;
                } else
                #endif
                {
                    rawX = -io.MouseDelta.x;
                    rawY =  io.MouseDelta.y;
                }

                // Pan X — konversi pixel → candle, akumulasi sub-pixel
                if (rawX != 0.f && pSz.x > 0.f && zoomLevel > 0.f) {
                    float pxPerCandle = pSz.x / zoomLevel;
                    xDragAcc += rawX / pxPerCandle;
                    int step = (int)xDragAcc;
                    if (step != 0) {
                        xDeltaCandles = (float)step;  // dibaca main.cpp → geser viewCenter
                        xDragAcc -= step;
                    }
                }

                // Pan Y — geser range harga naik/turun
                if (rawY != 0.f && pSz.y > 0.f) {
                    double range = y_max - y_min;
                    double shift = rawY * (range / pSz.y);
                    y_min += shift;
                    y_max += shift;
                }
            }
        }

        // ── MODE C: Zoom Y (scroll wheel + pinch) ─────────────────
        if (inPlotArea) {
            float scroll = 0.f;
            #ifdef __EMSCRIPTEN__
            if (g_js_zoom_delta != 0.f && inPlotArea) {
                scroll = g_js_zoom_delta * 0.06f;
                g_js_zoom_delta = 0.f;  // konsumsi agar panel lain tidak zoom juga
            } else
            #endif
            { scroll = ImGui::GetIO().MouseWheel; }

            if (fabs(scroll) > 0.f) {
                double range = y_max - y_min;
                double center = (y_min + y_max) * 0.5;
                double factor = std::clamp(1.0 - scroll * 0.08, 0.5, 2.0);
                double nr = range * factor;
                y_min = center - nr * 0.5;
                y_max = center + nr * 0.5;
                autoFit = false;
            }
        }

        // ── Double-click / double-tap: reset ke default ──────────
        if (inPlotArea && ImGui::IsMouseDoubleClicked(0)) {
            y_min = defMin; y_max = defMax;
            autoFit = true;
            xDragAcc = 0.0;
        }
    }
};
