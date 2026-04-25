// =================================================================================
// TextureHelper.h  fungsi untuk load texture ikon dari file dan deklarasi variabel global ImTextureID
// =================================================================================
#pragma once
#include <GLES2/gl2.h> // WebGL/OpenGL ES 2.0
#include "imgui.h"
#include <cstdio> // Untuk printf
#include "stb_image.h"

// =========================================================
// 1. DEKLARASI GLOBAL VARIABLE (EXTERN)
// =========================================================
// Variabel ini dideklarasikan di sini agar bisa diakses oleh ShapeEditUI.h
// Note: Jangan lupa definisikan variabel ini di main.cpp tanpa kata 'extern'!

// A. Ikon Toolbar Utama
extern ImTextureID texCursor;
extern ImTextureID texLine;
extern ImTextureID texFib;
extern ImTextureID texRect;
extern ImTextureID texBrush;
extern ImTextureID texText;
extern ImTextureID texElliot;
extern ImTextureID texTrash;
extern ImTextureID texOrderFlow;
// B. Ikon Popup Edit (NEW)
extern ImTextureID texPopupCopy;
extern ImTextureID texPopupColor;
extern ImTextureID texPopupThick;
extern ImTextureID texPopupLock;
extern ImTextureID texTrash1; // Khusus tombol hapus di popup
extern ImTextureID texIconIndicator; // <--- Tambah ini
extern ImTextureID texIconSymbol;    // <--- Tambah ini
extern ImTextureID texMaximize;
extern ImTextureID texAddChart;

// B. Ikon Popup Edit (NEW)
extern ImTextureID texPopupCopy1;
extern ImTextureID texPopupColor1;
extern ImTextureID texPopupThick1;
extern ImTextureID texPopupLock1;
extern ImTextureID texPopupSetting;
extern ImTextureID texTrash2; // Khusus tombol hapus di popup
extern ImTextureID texIconGold;  // XAUUSD
extern ImTextureID texIconEuro;  // EURUSD
extern ImTextureID texIconPound; // GBPUSD
extern ImTextureID texIconBTC;   // BTCUSD
extern ImTextureID texIconETH;   // ETHUSD

// Indicator Overlay Icons
extern ImTextureID texEyeShow;    // 👁 eye visible
extern ImTextureID texEyeHide;    // 👁 eye hidden  
extern ImTextureID texIndSettings; // ⚙ indicator settings

// 🔥 Right Bar Icons (buatan sendiri)
extern ImTextureID texTreeObj;     // icon show/hide UI Object Tree
extern ImTextureID texMarketWatch; // icon show/hide Market Watch
extern ImTextureID texReplayBtn;   // icon replay button

// =========================================================
// 2. FUNGSI LOAD TEXTURE
// =========================================================
static inline ImTextureID LoadTextureFromFile(const char* filename) {
    int width, height, channels;
    
    // Force 4 channels (RGBA) supaya transparansi aman
    unsigned char* data = stbi_load(filename, &width, &height, &channels, 4);
    
    if (data == NULL) {
        printf("[ERROR] GAGAL LOAD GAMBAR: %s (Cek folder assets apakah file ada?)\n", filename);
        return 0; // Return 0 (Hitam/Kosong) jika gagal
    }

    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    // Setting Filter: Linear biar halus, Clamp biar pinggiran gak bocor
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Upload Data ke GPU
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    
    // Bersihkan RAM (karena sudah masuk VRAM GPU)
    stbi_image_free(data);
    
    return (ImTextureID)(intptr_t)texture;
}

// =========================================================
// 3. FUNGSI INISIALISASI (PANGGIL 1X DI MAIN)
// =========================================================
static inline void InitIcons() {
    printf("[INFO] Memuat Texture Icons...\n");

    // A. Load Ikon Toolbar Utama
    texCursor = LoadTextureFromFile("assets/cursor.png");
    texLine   = LoadTextureFromFile("assets/line.png");
    texFib    = LoadTextureFromFile("assets/fib.png");
    texRect   = LoadTextureFromFile("assets/rect.png");
    texBrush  = LoadTextureFromFile("assets/brush.png");
    texText   = LoadTextureFromFile("assets/text.png");
    texElliot = LoadTextureFromFile("assets/elliot.png");
    texTrash  = LoadTextureFromFile("assets/trash.png");
   
    // B. Load Ikon Popup Edit Shape (Sesuai nama file di folder assets)
    texPopupCopy  = LoadTextureFromFile("assets/copy.png");
    texPopupColor = LoadTextureFromFile("assets/color.png");
    texPopupThick = LoadTextureFromFile("assets/thick.png");
    texPopupLock  = LoadTextureFromFile("assets/lock.png");
    texTrash1     = LoadTextureFromFile("assets/trash1.png"); 

    texPopupCopy1  = LoadTextureFromFile("assets/copy1.png");  // Pakai gambar copy yang sama
    texPopupColor1 = LoadTextureFromFile("assets/color1.png"); // Pakai gambar color yang sama
    texPopupThick1 = LoadTextureFromFile("assets/thick1.png"); 
    texPopupLock1  = LoadTextureFromFile("assets/lock1.png");
    texPopupSetting = LoadTextureFromFile("assets/setting.png");
    texTrash2      = LoadTextureFromFile("assets/trash2.png"); // Atau trash2.png jika ada gambar beda
    // --- TAMBAHAN BARU ---
    texIconIndicator = LoadTextureFromFile("assets/indicator.png");
    texIconSymbol    = LoadTextureFromFile("assets/simbol.png");
    // 🔥 LOAD GAMBAR MARKET (PASTIKAN FILE ADA DI FOLDER ASSETS)
    texIconGold  = LoadTextureFromFile("assets/gold.png");
    texIconEuro  = LoadTextureFromFile("assets/euro.png");
    texIconPound = LoadTextureFromFile("assets/pound.png");
    texIconBTC   = LoadTextureFromFile("assets/btc.png");
    texIconETH   = LoadTextureFromFile("assets/eth.png");
   
    
    // Indicator overlay icons
    texEyeShow     = LoadTextureFromFile("assets/eye_show.png");
    texEyeHide     = LoadTextureFromFile("assets/eye_hide.png");
    texIndSettings = LoadTextureFromFile("assets/setting.png"); // reuse setting.png
    texMaximize    = LoadTextureFromFile("assets/maximize.png");
    texAddChart    = LoadTextureFromFile("assets/add_chart.png");
    texOrderFlow   = LoadTextureFromFile("assets/orderflow.png"); // Ikon Order Flow baru

    // 🔥 Right Bar Icons (buatan sendiri)
    texTreeObj     = LoadTextureFromFile("assets/tree.png");
    texMarketWatch = LoadTextureFromFile("assets/marketwatch.png");
    texReplayBtn   = LoadTextureFromFile("assets/replay.png");
    printf("[INFO] Icon TV berhasil dimuat!\n");
}