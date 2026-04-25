#pragma once
#include "imgui.h"
#include "imgui_internal.h"

// Enum tetap dipertahankan agar logika pemilihan tool tidak berubah
enum ToolIconType { 
    ICON_TOOL_CURSOR, 
    ICON_TOOL_LINE, 
    ICON_TOOL_FIB, 
    ICON_TOOL_RECT, 
    ICON_TOOL_TEXT, 
    ICON_TOOL_BRUSH, 
    ICON_TOOL_ELLIOT, // Menggunakan ELLIOT sesuai pembaruan terakhir
    ICON_TOOL_TRASH 
};

// Fungsi baru yang menggunakan ImTextureID untuk ikon PNG berwarna asli
static bool ToolIconButton(const char* str_id, ImTextureID iconTexture, bool active, const ImVec2& size = ImVec2(40, 40)) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return false;

    const ImGuiID id = window->GetID(str_id);
    // Menggunakan ukuran tombol 40x40 sesuai standar sebelumnya
    const ImRect bb(window->DC.CursorPos, ImVec2(window->DC.CursorPos.x + size.x, window->DC.CursorPos.y + size.y));
    ImGui::ItemSize(bb);
    if (!ImGui::ItemAdd(bb, id)) return false;

    bool hovered, held;
    bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);

    // --- LOGIKA BACKGROUND (Dunia Asli) ---
    // Memberikan efek biru transparan saat tombol aktif atau dilewati kursor
    if (hovered || active) {
        // Jika aktif: Biru transparan agak pekat, Jika hover: Biru sangat tipis
        ImU32 colBg = active ? IM_COL32(41, 98, 255, 60) : IM_COL32(41, 98, 255, 30);
        window->DrawList->AddRectFilled(bb.Min, bb.Max, colBg, 6.0f); // Rounding diperhalus ke 6.0f
        
        // Garis pinggir menggunakan warna biru khas TradeView Anda
        ImU32 colBorder = active ? IM_COL32(41, 98, 255, 120) : IM_COL32(41, 98, 255, 50);
        window->DrawList->AddRect(bb.Min, bb.Max, colBorder, 6.0f, 0, 1.5f);
    }

    // --- RENDER IKON PNG ---
    // Memberikan padding 7 piksel agar ikon berukuran sekitar 26x26 di dalam tombol 40x40
    ImVec2 padding = ImVec2(7, 7); 
    window->DrawList->AddImage(
        iconTexture, 
        ImVec2(bb.Min.x + padding.x, bb.Min.y + padding.y), 
        ImVec2(bb.Max.x - padding.x, bb.Max.y - padding.y), 
        ImVec2(0, 0), ImVec2(1, 1), 
        IM_COL32(255, 255, 255, 255) // Putih murni menjaga warna asli PNG 3D Anda
    );

    return pressed;
}