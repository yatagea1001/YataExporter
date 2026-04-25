#pragma once
#include <vector>
#include "imgui.h"
#include "implot.h"

// Deteksi Environment
#ifdef __EMSCRIPTEN__
    #include <GLES3/gl3.h>
#else
    #include <glad/glad.h>
#endif

// Forward Declaration
struct Candle;

// Struktur Data Kompak untuk GPU (Hardware Instancing)
// Total: 8 float per candle = 32 bytes (Sangat Ringan!)
struct CandleInstance {
    float index;  // Lokasi X
    float open;   
    float close;
    float high;
    float low;
    float colorBits; // Warna dipacking jadi 1 float
    float padding1;  // Padding agar align 16-byte (opsional tapi bagus buat GPU)
    float padding2;
};

class GPUCandleRenderer {
public:
    bool Init();
    void Shutdown();

    // Fungsi Update baru: Lebih cepat, cuma salin data mentah
    void UpdateData(const std::vector<Candle>& candles);

    // Render Function
    void Render(float xMin, float xMax, float yMin, float yMax,
                ImVec2 plotPos, ImVec2 plotSize, ImVec2 screenSize, float candleWidth);

   //🔥 BARU: Setting Visual (Default Value)
    // Warna default: Hijau & Merah standar
    ImVec4 colorBull = ImVec4(0.0f, 1.0f, 0.0f, 1.0f); 
    ImVec4 colorBear = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
   // 🔥 BARU: Warna Wick 
    ImVec4 wickColorBull = ImVec4(0.0f, 1.0f, 0.0f, 1.0f); 
    ImVec4 wickColorBear = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
    
    float wickPixelWidth = 1.0f;

    // Hollow body mode — aktif saat FP style untuk lihat footprint lebih jelas
    // Body jadi frame kotak bolong, wick tetap solid line
    bool hollowBody = false;

    // 🔥 FIX: Clear GPU instances saat switch symbol
    // Reset bufferCapacity juga → force glBufferData (full realloc) pada UpdateData berikutnya.
    // Tanpa ini, glBufferSubData hanya overwrite sebagian — sisa data stale masih ada di GPU.
    void ClearInstances() { instanceCount = 0; bufferCapacity = 0; }
    int  GetInstanceCount() const { return instanceCount; }

private:
    bool initialized = false;
    
    GLuint program = 0;
    
    // Geometry Buffer (Kotak 1x1 statis)
    GLuint vaoRect = 0;
    GLuint vboRect = 0;

    // Instance Buffer (Data Candle berubah-ubah)
    GLuint vboInstance = 0; 
    
    int instanceCount = 0;
    
    // Cache kapasitas buffer biar gak realloc terus
    size_t bufferCapacity = 0; 
};