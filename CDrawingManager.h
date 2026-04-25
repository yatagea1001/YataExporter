#pragma once
#include "implot.h"
#include "imgui.h"
#include "imgui_internal.h"
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include <map>
#include <mutex>
#include <functional>
#include "Candle.h"
#include "ChartCanvas.h"
// =========================================================
// 🎨 DRAWING MANAGER (FIBONACCI UPDATE)
// =========================================================

#include "ShapeEditUI.h" 
#include "FiboEditUI.h"
#include "GlobalShapeManager.h"
extern GlobalShapeManager g_shapes;
#include "ElliotDrawing.h"
#include "CTextDrawing.h"
// Variabel Global External
extern std::map<std::string, std::vector<struct Candle>> g_allCandles;
extern std::string g_activeTF;
#ifdef __EMSCRIPTEN__
extern bool  g_isTouchActive;
extern float g_js_touch_start_x;
extern float g_js_touch_start_y;
extern float g_js_pan_delta_x;
extern float g_js_pan_delta_y;
extern float g_js_zoom_delta;
#endif

// Helper Math
static inline float LengthSqr(const ImVec2& v) { return v.x * v.x + v.y * v.y; }

static inline float GetPointLineDistanceSq(const ImVec2& point, const ImVec2& a, const ImVec2& b) {
    ImVec2 ab = b - a; 
    ImVec2 ap = point - a;
    float ab_len2 = ab.x * ab.x + ab.y * ab.y;
    if (ab_len2 == 0.0f) return ap.x * ap.x + ap.y * ap.y;
    float t = std::clamp((ap.x * ab.x + ap.y * ab.y) / ab_len2, 0.0f, 1.0f);
    ImVec2 closest = ImVec2(a.x + t * ab.x, a.y + t * ab.y);
    ImVec2 diff = point - closest;
    return diff.x * diff.x + diff.y * diff.y;
}

// ----------------------------------------------------
// DEFINISI TIPE DATA
// ----------------------------------------------------

enum class HandleType {
    NONE, BODY, TOP_LEFT, TOP_RIGHT, BOTTOM_LEFT, BOTTOM_RIGHT, TOP, BOTTOM, LEFT, RIGHT
};

struct DrawShape {
    enum Type { 
         NONE = 0,
        LINE = 1, 
        RECT = 2, 
        FIB = 3, 
        TEXT = 4,   
        BRUSH = 5,  
        ELLIOT = 6 
    } type;

    double x0 = 0, y0 = 0, x1 = 0, y1 = 0; 
    ImVec4 color = ImVec4(0.9f,0.8f,0.2f,1.0f);
    bool selected = false;
    std::string globalId = ""; 

    HandleType hoveredHandle = HandleType::NONE;
    HandleType activeDragHandle = HandleType::NONE;
    ImPlotPoint dragStartPoint;
};

struct TFShape {
    std::string tf; 
    std::string time0, time1;
    double price0 = 0, price1 = 0;
    ImVec4 color = ImVec4(1,1,1,0.9f);
    float thickness = 1.0f;
};
// ==========================================
// 🖌️ BRUSH DRAWING LOGIC (Freehand)
// ==========================================
struct BrushPoint {
    double time;
    double price;
};

class BrushDrawing {
public:
    bool isActive = false;
    std::vector<BrushPoint> points; 
    ImVec4 color = ImVec4(0.2f, 1.0f, 0.2f, 1.0f); // Default Hijau Neon
    float thickness = 2.0f;

    // 1. MULAI (Klik Pertama / Touch Start)
    void Start(const ImPlotPoint& startPt, const std::vector<Candle>& candles) {
        isActive = true;
        points.clear();
        
        // Langsung catat titik pertama sebagai "Jangkar"
        AddPoint(startPt, candles);
    }

    // 2. GERAK (Drag / Touch Move)
    void Update(const ImPlotPoint& currPt, const std::vector<Candle>& candles) {
        if (!isActive || points.empty()) return;

        // --- FILTER JARAK (SMOOTHING) ---
        // Kita hitung jarak pixel titik terakhir vs mouse sekarang
        ImVec2 pLast = PlotToPixelsMTF(points.back(), candles);
        ImVec2 pCurr = ImPlot::PlotToPixels(currPt);

        float dx = pCurr.x - pLast.x;
        float dy = pCurr.y - pLast.y;
        float distSq = dx*dx + dy*dy;

        // HANYA tambah titik baru jika geser > 5 pixel (25 px kuadrat)
        // Ini kuncinya biar garis tidak keriting/bergerigi
        if (distSq > 25.0f) {
            AddPoint(currPt, candles);
        }
    }

    // 3. RENDER PREVIEW (Saat sedang menggambar)
    void RenderPreview(ImDrawList* draw, const std::vector<Candle>& candles) {
        if (points.size() < 2) return;

        ImU32 col = ImGui::ColorConvertFloat4ToU32(color);
        
        // Konversi semua titik ke Pixel Layar untuk digambar
        std::vector<ImVec2> screenPoints;
        screenPoints.reserve(points.size());

        for (const auto& pt : points) {
            screenPoints.push_back(PlotToPixelsMTF(pt, candles));
        }

        // Pakai AddPolyline biar sambungannya mulus
        draw->AddPolyline(screenPoints.data(), (int)screenPoints.size(), col, 0, thickness);
    }

    // 4. SIMPAN KE DATABASE (Saat dilepas)
    void SaveToGlobal(const std::string& panel = "") {
        if (points.size() < 2) return;

        std::vector<double> tVec, pVec;
        tVec.reserve(points.size());
        pVec.reserve(points.size());

        for(const auto& pt : points) {
            tVec.push_back(pt.time);
            pVec.push_back(pt.price);
        }

        g_shapes.AddBrushShape(tVec, pVec, color, thickness, panel);
    }

    void Stop() {
        isActive = false;
        points.clear();
    }

private:
    // Helper: Simpan koordinat Waktu & Harga
    void AddPoint(const ImPlotPoint& pt, const std::vector<Candle>& candles) {
        if (candles.empty()) return;
        double tf = ChartCanvas::GetTimePerCandle(g_activeTF);
        
        // Konversi X (Index) ke Waktu (Time) — STABLE
        double time = ChartCanvas::GetStableTime(pt.x, candles, tf);
        points.push_back({ time, pt.y });
    }
    
    // Helper: Konversi Waktu & Harga ke Pixel Layar
    ImVec2 PlotToPixelsMTF(const BrushPoint& pt, const std::vector<Candle>& candles) {
        double tf = ChartCanvas::GetTimePerCandle(g_activeTF);
        double x = ChartCanvas::GetStableX(pt.time, candles, tf);
        return ImPlot::PlotToPixels(x, pt.price);
    }
    // ==========================================
    // 🔤 VARIABEL KHUSUS TEXT (Input Mode)
    // ==========================================
    
    bool isTypingNewText = false;      // Status: Lagi ngetik atau nggak?
    char textInputBuffer[256] = "";    // Wadah: Nampung huruf yang kamu ketik
    ImPlotPoint textSpawnPos;          // Jangkar: Titik (Waktu & Harga) di mana kamu klik

    // Helper untuk konversi (Opsional, kalau belum ada)
    // Ini biar teksnya nempel kuat di candle, bukan di layar
    double ScreenToTime(double plotX, const std::vector<Candle>& candles) {
        if (candles.empty()) return 0;
        double tf = ChartCanvas::GetTimePerCandle(g_activeTF);
        return ChartCanvas::GetStableTime(plotX, candles, tf);
    }
    
};
// =========================================
// 🧩 CDrawingManager (CLASS UTAMA)
// =========================================
class CDrawingManager {
public:
    // =========================================================
    // 🔥 TEMPELKAN FUNGSI INI DI SINI (PALING ATAS)
    // =========================================================
    double ScreenToTime(double plotX, const std::vector<Candle>& candles) {
        if (candles.empty()) return 0;
        double tf = ChartCanvas::GetTimePerCandle(g_activeTF);
        return ChartCanvas::GetStableTime(plotX, candles, tf);
    }
    DrawShape::Type activeTool = DrawShape::LINE;
    bool isDrawing = false;
    bool blockChart = false;
    // Panel tempat drawing aktif — di-set dari main.cpp sebelum Render()
    // "" = chart utama, "RSI" = panel RSI, "Volume" = panel Volume, dll
    std::string activePanel = ""; // panel aktif saat ini
    bool hasPanelMode          = false; // ada panel indicator → delay touch
    bool waitingPanelConfirm   = false; // mode panel: tap 1 = konfirmasi, tap 2 = mulai
    bool prevTouchDrawing      = false; // deteksi jari lepas untuk trigger release
    ImVec4 currentColor = ImVec4(0.9f, 0.8f, 0.2f, 1.0f);

    std::string selectedShapeId = "";
    HandleType hoveredHandle = HandleType::NONE;
    HandleType activeDragHandle = HandleType::NONE;
    ImPlotPoint dragStartPoint;

    // --- POPUP STATE ---
    std::string activePopupID = "";
    ImVec2 popupPos = ImVec2(0,0);
    bool isPopupOpen = false;
    float popupAnimProgress = 0.0f;

    // --- POPUP TRACKING (ikuti shape + mouse) ---
    const std::vector<Candle>* lastCandles = nullptr;

    // Shape bounds (diupdate tiap frame di HandleEditing saat shape selected)
    float popupShapeTopY = 0.0f;     // tepi atas shape (pixel)
    float popupShapeCenterX = 0.0f;  // tengah horizontal shape (pixel)
    bool hasShapeBounds = false;

    // Pin popup saat pertama kali buka — reset flag agar RenderShapePopup menghitung posisi sekali
    void PinPopup(ImVec2 mousePos) {
        popupPinned = false; // Biarkan RenderShapePopup hitung posisi sekali, lalu auto-pin
    }
    void ResetPopupPin() {
        popupPinned = false;
        hasShapeBounds = false;
    }
    bool popupPinned = false;

private:
    DrawShape tempDrawingShape;
    BrushDrawing brushHandler;
    ElliotDrawing elliotHandler;
    CTextDrawing textHandler;
    std::map<std::string, std::vector<TFShape>> drawingsByTF;
    std::mutex mtx; 
    bool waitingForSecondPoint = false;
  // ==========================================
    // 🔤 VARIABEL PENTING FITUR TEKS
    // ==========================================
    
    // 1. UNTUK INPUT TEKS BARU (Click & Type)
    bool isTypingNewText = false;       // Status: Lagi ngetik baru?
    ImPlotPoint textSpawnPos;           // Lokasi klik awal
    
    // 2. UNTUK EDIT TEKS LAMA (Click & Edit)
    bool isEditingExistingText = false; // Status: Lagi edit teks lama?
    std::string textEditTargetID = "";  // ID teks yang sedang diedit

    // 3. BUFFER UMUM (Dipakai untuk input baru maupun edit lama)
    char textInputBuffer[256] = "";     // Wadah karakter ketikan

    // ==========================================
    
public:
    CDrawingManager() = default;

    bool IsEditing() const { return activeDragHandle != HandleType::NONE || isPopupOpen; }

    // ---------------------------
    // RENDER UTAMA
    // ---------------------------
    // isPanel = true saat dipanggil dari panel indicator (RSI, Volume, dll)
    // Saat isPanel=true: drawing & editing tetap jalan, tapi shapes tidak dirender ulang
    // (shapes sudah dirender di chart utama via g_shapes.Render)
   void Render(const std::vector<Candle>& activeCandles, bool isPanel = false) {
    ImDrawList* draw = ImPlot::GetPlotDrawList();

    // mp = posisi touch/mouse dalam koordinat plot aktif
    // Saat touch di panel: SELALU pakai GetIO().MousePos (screen pixel terkini)
    // WASM bridge update MousePos tiap touchstart DAN touchmove
    // → akurat untuk tap maupun drag tanpa perlu bedakan kondisi
    // Bounds check memastikan posisi dalam panel ini saja
    ImPlotPoint mp = ImPlot::GetPlotMousePos();
    #ifdef __EMSCRIPTEN__
    if (isPanel && g_isTouchActive) {
        ImVec2 pPos    = ImPlot::GetPlotPos();
        ImVec2 pSz     = ImPlot::GetPlotSize();
        ImVec2 touchPx = ImGui::GetIO().MousePos; // selalu current, update tiap touch event
        if (touchPx.x >= pPos.x && touchPx.x <= pPos.x + pSz.x &&
            touchPx.y >= pPos.y && touchPx.y <= pPos.y + pSz.y) {
            // Convert screen pixel → koordinat plot panel ini (bukan chart utama!)
            mp = ImPlot::PixelsToPlot(touchPx);
        }
        // Jika posisi di luar panel (jari geser keluar), tetap pakai mp terakhir yang valid
        // → shape tidak loncat ke chart utama
    }
    #endif
  

    // =========================================================
    // 🚩 JALUR ELLIOT (DIISOLASI)
    // =========================================================
    if (activeTool == DrawShape::ELLIOT) {
        if (elliotHandler.isActive) {
            elliotHandler.Update(mp, draw, activeCandles);
        }

        // PERBAIKAN: Jika Elliot sudah selesai (SaveToGlobal sudah jalan),
        // Kita harus matikan saklar drawing pusat agar tidak bocor ke bawah
        if (!elliotHandler.isActive && isDrawing) {
            isDrawing = false;
            blockChart = false;
            // Patch sourcePanel pada shape Elliot yang baru saja ditambahkan
            if (!activePanel.empty() && !g_shapes.shapes.empty()) {
                std::lock_guard<std::mutex> lk(g_shapes.mtx);
                g_shapes.shapes.back().sourcePanel = activePanel;
            }
        }
        
        // JANGAN gunakan 'return' di sini lagi, 
        // supaya program bisa lanjut baca g_shapes.Render di bawah!
    } 
    // ✅ PERBAIKAN: Gunakan 'IsWindowHovered' yang lebih responsif untuk touch
        // atau cek manual apakah kita sedang menggambar.

        // isPlotAreaActive: true jika cursor ADA di plot ini (bukan plot lain)
        // Saat isPanel=true:
        //   - IsPlotHovered() = true  → cursor di panel ini → boleh drawing
        //   - IsPlotHovered() = false → cursor di chart utama/panel lain → SKIP
        // Ini mencegah titik drawing direkam dengan Y skala panel yang salah
        bool isThisPlotHovered = ImPlot::IsPlotHovered();
        #ifdef __EMSCRIPTEN__
        // Touch: IsPlotHovered() sering false → cek manual dengan posisi touch terkini
        if (!isThisPlotHovered && g_isTouchActive) {
            ImVec2 pPos = ImPlot::GetPlotPos();
            ImVec2 pSz  = ImPlot::GetPlotSize();
            ImVec2 cur  = ImGui::GetIO().MousePos; // posisi terkini (bukan touch_start yg stale)
            isThisPlotHovered = (cur.x >= pPos.x && cur.x <= pPos.x + pSz.x &&
                                 cur.y >= pPos.y && cur.y <= pPos.y + pSz.y);
        }
        #endif
        bool isPlotAreaActive  = isThisPlotHovered ||
                                 (!isPanel && (ImGui::IsMouseDown(ImGuiMouseButton_Left)
                                 #ifdef __EMSCRIPTEN__
                                 || g_isTouchActive
                                 #endif
                                 ));

        if (isDrawing && isPlotAreaActive) {
            HandleDrawing(mp, draw, activeCandles);
        }
    // Di dalam CDrawingManager::Render(...)

if (!activeCandles.empty()) {
    // Edit shapes (drag handle, select) — aktif di chart utama DAN panel indicator.
    // Aman karena HandleEditing sudah filter via (sourcePanel != activePanel).
    HandleEditing(mp, draw, activeCandles);

    // Render shapes:
    //   isPanel=false → chart utama, panggil g_shapes.Render di sini
    //   isPanel=true  → panel indicator, main.cpp yang panggil g_shapes.Render
    //                   (section 3 di loop panel) agar ClipRect panel terjaga.
    //                   Jangan double render di sini!
    if (!isPanel) {
        g_shapes.Render(draw, activeCandles, selectedShapeId, activePanel);
    }

    // Gambar preview cursor saat drawing aktif di panel yang di-hover
    if (isPanel && isDrawing && isThisPlotHovered) {
        ImVec2 pPos = ImPlot::GetPlotPos();
        ImVec2 pSz  = ImPlot::GetPlotSize();
        ImVec2 pMax = ImVec2(pPos.x + pSz.x, pPos.y + pSz.y);
        ImVec2 mpx  = ImPlot::PlotToPixels(mp);
        ImU32  hint = IM_COL32(255, 200, 50, 150);
        draw->PushClipRect(pPos, pMax, true);
        draw->AddLine(ImVec2(mpx.x, pPos.y), ImVec2(mpx.x, pMax.y), hint, 1.0f);
        draw->AddLine(ImVec2(pPos.x, mpx.y), ImVec2(pMax.x, mpx.y), hint, 1.0f);
        draw->AddCircle(mpx, 5.f, hint, 12, 1.5f);
        draw->PopClipRect();
        ImGui::SetMouseCursor(ImGuiMouseCursor_None);
    }

    // Block chart saat popup atau drag handle aktif
    if (isPopupOpen || activeDragHandle != HandleType::NONE) {
        blockChart = true;
    }
}

// Keyboard shortcut (Delete, Escape) — hanya di chart utama agar tidak double fire
if (!isPanel) {
    HandleInput(activeCandles, g_activeTF);
}
}
  // ------------------------------------------------------------------
    // LOGIC EDITING: HYBRID (ELLIOT + BRUSH + TEXT + STANDARD)
    // ------------------------------------------------------------------
// ------------------------------------------------------------------
    // LOGIC EDITING: HYBRID (ELLIOT CHAIN + STANDARD SHAPES) - FINAL FIX
    // ------------------------------------------------------------------
void HandleEditing(const ImPlotPoint& mp, ImDrawList* draw, const std::vector<Candle>& candles) {
        if (candles.empty()) return;

        // Simpan referensi candles untuk popup anchor
        lastCandles = &candles;

        ImVec2 mousePos = ImGui::GetIO().MousePos;
        bool isDraggingThisFrame = (activeDragHandle != HandleType::NONE && ImGui::IsMouseDown(ImGuiMouseButton_Left));
        bool isAnyShapeHovered = false;

        if (!isDraggingThisFrame) hoveredHandle = HandleType::NONE;

        // Ambil referensi data asli
        auto& all_shapes = g_shapes.GetEditableShapes(); 
        
        // Helper konversi (STABLE)
        double tf = ChartCanvas::GetTimePerCandle(g_activeTF);

        // Loop dari shape paling baru (layer paling atas)
        for (int i = (int)all_shapes.size() - 1; i >= 0; --i) {
            GlobalShape& global_s = all_shapes[i]; 
            if (!global_s.visible) continue;

            // 🔥 PANEL FILTER: Hanya edit shapes yang milik panel ini.
            // Main chart (activePanel="") → skip shapes dari RSI/Volume/dll.
            // RSI panel (activePanel="RSI") → skip shapes dari chart utama/panel lain.
            if (global_s.sourcePanel != activePanel) continue;

            bool isElliot = (global_s.type == "ELLIOT");
            bool isBrush  = (global_s.type == "BRUSH");
            bool isSelected = (global_s.id == selectedShapeId);

            // =========================================================
            // 🅰️ JALUR KHUSUS ELLIOT WAVE (MULTI-POINT)
            // =========================================================
            if (isElliot) {
                // 1. Render Garis Penghubung (Tulang)
                std::vector<ImVec2> pixels;
                for(size_t k=0; k<global_s.multiTime.size(); k++) {
                    double x = ChartCanvas::GetStableX(global_s.multiTime[k], candles, tf);
                    pixels.push_back(ImPlot::PlotToPixels(x, global_s.multiPrice[k]));
                }

                ImU32 col = ImGui::ColorConvertFloat4ToU32(global_s.color);
                for(size_t k=0; k<pixels.size()-1; k++) {
                    draw->AddLine(pixels[k], pixels[k+1], col, global_s.thickness);
                }

                // Cek Hover pada Garis
                if (!isDraggingThisFrame && !isSelected) {
                    for(size_t k=0; k<pixels.size()-1; k++) {
                        if (GetPointLineDistanceSq(mousePos, pixels[k], pixels[k+1]) < 36.0f) { 
                            isAnyShapeHovered = true;
                            if (ImGui::IsMouseClicked(0)) selectedShapeId = global_s.id;
                        }
                    }
                }

                // 2. Jika Terpilih: Tampilkan Handle & Logic Drag
                if (isSelected) {
                    // Update shape bounds tiap frame (untuk popup positioning)
                    if (!pixels.empty()) {
                        float minY = pixels[0].y, sumX = pixels[0].x;
                        for (size_t k = 1; k < pixels.size(); k++) {
                            if (pixels[k].y < minY) minY = pixels[k].y;
                            sumX += pixels[k].x;
                        }
                        popupShapeTopY = minY;
                        popupShapeCenterX = sumX / (float)pixels.size();
                        hasShapeBounds = true;
                    }
                    
                    for (size_t k = 0; k < pixels.size(); k++) {
                        ImVec2 pNode = pixels[k];
                        draw->AddCircleFilled(pNode, 5.0f, IM_COL32(255, 255, 255, 255));
                        draw->AddCircle(pNode, 6.0f, IM_COL32(0, 0, 0, 255));
                        
                        HandleType nodeHandleID = (HandleType)(1000 + k);

                        // Deteksi Klik Handle
                        if (!isDraggingThisFrame && LengthSqr(mousePos - pNode) < 64.0f) {
                            hoveredHandle = nodeHandleID;
                            if (ImGui::IsMouseClicked(0)) {
                                activeDragHandle = nodeHandleID;
                                dragStartPoint = mp;
                                isDraggingThisFrame = true;
                            }
                        }
                        // Proses Drag
                        if (activeDragHandle == nodeHandleID) {
                            if (ImGui::IsMouseDown(0)) {
                                global_s.multiTime[k] = ScreenToTime(mp.x, candles);
                                global_s.multiPrice[k] = mp.y;
                                isDraggingThisFrame = true;
                                blockChart = true; 
                            }
                        }
                    }
                    // Trigger Popup
                    if (!isPopupOpen || activePopupID != global_s.id) {
                         isPopupOpen = true; activePopupID = global_s.id; popupAnimProgress = 0.0f;
                         PinPopup(mousePos);
                    }
                }
            }
            // =========================================================
            // 🔥 🅱️ JALUR BRUSH
            // =========================================================
            else if (isBrush) {
                std::vector<ImVec2> pixels;
                ImVec2 minP(100000, 100000), maxP(-100000, -100000);

                for(size_t k=0; k<global_s.multiTime.size(); k++) {
                    double x = ChartCanvas::GetStableX(global_s.multiTime[k], candles, tf);
                    ImVec2 px = ImPlot::PlotToPixels(x, global_s.multiPrice[k]);
                    pixels.push_back(px);
                    if (px.x < minP.x) minP.x = px.x; if (px.y < minP.y) minP.y = px.y;
                    if (px.x > maxP.x) maxP.x = px.x; if (px.y > maxP.y) maxP.y = px.y;
                }
                float padding = 10.0f;
                minP.x -= padding; minP.y -= padding; maxP.x += padding; maxP.y += padding;

                if (isSelected) {
                    draw->AddRect(minP, maxP, IM_COL32(255, 255, 255, 100), 0.0f, ImDrawFlags_None, 1.0f);

                    // Update shape bounds tiap frame
                    popupShapeTopY = minP.y;
                    popupShapeCenterX = (minP.x + maxP.x) * 0.5f;
                    hasShapeBounds = true;

                    // Popup: buka sekali
                    if (!isPopupOpen || activePopupID != global_s.id) {
                         isPopupOpen = true; activePopupID = global_s.id; popupAnimProgress = 0.0f;
                         PinPopup(mousePos);
                    }

                    if (!global_s.locked) {
                        if (!isDraggingThisFrame && ImGui::IsMouseHoveringRect(minP, maxP)) {
                             hoveredHandle = HandleType::BODY;
                             ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
                             if (ImGui::IsMouseClicked(0)) {
                                 activeDragHandle = HandleType::BODY;
                                 dragStartPoint = mp;
                                 isDraggingThisFrame = true;
                             }
                        }
                        if (activeDragHandle == HandleType::BODY && isDraggingThisFrame) {
                            if (ImGui::IsMouseDown(0)) {
                                double dx_time = ScreenToTime(mp.x, candles) - ScreenToTime(dragStartPoint.x, candles);
                                double dy_price = mp.y - dragStartPoint.y;
                                for(size_t k=0; k<global_s.multiTime.size(); k++) {
                                    global_s.multiTime[k] += dx_time;
                                    global_s.multiPrice[k] += dy_price;
                                }
                                dragStartPoint = mp; 
                                blockChart = true;
                            }
                        }
                    }
                }
                if (!isDraggingThisFrame && !isSelected) {
                    if (ImGui::IsMouseHoveringRect(minP, maxP)) {
                        isAnyShapeHovered = true;
                        draw->AddRectFilled(minP, maxP, IM_COL32(255, 255, 255, 15)); 
                        if (ImGui::IsMouseClicked(0)) selectedShapeId = global_s.id;
                    }
                }
            }
            // =========================================================
            // 📝 JALUR BARU: TEXT
            // =========================================================
            else if (global_s.type == "TEXT") {
                if (textHandler.isActive && global_s.id == textHandler.editingId) continue;
                
                double xVal = ChartCanvas::GetStableX(global_s.time0, candles, tf);
                ImVec2 pos = ImPlot::PlotToPixels(xVal, global_s.price0);
                
                ImVec2 textSize = ImGui::GetFont()->CalcTextSizeA(global_s.fontSize, FLT_MAX, 0.0f, global_s.textContent.c_str());
                ImVec2 boxMin = ImVec2(pos.x - 5, pos.y - 5);
                ImVec2 boxMax = ImVec2(pos.x + textSize.x + 5, pos.y + textSize.y + 5);

                if (isSelected) {
                    draw->AddRect(boxMin, boxMax, IM_COL32(0, 255, 0, 255), 2.0f);
                    
                    if (!global_s.locked) {
                        if (!isDraggingThisFrame && ImGui::IsMouseHoveringRect(boxMin, boxMax)) {
                            hoveredHandle = HandleType::BODY; 
                            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                            if (ImGui::IsMouseClicked(0)) {
                                activeDragHandle = HandleType::BODY;
                                isDraggingThisFrame = true;
                                dragStartPoint = mp;
                            }
                        }
                        if (activeDragHandle == HandleType::BODY && isDraggingThisFrame) {
                            if (ImGui::IsMouseDown(0)) {
                                global_s.time0 = ScreenToTime(mp.x, candles); 
                                global_s.price0 = mp.y;
                                blockChart = true; 
                            }
                        }
                    }
                    // Input Text Logic
                    if (activeDragHandle == HandleType::NONE) {
                        ImGui::SetNextWindowPos(ImVec2(pos.x, pos.y - 35));
                        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, 4));
                        ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings;
                        std::string winName = "##Edit" + global_s.id; 
                        
                        if (ImGui::Begin(winName.c_str(), nullptr, flags)) {
                            static char editBuf[256];
                            static std::string lastTextEditId = "";
                            if (lastTextEditId != global_s.id || ImGui::IsWindowAppearing()) {
                                lastTextEditId = global_s.id;
                                memset(editBuf, 0, sizeof(editBuf));
                                strncpy(editBuf, global_s.textContent.c_str(), sizeof(editBuf));
                                ImGui::SetKeyboardFocusHere(0);
                            }
                            ImGui::SetNextItemWidth(150);
                            if (ImGui::InputText("##txt", editBuf, sizeof(editBuf))) {
                                global_s.textContent = std::string(editBuf);
                            }
                        }
                        ImGui::End();
                        ImGui::PopStyleVar();
                    }
                    // Update shape bounds tiap frame
                    popupShapeTopY = pos.y - 5;
                    popupShapeCenterX = pos.x + textSize.x * 0.5f;
                    hasShapeBounds = true;

                    // Popup: buka sekali
                    if (!isPopupOpen || activePopupID != global_s.id) {
                         isPopupOpen = true; activePopupID = global_s.id; popupAnimProgress = 0.0f;
                         PinPopup(mousePos);
                    }
                } else if (!isDraggingThisFrame && !isAnyShapeHovered) {
                    if (ImGui::IsMouseHoveringRect(boxMin, boxMax)) {
                         isAnyShapeHovered = true;
                         draw->AddRectFilled(boxMin, boxMax, IM_COL32(255, 255, 255, 30));
                         if (ImGui::IsMouseClicked(0)) selectedShapeId = global_s.id;
                    }
                }
            }
             // =========================================================
            // C. JALUR STANDARD (RECT / LINE / FIB)
            // =========================================================
            else {
                DrawShape temp_s;
                temp_s.type = (global_s.type == "RECT") ? DrawShape::RECT : (global_s.type == "FIB" ? DrawShape::FIB : DrawShape::LINE);
                temp_s.globalId = global_s.id; 
                temp_s.color = global_s.color;
                temp_s.selected = isSelected;

                temp_s.y0 = global_s.price0; temp_s.y1 = global_s.price1;
                temp_s.x0 = ChartCanvas::GetStableX(global_s.time0, candles, tf);
                temp_s.x1 = ChartCanvas::GetStableX(global_s.time1, candles, tf);

                if (temp_s.selected) {
                    ImVec2 p0 = ImPlot::PlotToPixels(temp_s.x0, temp_s.y0);
                    ImVec2 p1 = ImPlot::PlotToPixels(temp_s.x1, temp_s.y1);
                    
                    float topY = std::min(p0.y, p1.y); 
                    float centerX = (p0.x + p1.x) * 0.5f;

                    // Update shape bounds tiap frame
                    popupShapeTopY = topY;
                    popupShapeCenterX = centerX;
                    hasShapeBounds = true;

                    // Render Lingkaran Handle
                    ImU32 hFill = IM_COL32(255, 255, 255, 255); 
                    ImU32 hStroke = IM_COL32(0, 0, 0, 255);
                    float r = 7.0f;

                    if (temp_s.type == DrawShape::RECT) {
                        ImVec2 tl(std::min(p0.x, p1.x), std::min(p0.y, p1.y));
                        ImVec2 br(std::max(p0.x, p1.x), std::max(p0.y, p1.y));
                        ImVec2 tr(br.x, tl.y); ImVec2 bl(tl.x, br.y);
                        draw->AddCircleFilled(tl, r, hFill); draw->AddCircle(tl, r, hStroke);
                        draw->AddCircleFilled(tr, r, hFill); draw->AddCircle(tr, r, hStroke);
                        draw->AddCircleFilled(bl, r, hFill); draw->AddCircle(bl, r, hStroke);
                        draw->AddCircleFilled(br, r, hFill); draw->AddCircle(br, r, hStroke);
                    } else {
                        draw->AddCircleFilled(p0, r, hFill); draw->AddCircle(p0, r, hStroke);
                        draw->AddCircleFilled(p1, r, hFill); draw->AddCircle(p1, r, hStroke);
                    }
                    
                    // Render Garis Fibo Overlay (saat edit)
                    if (temp_s.type == DrawShape::FIB) DrawFibRetracement(temp_s, draw);

                    // Popup: buka sekali
                    if (!isPopupOpen || activePopupID != global_s.id) {
                         isPopupOpen = true; activePopupID = global_s.id; popupAnimProgress = 0.0f;
                         PinPopup(mousePos);
                    }

                    if (!global_s.locked) {
                        temp_s.activeDragHandle = activeDragHandle;
                        temp_s.dragStartPoint = dragStartPoint;
                        
                        if (!isDraggingThisFrame && ImPlot::IsPlotHovered()) {
                            if (temp_s.type == DrawShape::RECT) DetectRectangleHandles(temp_s, mousePos); 
                            else HandlePointDrag(temp_s, mp, mousePos, candles);
                            hoveredHandle = temp_s.hoveredHandle;
                        }

                        if (ImGui::IsMouseDown(0)) {
                             if (temp_s.type == DrawShape::RECT) HandleRectangleDrag(temp_s, mp, candles);
                             else HandlePointDrag(temp_s, mp, mousePos, candles);
                        }

                        activeDragHandle = temp_s.activeDragHandle;
                        dragStartPoint = temp_s.dragStartPoint;
                        if (temp_s.activeDragHandle != HandleType::NONE) {
                            isDraggingThisFrame = true;
                            blockChart = true;
                        }
                    }
                } 

                if (!isDraggingThisFrame && ImPlot::IsPlotHovered() && IsShapeHovered(temp_s, mousePos)) {
                    isAnyShapeHovered = true;
                    // 🔥 FIX: Standard shapes (LINE/RECT/FIB) sebelumnya hanya set
                    // isAnyShapeHovered tanpa selectedShapeId → tidak bisa diklik di panel.
                    // Elliot/Brush/Text sudah punya baris ini. Sekarang standard shapes juga.
                    if (ImGui::IsMouseClicked(0)) {
                        selectedShapeId = global_s.id;
                        isPopupOpen = false;
                        activePopupID = "";
                        popupAnimProgress = 0.0f;
                    }
                }
            } 
        } 

      

        if (ImGui::IsMouseReleased(0)) {
            activeDragHandle = HandleType::NONE;
            isDraggingThisFrame = false;
            blockChart = false;
        }

        if (!isDraggingThisFrame && ImPlot::IsPlotHovered() && ImGui::IsMouseClicked(0) && !isAnyShapeHovered) {
            // Reset settingsOpen pada shape yang sedang diselect sebelum deselect
            if (!activePopupID.empty()) {
                GlobalShape* prevShape = g_shapes.GetShapePtr(activePopupID);
                if (prevShape) prevShape->settingsOpen = false;
            }
            selectedShapeId = "";
            activeDragHandle = HandleType::NONE;
            isPopupOpen = false;
            activePopupID = "";
            popupAnimProgress = 0.0f;
            ResetPopupPin();
        }
    }
    void HandleInput(const std::vector<Candle>& candles, const std::string& activeTF) {
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !isDrawing && activeDragHandle == HandleType::NONE && ImPlot::IsPlotHovered()) {
            ImVec2 mousePos = ImGui::GetIO().MousePos;
            auto shapes = g_shapes.GetShapes();
            double tpCandle = ChartCanvas::GetTimePerCandle(activeTF);
            if (candles.empty()) return;

            // Simpan candles untuk konversi
            lastCandles = &candles;

            for (const auto& s : shapes) {
                if (!s.visible) continue;
                double x0 = ChartCanvas::GetStableX(s.time0, candles, tpCandle);
                double x1 = ChartCanvas::GetStableX(s.time1, candles, tpCandle);
                ImVec2 p0 = ImPlot::PlotToPixels(ImPlotPoint(x0, s.price0));
                ImVec2 p1 = ImPlot::PlotToPixels(ImPlotPoint(x1, s.price1));

                float distSq = 99999.0f;
                if (s.type == "RECT") {
                     ImVec2 minP = ImVec2(std::min(p0.x, p1.x), std::min(p0.y, p1.y));
                     ImVec2 maxP = ImVec2(std::max(p0.x, p1.x), std::max(p0.y, p1.y));
                     if (mousePos.x >= minP.x && mousePos.x <= maxP.x && mousePos.y >= minP.y && mousePos.y <= maxP.y) distSq = 0;
                } else distSq = GetPointLineDistanceSq(mousePos, p0, p1);

                if (distSq < 625.0f) {
                    selectedShapeId = s.id;
                    popupAnimProgress = 0.0f;
                    popupShapeTopY = std::min(p0.y, p1.y);
                    popupShapeCenterX = (p0.x + p1.x) * 0.5f;
                    hasShapeBounds = true;
                    PinPopup(mousePos);
                    break;
                }
            }
        }
    }
    void RenderShapePopup() {
        // Cek Validitas
        if (!isPopupOpen || activePopupID.empty()) return;
        GlobalShape* s = g_shapes.GetShapePtr(activePopupID);
        if (!s) { isPopupOpen = false; return; }

        // =========================================================
        // 🔥 POPUP POSISI: DI ATAS SHAPE (DIKUNCI, TIDAK IKUT MOUSE)
        // Hanya hitung sekali saat popup pertama kali muncul, lalu posisi dikunci
        // =========================================================
        if (!popupPinned) {
            ImVec2 mousePos = ImGui::GetIO().MousePos;
            float pw = 280.0f;
            float offsetAbove = 100.0f;
            ImVec2 displaySz = ImGui::GetIO().DisplaySize;

            // X: rata tengah di shape
            float px = popupShapeCenterX - pw * 0.5f;

            // Y: di atas shape (fixed, tidak ikut mouse)
            float py = hasShapeBounds ? popupShapeTopY - offsetAbove : mousePos.y - offsetAbove;

            // Clamp agar tidak keluar layar
            if (px < 5.0f) px = 5.0f;
            if (px + pw > displaySz.x - 5.0f) px = displaySz.x - pw - 5.0f;
            if (py < 5.0f) py = 10.0f;
            if (py + 50.0f > displaySz.y - 5.0f) py = displaySz.y - 55.0f;

            popupPos = ImVec2(px, py);
            popupPinned = true; // Kunci posisi setelah hitung pertama
        }
       // =========================================================
        // 🔥 LOGIKA JALUR FIBONACCI (BUBBLE + SETTINGS WINDOW)
        // =========================================================
        if (s->type == "FIB") {
            // Gunakan animasi popup yang sama dengan Rect/Line agar mulus
            float animSpeed = 8.0f; 
            popupAnimProgress += ImGui::GetIO().DeltaTime * animSpeed;
            if (popupAnimProgress > 1.0f) popupAnimProgress = 1.0f;
            float t = popupAnimProgress;
            float scale = 1.0f - std::pow(1.0f - t, 3.0f); 

            // Hitung lebar bubble (karena nambah ikon Settings, jadi lebih lebar)
            float targetW = 280.0f; // Lebih lebar dikit
            float targetH = 50.0f;
            float currentW = targetW * scale; float currentH = targetH * scale;
            float animX = popupPos.x + (targetW - currentW) * 0.5f;
            float animY = popupPos.y + (targetH - currentH) * 0.5f;

            ImGui::SetNextWindowPos(ImVec2(animX, animY), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(currentW, currentH));
            ImGui::SetNextWindowBgAlpha(0.90f * scale); 

            // Styling Bubble
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.0f * scale);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
            ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.15f, 0.15f, 0.15f, 0.95f)); 

            ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking | 
                                     ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | 
                                     ImGuiWindowFlags_NoFocusOnAppearing;
            
            if (activeDragHandle != HandleType::NONE) flags |= ImGuiWindowFlags_NoInputs; 

            // 1. RENDER BUBBLE KECIL
            if (ImGui::Begin("##FiboBubble", nullptr, flags)) {
                ImGui::SetWindowFontScale(scale);
                
                // Variabel static untuk menyimpan state window Settings
                // Static di sini aman selama popup ID unik (activePopupID)
                static bool isSettingsOpen = false;
                
                // Jika ganti shape, tutup setting dulu biar gak bingung
                static std::string lastFiboId = "";
                if (lastFiboId != activePopupID) {
                    isSettingsOpen = false;
                    lastFiboId = activePopupID;
                }

                // Panggil FiboEditUI (Bubble + Logic Window Besar)
                if (FiboEditUI::Render(*s, isSettingsOpen)) {
                    // Jika tombol Delete ditekan
                    g_shapes.RemoveShape(activePopupID);
                    isPopupOpen = false; activePopupID = ""; selectedShapeId = "";
                    ResetPopupPin();
                }

                ImGui::SetWindowFontScale(1.0f);
            }
            ImGui::End();
            ImGui::PopStyleColor(); ImGui::PopStyleVar(2);
        }
        // =========================================================
        // LOGIKA JALUR NON-FIB (ENHANCED: Bubble + Settings Window)
        // LINE, RECT, TEXT, BRUSH, ELLIOT → ShapeEditUI::Render()
        // =========================================================
        else {
            // Animasi Popup (Membesar halus)
            float animSpeed = 8.0f; 
            popupAnimProgress += ImGui::GetIO().DeltaTime * animSpeed;
            if (popupAnimProgress > 1.0f) popupAnimProgress = 1.0f;
            float t = popupAnimProgress;
            float scale = 1.0f - std::pow(1.0f - t, 3.0f); 

            // Lebih lebar karena ada 6 ikon (Lock, Copy, Color, Thick, Settings, Delete)
            float targetW = 280.0f; float targetH = 50.0f;
            float currentW = targetW * scale; float currentH = targetH * scale;
            float animX = popupPos.x + (targetW - currentW) * 0.5f;
            float animY = popupPos.y + (targetH - currentH) * 0.5f;

            ImGui::SetNextWindowPos(ImVec2(animX, animY), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(currentW, currentH));
            ImGui::SetNextWindowBgAlpha(0.90f * scale); 

            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.0f * scale);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
            ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.15f, 0.15f, 0.15f, 0.95f)); 

            ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking | 
                                     ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | 
                                     ImGuiWindowFlags_NoFocusOnAppearing;
            
            if (activeDragHandle != HandleType::NONE) flags |= ImGuiWindowFlags_NoInputs; 

            if (ImGui::Begin("##ShapeFloatingMenu", nullptr, flags)) {
                ImGui::SetWindowFontScale(scale);
                
                // Panggil Enhanced ShapeEditUI (Bubble Bar + Settings Window)
                ShapeAction act = ShapeEditUI::Render(*s);
                
                if (act == ACT_LOCK) s->locked = !s->locked;
                else if (act == ACT_COPY) {
                    g_shapes.DuplicateShape(activePopupID);
                    isPopupOpen = false; activePopupID = ""; selectedShapeId = "";
                    ResetPopupPin();
                }
                else if (act == ACT_DELETE) {
                    if (!s->locked) {
                        g_shapes.RemoveShape(activePopupID);
                        isPopupOpen = false; activePopupID = ""; selectedShapeId = "";
                        ResetPopupPin();
                    }
                }
                ImGui::SetWindowFontScale(1.0f);
            }
            ImGui::End();
            ImGui::PopStyleColor(); ImGui::PopStyleVar(2);
        }
    }
  void StartDrawing(DrawShape::Type type) { 
        activeTool = type; 
        isDrawing = true; 
        blockChart = true; 
        isPopupOpen = false; 
        selectedShapeId = ""; 

        // 🚩 RESET HANDLER (Matikan semua biar aman)
        elliotHandler.Cancel(); 
        textHandler.Stop();
        brushHandler.Stop(); // Tambahan: Matikan brush juga jaga-jaga

        // 🚩 START SPECIFIC HANDLER
        if (type == DrawShape::ELLIOT) {
            elliotHandler.Start(); 
        } 
        else if (type == DrawShape::TEXT) { 
            textHandler.Start();
        }
        else if (type == DrawShape::BRUSH) {
            // Brush start logic biasanya saat klik, tapi bisa reset state disini
            brushHandler.points.clear();
        }
    }
       
    // Fungsi lama ini tidak perlu diubah, tapi jarang dipakai kalau sudah ada StartDrawing di atas
    void StartElliotDrawing() { 
        activeTool = DrawShape::ELLIOT; 
        elliotHandler.Start(); 
        isDrawing = true; 
        blockChart = true; 
    }

    void StopDrawing() { 
        isDrawing = false; 
        blockChart = false; 
        
        // 🚩 TAMBAHAN OPSIONAL (Biar Rapi)
        // Matikan semua handler saat stop total
        elliotHandler.Cancel();
        textHandler.Stop();
    }
  void HandleDrawing(const ImPlotPoint& mp, ImDrawList* draw, const std::vector<Candle>& activeCandles) {
    
    // ========================================================
    // 🖌️ JALUR 1: BRUSH (Freehand Drag)
    // ========================================================
    if (activeTool == DrawShape::BRUSH) {
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            brushHandler.color = currentColor; 
            brushHandler.Start(mp, activeCandles);
            blockChart = true; 
        }
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            brushHandler.Update(mp, activeCandles);
            brushHandler.RenderPreview(draw, activeCandles);
        }
        if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
            brushHandler.SaveToGlobal(activePanel); 
            brushHandler.Stop();
            blockChart = false; 
            isDrawing = false;  
        }
        return; 
    }

    // ========================================================
    // 🌊 JALUR 2: ELLIOT (Multi-Point Tap)
    // ========================================================
    if (activeTool == DrawShape::ELLIOT) {
        if (!elliotHandler.isActive) elliotHandler.Start();
        elliotHandler.Update(mp, draw, activeCandles);
        if (!elliotHandler.isActive) {
            isDrawing = false;
            blockChart = false;
        }
        return; 
    }

        // ========================================================
        // 🔤 JALUR 3: TEXT TOOL (Clean Code)
        // ========================================================
        if (activeTool == DrawShape::TEXT) {
            
            // 👇 UPDATE BAGIAN INI (Tambahkan 'activeCandles' di akhir)
            textHandler.Update(
                mp, 
                draw, 
                selectedShapeId, 
                reinterpret_cast<int&>(activeTool), 
                isDrawing, 
                blockChart, 
                currentColor,
                activeCandles  // <--- TAMBAHAN: Kirim data candle ke handler
            );
            
            return;
        }
    // ========================================================
    // 📏 JALUR 4: LINE, RECT, FIBO (Standard Drag Style)
    // ========================================================
    //
    // MODE NORMAL (tidak ada panel indicator):
    //   Tap 1: IsMouseClicked → set titik awal
    //   Drag : IsMouseDown   → update titik akhir + preview
    //   Lepas: IsMouseReleased → simpan shape
    //
    // MODE PANEL INDICATOR (hasPanelMode = true):
    //   Tap 1: IsMouseClicked → HANYA cek/konfirmasi area (waitingPanelConfirm=true)
    //   Tap 2: IsMouseClicked lagi → baru set titik awal (mulai drawing)
    //   Drag : IsMouseDown   → update titik akhir + preview
    //   Lepas: IsMouseReleased → simpan shape
    // ========================================================

    // ── MODE PANEL: tap pertama hanya konfirmasi, tap kedua mulai ──
    #ifdef __EMSCRIPTEN__
    if (hasPanelMode && g_isTouchActive) {
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            if (!waitingPanelConfirm) {
                // Tap 1: tandai bahwa user sudah tap di panel
                waitingPanelConfirm = true;
                // Gambar lingkaran konfirmasi di posisi tap
                ImVec2 tapPx = ImPlot::PlotToPixels(mp);
                draw->AddCircle(tapPx, 12.f, IM_COL32(255,200,50,200), 16, 2.f);
                draw->AddCircle(tapPx, 6.f,  IM_COL32(255,200,50,120), 16, 1.f);
                return; // tunggu tap kedua
            } else {
                // Tap 2: mulai drawing
                waitingPanelConfirm = false;
                tempDrawingShape = {};
                tempDrawingShape.type  = activeTool;
                tempDrawingShape.x0    = mp.x;
                tempDrawingShape.y0    = mp.y;
                tempDrawingShape.x1    = mp.x;
                tempDrawingShape.y1    = mp.y;
                tempDrawingShape.color = currentColor;
                tempDrawingShape.dragStartPoint = mp;
            }
        }
    } else
    #endif
    {
        // ── MODE NORMAL: tap langsung mulai ─────────────────────────
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            waitingPanelConfirm = false;
            tempDrawingShape = {};
            tempDrawingShape.type  = activeTool;
            tempDrawingShape.x0    = mp.x;
            tempDrawingShape.y0    = mp.y;
            tempDrawingShape.x1    = mp.x;
            tempDrawingShape.y1    = mp.y;
            tempDrawingShape.color = currentColor;
            tempDrawingShape.dragStartPoint = mp;
        }
    }

    // DRAGGING — support mouse + touch
    // Touch: g_isTouchActive=true saat jari nempel, false saat lepas
    // prevTouchDrawing: deteksi transisi jari lepas → trigger "release"
    bool inputDown    = ImGui::IsMouseDown(ImGuiMouseButton_Left);
    bool inputRelease = ImGui::IsMouseReleased(ImGuiMouseButton_Left);
    #ifdef __EMSCRIPTEN__
    if (g_isTouchActive) {
        inputDown    = true;
        inputRelease = false;
    } else if (prevTouchDrawing) {
        // Frame ini touch baru saja lepas → perlakukan sebagai release
        inputRelease = true;
    }
    prevTouchDrawing = g_isTouchActive; // simpan untuk frame berikutnya
    #endif

    if (inputDown && !waitingPanelConfirm) {
        tempDrawingShape.x1 = mp.x;
        tempDrawingShape.y1 = mp.y;

        ImVec2 pStart = ImPlot::PlotToPixels(tempDrawingShape.dragStartPoint);
        ImVec2 pCurr  = ImPlot::PlotToPixels(mp);
        float distSq = (pCurr.x - pStart.x)*(pCurr.x - pStart.x)
                     + (pCurr.y - pStart.y)*(pCurr.y - pStart.y);
        if (distSq > 25.0f) {
            DrawShapeRender_Preview(tempDrawingShape, draw);
        }
    }

    // TOUCH END / MOUSE RELEASE
    if (inputRelease && !waitingPanelConfirm) {
        ImVec2 pStart = ImPlot::PlotToPixels(tempDrawingShape.dragStartPoint);
        ImVec2 pCurr  = ImPlot::PlotToPixels(ImPlotPoint(tempDrawingShape.x1, tempDrawingShape.y1));
        float distSq = (pCurr.x - pStart.x)*(pCurr.x - pStart.x)
                     + (pCurr.y - pStart.y)*(pCurr.y - pStart.y);

        if (distSq > 25.0f && !activeCandles.empty()) {
            double tf = ChartCanvas::GetTimePerCandle(g_activeTF);

            double time0 = ChartCanvas::GetStableTime(tempDrawingShape.x0, activeCandles, tf);
            double time1 = ChartCanvas::GetStableTime(tempDrawingShape.x1, activeCandles, tf);

            std::string typeStr = "LINE";
            bool isFilled = false;
            if (activeTool == DrawShape::RECT) { typeStr = "RECT"; isFilled = true; }
            else if (activeTool == DrawShape::FIB) { typeStr = "FIB"; }

            g_shapes.AddShape(typeStr, time0, tempDrawingShape.y0,
                              time1, tempDrawingShape.y1,
                              tempDrawingShape.color, 1.5f, isFilled, activePanel);
        }

        isDrawing  = false;
        blockChart = false;
        waitingPanelConfirm = false;
    }
}
    // ---------------------------
    // 🔥 UPDATE: DRAW FIBO & PREVIEW
    // ---------------------------
    // ---------------------------
    // 🔥 UPDATE: PREVIEW FIBO (SINKRON DENGAN SETTINGAN)
    // ---------------------------
    void DrawFibRetracement(const DrawShape& s, ImDrawList* draw) const {
        // 1. Ambil Konfigurasi Default yang Disimpan User
        // Ini kuncinya! Preview akan meniru settingan "Save Default"
        FibConfig cfg = g_shapes.GetDefaultFibConfig();

        // 2. Koordinat
        ImVec2 p0 = ImPlot::PlotToPixels(s.x0, s.y0);
        ImVec2 p1 = ImPlot::PlotToPixels(s.x1, s.y1);
        double y_diff = s.y1 - s.y0;

        // 3. GAMBAR TRENDLINE (Diagonal) - Support Dashed/Solid
        if (cfg.showTrendline) {
            ImU32 colTrend = ImGui::ColorConvertFloat4ToU32(cfg.trendlineColor);
            
            // Logic Garis Putus-putus (Sama dengan GlobalShapeManager)
            if (cfg.trendlineStyle == 0) {
                draw->AddLine(p0, p1, colTrend, 1.5f); // Solid
            } else {
                // Dashed / Dotted Logic
                float totalDist = std::sqrt((p1.x - p0.x)*(p1.x - p0.x) + (p1.y - p0.y)*(p1.y - p0.y));
                float dashSize = (cfg.trendlineStyle == 1) ? 10.0f : 2.0f;
                float gapSize  = (cfg.trendlineStyle == 1) ? 8.0f : 4.0f;
                
                if (totalDist > 0) {
                    ImVec2 dir = ImVec2((p1.x - p0.x) / totalDist, (p1.y - p0.y) / totalDist);
                    float currentDist = 0.0f;
                    while (currentDist < totalDist) {
                        float nextDist = std::min(currentDist + dashSize, totalDist);
                        ImVec2 start = ImVec2(p0.x + dir.x * currentDist, p0.y + dir.y * currentDist);
                        ImVec2 end   = ImVec2(p0.x + dir.x * nextDist,    p0.y + dir.y * nextDist);
                        draw->AddLine(start, end, colTrend, 1.5f);
                        currentDist += (dashSize + gapSize);
                    }
                }
            }
        }

        // 4. GAMBAR LEVELS (Horizontal)
        // Loop melalui level yang disimpan di Config, bukan array statis!
        for (const auto& lvl : cfg.levels) {
            if (!lvl.visible) continue;

            // Logic Reverse
            double effectiveCoeff = cfg.reversed ? (1.0 - lvl.coeff) : lvl.coeff;
            double yVal = s.y0 + y_diff * effectiveCoeff;
            
            // Hitung Pixel Y
            // Trik: Kita proyeksikan X0 dan Y_Level
            ImVec2 a = ImPlot::PlotToPixels(s.x0, yVal);
            ImVec2 b = ImPlot::PlotToPixels(s.x1, yVal);
            
            // Fix Y-Pixel agar lurus horizontal (karena PlotToPixels bisa miring kalau chart aspect ratio beda)
            // Kita paksa Y nya sama dengan proyeksi level
            // (Sebenarnya ImPlot::PlotToPixels sudah benar, tapi untuk aman):
            // float screenY = ImPlot::PlotToPixels(0, yVal).y; 
            // a.y = screenY; b.y = screenY;

            ImVec4 lineColVec = lvl.color;
            lineColVec.w *= 0.6f; // Preview agak transparan
            ImU32 lineCol = ImGui::ColorConvertFloat4ToU32(lineColVec);

            // Logic Style Horizontal (Solid/Dashed)
            if (cfg.horizStyle == 0) {
                draw->AddLine(a, b, lineCol, 1.0f);
            } else {
                float width = b.x - a.x;
                if (std::abs(width) > 0) {
                    float dashSize = (cfg.horizStyle == 1) ? 10.0f : 2.0f;
                    float gapSize  = (cfg.horizStyle == 1) ? 8.0f : 4.0f;
                    
                    float curX = a.x;
                    // Handle tarik dari kanan ke kiri (width negatif)
                    bool forward = (b.x > a.x);
                    
                    if (forward) {
                        while (curX < b.x) {
                            float nextX = std::min(curX + dashSize, b.x);
                            draw->AddLine(ImVec2(curX, a.y), ImVec2(nextX, b.y), lineCol, 1.0f);
                            curX += (dashSize + gapSize);
                        }
                    } else {
                        while (curX > b.x) {
                            float nextX = std::max(curX - dashSize, b.x);
                            draw->AddLine(ImVec2(curX, a.y), ImVec2(nextX, b.y), lineCol, 1.0f);
                            curX -= (dashSize + gapSize);
                        }
                    }
                }
            }
        }
    }

   void DrawShapeRender_Preview(const DrawShape& s, ImDrawList* draw) const {
        ImVec2 p0 = ImPlot::PlotToPixels(s.x0, s.y0); 
        ImVec2 p1 = ImPlot::PlotToPixels(s.x1, s.y1);
        ImU32 col = ImGui::ColorConvertFloat4ToU32(s.color);
        
        if(s.type==DrawShape::LINE) {
            draw->AddLine(p0, p1, col, 1.5f);
        }
        else if(s.type==DrawShape::RECT) { 
            draw->AddRectFilled(p0, p1, IM_COL32(255,255,255,40)); 
            draw->AddRect(p0, p1, col, 0.0f, 0, 1.5f); 
        }
        // 🔥 TAMBAHAN PREVIEW TEKS
        else if(s.type==DrawShape::TEXT) {
            // Gambar kotak bayangan (biar user tau seberapa besar teksnya nanti)
            draw->AddRect(p0, p1, IM_COL32(255, 255, 255, 100), 0.0f, 0, 1.0f);
            
            // Gambar Teks Dummy "Text Preview"
            // Posisinya di p0 (titik awal klik)
            draw->AddText(p0, col, "Text Preview");
        }
        else if(s.type==DrawShape::FIB) { 
            DrawFibRetracement(s, draw); 
        }
        else if(s.type==DrawShape::ELLIOT) { 
            draw->AddLine(p0, p1, col, 1.5f); 
            draw->AddText(p1, col, "(1)"); 
        } 
    }
    void DetectRectangleHandles(DrawShape& s, const ImVec2& mousePos) {
        if (s.activeDragHandle != HandleType::NONE) return;
        ImVec2 p0 = ImPlot::PlotToPixels(s.x0, s.y0); ImVec2 p1 = ImPlot::PlotToPixels(s.x1, s.y1);
        ImVec2 topLeft(std::min(p0.x, p1.x), std::min(p0.y, p1.y));
        ImVec2 bottomRight(std::max(p0.x, p1.x), std::max(p0.y, p1.y));
        ImVec2 topRight(bottomRight.x, topLeft.y); ImVec2 bottomLeft(topLeft.x, bottomRight.y);
        const float r = 100.0f; s.hoveredHandle = HandleType::NONE;
        if (LengthSqr(mousePos-topLeft)<r) s.hoveredHandle=HandleType::TOP_LEFT;
        else if (LengthSqr(mousePos-topRight)<r) s.hoveredHandle=HandleType::TOP_RIGHT;
        else if (LengthSqr(mousePos-bottomLeft)<r) s.hoveredHandle=HandleType::BOTTOM_LEFT;
        else if (LengthSqr(mousePos-bottomRight)<r) s.hoveredHandle=HandleType::BOTTOM_RIGHT;
        else if (ImGui::IsMouseHoveringRect(topLeft, bottomRight, false)) s.hoveredHandle=HandleType::BODY;
        if(s.hoveredHandle!=HandleType::NONE) ImGui::SetMouseCursor(s.hoveredHandle==HandleType::BODY?ImGuiMouseCursor_ResizeAll:ImGuiMouseCursor_ResizeNWSE);
    }

    void HandleRectangleDrag(DrawShape& s, const ImPlotPoint& mp, const std::vector<Candle>& candles) {
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && s.hoveredHandle != HandleType::NONE) { s.activeDragHandle = s.hoveredHandle; s.dragStartPoint = mp; }
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left) && s.activeDragHandle != HandleType::NONE) {
            ImGui::GetIO().MouseDelta = ImVec2(0,0);
            double dx=mp.x-s.dragStartPoint.x, dy=mp.y-s.dragStartPoint.y;
            if(s.activeDragHandle==HandleType::BODY){ s.x0+=dx;s.y0+=dy;s.x1+=dx;s.y1+=dy; }
            else if(s.activeDragHandle==HandleType::TOP_LEFT){s.x0=mp.x;s.y0=mp.y;}
            else if(s.activeDragHandle==HandleType::BOTTOM_RIGHT){s.x1=mp.x;s.y1=mp.y;}
            else if(s.activeDragHandle==HandleType::TOP_RIGHT){s.x1=mp.x;s.y0=mp.y;}
            else if(s.activeDragHandle==HandleType::BOTTOM_LEFT){s.x0=mp.x;s.y1=mp.y;}
            if(s.activeDragHandle==HandleType::BODY) s.dragStartPoint=mp;
            UpdateGlobalShape(s, candles);
        }
        if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) s.activeDragHandle=HandleType::NONE;
    }

    void HandlePointDrag(DrawShape& s, const ImPlotPoint& mp, const ImVec2& mousePos, const std::vector<Candle>& candles) {
        ImVec2 p0 = ImPlot::PlotToPixels(s.x0, s.y0); ImVec2 p1 = ImPlot::PlotToPixels(s.x1, s.y1);
        if (s.activeDragHandle == HandleType::NONE) {
            if (LengthSqr(mousePos - p0) < 100) s.hoveredHandle = HandleType::TOP_LEFT;
            else if (LengthSqr(mousePos - p1) < 100) s.hoveredHandle = HandleType::BOTTOM_RIGHT;
            else if (IsShapeHovered(s, mousePos)) s.hoveredHandle = HandleType::BODY;
            else s.hoveredHandle = HandleType::NONE;
        }
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && s.hoveredHandle != HandleType::NONE) { s.activeDragHandle = s.hoveredHandle; s.dragStartPoint = mp; }
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left) && s.activeDragHandle != HandleType::NONE) {
             ImGui::GetIO().MouseDelta = ImVec2(0, 0);
             if (s.activeDragHandle == HandleType::TOP_LEFT) { s.x0 = mp.x; s.y0 = mp.y; }
             else if (s.activeDragHandle == HandleType::BOTTOM_RIGHT) { s.x1 = mp.x; s.y1 = mp.y; }
             else if (s.activeDragHandle == HandleType::BODY) {
                 double dx = mp.x - s.dragStartPoint.x; double dy = mp.y - s.dragStartPoint.y;
                 s.x0 += dx; s.y0 += dy; s.x1 += dx; s.y1 += dy; s.dragStartPoint = mp;
             }
             UpdateGlobalShape(s, candles);
        }
        if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) s.activeDragHandle = HandleType::NONE;
    }

    void UpdateGlobalShape(const DrawShape& s, const std::vector<Candle>& candles) {
        double tf = ChartCanvas::GetTimePerCandle(g_activeTF);
        double time0 = ChartCanvas::GetStableTime(s.x0, candles, tf);
        double time1 = ChartCanvas::GetStableTime(s.x1, candles, tf);
        g_shapes.UpdateShape(s.globalId, time0, s.y0, time1, s.y1);
    }
    
    bool IsShapeHovered(const DrawShape& s, const ImVec2& mousePos) const {
        ImVec2 p0 = ImPlot::PlotToPixels(s.x0, s.y0); ImVec2 p1 = ImPlot::PlotToPixels(s.x1, s.y1);
        if (s.type == DrawShape::RECT) {
            ImVec2 min_p(std::min(p0.x, p1.x), std::min(p0.y, p1.y));
            ImVec2 max_p(std::max(p0.x, p1.x), std::max(p0.y, p1.y));
            return ImGui::IsMouseHoveringRect(min_p, max_p, false);
        } else return GetPointLineDistanceSq(mousePos, p0, p1) < 25.0f;
    }

    // ==========================================
// FILE: CDrawingManager.h
// ==========================================
// FILE: CDrawingManager.h

// 1. Ubah Parameter: Tambahkan 'currentViewCandles'
void RenderTFOverlays(const std::string& activeTF, 
                      const std::vector<Candle>& currentViewCandles, // <--- INI TAMBAHAN PENTING
                      ImDrawList* draw, 
                      std::function<float(const std::string&)> timeConverter, 
                      std::function<float(double)> priceConverter) 
{
    std::lock_guard<std::mutex> lock(mtx);
    
    // HAPUS BARIS INI: if (g_allCandles.count(activeTF)) ...
    // HAPUS BARIS INI: auto& candles = g_allCandles.at(activeTF);
    
    // GANTI JADI INI:
    // Kita pakai data yang dikirim dari luar (bisa data Replay, bisa data Live)
    if (currentViewCandles.empty()) return;
    const auto& candles = currentViewCandles;

    // Ambil durasi candle untuk hitungan proyeksi
    double currentTFSec = ChartCanvas::GetTimePerCandle(activeTF);

    for (auto& kv : drawingsByTF) {
        const auto& drawings = kv.second;

        for (const auto& s : drawings) {
            double t0_val = ChartCanvas::TimeStrToDouble(s.time0);
            double t1_val = ChartCanvas::TimeStrToDouble(s.time1);

            // Sekarang GetStableX menghitung berdasarkan 'candles' yang dipotong (Replay Safe)
            float x0 = (float)ChartCanvas::GetStableX(t0_val, candles, currentTFSec);
            float x1 = (float)ChartCanvas::GetStableX(t1_val, candles, currentTFSec);

            ImVec2 p0 = ImPlot::PlotToPixels(ImPlotPoint(x0, s.price0));
            ImVec2 p1 = ImPlot::PlotToPixels(ImPlotPoint(x1, s.price1));

            // Render Shape
            draw->AddRectFilled(p0, p1, IM_COL32(255, 255, 255, 25));
            draw->AddRect(p0, p1, ImGui::ColorConvertFloat4ToU32(s.color), 0.0f, 0, s.thickness);
        }
    }
}
    void AddTFOverlay(const std::string& tf, const TFShape& t) { std::lock_guard<std::mutex> lock(mtx); drawingsByTF[tf].push_back(t); }
    void ClearTF(const std::string& tf) { std::lock_guard<std::mutex> lock(mtx); drawingsByTF[tf].clear(); }
    void HandleFibonacciInteraction(DrawShape& s, const ImPlotPoint& mp, const ImVec2& mousePos, const std::vector<Candle>& candles) { HandlePointDrag(s, mp, mousePos, candles); }
};