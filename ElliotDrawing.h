#pragma once
#include "implot.h"
#include "imgui.h"
#include <vector>
#include <string>
#include "Candle.h"
#include "ChartCanvas.h"
#include "GlobalShapeManager.h"

extern GlobalShapeManager g_shapes;

struct ElliotPoint {
    double time;
    double price;
};

class ElliotDrawing {
public:
    bool isActive = false;
    std::vector<ElliotPoint> points; 
    ImVec4 color = ImVec4(1, 1, 0, 1); 

    // 🔥 VAR BARU: Untuk Mencegah Double Click
    double lastClickTime = 0.0; 

    void Start() {
        isActive = true;
        points.clear();
        lastClickTime = ImGui::GetTime(); // Reset waktu saat mulai
    }

    void Cancel() {
        isActive = false;
        points.clear();
    }

    void Update(const ImPlotPoint& mp, ImDrawList* drawList, const std::vector<Candle>& candles) {
        if (!isActive) return;

        // 1. BATALKAN (Klik Kanan)
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
            Cancel();
            return;
        }

        // 2. INPUT LOGIC (DEGAN SENSOR PENUNDAAN)
        double now = ImGui::GetTime(); // Waktu sekarang
        
        // Cek: Apakah mouse diklik DAN sudah lewat 0.3 detik dari klik terakhir?
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && ImPlot::IsPlotHovered()) {
            
            // 🔥 LOGIKA JEDA: Kalau terlalu cepat (< 0.25 detik), ABAIKAN!
            if ((now - lastClickTime) < 0.25) {
                return; 
            }

            // Jika lolos sensor waktu, baru kita simpan titiknya
            double t = ScreenToTime(mp.x, candles);
            points.push_back({ t, mp.y });
            
            // Catat waktu klik ini
            lastClickTime = now;

            // Cek apakah SUDAH PAS 6 TITIK (0, 1, 2, 3, 4, 5)
            // Jangan simpan kalau belum pas 6!
            if (points.size() >= 6) {
                SaveToGlobal(); 
                Cancel();       
                return;
            }
        }

        // 3. RENDER PREVIEW
        RenderPreview(mp, drawList, candles);
    }

private:
    // Di dalam class ElliotDrawing...

    // 🔥 KEMBALIKAN KE LOGIKA SIMPAN PAKET (Supaya bisa diedit menyambung)
    void SaveToGlobal() {
        std::vector<double> tVec, pVec;
        
        // Kumpulkan semua titik tap-tap tadi
        for(auto& p : points) { 
            tVec.push_back(p.time); 
            pVec.push_back(p.price); 
        }
        
        // Kirim ke gudang sebagai "ELLIOT" (Satu Paket)
        // Pastikan GlobalShapeManager kamu punya fungsi AddElliotShape
        g_shapes.AddElliotShape(tVec, pVec, color);
    }
    void RenderPreview(const ImPlotPoint& mp, ImDrawList* draw, const std::vector<Candle>& candles) {
        // Jangan gambar apa-apa kalau belum ada titik
        // Tapi kita tetap butuh visual kursor mouse
        
        ImU32 col = ImGui::ColorConvertFloat4ToU32(color);
        ImVec2 lastPos;
        bool hasLastPos = false;

        // A. GAMBAR GARIS PERMANEN (Titik yang sudah di-Tap)
        for (size_t i = 0; i < points.size(); ++i) {
            ImVec2 px = PlotToPixelsMTF(points[i], candles);
            
            // Sambungkan titik sebelumnya ke titik ini
            if (i > 0) {
                draw->AddLine(lastPos, px, col, 2.0f);
            }
            
            // Label Angka (0), (1), (2)...
            char buf[4]; sprintf(buf, "(%d)", (int)i);
            draw->AddText(ImVec2(px.x+5, px.y-15), col, buf);
            draw->AddCircleFilled(px, 3.0f, col);
            
            lastPos = px;
            hasLastPos = true;
        }

        // B. GAMBAR "BENANG" KE ARAH JARI
        // Hanya muncul jika minimal sudah ada 1 titik
        if (hasLastPos) {
            // Konversi mouse position ke pixel langsung (karena mp sudah di koordinat plot yang benar)
            ImVec2 mousePx = ImPlot::PlotToPixels(mp.x, mp.y);
            
            // Garis putus-putus ke jari
            draw->AddLine(lastPos, mousePx, IM_COL32(255, 255, 255, 150), 1.0f);
            
            // Info langkah berikutnya
            char nextNum[16]; 
            sprintf(nextNum, "Titik ke-%d", (int)points.size());
            draw->AddText(ImVec2(mousePx.x + 15, mousePx.y), IM_COL32(255, 255, 255, 200), nextNum);
            
            // Lingkaran target di jari
            draw->AddCircle(mousePx, 5.0f, IM_COL32(255, 255, 255, 150));
        }
    }

    ImVec2 PlotToPixelsMTF(const ElliotPoint& pt, const std::vector<Candle>& candles) {
        double timePerCandle = ChartCanvas::GetTimePerCandle(g_activeTF);
        double lastTime = candles.back().time;
        int lastIndex = (int)candles.size() - 1;
        double x = lastIndex + (pt.time - lastTime) / timePerCandle;
        return ImPlot::PlotToPixels(x, pt.price);
    }

    double ScreenToTime(double plotX, const std::vector<Candle>& candles) {
        if (candles.empty()) return 0;
        double timePerCandle = ChartCanvas::GetTimePerCandle(g_activeTF);
        return candles.back().time + (plotX - ((int)candles.size() - 1)) * timePerCandle;
    }
};