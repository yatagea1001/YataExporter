// ==========================================
// FORCE MODULES FIX – biar semua modul bisa compile
// ==========================================

// >>> WAJIB: load ImGui & ImPlot dulu <<<
// ImGui core
#include "imgui.h"
#include "imgui_internal.h"

// ImGui backends (GLFW + OpenGL3)
#include "imgui/backends/imgui_impl_glfw.h"
#include "imgui/backends/imgui_impl_opengl3.h"

// ImPlot
#include "implot.h"
#include "implot_internal.h"

// >>> Baru load semua modul .h kamu <<<
#include "Candle.h"
#include "ChartCanvas.h"
#include "ChartAxisTicks.h"
#include "CDrawingManager.h"
#include "CViewWindowManager.h"
#include "TradeModule.h"
#include "Indicators.h"
#include "GPUCandleRenderer.h"
#include "GlobalShapeManager.h"
#include "MTFDrawingEngine.h"
#include "ChartModule.h"
#include "CReplayManager.h"

// Tidak ada kode lain
