// ============================================================
// AiAssistant.cpp — WASM implementation for Jarvis AI Chat
// Handles emscripten_fetch to communicate with backend API
// ============================================================

#ifdef __EMSCRIPTEN__

#include <emscripten.h>
#include <emscripten/fetch.h>
#include <string.h>
#include <stdlib.h>
#include <cstdio>

// Backend API URL — adjust this to your server
// For development: jarvis_backend runs on port 3000
// For production: your deployed URL
static const char* JARVIS_API_URL = "http://localhost:3000/api/chat";

// ============================================================
// Forward declarations — these are defined in AiAssistant.h
// with extern "C" linkage and EMSCRIPTEN_KEEPALIVE
// ============================================================
extern "C" void jarvis_on_response(const char* response_json);
extern "C" void jarvis_on_error(const char* err_text);

// ============================================================
// Fetch callbacks (these run on the main thread after fetch completes)
// ============================================================

static void on_fetch_success(emscripten_fetch_t* fetch) {
    // Copy response data (null-terminated)
    int len = fetch->numBytes;
    char* buf = (char*)malloc(len + 1);
    memcpy(buf, fetch->data, len);
    buf[len] = '\0';

    // Call the global handler defined in AiAssistant.h
    jarvis_on_response(buf);

    free(buf);
    emscripten_fetch_close(fetch);
}

static void on_fetch_error(emscripten_fetch_t* fetch) {
    // Build error message
    char errBuf[512];
    snprintf(errBuf, sizeof(errBuf), "Fetch failed: HTTP %d", fetch->status);

    jarvis_on_error(errBuf);

    emscripten_fetch_close(fetch);
}

// ============================================================
// jarvis_send_to_api — POST chat message to backend
// extern "C" required so the symbol name matches EXPORTED_FUNCTIONS
// ============================================================
extern "C" {

EMSCRIPTEN_KEEPALIVE
void jarvis_send_to_api(const char* message_json) {
    if (!message_json || strlen(message_json) == 0) {
        jarvis_on_error("Empty message");
        return;
    }

    // Create fetch attributes
    emscripten_fetch_attr_t attr;
    emscripten_fetch_attr_init(&attr);

    attr.attributes = EMSCRIPTEN_FETCH_REPLACE | EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;
    strcpy(attr.requestMethod, "POST");
    attr.requestData = message_json;
    attr.requestDataSize = strlen(message_json);

    // Set Content-Type header
    static const char* headers[] = {
        "Content-Type", "application/json",
        NULL  // null-terminated
    };
    attr.requestHeaders = headers;

    attr.onsuccess = on_fetch_success;
    attr.onerror = on_fetch_error;

    // Start the fetch
    emscripten_fetch(&attr, JARVIS_API_URL);

    printf("[Jarvis] Sending message to API...\n");
}

} // extern "C"

#endif // __EMSCRIPTEN__
