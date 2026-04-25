#pragma once
#include <string>
#include <vector>
#include <unordered_map> 
#include <mutex>
#include <cmath>
#include <algorithm>
#include <iostream>
// Helper: hitung panjang 1 karakter UTF-8 (1-4 bytes)
static inline int Utf8CharLen(const char* s) {
    if (!s) return 0;
    unsigned char c = (unsigned char)*s;
    if (c < 0x80)    return 1; // ASCII
    if ((c & 0xE0) == 0xC0) return 2;
    if ((c & 0xF0) == 0xE0) return 3;
    if ((c & 0xF8) == 0xF0) return 4;
    return 1; // fallback
}
#include <iomanip>  // <--- Buat std::hex
#include <random>   // <--- Buat random_device & mt19937
#include <sstream>  // <--- Buat stringstream (bikin UUID string)
#include "implot.h"
#include "ChartCanvas.h"
#include "nlohmann/json.hpp"
#include "Candle.h"

// Helper Operator ImVec2
static inline ImVec2 operator+(const ImVec2& a, const ImVec2& b) { return ImVec2(a.x + b.x, a.y + b.y); }
static inline ImVec2 operator-(const ImVec2& a, const ImVec2& b) { return ImVec2(a.x - b.x, a.y - b.y); }
static inline ImVec2 operator*(const ImVec2& a, float s) { return ImVec2(a.x * s, a.y * s); }

extern std::string g_activeTF;
// Tambahkan struct ini untuk menampung settingan per level Fibo
struct FibLevel {
    double coeff;       // Contoh: 0.618
    ImVec4 color;       // Warna khusus level ini
    bool visible;  
    char label[32] = "";
         // Apakah level ini ditampilkan?
};

// Struct untuk settingan Global Fibo (mirip logic fibo1.cpp)
struct FibConfig {
    bool extendLeft = false;
    bool extendRight = false;
    bool showLabels = true;
    bool showBackground = true;
    bool labelRight = true;
    bool reversed = false;
    // 🔥 FITUR BARU DARI SCRIPT JS:
    // 🔥 FITUR BARU: TRENDLINE SETTINGS
    bool showTrendline = true;      // Tampilkan garis diagonal?
    int trendlineStyle = 1;         // 0=Solid, 1=Dashed, 2=Dotted
    ImVec4 trendlineColor = ImVec4(1.0f, 1.0f, 1.0f, 0.5f); // Default Putih Transparan
    int horizStyle = 0;
    // Daftar Level Default (Mengadopsi standar TradingView/JS yang kamu kirim)
    std::vector<FibLevel> levels = {
        { 0.0,   ImVec4(0.5f, 0.5f, 0.5f, 1.0f), true }, // Abu-abu
        { 0.236, ImVec4(1.0f, 0.2f, 0.2f, 1.0f), true }, // Merah
        { 0.382, ImVec4(0.2f, 1.0f, 0.2f, 1.0f), true }, // Hijau muda
        { 0.5,   ImVec4(0.2f, 1.0f, 0.2f, 1.0f), true }, // Hijau
        { 0.618, ImVec4(0.2f, 1.0f, 0.2f, 1.0f), true }, // Hijau (Golden Ratio)
        { 0.786, ImVec4(1.0f, 0.2f, 0.2f, 1.0f), true }, // Merah
        { 1.0,   ImVec4(0.5f, 0.5f, 0.5f, 1.0f), true }, // Abu-abu
        { 1.618, ImVec4(1.0f, 0.2f, 0.2f, 1.0f), true }, // Extension
        { 2.618, ImVec4(1.0f, 0.2f, 0.2f, 1.0f), false },
        { 3.618, ImVec4(1.0f, 0.2f, 0.2f, 1.0f), false },
        { 4.236, ImVec4(1.0f, 0.2f, 0.2f, 1.0f), false }
    };
};
struct GlobalShape {
    std::string id;        
    std::string type;      // "ELLIOT", "RECT", "LINE", "FIB", "TEXT", "BRUSH"
    // Panel tempat shape digambar:
    //   ""       = chart utama (default, koordinat Y = harga)
    //   "RSI"    = panel RSI (koordinat Y = 0..100)
    //   "Volume" = panel Volume, dst
    std::string sourcePanel = "";
    FibConfig fibConfig;      
    // DATA STANDARD (Untuk Line/Rect/Fib)
    double time0 = 0; double price0 = 0;
    double time1 = 0; double price1 = 0;
    std::string textContent = "klik Untuk membuat teks"; // Default isi teks
    float fontSize = 20.0f;  
     
    // DATA KHUSUS ELLIOT & BRUSH (Multi-Point Chain)
    std::vector<double> multiTime;
    std::vector<double> multiPrice;

    // === SHARED PROPERTIES ===
    ImVec4 color = ImVec4(1, 1, 1, 1);
    float thickness = 1.2f;
    bool visible = true;
    bool locked = false; 
    bool filled = false;
    int lineStyle = 0;            // 0=Solid, 1=Dashed, 2=Dotted (LINE, RECT, BRUSH, ELLIOT)

    // === LINE PROPERTIES ===
    bool extendLeft = false;      // Perpanjang garis ke kiri
    bool extendRight = false;     // Perpanjang garis ke kanan
    bool showEndpoints = false;   // Titik bulat di ujung P0 & P1

    // === RECT PROPERTIES ===
    float fillOpacity = 0.15f;    // Transparansi fill area
    bool showDimensions = false;  // Tampilkan W x H di dalam kotak
    ImVec4 textColor = ImVec4(1, 1, 1, 1); // Warna teks label rectangle
    std::string rectLabel = "";           // Custom label teks (kosong = tampilkan dimensi)
    ImVec4 fillColor = ImVec4(0.2f, 0.4f, 0.8f, 1.0f); // Warna fill terpisah dari border

    // RECT: Style (Corak)
    bool rectBorderVisible = true;        // Tampilkan garis border
    bool centerLine = false;              // Garis tengah horizontal
    ImVec4 centerLineColor = ImVec4(1, 1, 1, 0.5f); // Warna garis tengah
    int rectExtend = 0;                   // 0=Jangan perpanjang, 1=Perpanjang kiri, 2=Perpanjang kanan, 3=Keduanya

    // RECT: Text (Teks)
    float rectFontSize = 8.0f;            // Ukuran font teks dalam rect
    bool rectBold = false;                // Bold text
    bool rectItalic = false;              // Italic text
    int rectTextAlign = 0;                // 0=Kiri, 1=Tengah, 2=Kanan
    int rectVertAlign = 2;                // 0=Atas, 1=Tengah, 2=Bawah (default)

    // === TEXT PROPERTIES ===
    bool textBg = false;                  // Background kotak di belakang teks
    ImVec4 textBgColor = ImVec4(0, 0, 0, 0.7f); // Warna background

    // === BRUSH PROPERTIES ===
    float brushOpacity = 1.0f;     // Transparansi stroke brush

    // === ELLIOT PROPERTIES ===
    int elliLabelFormat = 0;       // 0="(0)", 1="0"
    bool elliShowPrice = false;    // Tampilkan harga di tiap titik

    // === SETTINGS STATE (untuk popup) ===
    bool settingsOpen = false;     // Apakah window settings sedang terbuka?
    
};

// ==========================================
// 🌍 GLOBAL SHAPE MANAGER (FINAL FIX)
// ==========================================
class GlobalShapeManager {
public:

    std::vector<GlobalShape> shapes;
    mutable std::mutex mtx;

    // 🔥 dirty flag: di-set true setiap ada perubahan shapes
    // Dibaca oleh SaveWebLayout() untuk tahu kapan perlu simpan ke localStorage
    bool dirty = false;

    // =========================================================
    // 🔥 BAGIAN 1: SISIPKAN VARIABEL & CONSTRUCTOR DI SINI
    // =========================================================
    
    // Variabel untuk menyimpan settingan default user
    FibConfig defaultFibConfig; 

    // Constructor: Dipanggil otomatis saat program mulai
    GlobalShapeManager() {
        // Kita isi defaultFibConfig dengan settingan standar MT5/TradingView
        defaultFibConfig.levels = {
            { 0.0,   ImVec4(0.5f, 0.5f, 0.5f, 1.0f), true, "Start" },
            { 0.236, ImVec4(1.0f, 0.2f, 0.2f, 1.0f), true, "" },
            { 0.382, ImVec4(0.2f, 1.0f, 0.2f, 1.0f), true, "" },
            { 0.5,   ImVec4(0.2f, 1.0f, 0.2f, 1.0f), true, "Half" },
            { 0.618, ImVec4(0.2f, 1.0f, 0.2f, 1.0f), true, "Golden" }, 
            { 0.786, ImVec4(1.0f, 0.2f, 0.2f, 1.0f), true, "" },
            { 1.0,   ImVec4(0.5f, 0.5f, 0.5f, 1.0f), true, "End" },
            { 1.618, ImVec4(1.0f, 0.2f, 0.2f, 1.0f), true, "Target 1" }
        };
        // Default style lainnya
        defaultFibConfig.showLabels = true;
        defaultFibConfig.showTrendline = true;
        defaultFibConfig.trendlineStyle = 1; // Dashed
    }

    // Helper: Ambil default (dipakai saat bikin shape baru)
    FibConfig GetDefaultFibConfig() {
        return defaultFibConfig;
    }
    
    // Helper: Simpan settingan saat ini jadi default baru
    void SaveAsDefault(const FibConfig& cfg) {
        defaultFibConfig = cfg;
        // Opsional: Nanti bisa ditambah logika simpan ke file JSON biar permanen
    }
    
    std::string GenerateUUID() {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_int_distribution<> dis(0, 15);
        std::stringstream ss;
        for (int i = 0; i < 8; i++) ss << std::hex << dis(gen);
        ss << "-";
        for (int i = 0; i < 4; i++) ss << std::hex << dis(gen);
        return ss.str();
    }

   // sourcePanel: "" = chart utama, "RSI" = panel RSI, dll
    void AddShape(const std::string& type, double time0, double price0, double time1, double price1,
                  ImVec4 color, float thickness, bool filled,
                  const std::string& sourcePanel = "") {
        std::lock_guard<std::mutex> lock(mtx);
        GlobalShape s;
        s.id = GenerateUUID();
        s.type = type;
        s.time0 = time0; s.price0 = price0;
        s.time1 = time1; s.price1 = price1;
        s.color = color; s.thickness = thickness;
        s.filled = filled;
        s.sourcePanel = sourcePanel;  // ← simpan panel asal

        if (type == "FIB") {
            s.fibConfig = defaultFibConfig;
        }
        shapes.push_back(s);
        dirty = true;
    }
    // --- [TAMBAHKAN INI DI BAWAHNYA] ---
    // Overload baru untuk memasukkan Shape yang sudah jadi (seperti Teks)
    void AddShape(const GlobalShape& s) {
        std::lock_guard<std::mutex> lock(mtx);
        shapes.push_back(s);
        dirty = true;
    }
    

    // FUNGSI BARU: Simpan Elliot sebagai SATU PAKET
    void AddElliotShape(const std::vector<double>& times, const std::vector<double>& prices,
                        ImVec4 color, const std::string& sourcePanel = "") {
        std::lock_guard<std::mutex> lock(mtx);
        GlobalShape s;
        s.id = GenerateUUID();
        s.type = "ELLIOT";
        s.multiTime = times;
        s.multiPrice = prices;
        s.color = color;
        s.thickness = 1.5f;
        s.sourcePanel = sourcePanel;
        shapes.push_back(s);
        dirty = true;
    }
    void AddBrushShape(const std::vector<double>& times, const std::vector<double>& prices,
                       ImVec4 color, float thickness, const std::string& sourcePanel = "") {
        std::lock_guard<std::mutex> lock(mtx);
        GlobalShape s;
        s.id = GenerateUUID();
        s.type = "BRUSH";
        s.multiTime = times;
        s.multiPrice = prices;
        s.color = color;
        s.thickness = thickness;
        s.sourcePanel = sourcePanel;
        shapes.push_back(s);
        dirty = true;
    }

    void UpdateShape(const std::string& id, double time0, double price0, double time1, double price1) {
        std::lock_guard<std::mutex> lock(mtx);
        for (auto& s : shapes) {
            if (s.id == id) {
                s.time0 = time0; s.price0 = price0;
                s.time1 = time1; s.price1 = price1;
                dirty = true;
                return;
            }
        }
    }
    
    // 🔥 FIX 1: Accessor untuk Edit Realtime (Return Reference &)
    // Tanpa tanda '&', kamu cuma mengedit copy-an, bukan data asli!
    std::vector<GlobalShape>& GetEditableShapes() {
        // Warning: Hati-hati multithreading di sini, tapi untuk WASM Single Thread ini aman & wajib.
        return shapes;
    }

    // Tetap sediakan versi Copy untuk keamanan render thread lain jika perlu
    std::vector<GlobalShape> GetShapes() {
        std::lock_guard<std::mutex> lock(mtx);
        return shapes;
    }

    GlobalShape* GetShapePtr(const std::string& id) {
        std::lock_guard<std::mutex> lock(mtx);
        for (auto& s : shapes) if (s.id == id) return &s;
        return nullptr;
    }

    void RemoveShape(const std::string& id) {
        std::lock_guard<std::mutex> lock(mtx);
        shapes.erase(std::remove_if(shapes.begin(), shapes.end(),
            [&](const GlobalShape& s) { return s.id == id; }), shapes.end());
        dirty = true;
    }

    void DuplicateShape(const std::string& originalId) {
        std::lock_guard<std::mutex> lock(mtx);
        auto it = std::find_if(shapes.begin(), shapes.end(), [&](const GlobalShape& s){ return s.id == originalId; });
        if(it != shapes.end()){
            GlobalShape copy = *it;
            copy.id = GenerateUUID();
            
            // Geser sedikit biar kelihatan duplikatnya
            double shiftP = (copy.price1 - copy.price0) * 0.1;
            if (shiftP == 0) shiftP = 10.0; 

            // Logic geser untuk Line/Rect
            double shiftT = (copy.time1 - copy.time0) * 0.1;
            copy.time0 += shiftT; copy.time1 += shiftT;
            copy.price0 += shiftP; copy.price1 += shiftP;
            
            // Logic geser untuk Elliot (Vector)
            if (!copy.multiTime.empty()) {
                for(size_t i=0; i<copy.multiTime.size(); i++) {
                    copy.multiTime[i] += shiftT; // Geser waktu (secara kasar)
                    copy.multiPrice[i] += shiftP;
                }
            }
            shapes.push_back(copy);
            dirty = true;
        }
    }

    void Clear() {
        std::lock_guard<std::mutex> lock(mtx);
        shapes.clear();
    }

  // =========================================================
    // 🎨 RENDER UTAMA (Menampilkan Semua Shape ke Layar)
    // =========================================================
    // panelFilter = "" → render hanya shapes dari chart utama
    // panelFilter = "RSI" → render hanya shapes dari panel RSI
    // panelFilter = "*" → render SEMUA shapes (tidak dipakai biasanya)
    void Render(ImDrawList* draw, const std::vector<Candle>& candles,
                const std::string& selectedId,
                const std::string& panelFilter = "") {
        std::lock_guard<std::mutex> lock(mtx);
        if (candles.empty()) return;

        double tf = ChartCanvas::GetTimePerCandle(g_activeTF);

        // Clip ke area plot aktif agar shape tidak bocor ke subplot lain
        ImVec2 plotPos = ImPlot::GetPlotPos();
        ImVec2 plotSz  = ImPlot::GetPlotSize();
        ImVec2 plotMax = ImVec2(plotPos.x + plotSz.x, plotPos.y + plotSz.y);
        draw->PushClipRect(plotPos, plotMax, true);

        for (const auto& s : shapes) {
            if (!s.visible) continue;
            // Filter: hanya render shape yang sesuai dengan panel ini
            if (s.sourcePanel != panelFilter) continue;
            
            // 1. Konversi Warna
            ImU32 col = ImGui::ColorConvertFloat4ToU32(s.color);

            // =============================================
            // A. LOGIKA ELLIOT & BRUSH (Multi-Point)
            // =============================================
            if ((s.type == "ELLIOT" || s.type == "BRUSH") && s.multiTime.size() >= 2) {
                std::vector<ImVec2> pixels;
                // Konversi semua titik ke Pixel
                for(size_t i=0; i<s.multiTime.size(); i++) {
                    double x = ChartCanvas::GetStableX(s.multiTime[i], candles, tf);
                    pixels.push_back(ImPlot::PlotToPixels(x, s.multiPrice[i]));
                }

                // ELLIOT: Gambar garis + Label + lineStyle support
                if (s.type == "ELLIOT") {
                    // Apply opacity via color alpha
                    ImVec4 ellCol = s.color;
                    ellCol.w *= s.brushOpacity; // reuse brushOpacity for elliot opacity
                    ImU32 ellColU32 = ImGui::ColorConvertFloat4ToU32(ellCol);

                    // Garis antar titik (dukungan lineStyle)
                    for(size_t i=0; i<pixels.size()-1; i++) {
                        if (s.lineStyle == 0) {
                            draw->AddLine(pixels[i], pixels[i+1], ellColU32, s.thickness);
                        } else {
                            // Dashed/Dotted
                            float dist = std::sqrt((float)((pixels[i+1].x-pixels[i].x)*(pixels[i+1].x-pixels[i].x)+(pixels[i+1].y-pixels[i].y)*(pixels[i+1].y-pixels[i].y)));
                            if (dist > 0) {
                                float dashSz = (s.lineStyle == 1) ? 10.0f : 2.0f;
                                float gapSz  = (s.lineStyle == 1) ? 8.0f : 4.0f;
                                ImVec2 dir((pixels[i+1].x - pixels[i].x) / dist, (pixels[i+1].y - pixels[i].y) / dist);
                                float curDist = 0.0f;
                                while (curDist < dist) {
                                    float nextDist = std::min(curDist + dashSz, dist);
                                    draw->AddLine(
                                        ImVec2(pixels[i].x + dir.x * curDist, pixels[i].y + dir.y * curDist),
                                        ImVec2(pixels[i].x + dir.x * nextDist, pixels[i].y + dir.y * nextDist),
                                        ellColU32, s.thickness);
                                    curDist += (dashSz + gapSz);
                                }
                            }
                        }
                    }
                    // Label tiap titik
                    for(size_t i=0; i<pixels.size(); i++) {
                        char buf[16];
                        // Format label berdasarkan elliLabelFormat
                        if (s.elliLabelFormat == 0)      sprintf(buf, "(%d)", (int)i);
                        else if (s.elliLabelFormat == 1) sprintf(buf, "%d", (int)i);
                        else if (s.elliLabelFormat == 2) {
                            // Huruf: A, B, C, D, E, F, ... (max 26 titik)
                            if (i < 26) sprintf(buf, "(%c)", 'A' + (char)i);
                            else        sprintf(buf, "(%d)", (int)i);
                        } else if (s.elliLabelFormat == 3) {
                            // Huruf tanpa kurung: A, B, C, D, ...
                            if (i < 26) sprintf(buf, "%c", 'A' + (char)i);
                            else        sprintf(buf, "%d", (int)i);
                        }
                        else                             sprintf(buf, "Wave-%d", (int)i);
                        draw->AddText(ImVec2(pixels[i].x+5, pixels[i].y-15), ellColU32, buf);

                        // Harga di tiap titik (opsional)
                        if (s.elliShowPrice) {
                            char priceBuf[32];
                            sprintf(priceBuf, "%.2f", s.multiPrice[i]);
                            draw->AddText(ImVec2(pixels[i].x+5, pixels[i].y+2), ellColU32, priceBuf);
                        }

                        // Highlight titik jika dipilih
                        if (s.id == selectedId) draw->AddCircleFilled(pixels[i], 4.0f, IM_COL32(255,255,255,200));
                    }
                }
                // BRUSH: Gambar Polyline (Lebih Halus) + opacity support + lineStyle
                else if (s.type == "BRUSH") {
                    ImVec4 brCol = s.color;
                    brCol.w *= s.brushOpacity;
                    ImU32 brColU32 = ImGui::ColorConvertFloat4ToU32(brCol);

                    if (s.lineStyle == 0 && pixels.size() >= 2) {
                        // Solid — langsung AddPolyline
                        draw->AddPolyline(pixels.data(), (int)pixels.size(), brColU32, 0, s.thickness);
                    } else if (pixels.size() >= 2) {
                        // Dashed / Dotted — manual per-segment
                        float dashSz = (s.lineStyle == 1) ? 10.0f : 2.0f;
                        float gapSz  = (s.lineStyle == 1) ? 8.0f  : 4.0f;
                        for (size_t i = 0; i < pixels.size() - 1; i++) {
                            float dx = pixels[i+1].x - pixels[i].x;
                            float dy = pixels[i+1].y - pixels[i].y;
                            float dist = std::sqrt(dx*dx + dy*dy);
                            if (dist > 0) {
                                ImVec2 dir(dx / dist, dy / dist);
                                float curD = 0.0f;
                                while (curD < dist) {
                                    float nextD = std::min(curD + dashSz, dist);
                                    draw->AddLine(
                                        ImVec2(pixels[i].x + dir.x * curD, pixels[i].y + dir.y * curD),
                                        ImVec2(pixels[i].x + dir.x * nextD, pixels[i].y + dir.y * nextD),
                                        brColU32, s.thickness);
                                    curD += (dashSz + gapSz);
                                }
                            }
                        }
                    }
                }
            }

            // =============================================
            // B. LOGIKA TEKS (dukung background)
            // =============================================
            else if (s.type == "TEXT") {
                // Konversi Time/Price ke Pixel
                double x0 = ChartCanvas::GetStableX(s.time0, candles, tf);
                ImVec2 p0 = ImPlot::PlotToPixels(x0, s.price0);
                
                // Hitung ukuran teks
                ImVec2 textSize = ImGui::GetFont()->CalcTextSizeA(s.fontSize, FLT_MAX, 0.0f, s.textContent.c_str());

                // Background teks (jika aktif)
                if (s.textBg) {
                    ImU32 bgCol = ImGui::ColorConvertFloat4ToU32(s.textBgColor);
                    float pad = 4.0f;
                    draw->AddRectFilled(
                        ImVec2(p0.x - pad, p0.y - pad),
                        ImVec2(p0.x + textSize.x + pad, p0.y + textSize.y + pad),
                        bgCol, 3.0f);
                }

                // Render Teks
                draw->AddText(ImGui::GetFont(), s.fontSize, p0, col, s.textContent.c_str());
                
                // Jika sedang dipilih, kasih kotak hijau visual saja
                if (s.id == selectedId) {
                    float pad = 5.0f;
                    draw->AddRect(
                        ImVec2(p0.x - pad, p0.y - pad),
                        ImVec2(p0.x + textSize.x + pad, p0.y + textSize.y + pad),
                        IM_COL32(0,255,0,255), 2.0f);
                }
            }
            // =============================================
            // C. LOGIKA STANDARD (Line/Rect/Fib)
            // =============================================
            else {
                // Konversi Koordinat Standard
                double x0 = ChartCanvas::GetStableX(s.time0, candles, tf);
                double x1 = ChartCanvas::GetStableX(s.time1, candles, tf);
                ImVec2 p0 = ImPlot::PlotToPixels(x0, s.price0);
                ImVec2 p1 = ImPlot::PlotToPixels(x1, s.price1);

                if (s.type == "RECT") {
                    ImVec2 minP(std::min(p0.x, p1.x), std::min(p0.y, p1.y));
                    ImVec2 maxP(std::max(p0.x, p1.x), std::max(p0.y, p1.y));

                    // Extend kiri/kanan
                    float screenMinX = plotPos.x;
                    float screenMaxX = plotPos.x + plotSz.x;
                    if (s.extendLeft)  minP.x = screenMinX;
                    if (s.extendRight) maxP.x = screenMaxX;
                    
                    // Fill dengan opacity — pakai fillColor terpisah dari border
                    if (s.filled) {
                        ImVec4 fillCol = s.fillColor;
                        fillCol.w = s.fillOpacity;
                        ImU32 fillColU32 = ImGui::ColorConvertFloat4ToU32(fillCol);
                        draw->AddRectFilled(minP, maxP, fillColU32, 0.0f);
                    }

                    // Border — hanya jika rectBorderVisible ON
                    if (s.rectBorderVisible) {
                        if (s.lineStyle == 0) {
                            draw->AddRect(minP, maxP, col, 0.0f, 0, s.thickness);
                        } else {
                            // Dashed/Dotted border: 4 sisi
                            float dashSz = (s.lineStyle == 1) ? 10.0f : 2.0f;
                            float gapSz  = (s.lineStyle == 1) ? 8.0f : 4.0f;
                            ImVec2 sides[4][2] = {{minP, ImVec2(maxP.x,minP.y)}, {ImVec2(maxP.x,minP.y), maxP}, {maxP, ImVec2(minP.x,maxP.y)}, {ImVec2(minP.x,maxP.y), minP}};
                            for (int si = 0; si < 4; si++) {
                                float dist = std::sqrt((sides[si][1].x-sides[si][0].x)*(sides[si][1].x-sides[si][0].x)+(sides[si][1].y-sides[si][0].y)*(sides[si][1].y-sides[si][0].y));
                                if (dist > 0) {
                                    ImVec2 dir((sides[si][1].x-sides[si][0].x)/dist, (sides[si][1].y-sides[si][0].y)/dist);
                                    float curD = 0.0f;
                                    while (curD < dist) {
                                        float nextD = std::min(curD + dashSz, dist);
                                        draw->AddLine(ImVec2(sides[si][0].x+dir.x*curD, sides[si][0].y+dir.y*curD), ImVec2(sides[si][0].x+dir.x*nextD, sides[si][0].y+dir.y*nextD), col, s.thickness);
                                        curD += (dashSz + gapSz);
                                    }
                                }
                            }
                        }
                    }

                    // Center Line (garis tengah horizontal)
                    if (s.centerLine) {
                        float centerY = (minP.y + maxP.y) * 0.5f;
                        ImU32 clCol = ImGui::ColorConvertFloat4ToU32(s.centerLineColor);
                        draw->AddLine(ImVec2(minP.x, centerY), ImVec2(maxP.x, centerY), clCol, 1.0f);
                    }

                    // Label: Custom Text atau Dimensions (W x H)
                    if (!s.rectLabel.empty()) {
                        // Custom label — pakai warna textColor + rectFontSize + bold/italic + alignment
                        ImU32 txtColU32 = ImGui::ColorConvertFloat4ToU32(s.textColor);
                        float fsize = (s.rectFontSize > 0) ? s.rectFontSize : 8.0f;

                        // Pilih font: pakai g_fontBold/g_fontItalic kalau tersedia,
                        // kalau tidak ada (nullptr) fallback ke simulasi shadow/skew
                        extern ImFont* g_fontBold;
                        extern ImFont* g_fontItalic;

                        ImFont* drawFont = ImGui::GetFont(); // default: regular
                        bool useRealBold   = (s.rectBold   && g_fontBold);
                        bool useRealItalic = (s.rectItalic && g_fontItalic);

                        if (useRealBold && useRealItalic) {
                            // BoldItalic: pakai bold font, simulasikan italic via skew
                            drawFont = g_fontBold;
                        } else if (useRealBold) {
                            drawFont = g_fontBold;
                        } else if (useRealItalic) {
                            drawFont = g_fontItalic;
                        }

                        // Kalau font bold/italic TIDAK tersedia (nullptr), gunakan simulasi fallback
                        bool simBold   = s.rectBold   && !g_fontBold;
                        bool simItalic = s.rectItalic && !g_fontItalic;

                        ImVec2 txtSz = drawFont->CalcTextSizeA(fsize, FLT_MAX, 0.0f, s.rectLabel.c_str());

                        // Horizontal alignment
                        float txtX = 0;
                        if (s.rectTextAlign == 0)      txtX = minP.x + 5.0f;                          // Kiri
                        else if (s.rectTextAlign == 1) txtX = (minP.x + maxP.x) * 0.5f - txtSz.x * 0.5f; // Tengah
                        else                            txtX = maxP.x - txtSz.x - 5.0f;                  // Kanan

                        // Vertical alignment
                        float txtY = 0;
                        if (s.rectVertAlign == 0)      txtY = minP.y + 5.0f;                           // Atas
                        else if (s.rectVertAlign == 1) txtY = (minP.y + maxP.y) * 0.5f - txtSz.y * 0.5f; // Tengah
                        else                            txtY = maxP.y - txtSz.y - 5.0f;                   // Bawah

                        // --- RENDER PATH ---

                        if (!simBold && !simItalic) {
                            // ✅ FONT ASLI tersedia — render langsung (kualitas terbaik)
                            // BoldItalic kombinasi (bold font + italic skew):
                            if (useRealBold && useRealItalic) {
                                float skewPx = fsize * 0.12f;
                                float cx = txtX;
                                const char* ch = s.rectLabel.c_str();
                                while (*ch) {
                                    int bytes = Utf8CharLen(ch);
                                    if (bytes <= 0) { ch++; continue; }
                                    ImVec2 charSz = drawFont->CalcTextSizeA(fsize, FLT_MAX, 0.0f, ch, ch + bytes);
                                    draw->AddText(drawFont, fsize, ImVec2(cx + skewPx, txtY), txtColU32, ch, ch + bytes);
                                    cx += charSz.x;
                                    ch += bytes;
                                }
                            } else {
                                // Bold saja, Italic saja, atau Normal
                                draw->AddText(drawFont, fsize, ImVec2(txtX, txtY), txtColU32, s.rectLabel.c_str());
                            }
                        } else {
                            // ⚠️ FALLBACK: font bold/italic tidak tersedia, gunakan simulasi
                            // BOLD: multi-pass shadow (6 render di offset kecil)
                            if (simBold) {
                                draw->AddText(drawFont, fsize, ImVec2(txtX - 0.7f, txtY),          txtColU32, s.rectLabel.c_str());
                                draw->AddText(drawFont, fsize, ImVec2(txtX + 0.7f, txtY),          txtColU32, s.rectLabel.c_str());
                                draw->AddText(drawFont, fsize, ImVec2(txtX,          txtY - 0.7f),  txtColU32, s.rectLabel.c_str());
                                draw->AddText(drawFont, fsize, ImVec2(txtX,          txtY + 0.7f),  txtColU32, s.rectLabel.c_str());
                                draw->AddText(drawFont, fsize, ImVec2(txtX - 0.5f, txtY - 0.5f),  txtColU32, s.rectLabel.c_str());
                                draw->AddText(drawFont, fsize, ImVec2(txtX + 0.5f, txtY + 0.5f),  txtColU32, s.rectLabel.c_str());
                            }

                            // ITALIC: karakter-per-karakter dengan skew
                            if (simItalic) {
                                float skewPx = fsize * 0.12f;
                                float cx = txtX;
                                const char* ch = s.rectLabel.c_str();
                                while (*ch) {
                                    int bytes = Utf8CharLen(ch);
                                    if (bytes <= 0) { ch++; continue; }
                                    ImVec2 charSz = drawFont->CalcTextSizeA(fsize, FLT_MAX, 0.0f, ch, ch + bytes);
                                    if (simBold) {
                                        draw->AddText(drawFont, fsize, ImVec2(cx + skewPx - 0.5f, txtY),          txtColU32, ch, ch + bytes);
                                        draw->AddText(drawFont, fsize, ImVec2(cx + skewPx + 0.5f, txtY),          txtColU32, ch, ch + bytes);
                                        draw->AddText(drawFont, fsize, ImVec2(cx + skewPx,          txtY - 0.5f),  txtColU32, ch, ch + bytes);
                                        draw->AddText(drawFont, fsize, ImVec2(cx + skewPx,          txtY + 0.5f),  txtColU32, ch, ch + bytes);
                                    }
                                    draw->AddText(drawFont, fsize, ImVec2(cx + skewPx, txtY), txtColU32, ch, ch + bytes);
                                    cx += charSz.x;
                                    ch += bytes;
                                }
                            } else {
                                // Normal render (bold shadow sudah di atas)
                                draw->AddText(drawFont, fsize, ImVec2(txtX, txtY), txtColU32, s.rectLabel.c_str());
                            }
                        }
                    }
                    else if (s.showDimensions) {
                        // Dimensions W x H — fallback warna putih, di tengah
                        double priceW = std::abs(s.price1 - s.price0);
                        double timeW = std::abs(s.time1 - s.time0);
                        char dimBuf[64];
                        double timeDiffSec = timeW;
                        int hours = (int)(timeDiffSec / 3600);
                        int mins  = (int)(fmod(timeDiffSec, 3600.0) / 60.0);
                        if (hours > 0)
                            sprintf(dimBuf, "%.2f | %dh %dm", priceW, hours, mins);
                        else
                            sprintf(dimBuf, "%.2f | %dm", priceW, mins);
                        ImVec2 center = ImVec2((minP.x + maxP.x) * 0.5f, (minP.y + maxP.y) * 0.5f);
                        ImVec2 txtSz = ImGui::CalcTextSize(dimBuf);
                        draw->AddText(ImVec2(center.x - txtSz.x * 0.5f, center.y - txtSz.y * 0.5f), IM_COL32(255,255,255,180), dimBuf);
                    }
                } 
                    else if (s.type == "FIB") {
                        // -----------------------------------------------------------
                        // 1. PERSIAPAN DATA & KOORDINAT
                        // -----------------------------------------------------------
                        double x0_val = ChartCanvas::GetStableX(s.time0, candles, tf);
                        double x1_val = ChartCanvas::GetStableX(s.time1, candles, tf);
                        ImVec2 p0 = ImPlot::PlotToPixels(x0_val, s.price0);
                        ImVec2 p1 = ImPlot::PlotToPixels(x1_val, s.price1);
                        //Logika baru  TRandline line diagonal 
                        if(s.fibConfig.showTrendline){
                            ImU32 colTrend = ImGui :: ColorConvertFloat4ToU32(s.fibConfig.trendlineColor);
                            //style Solid = 0 
                            if(s.fibConfig.trendlineStyle == 0){
                                draw->AddLine(p0, p1, colTrend, s.thickness);
                            }
                            else{
                            float totalDist = std :: sqrt ((p1.x - p0.x)*(p1.x - p0.x)+(p1.y - p0.y));
                            float dashSize = (s.fibConfig.trendlineStyle == 1 )? 10.0f : 2.0f; 
                            float gapSize =  (s.fibConfig.trendlineStyle == 1 )? 8.0f : 4.0f; 

                            if (totalDist > 0 ){
                                ImVec2 dir = ImVec2((p1.x - p0.x)/totalDist, (p1.y - p0.y)/totalDist);
                                float currentDist = 0.0f;

                                while (currentDist < totalDist) {
                                float nextDist = std::min(currentDist + dashSize, totalDist);
                                ImVec2 start = ImVec2(p0.x + dir.x * currentDist, p0.y + dir.y * currentDist);
                                ImVec2 end   = ImVec2(p0.x + dir.x * nextDist,    p0.y + dir.y * nextDist);
                                draw->AddLine(start, end, colTrend, s.thickness);
                                currentDist += (dashSize + gapSize);
                            }
                         }
                    }
                }
                
            float y_diff = (float)(p1.y - p0.y);
            
            // Batas Layar untuk Fitur Extend
            ImVec2 plotPos = ImPlot::GetPlotPos();
            ImVec2 plotSize = ImPlot::GetPlotSize();
            float screenMinX = plotPos.x;
            float screenMaxX = plotPos.x + plotSize.x;

            float startX = p0.x;
            float endX = p1.x;

            if (s.fibConfig.extendLeft) startX = screenMinX;
            if (s.fibConfig.extendRight) endX = screenMaxX;

            // Copy level ke vector sementara untuk di-SORT
            std::vector<FibLevel> sortedLevels = s.fibConfig.levels;

            // LOGIKA REVERSE UNTUK BACKGROUND:
            if (s.fibConfig.reversed) {
                for(auto& l : sortedLevels) l.coeff = 1.0 - l.coeff;
            }

            // Sort dari kecil ke besar
            std::sort(sortedLevels.begin(), sortedLevels.end(), 
                [](const FibLevel& a, const FibLevel& b) { return a.coeff < b.coeff; });

            // -----------------------------------------------------------
            // 2. LAYER 1: RAINBOW BACKGROUND
            // -----------------------------------------------------------
            if (s.fibConfig.showBackground && sortedLevels.size() > 1) {
                for (size_t i = 0; i < sortedLevels.size() - 1; ++i) {
                    const auto& lvlA = sortedLevels[i];
                    const auto& lvlB = sortedLevels[i+1];

                    if (!lvlA.visible || !lvlB.visible) continue;

                    float yA = p0.y + y_diff * (float)lvlA.coeff;
                    float yB = p0.y + y_diff * (float)lvlB.coeff;

                    ImVec4 fillColVec = lvlA.color;
                    fillColVec.w = 0.12f; // Transparansi 12%
                    ImU32 fillCol = ImGui::ColorConvertFloat4ToU32(fillColVec);

                    draw->AddRectFilled(ImVec2(startX, yA), ImVec2(endX, yB), fillCol);
                }
            }

           // -----------------------------------------------------------
            // 3. LAYER 2: GARIS LEVEL (HORIZONTAL) & TEKS
            // -----------------------------------------------------------
            for (const auto& lvl : s.fibConfig.levels) { 
                if (!lvl.visible) continue;

                // Logika Reverse
                double effectiveCoeff = s.fibConfig.reversed ? (1.0 - lvl.coeff) : lvl.coeff;
                float y = p0.y + y_diff * (float)effectiveCoeff;
                
                ImVec2 a(startX, y);
                ImVec2 b(endX, y);

                ImVec4 lineColVec = lvl.color;
                lineColVec.w *= 0.8f; 
                ImU32 lineCol = ImGui::ColorConvertFloat4ToU32(lineColVec);

                // 🔥 LOGIKA BARU: HORIZONTAL STYLE (SOLID / DASHED)
                if (s.fibConfig.horizStyle == 0) {
                    // STYLE 0: SOLID (Biasa)
                    draw->AddLine(a, b, lineCol, 1.0f);
                } 
                else {
                    // STYLE 1 & 2: DASHED / DOTTED
                    float width = b.x - a.x; // Panjang garis horizontal
                    if (width > 0) {
                        float dashSize = (s.fibConfig.horizStyle == 1) ? 10.0f : 2.0f; 
                        float gapSize  = (s.fibConfig.horizStyle == 1) ? 8.0f : 4.0f;
                        
                        float curX = a.x;
                        while (curX < b.x) {
                            float nextX = std::min(curX + dashSize, b.x);
                            draw->AddLine(ImVec2(curX, y), ImVec2(nextX, y), lineCol, 1.0f);
                            curX += (dashSize + gapSize);
                        }
                    }
                }

                // GANTI BAGIAN RENDER TEKS DENGAN INI:
                if (s.fibConfig.showLabels) {
                    double priceAtLevel = s.price0 + (s.price1 - s.price0) * effectiveCoeff;
                    char buf[64];
                    
                    // 🔥 LOGIKA BARU: DESKRIPSI
                    // Cek apakah user menulis deskripsi di level ini?
                    if (strlen(lvl.label) > 0) {
                        // Jika ada, formatnya: "Golden (1920.55)"
                        snprintf(buf, sizeof(buf), "%s (%.2f)", lvl.label, priceAtLevel);
                    } else {
                        // Jika kosong, pakai format angka biasa: "0.618 (1920.55)"
                        snprintf(buf, sizeof(buf), "%.3f (%.2f)", lvl.coeff, priceAtLevel);
                    }
                    
                    ImVec2 textSize = ImGui::CalcTextSize(buf);
                    float textX = s.fibConfig.labelRight ? (endX + 5) : (startX - textSize.x - 5);
                    draw->AddText(ImVec2(textX, y - textSize.y * 0.5f), lineCol, buf);
                }
            }
                    }
                else { // LINE (Default) + lineStyle, extend, endpoints support
                    // Hitung batas layar untuk extend
                    float screenMinX = plotPos.x;
                    float screenMaxX = plotPos.x + plotSz.x;

                    ImVec2 lineP0 = p0;
                    ImVec2 lineP1 = p1;

                    // Extend ke kiri (hitung intersection dengan screenMinX)
                    if (s.extendLeft) lineP0.x = screenMinX;
                    // Extend ke kanan
                    if (s.extendRight) lineP1.x = screenMaxX;

                    // Gambar garis dengan lineStyle
                    if (s.lineStyle == 0) {
                        draw->AddLine(lineP0, lineP1, col, s.thickness);
                    } else {
                        // Dashed/Dotted
                        float dist = std::sqrt((lineP1.x-lineP0.x)*(lineP1.x-lineP0.x)+(lineP1.y-lineP0.y)*(lineP1.y-lineP0.y));
                        if (dist > 0) {
                            float dashSz = (s.lineStyle == 1) ? 10.0f : 2.0f;
                            float gapSz  = (s.lineStyle == 1) ? 8.0f : 4.0f;
                            ImVec2 dir((lineP1.x-lineP0.x)/dist, (lineP1.y-lineP0.y)/dist);
                            float curD = 0.0f;
                            while (curD < dist) {
                                float nextD = std::min(curD + dashSz, dist);
                                draw->AddLine(ImVec2(lineP0.x+dir.x*curD, lineP0.y+dir.y*curD), ImVec2(lineP0.x+dir.x*nextD, lineP0.y+dir.y*nextD), col, s.thickness);
                                curD += (dashSz + gapSz);
                            }
                        }
                    }

                    // Show Endpoints (titik bulat di P0 & P1)
                    if (s.showEndpoints) {
                        draw->AddCircleFilled(p0, 5.0f, col);
                        draw->AddCircleFilled(p1, 5.0f, col);
                        // Outline hitam biar kontras
                        draw->AddCircle(p0, 5.0f, IM_COL32(0,0,0,200));
                        draw->AddCircle(p1, 5.0f, IM_COL32(0,0,0,200));
                    }
                }
            }
        } // end for shapes
        draw->PopClipRect(); // restore clip setelah render shapes
    }
   
    // ==========================================
    // 🔥 UPDATE SERIALIZATION (TO JSON) - FINAL
    // ==========================================
    nlohmann::json ToJSON() const {
        std::lock_guard<std::mutex> lock(mtx);
        nlohmann::json j;

        for (const auto& s : shapes) {
            // 1. Property Dasar
            nlohmann::json item = {
                {"id", s.id}, {"type", s.type},
                {"time0", s.time0}, {"price0", s.price0},
                {"time1", s.time1}, {"price1", s.price1},
                {"color", {s.color.x, s.color.y, s.color.z, s.color.w}},
                {"thickness", s.thickness}, {"filled", s.filled}, {"locked", s.locked}
            };

            // 2. Shared Properties (baru)
            item["lineStyle"] = s.lineStyle;

            // 3. LINE Properties
            if (s.type == "LINE") {
                item["extendLeft"] = s.extendLeft;
                item["extendRight"] = s.extendRight;
                item["showEndpoints"] = s.showEndpoints;
            }

            // 4. RECT Properties
            if (s.type == "RECT") {
                item["fillOpacity"] = s.fillOpacity;
                item["showDimensions"] = s.showDimensions;
                item["textColor"] = {s.textColor.x, s.textColor.y, s.textColor.z, s.textColor.w};
                item["rectLabel"] = s.rectLabel;
                item["fillColor"] = {s.fillColor.x, s.fillColor.y, s.fillColor.z, s.fillColor.w};
                // RECT Style (Corak)
                item["rectBorderVisible"] = s.rectBorderVisible;
                item["centerLine"] = s.centerLine;
                item["centerLineColor"] = {s.centerLineColor.x, s.centerLineColor.y, s.centerLineColor.z, s.centerLineColor.w};
                item["rectExtend"] = s.rectExtend;
                // RECT Text (Teks)
                item["rectFontSize"] = s.rectFontSize;
                item["rectBold"] = s.rectBold;
                item["rectItalic"] = s.rectItalic;
                item["rectTextAlign"] = s.rectTextAlign;
                item["rectVertAlign"] = s.rectVertAlign;
            }

            // 5. Data Elliot & Brush (Multi-Point)
            if (!s.multiTime.empty()) {
                item["multiTime"] = s.multiTime;
                item["multiPrice"] = s.multiPrice;
            }

            // 6. Data Text
            if (s.type == "TEXT") {
                item["textContent"] = s.textContent;
                item["fontSize"] = s.fontSize;
                item["textBg"] = s.textBg;
                item["textBgColor"] = {s.textBgColor.x, s.textBgColor.y, s.textBgColor.z, s.textBgColor.w};
            }

            // 7. BRUSH Properties
            if (s.type == "BRUSH") {
                item["brushOpacity"] = s.brushOpacity;
            }

            // 8. ELLIOT Properties
            if (s.type == "ELLIOT") {
                item["elliLabelFormat"] = s.elliLabelFormat;
                item["elliShowPrice"] = s.elliShowPrice;
            }

            // 9. FIBONACCI CONFIG (FULL SAVE)
            if (s.type == "FIB") {
                nlohmann::json fibData;
                
                // General
                fibData["extendLeft"] = s.fibConfig.extendLeft;
                fibData["extendRight"] = s.fibConfig.extendRight;
                fibData["showLabels"] = s.fibConfig.showLabels;
                fibData["showBackground"] = s.fibConfig.showBackground;
                fibData["labelRight"] = s.fibConfig.labelRight;
                fibData["reversed"] = s.fibConfig.reversed;

                // 🔥 Trendline & Style Settings (BARU)
                fibData["showTrendline"] = s.fibConfig.showTrendline;
                fibData["trendlineStyle"] = s.fibConfig.trendlineStyle;
                fibData["horizStyle"] = s.fibConfig.horizStyle; // Simpan Style Horizontal
                
                // Simpan Warna Trendline
                fibData["trendlineColor"] = { 
                    s.fibConfig.trendlineColor.x, s.fibConfig.trendlineColor.y, 
                    s.fibConfig.trendlineColor.z, s.fibConfig.trendlineColor.w 
                };

                // Array Levels
                nlohmann::json levelsArr = nlohmann::json::array();
                for (const auto& lvl : s.fibConfig.levels) {
                    levelsArr.push_back({
                        {"coeff", lvl.coeff},
                        {"visible", lvl.visible},
                        {"color", {lvl.color.x, lvl.color.y, lvl.color.z, lvl.color.w}}
                    });
                }
                fibData["levels"] = levelsArr;
                
                item["fibConfig"] = fibData;
            }

            j.push_back(item);
        }
        return j;
    }

    // ==========================================
    // 🔥 UPDATE DESERIALIZATION (FROM JSON) - FINAL
    // ==========================================
    void FromJSON(const nlohmann::json& j) {
        std::lock_guard<std::mutex> lock(mtx);
        shapes.clear();
        if (!j.is_array()) return;

        for (auto& item : j) {
            GlobalShape s;
            s.id = item.value("id", GenerateUUID());
            s.type = item.value("type", "LINE");
            
            // Load Koordinat
            s.time0 = item.value("time0", 0.0);
            s.price0 = item.value("price0", 0.0);
            s.time1 = item.value("time1", 0.0);
            s.price1 = item.value("price1", 0.0);
            
            // Load Warna Utama
            auto c = item.value("color", std::vector<float>{1,1,1,1});
            if (c.size() == 4) s.color = ImVec4(c[0], c[1], c[2], c[3]);
            
            s.thickness = item.value("thickness", 1.2f);
            s.filled = item.value("filled", false);
            s.locked = item.value("locked", false);

            // Load Shared Properties (baru)
            s.lineStyle = item.value("lineStyle", 0);

            // Load LINE Properties
            if (s.type == "LINE") {
                s.extendLeft = item.value("extendLeft", false);
                s.extendRight = item.value("extendRight", false);
                s.showEndpoints = item.value("showEndpoints", false);
            }

            // Load RECT Properties
            if (s.type == "RECT") {
                s.fillOpacity = item.value("fillOpacity", 0.15f);
                s.showDimensions = item.value("showDimensions", false);
                s.rectLabel = item.value("rectLabel", "");
                if (item.contains("textColor")) {
                    auto tc = item["textColor"];
                    if (tc.is_array() && tc.size() == 4)
                        s.textColor = ImVec4(tc[0], tc[1], tc[2], tc[3]);
                }
                if (item.contains("fillColor")) {
                    auto fc = item["fillColor"];
                    if (fc.is_array() && fc.size() == 4)
                        s.fillColor = ImVec4(fc[0], fc[1], fc[2], fc[3]);
                }
                // RECT Style (Corak)
                s.rectBorderVisible = item.value("rectBorderVisible", true);
                s.centerLine = item.value("centerLine", false);
                if (item.contains("centerLineColor")) {
                    auto clc = item["centerLineColor"];
                    if (clc.is_array() && clc.size() == 4)
                        s.centerLineColor = ImVec4(clc[0], clc[1], clc[2], clc[3]);
                }
                s.rectExtend = item.value("rectExtend", 0);
                // RECT Text (Teks)
                s.rectFontSize = item.value("rectFontSize", 8.0f);
                s.rectBold = item.value("rectBold", false);
                s.rectItalic = item.value("rectItalic", false);
                s.rectTextAlign = item.value("rectTextAlign", 0);
                s.rectVertAlign = item.value("rectVertAlign", 2);
            }

            // Load Text
            s.textContent = item.value("textContent", "Text");
            s.fontSize = item.value("fontSize", 20.0f);
            s.textBg = item.value("textBg", false);
            if (item.contains("textBgColor")) {
                auto tbc = item["textBgColor"];
                if (tbc.is_array() && tbc.size() == 4)
                    s.textBgColor = ImVec4(tbc[0], tbc[1], tbc[2], tbc[3]);
            }

            // Load Elliot & Brush (Multi-Point)
            if (item.contains("multiTime")) s.multiTime = item["multiTime"].get<std::vector<double>>();
            if (item.contains("multiPrice")) s.multiPrice = item["multiPrice"].get<std::vector<double>>();

            // Load BRUSH Properties
            if (s.type == "BRUSH") {
                s.brushOpacity = item.value("brushOpacity", 1.0f);
            }

            // Load ELLIOT Properties
            if (s.type == "ELLIOT") {
                s.elliLabelFormat = item.value("elliLabelFormat", 0);
                s.elliShowPrice = item.value("elliShowPrice", false);
            }

            // 🔥 LOAD FIBONACCI CONFIG (FULL LOAD)
            if (item.contains("fibConfig")) {
                auto& fc = item["fibConfig"];
                
                // General
                s.fibConfig.extendLeft = fc.value("extendLeft", false);
                s.fibConfig.extendRight = fc.value("extendRight", false);
                s.fibConfig.showLabels = fc.value("showLabels", true);
                s.fibConfig.showBackground = fc.value("showBackground", true);
                s.fibConfig.labelRight = fc.value("labelRight", true);
                s.fibConfig.reversed = fc.value("reversed", false);

                // 🔥 Trendline & Style Settings (BARU)
                s.fibConfig.showTrendline = fc.value("showTrendline", true);
                s.fibConfig.trendlineStyle = fc.value("trendlineStyle", 1); // Default 1 (Dashed)
                s.fibConfig.horizStyle = fc.value("horizStyle", 0);         // Default 0 (Solid)

                // Load Warna Trendline
                if (fc.contains("trendlineColor")) {
                    auto tc = fc["trendlineColor"];
                    if (tc.is_array() && tc.size() == 4) 
                        s.fibConfig.trendlineColor = ImVec4(tc[0], tc[1], tc[2], tc[3]);
                }

                // Load Levels Array
                if (fc.contains("levels") && fc["levels"].is_array()) {
                    s.fibConfig.levels.clear(); // Hapus default
                    for (auto& l : fc["levels"]) {
                        FibLevel newLvl;
                        newLvl.coeff = l.value("coeff", 0.0);
                        newLvl.visible = l.value("visible", true);
                        
                        // Load Warna Level
                        if (l.contains("color")) {
                            auto lc = l["color"];
                            if (lc.is_array() && lc.size() == 4) 
                                newLvl.color = ImVec4(lc[0], lc[1], lc[2], lc[3]);
                            else 
                                newLvl.color = s.color;
                        }
                        s.fibConfig.levels.push_back(newLvl);
                    }
                }
            }
            shapes.push_back(s);
        }
    }
}; 