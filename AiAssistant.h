// ============================================================
// AiAssistant.h — Jarvis AI Chat UI untuk ImGui/WASM
// Phase 2: Real chart execution via JarvisBridge
// ============================================================
#pragma once

#include "imgui.h"
#include "nlohmann/json.hpp"
#include "JarvisBridge.h"
#include <string>
#include <vector>
#include <functional>

using json = nlohmann::json;

// ============================================================
// DATA STRUCTURES
// ============================================================

struct ChatMsg {
    std::string role;       // "user", "assistant", "system"
    std::string content;    // text content
    std::string timestamp;
    bool is_error = false;

    // Tool call info
    std::string tool_name;
    std::string tool_args;
    std::string tool_result;
    bool tool_success = false;
    int tool_duration_ms = 0;
};

struct JarvisState {
    bool show_window = false;
    bool is_loading = false;
    std::string error_msg;
    std::string input_buf;
    std::string response_buf;  // streaming accumulation

    std::vector<ChatMsg> messages;

    // UI state
    float scroll_to_bottom = -1.0f;  // trigger auto-scroll
    bool input_focused = false;
    int msg_id_counter = 0;
};

// Global state
static JarvisState g_jarvis;

// ============================================================
// HISTORY FORMAT FOR API (last N messages as role+content)
// ============================================================
static json BuildHistory(const std::vector<ChatMsg>& msgs, int maxPairs = 10) {
    json arr = json::array();
    int start = (int)msgs.size() - maxPairs * 2;
    if (start < 0) start = 0;
    for (int i = start; i < (int)msgs.size(); i++) {
        arr.push_back({
            {"role", msgs[i].role},
            {"content", msgs[i].content}
        });
    }
    return arr;
}

// ============================================================
// ALL JARVIS C-LINKAGE DECLARATIONS (extern "C")
// These functions are exported to JavaScript via EXPORTED_FUNCTIONS
// and MUST use C linkage to avoid C++ name mangling.
// ============================================================
extern "C" {

#ifdef __EMSCRIPTEN__
// Forward declaration — defined in AiAssistant.cpp with EMSCRIPTEN_KEEPALIVE
void jarvis_send_to_api(const char* message_json);
void jarvis_on_response(const char* response_json);
void jarvis_on_error(const char* err_text);
#else
// Desktop stubs — just print (no actual network call)
static inline void jarvis_send_to_api(const char* message_json) {
    printf("[Jarvis Desktop] Would send: %s\n", message_json);
}
static inline void jarvis_on_response(const char* response_json) {
    printf("[Jarvis Desktop] Response: %s\n", response_json);
}
static inline void jarvis_on_error(const char* err_text) {
    printf("[Jarvis Desktop] Error: %s\n", err_text);
}
#endif

} // extern "C"

// ============================================================
// jarvis_on_response — CALLED WHEN API RESPONSE ARRIVES
// (Implementation for WASM build)
// ============================================================
#ifdef __EMSCRIPTEN__

extern "C" {

EMSCRIPTEN_KEEPALIVE
void jarvis_on_response(const char* response_json) {
    // Parse response from backend API
    // Expected format: {"response": "...", "actions": [...], "updatedHistory": [...]}
    try {
        json resp = json::parse(response_json);

        // Set response text
        std::string text = resp.value("response", "");
        g_jarvis.response_buf = text;

        // Add assistant message
        ChatMsg msg;
        msg.role = "assistant";
        msg.content = text;
        msg.timestamp = "now";
        g_jarvis.messages.push_back(msg);

        // Handle actions (tool calls that were executed on server)
        if (resp.contains("actions") && resp["actions"].is_array()) {
            for (const auto& action : resp["actions"]) {
                std::string act_name = action.value("tool", "");
                json act_args = action.value("arguments", json::object());
                bool act_success = action.value("result", json::object()).value("success", false);

                // ACTUALLY EXECUTE THE TOOL IN C++ (bridge to chart)
                if (act_success) {
                    if (act_name == "chart_add_symbol" && act_args.contains("symbol")) {
                        std::string sym = act_args["symbol"].get<std::string>();
                        // === REAL EXECUTION via Bridge ===
                        JarvisBridge_AddSymbol(sym);
                    }
                    else if (act_name == "chart_add_indicator" && act_args.contains("symbol")) {
                        std::string ind   = act_args.value("indicator", "");
                        int period        = act_args.value("period", 14);
                        // === REAL EXECUTION via Bridge ===
                        JarvisBridge_AddIndicator(ind, period);
                    }
                }
            }
        }

        // Trigger scroll to bottom
        g_jarvis.scroll_to_bottom = ImGui::GetTime();
        g_jarvis.is_loading = false;
        g_jarvis.error_msg.clear();

    } catch (const std::exception& e) {
        g_jarvis.is_loading = false;
        g_jarvis.error_msg = std::string("Parse error: ") + e.what();
    }
}

EMSCRIPTEN_KEEPALIVE
void jarvis_on_error(const char* err_text) {
    g_jarvis.is_loading = false;
    g_jarvis.error_msg = err_text ? err_text : "Unknown error";

    // Add error message to chat
    ChatMsg msg;
    msg.role = "system";
    msg.content = std::string("Error: ") + (err_text ? err_text : "Unknown error");
    msg.is_error = true;
    msg.timestamp = "now";
    g_jarvis.messages.push_back(msg);
    g_jarvis.scroll_to_bottom = ImGui::GetTime();
}

} // extern "C"

#endif // __EMSCRIPTEN__

// ============================================================
// SEND MESSAGE — triggered by user pressing Enter or Send button
// ============================================================
static void JarvisSendMessage() {
    std::string text = g_jarvis.input_buf;
    if (text.empty() || g_jarvis.is_loading) return;

    // Add user message
    ChatMsg userMsg;
    userMsg.role = "user";
    userMsg.content = text;
    userMsg.timestamp = "now";
    g_jarvis.messages.push_back(userMsg);

    // Clear input
    g_jarvis.input_buf.clear();
    g_jarvis.is_loading = true;
    g_jarvis.error_msg.clear();
    g_jarvis.scroll_to_bottom = ImGui::GetTime();

    // Build request JSON
    json req_body;
    req_body["message"] = text;
    req_body["history"] = BuildHistory(g_jarvis.messages);

// SESUDAH (fix):
    req_body["chart_status"] = json::parse(JarvisBridge_GetChartStatus());

    // Pre-computed tool data (swing analysis + key levels)
    // Server hanya inject ke LLM jika AI memanggil tool tersebut
    json tool_data;
    try {
        tool_data["swing_analysis"] = json::parse(JarvisBridge_GetSwingAnalysis());
        tool_data["key_levels"]    = json::parse(JarvisBridge_GetKeyLevels());
    } catch (...) {
        // Kalau data belum ada (belum ada candle), kirim empty
    }
    req_body["tool_data"] = tool_data;

    std::string req_str = req_body.dump();

    // Send to API (via emscripten_fetch on WASM)
    jarvis_send_to_api(req_str.c_str());
}

// ============================================================
// QUICK PROMPTS (Welcome screen chips)
// ============================================================
static const char* QUICK_PROMPTS[] = {
    "Tambah BTCUSDT ke chart",
    "Pasang RSI di XAUUSD",
    "Analisa ETHUSDT",
    "Tambah SMA 50 dan EMA 200",
};
static const int NUM_QUICK_PROMPTS = 4;

// ============================================================
// RENDER — Main ImGui render function
// ============================================================
static void RenderJarvisWindow() {
    if (!g_jarvis.show_window) return;

    ImGui::SetNextWindowSize(ImVec2(480, 600), ImGuiCond_FirstUseEver);
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoScrollbar;

    if (!ImGui::Begin("Jarvis AI", &g_jarvis.show_window, flags)) {
        ImGui::End();
        return;
    }

    ImVec2 winSize = ImGui::GetContentRegionAvail();
    float inputAreaHeight = 80.0f;

    // ============================================================
    // MESSAGES AREA (scrollable child)
    // ============================================================
    ImGui::BeginChild("##Messages", ImVec2(0, -inputAreaHeight), false,
                      ImGuiWindowFlags_None);

    // Welcome screen (no messages yet)
    if (g_jarvis.messages.empty() && !g_jarvis.is_loading) {
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 40.0f);

        // Title
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.7f, 0.2f, 1.0f)); // Amber
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - 120.0f) * 0.5f);
        ImGui::Text("JARVIS AI");
        ImGui::PopStyleColor();

        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - 200.0f) * 0.5f);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
        ImGui::Text("AI Assistant untuk Chart Trading");
        ImGui::PopStyleColor();

        ImGui::Dummy(ImVec2(0, 20.0f));

        // Quick prompt chips
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - 350.0f) * 0.5f);
        float chipWidth = 165.0f;
        for (int i = 0; i < NUM_QUICK_PROMPTS; i++) {
            if (i > 0 && i % 2 == 0) {
                ImGui::SetCursorPosX((ImGui::GetWindowWidth() - 350.0f) * 0.5f);
                ImGui::Dummy(ImVec2(0, 4.0f));
            }
            ImGui::SameLine(i % 2 == 0 ? 0 : 0);
            if (i % 2 == 1) ImGui::SameLine();

            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.2f, 0.25f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.3f, 0.35f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.25f, 0.25f, 0.3f, 1.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
            if (ImGui::SmallButton(QUICK_PROMPTS[i])) {
                g_jarvis.input_buf = QUICK_PROMPTS[i];
                JarvisSendMessage();
            }
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(3);
        }
    }

    // Render messages
    float prevScrollY = ImGui::GetScrollY();
    for (int i = 0; i < (int)g_jarvis.messages.size(); i++) {
        const auto& msg = g_jarvis.messages[i];

        // Role label + timestamp
        if (msg.role == "user") {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.8f, 1.0f, 1.0f)); // Blue
            ImGui::Text("You");
            ImGui::PopStyleColor();
        } else if (msg.role == "assistant") {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.7f, 0.2f, 1.0f)); // Amber
            ImGui::Text("Jarvis");
            ImGui::PopStyleColor();
        } else {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
            ImGui::Text("System");
            ImGui::PopStyleColor();
        }

        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.4f, 0.4f, 1.0f));
        ImGui::Text("%s", msg.timestamp.c_str());
        ImGui::PopStyleColor();
        ImGui::Separator();

        // Bubble background
        ImVec4 bgCol;
        if (msg.is_error) {
            bgCol = ImVec4(0.3f, 0.1f, 0.1f, 0.8f);       // Dark red
        } else if (msg.role == "user") {
            bgCol = ImVec4(0.15f, 0.2f, 0.3f, 0.8f);       // Dark blue
        } else {
            bgCol = ImVec4(0.15f, 0.15f, 0.18f, 0.8f);      // Neutral dark
        }

        ImGui::PushStyleColor(ImGuiCol_ChildBg, bgCol);
        ImGui::BeginChild(
            ("##bubble_" + std::to_string(i)).c_str(),
            ImVec2(-1, 0), ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Borders,
            ImGuiWindowFlags_None
        );

        // Content
        if (msg.is_error) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
        } else {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.9f, 0.9f, 1.0f));
        }
        ImGui::TextWrapped("%s", msg.content.c_str());
        ImGui::PopStyleColor();

        ImGui::EndChild();
        ImGui::PopStyleColor();

        ImGui::Dummy(ImVec2(0, 8.0f));
    }

    // Loading indicator
    if (g_jarvis.is_loading) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.7f, 0.2f, 1.0f));
        float t = ImGui::GetTime();
        int dots = ((int)(t * 2.5f)) % 3 + 1;
        std::string dotsStr(dots, '.');
        ImGui::Text("Jarvis%s  thinking...", dotsStr.c_str());
        ImGui::PopStyleColor();
    }

    // Auto-scroll logic
    if (g_jarvis.scroll_to_bottom > 0.0f) {
        ImGui::SetScrollHereY(1.0f);
        g_jarvis.scroll_to_bottom = -1.0f;
    } else {
        // If user hasn't scrolled up, auto-scroll
        float maxScroll = ImGui::GetScrollMaxY();
        float curScroll = ImGui::GetScrollY();
        if (curScroll >= maxScroll - 30.0f) {
            ImGui::SetScrollHereY(1.0f);
        }
    }

    ImGui::EndChild(); // ##Messages

    // ============================================================
    // INPUT AREA
    // ============================================================
    ImGui::Separator();

    // Error message
    if (!g_jarvis.error_msg.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
        ImGui::TextWrapped("! %s", g_jarvis.error_msg.c_str());
        ImGui::PopStyleColor();
    }

    // Buttons row
    if (!g_jarvis.is_loading) {
        // Send button (right aligned)
        float btnWidth = 70.0f;
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - btnWidth);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.9f, 0.6f, 0.1f, 1.0f)); // Amber
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 0.7f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.8f, 0.5f, 0.1f, 1.0f));
        if (ImGui::Button("Send", ImVec2(btnWidth, 0)) && !g_jarvis.input_buf.empty()) {
            JarvisSendMessage();
        }
        ImGui::PopStyleColor(3);
    } else {
        // Stop/Cancel button
        float btnWidth = 70.0f;
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - btnWidth);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.15f, 0.15f, 1.0f)); // Red
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
        if (ImGui::Button("Cancel", ImVec2(btnWidth, 0))) {
            g_jarvis.is_loading = false;
        }
        ImGui::PopStyleColor(2);
    }

    // Text input (multi-line)
    char inputBuf[2048];
    strncpy(inputBuf, g_jarvis.input_buf.c_str(), sizeof(inputBuf) - 1);
    inputBuf[sizeof(inputBuf) - 1] = '\0';

    ImGuiInputTextFlags inputFlags = ImGuiInputTextFlags_AllowTabInput
                                   | ImGuiInputTextFlags_EnterReturnsTrue;

    if (g_jarvis.input_focused) {
        inputFlags |= ImGuiInputTextFlags_AutoSelectAll;
        ImGui::SetKeyboardFocusHere(-1);
        g_jarvis.input_focused = false;
    }

    bool submitted = ImGui::InputTextMultiline("##chat_input", inputBuf, sizeof(inputBuf),
                                                ImVec2(-1, 50), inputFlags);

    // Update buffer from ImGui
    g_jarvis.input_buf = inputBuf;

    // Handle Enter key (submit on Enter)
    if (submitted && !g_jarvis.input_buf.empty() && !g_jarvis.is_loading) {
        // Remove trailing newline if Enter was pressed
        if (!g_jarvis.input_buf.empty() && g_jarvis.input_buf.back() == '\n') {
            g_jarvis.input_buf.pop_back();
        }
        if (!g_jarvis.input_buf.empty()) {
            JarvisSendMessage();
        }
    }

    ImGui::End();
}

// ============================================================
// TOOLBAR BUTTON — Call this from your toolbar render function
// ============================================================
static void RenderJarvisToolbarButton() {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.15f, 0.2f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.25f, 0.35f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.2f, 0.2f, 0.3f, 1.0f));

    if (ImGui::Button("J", ImVec2(g_iconSize, g_iconSize))) {
        g_jarvis.show_window = !g_jarvis.show_window;
        if (g_jarvis.show_window) {
            g_jarvis.input_focused = true;
        }
    }

    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Jarvis AI");
    }

    ImGui::PopStyleColor(3);
}
