#pragma once
#include "imgui.h"
#include "imgui_internal.h"
#include "GlobalShapeManager.h"

// =========================================================
// ShapeEditUI.h — ENHANCED (Universal Property Popup)
// =========================================================
// Menangani semua tools non-FIB: LINE, RECT, TEXT, BRUSH, ELLIOT
// FIB tetap di-handle oleh FiboEditUI.h
// =========================================================

// Ambil variabel texture dari luar
extern ImTextureID texPopupCopy;
extern ImTextureID texPopupColor;
extern ImTextureID texPopupThick;
extern ImTextureID texPopupLock;
extern ImTextureID texTrash1;
extern ImTextureID texPopupSetting; // Ikon Gerigi (Settings)

// Tipe Aksi Balikan
enum ShapeAction { ACT_NONE, ACT_COPY, ACT_COLOR, ACT_THICK, ACT_LOCK, ACT_DELETE };

class ShapeEditUI {
private:
    // ---------------------------------------------------------
    // STATIC STATE (inline static = C++17, aman header-only)
    // ---------------------------------------------------------
    inline static std::string lastRectPopupId;  // track shape aktif RECT
    inline static char rectLabelBuf[128] = "";   // buffer input label RECT
    inline static int activeRectTab = 0;         // tab aktif RECT: 0=Corak, 1=Teks, 2=Koordinat, 3=Visibilitas
    inline static char coordBuf0[32] = "";       // buffer koordinat #1
    inline static char coordBuf1[32] = "";       // buffer koordinat #2

    // BRUSH state
    inline static std::string lastBrushPopupId = "";
    inline static int activeBrushTab = 0;        // 0=Corak, 1=Koordinat, 2=Visibilitas

    // ELLIOT state
    inline static std::string lastElliotPopupId = "";
    inline static int activeElliotTab = 0;       // 0=Corak, 1=Label, 2=Koordinat, 3=Visibilitas

    // ---------------------------------------------------------
    // HELPER: ICON BUTTON (Visual Tombol Kecil dengan Texture)
    // ---------------------------------------------------------
    static bool IconButton(const char* idStr, ImTextureID user_texture_id, ImU32 color, bool isActive = false) {
        ImGuiWindow* win = ImGui::GetCurrentWindow();
        if (win->SkipItems) return false;

        const ImGuiID id = win->GetID(idStr);
        ImVec2 size(43, 43);

        const ImRect bb(win->DC.CursorPos, ImVec2(win->DC.CursorPos.x + size.x, win->DC.CursorPos.y + size.y));
        ImGui::ItemSize(bb);
        if (!ImGui::ItemAdd(bb, id)) return false;

        bool hovered, held;
        bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);

        // Background semi-transparan saat hover/active
        if (hovered || isActive)
            win->DrawList->AddRectFilled(bb.Min, bb.Max, IM_COL32(255, 255, 255, 40), 6.0f);

        // Border hijau saat mode aktif (misal Lock ON)
        if (isActive)
            win->DrawList->AddRect(bb.Min, bb.Max, IM_COL32(0, 255, 0, 200), 6.0f);

        // Render icon gambar
        if (user_texture_id != 0) {
            float padding = 8.0f;
            ImVec2 pMin = ImVec2(bb.Min.x + padding, bb.Min.y + padding);
            ImVec2 pMax = ImVec2(bb.Max.x - padding, bb.Max.y - padding);
            win->DrawList->AddImage(user_texture_id, pMin, pMax, ImVec2(0,0), ImVec2(1,1), color);
        }

        return pressed;
    }

    // ---------------------------------------------------------
    // HELPER: DASHED LINE RENDERER
    // Digunakan untuk LINE dan RECT saat lineStyle != Solid
    // ---------------------------------------------------------
    static void DrawStyledLine(ImDrawList* draw, const ImVec2& p0, const ImVec2& p1, ImU32 col, float thickness, int style) {
        if (style == 0) {
            // Solid
            draw->AddLine(p0, p1, col, thickness);
        } else {
            // Dashed (1) or Dotted (2)
            float totalDist = std::sqrt((p1.x - p0.x) * (p1.x - p0.x) + (p1.y - p0.y) * (p1.y - p0.y));
            if (totalDist <= 0) return;

            float dashSize = (style == 1) ? 10.0f : 2.0f;
            float gapSize  = (style == 1) ? 8.0f : 4.0f;

            ImVec2 dir((p1.x - p0.x) / totalDist, (p1.y - p0.y) / totalDist);
            float currentDist = 0.0f;

            while (currentDist < totalDist) {
                float nextDist = std::min(currentDist + dashSize, totalDist);
                ImVec2 start = ImVec2(p0.x + dir.x * currentDist, p0.y + dir.y * currentDist);
                ImVec2 end   = ImVec2(p0.x + dir.x * nextDist,    p0.y + dir.y * nextDist);
                draw->AddLine(start, end, col, thickness);
                currentDist += (dashSize + gapSize);
            }
        }
    }

public:
    // =========================================================
    // FUNGSI UTAMA: RENDER BUBBLE BAR + SETTINGS WINDOW
    // Dipanggil oleh CDrawingManager::RenderShapePopup()
    // =========================================================
    static ShapeAction Render(GlobalShape& s) {
        ShapeAction act = ACT_NONE;
        ImU32 white = IM_COL32(240, 240, 240, 255);
        ImU32 gold  = IM_COL32(255, 215, 0, 255);

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(5, 5));

        // -----------------------------------------------------
        // A. MENU BUBBLE BAR (Ikon-Ikon)
        // -----------------------------------------------------

        // 1. LOCK
        if (IconButton("##L", texPopupLock, s.locked ? gold : white, s.locked)) act = ACT_LOCK;
        ImGui::SameLine();

        // 2. COPY
        if (IconButton("##C", texPopupCopy, white)) act = ACT_COPY;
        ImGui::SameLine();

        // 3. COLOR
        if (IconButton("##Col", texPopupColor, ImGui::ColorConvertFloat4ToU32(s.color)))
            ImGui::OpenPopup("CP");
        if (ImGui::BeginPopup("CP")) {
            ImGui::ColorPicker4("##p", (float*)&s.color,
                ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_NoSmallPreview);
            ImGui::EndPopup();
        }
        ImGui::SameLine();

        // 4. THICKNESS
        if (IconButton("##T", texPopupThick, white))
            ImGui::OpenPopup("TS");
        if (ImGui::BeginPopup("TS")) {
            ImGui::SetNextItemWidth(150);
            ImGui::SliderFloat("##w", &s.thickness, 0.5f, 10.0f, "Tebal: %.1f");
            ImGui::EndPopup();
        }
        ImGui::SameLine();

        // 5. SETTINGS (Tombol Gerigi)
        ImU32 settingCol = s.settingsOpen ? IM_COL32(100, 255, 100, 255) : white;
        if (IconButton("##Set", texPopupSetting, settingCol, s.settingsOpen)) {
            s.settingsOpen = !s.settingsOpen;
        }
        ImGui::SameLine();

        // 6. DELETE
        if (IconButton("##D", texTrash1, IM_COL32(255, 100, 100, 255)))
            act = ACT_DELETE;

        ImGui::PopStyleVar();

        // -----------------------------------------------------
        // B. SETTINGS WINDOW (Per-Tool, terbuka saat Settings ON)
        // -----------------------------------------------------
        if (s.settingsOpen) {
            char title[64];
            sprintf(title, "Settings##%s", s.id.c_str());
            // RECT, BRUSH, ELLIOT pakai TabBar → perlu lebih lebar
            float winW = 380.0f;
            float winH = 400.0f;
            ImGui::SetNextWindowSize(ImVec2(winW, winH), ImGuiCond_FirstUseEver);

            if (ImGui::Begin(title, &s.settingsOpen)) {

                // ==============================
                // === SECTION: LINE ===
                // ==============================
                if (s.type == "LINE") {
                    RenderLineSettings(s);
                }
                // ==============================
                // === SECTION: RECT ===
                // ==============================
                else if (s.type == "RECT") {
                    RenderRectSettings(s);
                }
                // ==============================
                // === SECTION: TEXT ===
                // ==============================
                else if (s.type == "TEXT") {
                    RenderTextSettings(s);
                }
                // ==============================
                // === SECTION: BRUSH ===
                // ==============================
                else if (s.type == "BRUSH") {
                    RenderBrushSettings(s);
                }
                // ==============================
                // === SECTION: ELLIOT ===
                // ==============================
                else if (s.type == "ELLIOT") {
                    RenderElliotSettings(s);
                }
            }
            ImGui::End();
        }

        return act;
    }

    // =========================================================
    // LINE SETTINGS
    // =========================================================
    static void RenderLineSettings(GlobalShape& s) {
        // --- Line Style ---
        if (ImGui::CollapsingHeader("Line Style", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.9f, 1.0f), "Garisis");
            ImGui::SetNextItemWidth(180);
            const char* styles[] = { "Solid", "Dashed", "Dotted" };
            ImGui::Combo("##linestyle", &s.lineStyle, styles, IM_ARRAYSIZE(styles));

            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.9f, 1.0f), "Extend");
            ImGui::Checkbox("Extend Left", &s.extendLeft);
            ImGui::SameLine();
            ImGui::Checkbox("Extend Right", &s.extendRight);

            ImGui::Spacing();
            ImGui::Checkbox("Show Endpoints", &s.showEndpoints);
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Tampilkan titik bulat di ujung P0 dan P1");
            }
        }

        // --- Appearance ---
        if (ImGui::CollapsingHeader("Appearance", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.9f, 1.0f), "Color");
            ImGui::ColorEdit4("##linecol", (float*)&s.color,
                ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);
            ImGui::SameLine();

            ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.9f, 1.0f), "Thickness");
            ImGui::SetNextItemWidth(150);
            ImGui::SliderFloat("##linethick", &s.thickness, 0.5f, 10.0f, "%.1f px");
        }
    }

    // =========================================================
    // RECT SETTINGS (TABBED UI — rapi, 2 kolom)
    // =========================================================
    static void RenderRectSettings(GlobalShape& s) {
        // --- Sync buffer saat ganti shape RECT ---
        if (lastRectPopupId != s.id) {
            lastRectPopupId = s.id;
            memset(rectLabelBuf, 0, sizeof(rectLabelBuf));
            strncpy(rectLabelBuf, s.rectLabel.c_str(), sizeof(rectLabelBuf) - 1);
            activeRectTab = 0;
        }

        // Helper: baris label+kontrol sejajar (label kiri tetap lebar)
        auto Row = [](const char* label) {
            ImGui::TextColored(ImVec4(0.82f, 0.82f, 0.82f, 1.0f), "%s", label);
            ImGui::SameLine(110.0f);  // semua kontrol mulai di x=110
        };
        auto Sep = []() { ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing(); };

        if (ImGui::BeginTabBar("##RectTabBar", ImGuiTabBarFlags_None)) {

            // =================================================
            // TAB 1: CORAK (STYLE)
            // =================================================
            if (ImGui::BeginTabItem("Corak")) {
                ImGui::Spacing();

                // Perpanjang: 2 checkbox sejajar (kayak LINE tool)
                Row("Perpanjang");
                ImGui::Checkbox("Kiri", &s.extendLeft);
                ImGui::SameLine(170.0f);
                ImGui::Checkbox("Kanan", &s.extendRight);

                Sep();

                // Batas (Border) toggle
                Row("Batas");
                ImGui::Checkbox("Aktif##bordvis", &s.rectBorderVisible);

                if (s.rectBorderVisible) {
                    Row("Warna Border");
                    ImGui::ColorEdit4("##rbordcol", (float*)&s.color,
                        ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);

                    Row("Garis");
                    ImGui::SetNextItemWidth(160);
                    const char* styles[] = { "Solid", "Dashed", "Dotted" };
                    ImGui::Combo("##rectstyle", &s.lineStyle, styles, IM_ARRAYSIZE(styles));

                    Row("Tebal");
                    ImGui::SetNextItemWidth(160);
                    ImGui::SliderFloat("##rectthick", &s.thickness, 0.5f, 10.0f, "%.1f px");
                }

                Sep();

                // Latar (Fill)
                Row("Latar");
                ImGui::Checkbox("Aktif##fillvis", &s.filled);

                if (s.filled) {
                    Row("Warna Latar");
                    ImGui::ColorEdit4("##rfillcol", (float*)&s.fillColor,
                        ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);

                    Row("Opacity");
                    ImGui::SetNextItemWidth(160);
                    ImGui::SliderFloat("##rectopacity", &s.fillOpacity, 0.0f, 1.0f, "%.2f");
                }

                ImGui::EndTabItem();
            }

            // =================================================
            // TAB 2: TEKS (TEXT)
            // =================================================
            if (ImGui::BeginTabItem("Teks")) {
                ImGui::Spacing();

                // Teks input
                Row("Teks");
                ImGui::SetNextItemWidth(220);
                if (ImGui::InputText("##rectlabel", rectLabelBuf, sizeof(rectLabelBuf))) {
                    s.rectLabel = std::string(rectLabelBuf);
                }

                Sep();

                // Warna Teks
                Row("Warna");
                ImGui::ColorEdit4("##rtxtcol", (float*)&s.textColor,
                    ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);

                // Ukuran Font — dropdown preset (kayak ganti TF)
                Row("Ukuran");
                ImGui::SetNextItemWidth(160);
                const char* fontSizes[] = { "6", "7", "8", "9", "10", "11", "12", "14", "16", "18", "20", "24", "28", "32", "36", "48" };
                static const float fontVals[] = { 6,7,8,9,10,11,12,14,16,18,20,24,28,32,36,48 };
                // Find current index
                int fontIdx = 3; // default 9
                for (int i = 0; i < 16; i++) {
                    if (std::abs(s.rectFontSize - fontVals[i]) < 0.1f) { fontIdx = i; break; }
                }
                if (ImGui::Combo("##rfontsize", &fontIdx, fontSizes, 16)) {
                    s.rectFontSize = fontVals[fontIdx];
                }

                Sep();

                // Style: Bold & Italic toggle
                Row("Style");
                bool bActive = s.rectBold;
                bool iActive = s.rectItalic;
                if (bActive) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.5f, 0.9f, 1.0f));
                if (ImGui::Button("B", ImVec2(35, 24))) s.rectBold = !s.rectBold;
                if (bActive) ImGui::PopStyleColor();
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Bold");

                ImGui::SameLine(155.0f);
                if (iActive) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.5f, 0.9f, 1.0f));
                if (ImGui::Button("I", ImVec2(35, 24))) s.rectItalic = !s.rectItalic;
                if (iActive) ImGui::PopStyleColor();
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Italic");

                Sep();

                // Perataan Horizontal
                Row("Perataan");
                ImGui::SetNextItemWidth(160);
                const char* hAlignOpts[] = { "Kiri", "Tengah", "Kanan" };
                ImGui::Combo("##halign", &s.rectTextAlign, hAlignOpts, IM_ARRAYSIZE(hAlignOpts));

                // Posisi Vertikal
                Row("Vertikal");
                ImGui::SetNextItemWidth(160);
                const char* vAlignOpts[] = { "Atas", "Tengah", "Bawah" };
                ImGui::Combo("##valign", &s.rectVertAlign, vAlignOpts, IM_ARRAYSIZE(vAlignOpts));

                ImGui::EndTabItem();
            }

            // =================================================
            // TAB 3: KOORDINAT
            // =================================================
            if (ImGui::BeginTabItem("Koordinat")) {
                ImGui::Spacing();

                // Point #1
                ImGui::TextColored(ImVec4(0.6f, 0.85f, 0.6f, 1.0f), "Titik #1");
                Sep();

                Row("Harga");
                ImGui::SetNextItemWidth(160);
                char p0Buf[32];
                snprintf(p0Buf, sizeof(p0Buf), "%.2f", s.price0);
                if (ImGui::InputText("##p0harga", p0Buf, sizeof(p0Buf), ImGuiInputTextFlags_CharsDecimal))
                    s.price0 = atof(p0Buf);

                Row("Bar");
                ImGui::SetNextItemWidth(160);
                char t0Buf[32];
                snprintf(t0Buf, sizeof(t0Buf), "%.0f", s.time0);
                if (ImGui::InputText("##p0bar", t0Buf, sizeof(t0Buf), ImGuiInputTextFlags_CharsDecimal))
                    s.time0 = atof(t0Buf);

                Sep();

                // Point #2
                ImGui::TextColored(ImVec4(0.6f, 0.85f, 0.6f, 1.0f), "Titik #2");
                Sep();

                Row("Harga");
                ImGui::SetNextItemWidth(160);
                char p1Buf[32];
                snprintf(p1Buf, sizeof(p1Buf), "%.2f", s.price1);
                if (ImGui::InputText("##p1harga", p1Buf, sizeof(p1Buf), ImGuiInputTextFlags_CharsDecimal))
                    s.price1 = atof(p1Buf);

                Row("Bar");
                ImGui::SetNextItemWidth(160);
                char t1Buf[32];
                snprintf(t1Buf, sizeof(t1Buf), "%.0f", s.time1);
                if (ImGui::InputText("##p1bar", t1Buf, sizeof(t1Buf), ImGuiInputTextFlags_CharsDecimal))
                    s.time1 = atof(t1Buf);

                ImGui::EndTabItem();
            }

            // =================================================
            // TAB 4: VISIBILITAS
            // =================================================
            if (ImGui::BeginTabItem("Visibilitas")) {
                ImGui::Spacing();

                Row("Tampilkan");
                ImGui::Checkbox("##vis", &s.visible);

                Row("Kunci");
                ImGui::Checkbox("##lock", &s.locked);

                Row("Dimensi");
                ImGui::Checkbox("Tampilkan W x H##dim", &s.showDimensions);

                Sep();

                // Info read-only
                ImGui::TextColored(ImVec4(0.45f, 0.45f, 0.45f, 1.0f), "ID  %s", s.id.c_str());

                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }
    }

    // =========================================================
    // TEXT SETTINGS
    // =========================================================
    static void RenderTextSettings(GlobalShape& s) {
        // --- Font ---
        if (ImGui::CollapsingHeader("Font", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.9f, 1.0f), "Text Color");
            ImGui::ColorEdit4("##textcol", (float*)&s.color,
                ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);

            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.9f, 1.0f), "Font Size");
            ImGui::SetNextItemWidth(150);
            ImGui::SliderFloat("##textsize", &s.fontSize, 10.0f, 72.0f, "%.0f px");
        }

        // --- Background ---
        if (ImGui::CollapsingHeader("Background", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Checkbox("Show Background", &s.textBg);

            if (s.textBg) {
                ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.9f, 1.0f), "BG Color");
                ImGui::SameLine();
                ImGui::ColorEdit4("##textbgcol", (float*)&s.textBgColor,
                    ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);
            }
        }
    }

    // =========================================================
    // BRUSH SETTINGS (TABBED UI — rapi seperti RECT)
    // =========================================================
    static void RenderBrushSettings(GlobalShape& s) {
        // --- Sync buffer saat ganti shape BRUSH ---
        if (lastBrushPopupId != s.id) {
            lastBrushPopupId = s.id;
            activeBrushTab = 0;
        }

        auto Row = [](const char* label) {
            ImGui::TextColored(ImVec4(0.82f, 0.82f, 0.82f, 1.0f), "%s", label);
            ImGui::SameLine(110.0f);
        };
        auto Sep = []() { ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing(); };

        if (ImGui::BeginTabBar("##BrushTabBar", ImGuiTabBarFlags_None)) {

            // =================================================
            // TAB 1: CORAK (STYLE)
            // =================================================
            if (ImGui::BeginTabItem("Corak")) {
                ImGui::Spacing();

                Row("Warna");
                ImGui::ColorEdit4("##brushcol", (float*)&s.color,
                    ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);

                Row("Garis");
                ImGui::SetNextItemWidth(160);
                const char* styles[] = { "Solid", "Dashed", "Dotted" };
                ImGui::Combo("##brushstyle", &s.lineStyle, styles, IM_ARRAYSIZE(styles));

                Sep();

                Row("Tebal");
                ImGui::SetNextItemWidth(160);
                ImGui::SliderFloat("##brushthick", &s.thickness, 1.0f, 20.0f, "%.1f px");

                Row("Opacity");
                ImGui::SetNextItemWidth(160);
                ImGui::SliderFloat("##brushopacity", &s.brushOpacity, 0.1f, 1.0f, "%.2f");

                ImGui::EndTabItem();
            }

            // =================================================
            // TAB 2: KOORDINAT
            // =================================================
            if (ImGui::BeginTabItem("Koordinat")) {
                ImGui::Spacing();

                // Titik Awal (index 0 dari multiTime/multiPrice)
                if (!s.multiTime.empty()) {
                    ImGui::TextColored(ImVec4(0.6f, 0.85f, 0.6f, 1.0f), "Titik Awal");
                    Sep();

                    Row("Harga");
                    ImGui::SetNextItemWidth(160);
                    char pBuf[32];
                    snprintf(pBuf, sizeof(pBuf), "%.2f", s.multiPrice[0]);
                    if (ImGui::InputText("##brushpharga", pBuf, sizeof(pBuf), ImGuiInputTextFlags_CharsDecimal))
                        if (!s.multiPrice.empty()) s.multiPrice[0] = atof(pBuf);

                    Row("Bar");
                    ImGui::SetNextItemWidth(160);
                    char tBuf[32];
                    snprintf(tBuf, sizeof(tBuf), "%.0f", s.multiTime[0]);
                    if (ImGui::InputText("##brushtbar", tBuf, sizeof(tBuf), ImGuiInputTextFlags_CharsDecimal))
                        if (!s.multiTime.empty()) s.multiTime[0] = atof(tBuf);

                    Sep();
                    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Total titik: %d", (int)s.multiTime.size());
                } else {
                    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Belum ada titik.");
                }

                ImGui::EndTabItem();
            }

            // =================================================
            // TAB 3: VISIBILITAS
            // =================================================
            if (ImGui::BeginTabItem("Visibilitas")) {
                ImGui::Spacing();

                Row("Tampilkan");
                ImGui::Checkbox("##brushvis", &s.visible);

                Row("Kunci");
                ImGui::Checkbox("##brushlock", &s.locked);

                Sep();

                ImGui::TextColored(ImVec4(0.45f, 0.45f, 0.45f, 1.0f), "ID  %s", s.id.c_str());

                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }
    }

    // =========================================================
    // ELLIOT SETTINGS (TABBED UI — rapi seperti RECT)
    // =========================================================
    static void RenderElliotSettings(GlobalShape& s) {
        // --- Sync buffer saat ganti shape ELLIOT ---
        if (lastElliotPopupId != s.id) {
            lastElliotPopupId = s.id;
            activeElliotTab = 0;
        }

        auto Row = [](const char* label) {
            ImGui::TextColored(ImVec4(0.82f, 0.82f, 0.82f, 1.0f), "%s", label);
            ImGui::SameLine(110.0f);
        };
        auto Sep = []() { ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing(); };

        if (ImGui::BeginTabBar("##ElliotTabBar", ImGuiTabBarFlags_None)) {

            // =================================================
            // TAB 1: CORAK (STYLE)
            // =================================================
            if (ImGui::BeginTabItem("Corak")) {
                ImGui::Spacing();

                Row("Warna");
                ImGui::ColorEdit4("##ellicol", (float*)&s.color,
                    ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);

                Row("Garis");
                ImGui::SetNextItemWidth(160);
                const char* styles[] = { "Solid", "Dashed", "Dotted" };
                ImGui::Combo("##ellistyle", &s.lineStyle, styles, IM_ARRAYSIZE(styles));

                Sep();

                Row("Tebal");
                ImGui::SetNextItemWidth(160);
                ImGui::SliderFloat("##ellithick", &s.thickness, 0.5f, 5.0f, "%.1f px");

                Row("Opacity");
                ImGui::SetNextItemWidth(160);
                ImGui::SliderFloat("##elliopacity", &s.brushOpacity, 0.1f, 1.0f, "%.2f");

                ImGui::EndTabItem();
            }

            // =================================================
            // TAB 2: LABEL
            // =================================================
            if (ImGui::BeginTabItem("Label")) {
                ImGui::Spacing();

                Row("Format");
                ImGui::SetNextItemWidth(160);
                const char* formats[] = { "(0)", "0", "(A)", "A", "Wave-0" };
                ImGui::Combo("##ellilabel", &s.elliLabelFormat, formats, IM_ARRAYSIZE(formats));

                Sep();

                Row("Harga");
                ImGui::Checkbox("Tampilkan harga##elliprice", &s.elliShowPrice);
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Tampilkan harga di tiap titik Elliot");
                }

                ImGui::EndTabItem();
            }

            // =================================================
            // TAB 3: KOORDINAT
            // =================================================
            if (ImGui::BeginTabItem("Koordinat")) {
                ImGui::Spacing();

                // Tampilkan semua titik Elliot (0, 1, 2, 3, 4)
                int nPts = (int)std::min(s.multiTime.size(), s.multiPrice.size());
                if (nPts > 0) {
                    for (int i = 0; i < nPts; i++) {
                        // Label titik: (0), A, dll sesuai format
                        char ptLabel[16];
                        if (s.elliLabelFormat == 2 || s.elliLabelFormat == 3) {
                            if (i < 26) snprintf(ptLabel, sizeof(ptLabel), "Titik %c", 'A' + (char)i);
                            else        snprintf(ptLabel, sizeof(ptLabel), "Titik %d", i);
                        } else {
                            snprintf(ptLabel, sizeof(ptLabel), "Titik %d", i);
                        }
                        ImGui::TextColored(ImVec4(0.6f, 0.85f, 0.6f, 1.0f), "%s", ptLabel);
                        Sep();

                        Row("Harga");
                        ImGui::SetNextItemWidth(160);
                        char pBuf[32];
                        snprintf(pBuf, sizeof(pBuf), "%.2f", s.multiPrice[i]);
                        if (ImGui::InputText(("##ellip" + std::to_string(i) + "harga").c_str(), pBuf, sizeof(pBuf), ImGuiInputTextFlags_CharsDecimal))
                            s.multiPrice[i] = atof(pBuf);

                        Row("Bar");
                        ImGui::SetNextItemWidth(160);
                        char tBuf[32];
                        snprintf(tBuf, sizeof(tBuf), "%.0f", s.multiTime[i]);
                        if (ImGui::InputText(("##ellip" + std::to_string(i) + "bar").c_str(), tBuf, sizeof(tBuf), ImGuiInputTextFlags_CharsDecimal))
                            s.multiTime[i] = atof(tBuf);

                        if (i < nPts - 1) Sep();
                    }
                } else {
                    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Belum ada titik.");
                }

                ImGui::EndTabItem();
            }

            // =================================================
            // TAB 4: VISIBILITAS
            // =================================================
            if (ImGui::BeginTabItem("Visibilitas")) {
                ImGui::Spacing();

                Row("Tampilkan");
                ImGui::Checkbox("##ellivis", &s.visible);

                Row("Kunci");
                ImGui::Checkbox("##ellilock", &s.locked);

                Sep();

                ImGui::TextColored(ImVec4(0.45f, 0.45f, 0.45f, 1.0f), "ID  %s", s.id.c_str());

                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }
    }
};
