#pragma once
#include "imgui.h"
#include "implot.h"
#include <vector>
#include <algorithm>
#include <cmath>


struct ChartView {
    float zoomLevel = 1500.0f;
    float targetZoom = 1500.0f;
    double y_min = 0.0, y_max = 0.0;
    bool autoFitY = true;
    bool draggingY = false;
    bool draggingX = false;

    void HandleZoom(const ImGuiIO& io) {
        float wheel = io.MouseWheel;
        if (fabs(wheel) > 0.0f) {
            targetZoom -= wheel * 150.0f;
            targetZoom = std::clamp(targetZoom, 100.0f, 5000.0f);
        }
        zoomLevel += (targetZoom - zoomLevel) * 0.15f;
    }

    void HandleDrag(const ImGuiIO& io, bool replayMode, int& currentIndex, int total, double& y_min, double& y_max) {
        ImPlotPoint plotPos = ImPlot::GetPlotPos();
        ImVec2 plotSize = ImPlot::GetPlotSize();
        ImVec2 mousePos = io.MousePos;

        bool inChartArea = (mousePos.x >= plotPos.x && mousePos.x <= plotPos.x + plotSize.x);
        if (inChartArea && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            if (fabs(io.MouseDelta.y) > fabs(io.MouseDelta.x)) {
                draggingY = true;
                double deltaY = io.MouseDelta.y * (y_max - y_min) * 0.002;
                y_min += deltaY;
                y_max += deltaY;
                autoFitY = false;
            } else if (!replayMode) {
                draggingX = true;
                currentIndex -= (int)io.MouseDelta.x;
                currentIndex = std::clamp(currentIndex, 0, total - 1);
            }
        } else {
            draggingX = draggingY = false;
        }
    }

    void PlotCandles(const std::vector<Candle>& candles, int start, int end) {
        std::vector<double> xs, opens, highs, lows, closes;
        for (int i = start; i <= end; i++) {
            xs.push_back(i);
            opens.push_back(candles[i].open);
            highs.push_back(candles[i].high);
            lows.push_back(candles[i].low);
            closes.push_back(candles[i].close);
        }

        for (int i = 0; i < xs.size(); i++) {
            double x = xs[i];
            double o = opens[i];
            double h = highs[i];
            double l = lows[i];
            double c = closes[i];

            ImVec4 bodyColor = (c >= o)
                ? ImVec4(0.1f, 0.85f, 0.1f, 1.0f)
                : ImVec4(0.9f, 0.15f, 0.15f, 1.0f);

            ImVec4 wickColor = ImVec4(bodyColor.x + 0.05f, bodyColor.y + 0.05f, bodyColor.z + 0.05f, 0.9f);

            double wickX[2] = {x, x};
            double wickY[2] = {l, h};
            ImPlot::SetNextLineStyle(wickColor, 1.6f);
            ImPlot::PlotLine("##wick", wickX, wickY, 2);

            double expand = 0.15;
            double left  = x - 0.3f * (0.5 + expand);
            double right = x + 0.3f * (0.5 + expand);
            double top    = (c >= o) ? c : o;
            double bottom = (c >= o) ? o : c;

            ImU32 fillColor = ImGui::ColorConvertFloat4ToU32(bodyColor);
            ImPlot::GetPlotDrawList()->AddRectFilled(
                ImPlot::PlotToPixels(ImPlotPoint(left, top)),
                ImPlot::PlotToPixels(ImPlotPoint(right, bottom)),
                fillColor);

            ImU32 borderColor = ImGui::ColorConvertFloat4ToU32(ImVec4(0, 0, 0, 0.4f));
            ImPlot::GetPlotDrawList()->AddRect(
                ImPlot::PlotToPixels(ImPlotPoint(left, top)),
                ImPlot::PlotToPixels(ImPlotPoint(right, bottom)),
                borderColor, 0, 0, 1.0f);
        }
    }
};
