#pragma once
// ================================================================
// 🗂️  UI_ObjectTree.h — Drawing Object Tree Panel
//     Mirip "Pohon Objek" di TradingView — list semua drawing
//     yang tampil di chart utama dan panel indicator.
//
//     Cara pakai di main.cpp:
//       1. #include "UI_ObjectTree.h"
//       2. g_objectTree.Render();   // panggil tiap frame
//       3. g_objectTree.Toggle();   // dari tombol icon
// ================================================================
#include "imgui.h"
#include "imgui_internal.h"
#include "GlobalShapeManager.h"
#include "CDrawingManager.h"
#include "TextureHelper.h"
#include <string>
#include <vector>

extern GlobalShapeManager g_shapes;
extern CDrawingManager    g_draw;

// ================================================================
// 🔧 HELPER: Tombol icon kecil (16x16, transparan background)
// ================================================================
static bool _OT_IconBtn(const char* id, ImTextureID tex, ImVec4 tint = ImVec4(1,1,1,0.75f),
                         ImVec2 size = ImVec2(18,18)) {
    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1,1,1,0.12f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(1,1,1,0.20f));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2,2));
    bool pressed = ImGui::ImageButton(id, tex, size, ImVec2(0,0), ImVec2(1,1),
                                      ImVec4(0,0,0,0), tint);
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);
    return pressed;
}

// ================================================================
// 🏷️ HELPER: Label nama shape yang bagus
// ================================================================
static const char* _OT_ShapeLabel(const std::string& type) {
    if (type == "LINE")   return "Garis Tren";
    if (type == "RECT")   return "Rectangle";
    if (type == "FIB")    return "Fib Retracement";
    if (type == "ELLIOT") return "Elliot Wave";
    if (type == "BRUSH")  return "Brush";
    if (type == "TEXT")   return "Teks";
    return "Shape";
}

// ================================================================
// 🏷️ HELPER: Icon texture per tipe shape
// ================================================================
static ImTextureID _OT_ShapeIcon(const std::string& type) {
    if (type == "LINE")   return texLine;
    if (type == "RECT")   return texRect;
    if (type == "FIB")    return texFib;
    if (type == "ELLIOT") return texElliot;
    if (type == "BRUSH")  return texBrush;
    if (type == "TEXT")   return texText;
    return texLine;
}

// ================================================================
// 🌲 MAIN CLASS
// ================================================================
class ObjectTreePanel {
public:
    bool isOpen = true; // default tampil, dockable otomatis

    // Toggle buka/tutup dari tombol icon di right-bar
    void Toggle() { isOpen = !isOpen; }
    void Open()   { isOpen = true;  }
    void Close()  { isOpen = false; }

    // ============================================================
    // 🎨 RENDER — panggil tiap frame di main.cpp
    // ============================================================
    void Render() {
        if (!isOpen) return;

        // === Posisi: di sebelah kiri right-icon-bar ===
        // Kita pakai window biasa yang bisa di-dock/dipindah
        ImGuiIO& io = ImGui::GetIO();
        float panelW = 280.0f;
        float panelH = io.DisplaySize.y * 0.6f;

        ImGui::SetNextWindowSize(ImVec2(panelW, panelH), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowPos(
            ImVec2(io.DisplaySize.x - panelW - 52, 80),
            ImGuiCond_FirstUseEver);

        ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse;

        ImGui::PushStyleColor(ImGuiCol_WindowBg,     ImVec4(0.10f, 0.10f, 0.12f, 0.97f));
        ImGui::PushStyleColor(ImGuiCol_TitleBg,      ImVec4(0.08f, 0.08f, 0.10f, 1.00f));
        ImGui::PushStyleColor(ImGuiCol_TitleBgActive, ImVec4(0.12f, 0.12f, 0.16f, 1.00f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 6.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

        char wTitle[64]; snprintf(wTitle, sizeof(wTitle), "Pohon Objek (%d)###ObjectTree", (int)g_shapes.GetEditableShapes().size()); if (ImGui::Begin(wTitle, &isOpen, flags)) {
            _RenderHeader();
            _RenderBody();
        }
        ImGui::End();

        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(3);
    }

private:

    // ============================================================
    // HEADER — judul + tombol clear all
    // ============================================================
    void _RenderHeader() {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.08f, 0.08f, 0.10f, 1.0f));
        ImGui::BeginChild("##OT_Header", ImVec2(0, 36), false);

        ImGui::SetCursorPos(ImVec2(10, 9));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.85f, 0.90f, 1.0f));
        ImGui::TextUnformatted("Pohon Objek");
        ImGui::PopStyleColor();

        // Jumlah shapes
        auto& shapes = g_shapes.GetEditableShapes();
        int total = (int)shapes.size();
        if (total > 0) {
            ImGui::SameLine();
            char buf[16]; snprintf(buf, sizeof(buf), "(%d)", total);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.6f, 1.0f));
            ImGui::TextUnformatted(buf);
            ImGui::PopStyleColor();
        }

        // Tombol trash hapus semua (pojok kanan)
        float rightX = ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX() - 28;
        ImGui::SameLine();
        ImGui::SetCursorPosX(rightX);
        ImGui::SetCursorPosY(6);
        if (_OT_IconBtn("##OT_ClearAll", texTrash2, ImVec4(1.0f,0.4f,0.4f,0.8f), ImVec2(20,20))) {
            ImGui::OpenPopup("##OT_ConfirmClear");
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Hapus semua drawing");

        // Konfirmasi hapus semua
        if (ImGui::BeginPopup("##OT_ConfirmClear")) {
            ImGui::TextUnformatted("Hapus SEMUA drawing?");
            ImGui::Spacing();
            if (ImGui::Button("Ya, hapus", ImVec2(90,0))) {
                g_shapes.GetEditableShapes().clear();
                g_draw.selectedShapeId = "";
                g_draw.isPopupOpen     = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Batal", ImVec2(60,0)))
                ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }

        ImGui::EndChild();
        ImGui::PopStyleColor();

        // Garis separator
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 p = ImGui::GetCursorScreenPos();
        dl->AddLine(p, ImVec2(p.x + ImGui::GetContentRegionAvail().x, p.y),
                    IM_COL32(255,255,255,25), 1.0f);
    }

    // ============================================================
    // BODY — daftar shapes dikelompokkan per panel
    // ============================================================
    void _RenderBody() {
        auto& shapes = g_shapes.GetEditableShapes();
        if (shapes.empty()) {
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 20);
            ImGui::SetCursorPosX((ImGui::GetContentRegionAvail().x - 160) * 0.5f);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f,0.4f,0.5f,1.0f));
            ImGui::TextUnformatted("Belum ada drawing");
            ImGui::PopStyleColor();
            return;
        }

        // Kumpulkan semua panel unik
        std::vector<std::string> panels;
        panels.push_back(""); // Chart utama selalu pertama
        for (const auto& s : shapes) {
            if (s.sourcePanel.empty()) continue;
            bool found = false;
            for (const auto& p : panels) if (p == s.sourcePanel) { found = true; break; }
            if (!found) panels.push_back(s.sourcePanel);
        }

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 1));
        ImGui::BeginChild("##OT_List", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

        for (const auto& panelName : panels) {
            // Hitung berapa shapes di panel ini
            int count = 0;
            for (const auto& s : shapes)
                if (s.sourcePanel == panelName) count++;
            if (count == 0) continue;

            _RenderGroup(panelName, shapes);
        }

        ImGui::EndChild();
        ImGui::PopStyleVar();
    }

    // ============================================================
    // GROUP HEADER (Chart Utama / RSI / dll)
    // ============================================================
    void _RenderGroup(const std::string& panelName, std::vector<GlobalShape>& shapes) {
        // Label grup
        const char* groupLabel = panelName.empty() ? "Chart Utama" : panelName.c_str();

        ImGui::PushStyleColor(ImGuiCol_Header,        ImVec4(0.15f,0.15f,0.18f,1.0f));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.18f,0.18f,0.22f,1.0f));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive,  ImVec4(0.20f,0.20f,0.25f,1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 4));

        bool open = ImGui::CollapsingHeader(groupLabel,
                        ImGuiTreeNodeFlags_DefaultOpen |
                        ImGuiTreeNodeFlags_SpanFullWidth);

        ImGui::PopStyleVar();
        ImGui::PopStyleColor(3);

        if (!open) return;

        // List shapes dalam grup ini — dari bawah ke atas (layer paling atas = paling atas)
        for (int i = (int)shapes.size() - 1; i >= 0; --i) {
            GlobalShape& s = shapes[i];
            if (s.sourcePanel != panelName) continue;
            _RenderRow(s, i);
        }
    }

    // ============================================================
    // ROW — satu baris per shape
    // FIX: Pakai IsMouseHoveringRect + IsMouseClicked, bukan nested
    //      InvisibleButton yang saling menelan area klik.
    // ============================================================
    void _RenderRow(GlobalShape& s, int idx) {
        bool isSelected = (s.id == g_draw.selectedShapeId);

        ImVec2 rowMin = ImGui::GetCursorScreenPos();
        float  rowH   = 30.0f;
        float  rowW   = ImGui::GetContentRegionAvail().x;
        ImVec2 rowMax = ImVec2(rowMin.x + rowW, rowMin.y + rowH);
        ImDrawList* dl = ImGui::GetWindowDrawList();

        // Cek hover seluruh row
        bool rowHovered = ImGui::IsMouseHoveringRect(rowMin, rowMax);

        // Background
        if (isSelected)
            dl->AddRectFilled(rowMin, rowMax, IM_COL32(50, 100, 200, 60));
        else if (rowHovered)
            dl->AddRectFilled(rowMin, rowMax, IM_COL32(255, 255, 255, 18));

        // Garis biru kiri saat selected
        if (isSelected)
            dl->AddLine(ImVec2(rowMin.x, rowMin.y), ImVec2(rowMin.x, rowMax.y),
                        IM_COL32(80, 140, 255, 220), 2.0f);

        ImGui::PushID(s.id.c_str());

        // ── ACTION BUTTONS (kanan row) ──────────────────────────
        // Harus diperiksa SEBELUM row-click agar tidak terblokir.
        bool actionClicked = false;
        if (rowHovered || isSelected) {
            actionClicked = _RenderActionButtons(s, rowMin, rowMax);
        }

        // ── ROW CLICK (select shape) ─────────────────────────────
        // Hanya trigger kalau tidak ada tombol aksi yang diklik.
        if (!actionClicked && rowHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            // Pastikan klik bukan di zona tombol aksi (60px dari kanan)
            float actZoneX = rowMax.x - 62.0f;
            if (ImGui::GetIO().MousePos.x < actZoneX) {
                g_draw.selectedShapeId   = s.id;
                g_draw.isPopupOpen       = true;
                g_draw.activePopupID     = s.id;
                g_draw.popupAnimProgress = 0.0f;
            }
        }

        // Advance cursor (biar layout flow benar)
        ImGui::Dummy(ImVec2(rowW, rowH));

        // ── KONTEN ROW (gambar di atas Dummy) ───────────────────
        // Icon tipe shape (warna = warna shape)
        ImTextureID shapeIcon = _OT_ShapeIcon(s.type);
        ImU32 iconTint = s.visible
            ? ImGui::ColorConvertFloat4ToU32(ImVec4(s.color.x, s.color.y, s.color.z, 0.9f))
            : IM_COL32(100, 100, 110, 120);
        dl->AddImage(shapeIcon,
                     ImVec2(rowMin.x + 10, rowMin.y + 7),
                     ImVec2(rowMin.x + 26, rowMin.y + 23),
                     ImVec2(0,0), ImVec2(1,1), iconTint);

        // Label nama
        float textX = rowMin.x + 34;
        float textY = rowMin.y + 9;
        ImU32 textCol = !s.visible
            ? IM_COL32(100, 100, 110, 140)
            : (isSelected ? IM_COL32(160, 200, 255, 255) : IM_COL32(200, 200, 210, 220));
        const char* label = _OT_ShapeLabel(s.type);
        dl->AddText(ImVec2(textX, textY), textCol, label);

        // Warna dot
        ImU32 dotCol = s.visible
            ? ImGui::ColorConvertFloat4ToU32(s.color)
            : IM_COL32(80, 80, 90, 120);
        float lblW = ImGui::CalcTextSize(label).x;
        dl->AddCircleFilled(ImVec2(textX + lblW + 8, rowMin.y + 15), 4.0f, dotCol);

        // Lock indicator (kecil di pojok kiri bawah icon)
        if (s.locked) {
            dl->AddText(ImVec2(rowMin.x + 18, rowMin.y + 16),
                        IM_COL32(255, 215, 0, 200), "L");
        }

        ImGui::PopID();

        // Garis separator antar row
        dl->AddLine(ImVec2(rowMin.x + 8, rowMax.y - 1),
                    ImVec2(rowMax.x, rowMax.y - 1),
                    IM_COL32(255, 255, 255, 10));

        if (rowHovered)
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    }

    // ============================================================
    // TOMBOL AKSI: [👁] [🔒] [🗑️]
    // Return true jika ada tombol yang diklik (supaya row-click skip)
    // FIX: Pakai IsMouseHoveringRect + IsMouseClicked langsung,
    //      BUKAN InvisibleButton — menghindari konflik area klik.
    // ============================================================
    bool _RenderActionButtons(GlobalShape& s, ImVec2 rowMin, ImVec2 rowMax) {
        ImDrawList* dl  = ImGui::GetWindowDrawList();
        bool clicked    = false;
        float btnSize   = 16.0f;
        float btnPad    = 3.0f;
        float btnY      = rowMin.y + (rowMax.y - rowMin.y - btnSize) * 0.5f;
        float rightEdge = rowMax.x - 5.0f;

        // Helper lambda — gambar satu tombol, return true jika diklik
        auto Btn = [&](ImTextureID tex, ImU32 tint, ImU32 hoverBg,
                       float x, const char* tip) -> bool {
            ImVec2 bMin(x, btnY);
            ImVec2 bMax(x + btnSize, btnY + btnSize);
            bool hov = ImGui::IsMouseHoveringRect(bMin, bMax);
            if (hov) {
                dl->AddRectFilled(bMin, bMax, hoverBg, 3.0f);
                ImGui::SetTooltip("%s", tip);
            }
            dl->AddImage(tex, bMin, bMax, ImVec2(0,0), ImVec2(1,1),
                         hov ? IM_COL32(255,255,255,255) : tint);
            if (hov && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                return true;
            return false;
        };

        // 🗑️ DELETE (paling kanan)
        float delX = rightEdge - btnSize;
        if (Btn(texTrash2, IM_COL32(255,90,90,180), IM_COL32(180,30,30,80),
                delX, "Hapus")) {
            g_shapes.RemoveShape(s.id);
            if (g_draw.selectedShapeId == s.id) {
                g_draw.selectedShapeId = "";
                g_draw.isPopupOpen     = false;
                g_draw.activePopupID   = "";
            }
            clicked = true;
        }

        // 🔒 LOCK
        float lockX = delX - btnSize - btnPad;
        ImU32 lockTint = s.locked ? IM_COL32(255,215,0,230) : IM_COL32(160,160,180,140);
        if (Btn(texPopupLock1, lockTint, IM_COL32(100,80,0,60),
                lockX, s.locked ? "Unlock" : "Lock")) {
            s.locked = !s.locked;
            clicked = true;
        }

        // 👁 EYE
        float eyeX = lockX - btnSize - btnPad;
        ImTextureID eyeTex  = s.visible ? texEyeShow : texEyeHide;
        ImU32       eyeTint = s.visible ? IM_COL32(160,210,255,180) : IM_COL32(100,100,120,120);
        if (Btn(eyeTex, eyeTint, IM_COL32(30,60,100,60),
                eyeX, s.visible ? "Sembunyikan" : "Tampilkan")) {
            s.visible = !s.visible;
            clicked = true;
        }

        return clicked;
    }
};

// ================================================================
// GLOBAL INSTANCE
// ================================================================
inline ObjectTreePanel g_objectTree;
