#pragma once
#include "imgui.h"
#include "implot.h"
#include "GlobalShapeManager.h"
#include "ChartCanvas.h" // Butuh ini untuk GetTimePerCandle
#include "Candle.h"      // Butuh ini untuk struct Candle

extern GlobalShapeManager g_shapes;
extern std::string g_activeTF; // Butuh ini untuk tahu timeframe aktif

class CTextDrawing {
public:
    bool isActive = false;
    std::string editingId = ""; 
    char textBuffer[256] = "";

    void Start() {
        isActive = true;
        editingId = "";
        memset(textBuffer, 0, sizeof(textBuffer));
    }

    void Stop() {
        isActive = false;
        editingId = "";
    }

    // 👇 TAMBAHAN PARAMETER: 'const std::vector<Candle>& candles'
    void Update(const ImPlotPoint& mousePos, 
                ImDrawList* draw, 
                std::string& selectedId, 
                int& activeTool, 
                bool& isDrawing, 
                bool& blockChart, 
                const ImVec4& currentColor,
                const std::vector<Candle>& candles) // <--- PENTING!
    {
        if (!isActive || candles.empty()) return; // Safety check

        // -----------------------------------------------------------
        // 🧮 RUMUS SAKTI: KONVERSI MOUSE INDEX -> REAL TIME
        // -----------------------------------------------------------
        double mouseIndex = mousePos.x;
        double mousePrice = mousePos.y;

        double lastTime = candles.back().time;
        double lastIndex = (double)candles.size() - 1.0;
        double timePerCandle = ChartCanvas::GetTimePerCandle(g_activeTF);

        // Rumus kebalikan dari Render:
        // Time = LastTime + (MouseIndex - LastIndex) * TimePerCandle
        double realTime = lastTime + (mouseIndex - lastIndex) * timePerCandle;
        // -----------------------------------------------------------

        // FASE 1: BUAT BARU
        if (editingId.empty()) {
            if (ImPlot::IsPlotHovered()) {
                ImGui::SetMouseCursor(ImGuiMouseCursor_TextInput);
                ImVec2 pixelPos = ImPlot::PlotToPixels(mousePos);
                draw->AddText(ImGui::GetFont(), 18.0f, pixelPos, IM_COL32(255, 255, 255, 150), "Click to Anchor");

                if (ImGui::IsMouseClicked(0)) {
                    GlobalShape s;
                    s.id = g_shapes.GenerateUUID();
                    s.type = "TEXT";
                    
                    // 🔥 SIMPAN WAKTU ASLI, BUKAN INDEX MOUSE!
                    s.time0 = realTime;  
                    s.price0 = mousePrice;
                    
                    s.textContent = ""; 
                    s.fontSize = 20.0f;
                    s.color = currentColor;
                    s.visible = true;

                    g_shapes.AddShape(s);
                    editingId = s.id;
                    memset(textBuffer, 0, sizeof(textBuffer));
                    blockChart = true;
                }
            }
        }
        
        // FASE 2: EDITING (Posisi ikut chart karena pakai Waktu Asli)
        else {
            GlobalShape* s = nullptr;
            for (auto& shape : g_shapes.shapes) {
                if (shape.id == editingId) { s = &shape; break; }
            }

            if (!s) { Stop(); return; }

            // 🧮 KONVERSI BALIK (WAKTU -> LAYAR) UNTUK INPUT BOX
            // Agar input box nempel terus di candle walau digeser
            double xPosIndex = lastIndex + (s->time0 - lastTime) / timePerCandle;
            ImVec2 anchorPx = ImPlot::PlotToPixels(xPosIndex, s->price0);
            
            draw->AddCircleFilled(anchorPx, 4.0f, IM_COL32(0, 255, 0, 255)); 

            ImGui::SetNextWindowPos(ImVec2(anchorPx.x + 10, anchorPx.y - 15));
            // ... (Kode UI Input Box sama seperti sebelumnya) ...
            
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6, 4));
            ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.15f, 0.15f, 0.25f, 1.0f)); 
            ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove;

            if (ImGui::Begin("##TvInputLive", nullptr, flags)) {
                if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere(0);
                ImGui::SetNextItemWidth(200);
                
                if (ImGui::InputText("##in", textBuffer, sizeof(textBuffer))) {
                     s->textContent = std::string(textBuffer);
                }

                bool enterPressed = ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter);
                bool clickedOutside = ImGui::IsMouseClicked(0) && !ImGui::IsWindowHovered();

                if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                    g_shapes.RemoveShape(editingId);
                    Stop(); activeTool = 0; isDrawing = false; blockChart = false;
                }
                else if (enterPressed || clickedOutside) {
                    if (s->textContent.empty()) g_shapes.RemoveShape(editingId);
                    else selectedId = editingId;
                    Stop(); activeTool = 0; isDrawing = false; blockChart = false;
                }
            }
            ImGui::End();
            ImGui::PopStyleColor();
            ImGui::PopStyleVar();
        }
    }
};