static const char* INSTANCED_VS = R"(#version 300 es
precision highp float;

layout(location=0) in vec2 aPos;       
layout(location=1) in vec4 aInstData1; 
layout(location=2) in vec4 aInstData2; 

uniform float uXMin, uXMax, uYMin, uYMax;
uniform vec2 uPlotPos, uPlotSize, uScreen;
uniform float uCandleWidth;
uniform float uMode;   // 0=Body, 1=Wick
uniform float uHollow; // 0=Filled, 1=Hollow frame

// --- WARNA BODY ---
uniform vec4 uColorBull;
uniform vec4 uColorBear;

// --- WARNA WICK ---
uniform vec4 uWickColorBull;
uniform vec4 uWickColorBear;

out vec4 vColor;
out vec2 vUV;   // koordinat 0..1 di dalam rect (untuk hollow discard di FS)

void main() {
    float idx   = aInstData1.x;
    float open  = aInstData1.y;
    float close = aInstData1.z;
    float high  = aInstData1.w;
    float low   = aInstData2.x;
    float type  = aInstData2.y; // 1.0 = Bull, 0.0 = Bear
    
    // 🔥 LOGIKA PEWARNAAN CANGGIH
    if (type > 0.5) {
        // BULLISH
        if (uMode > 0.5) vColor = uWickColorBull; // Sedang render Wick? Pakai warna Wick Bull
        else             vColor = uColorBull;     // Sedang render Body? Pakai warna Body Bull
    } else {
        // BEARISH
        if (uMode > 0.5) vColor = uWickColorBear; // Sedang render Wick? Pakai warna Wick Bear
        else             vColor = uColorBear;     // Sedang render Body? Pakai warna Body Bear
    }

    float yTop, yBtm;

    if (uMode > 0.5) { // Mode Wick
        yTop = high;
        yBtm = low;
    } else { // Mode Body
        yTop = max(open, close);
        yBtm = min(open, close);
        
        // Anti-Gepeng Logic
        float pixelHeightWorld = (uYMax - uYMin) / uPlotSize.y;
        if ((yTop - yBtm) < pixelHeightWorld) {
            float center = (yTop + yBtm) * 0.5;
            yTop = center + pixelHeightWorld * 0.5; 
            yBtm = center - pixelHeightWorld * 0.5;
        }
    }

    float xWorld = idx + (aPos.x - 0.5) * uCandleWidth;
    float yWorld = mix(yBtm, yTop, aPos.y);

    // Kirim UV ke FS untuk hollow detection
    vUV = aPos;

    float nx = (xWorld - uXMin) / (uXMax - uXMin);
    float ny = (yWorld - uYMin) / (uYMax - uYMin);

    vec2 pix;
    pix.x = uPlotPos.x + nx * uPlotSize.x;
    pix.y = uPlotPos.y + (1.0 - ny) * uPlotSize.y;

    float ndcX = (pix.x / uScreen.x) * 2.0 - 1.0;
    float ndcY = (1.0 - (pix.y / uScreen.y)) * 2.0 - 1.0; 

    gl_Position = vec4(ndcX, ndcY, 0.0, 1.0);
})";
// =====================================================================
// FRAGMENT SHADER — hollow body pakai GLSL derivatives
//
// dFdx / dFdy (built-in di #version 300 es, tidak butuh extension)
// memberi tahu berapa unit UV per screen pixel.
// → border selalu TEPAT 2px di semua 4 sisi, apapun ukuran candle.
//   Candle kecil  → bX & bY besar  (frame tetap keliatan)
//   Candle besar  → bX & bY kecil  (frame tipis proporsional)
//   Semua konsisten, tidak ada sisi yang lebih tebal.
// =====================================================================
static const char* INSTANCED_FS = R"(#version 300 es
precision highp float;

in vec4 vColor;
in vec2 vUV;

uniform float uHollow; // 0=filled, 1=hollow frame
uniform float uMode;   // 0=body, 1=wick

out vec4 FragColor;

void main() {
    // Hollow mode hanya berlaku saat render body (uMode < 0.5)
    if (uHollow > 0.5 && uMode < 0.5) {
        // Hitung berapa UV unit yang setara 1 screen pixel di tiap arah
        // dFdx = laju perubahan vUV.x per pixel horizontal
        // dFdy = laju perubahan vUV.y per pixel vertikal
        float uvPerPxX = abs(dFdx(vUV.x)); // UV/px di arah X
        float uvPerPxY = abs(dFdy(vUV.y)); // UV/px di arah Y

        // Border = 2px → konversi ke UV fraction
        // Clamp agar tidak lebih dari 0.4 (candle sangat kecil tetap punya interior)
        float bX = clamp(2.0 * uvPerPxX, 0.0, 0.40);
        float bY = clamp(2.0 * uvPerPxY, 0.0, 0.40);

        // Discard interior — sisakan frame border 2px semua sisi
        if (vUV.x > bX && vUV.x < (1.0 - bX) &&
            vUV.y > bY && vUV.y < (1.0 - bY)) {
            discard;
        }
    }
    FragColor = vColor;
})";