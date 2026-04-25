#include "GPUCandleRenderer.h"
#include "GPUCandleShaders.h"
#include "Candle.h"
#include <iostream>
#include <cstring>
#include <cstdio> 

bool GPUCandleRenderer::Init() {
    if (initialized) return true;

    // Compile Shader (Sama seperti sebelumnya)
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &INSTANCED_VS, NULL);
    glCompileShader(vs);
    
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &INSTANCED_FS, NULL);
    glCompileShader(fs);

    program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);

    glDeleteShader(vs);
    glDeleteShader(fs);

    // Setup Geometri Kotak (0..1)
    float rectVerts[] = {
        0.0f, 0.0f, 
        1.0f, 0.0f, 
        0.0f, 1.0f, 
        1.0f, 1.0f  
    };

    glGenVertexArrays(1, &vaoRect);
    glGenBuffers(1, &vboRect);
    glGenBuffers(1, &vboInstance);

    glBindVertexArray(vaoRect);

    glBindBuffer(GL_ARRAY_BUFFER, vboRect);
    glBufferData(GL_ARRAY_BUFFER, sizeof(rectVerts), rectVerts, GL_STATIC_DRAW);
    
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);

    glBindBuffer(GL_ARRAY_BUFFER, vboInstance);

    // Attribute 1: Index, Open, Close, High
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(CandleInstance), (void*)0);
    glVertexAttribDivisor(1, 1); 

    // Attribute 2: Low, Type(Color), Padding...
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(CandleInstance), (void*)(4 * sizeof(float)));
    glVertexAttribDivisor(2, 1); 

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    initialized = true;
    printf("✅ GPU Renderer (Simple Color) Initialized!\n");
    return true;
}

void GPUCandleRenderer::Shutdown() {
    if (program) glDeleteProgram(program);
    initialized = false;
}

void GPUCandleRenderer::UpdateData(const std::vector<Candle>& candles) {
    if (!initialized) return;

    // 🔥 FIX: Kalau data kosong, reset instanceCount → GPU tidak render stale data
    if (candles.empty()) {
        instanceCount = 0;
        return;
    }

    // Data sudah di-filter dari corrupt candles di main.cpp (candles_to_render guard)
    // GPU tinggal render apa adanya.
    std::vector<CandleInstance> data;
    data.reserve(candles.size());

    for (int i = 0; i < (int)candles.size(); i++) {
        const auto& c = candles[i];
        CandleInstance inst;
        inst.index = (float)i;
        inst.open = (float)c.open;
        inst.close = (float)c.close;
        inst.high = (float)c.high;
        inst.low = (float)c.low;

        if (c.close >= c.open) 
            inst.colorBits = 1.0f;
        else 
            inst.colorBits = 0.0f;

        inst.padding1 = 0;
        inst.padding2 = 0;
        data.push_back(inst);
    }

    instanceCount = (int)data.size();

    glBindBuffer(GL_ARRAY_BUFFER, vboInstance);
    if (data.size() > bufferCapacity) {
        bufferCapacity = data.size() * 1.5;
        glBufferData(GL_ARRAY_BUFFER, bufferCapacity * sizeof(CandleInstance), data.data(), GL_STREAM_DRAW);
    } else {
        glBufferSubData(GL_ARRAY_BUFFER, 0, data.size() * sizeof(CandleInstance), data.data());
    }
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void GPUCandleRenderer::Render(float xMin, float xMax, float yMin, float yMax,
                               ImVec2 plotPos, ImVec2 plotSize, ImVec2 screenSize, float candleWidth) {
    if (!initialized || instanceCount == 0) return;

    glUseProgram(program);
    
    // Matikan Depth Test agar tidak saling menimpa secara aneh di 2D
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glUniform1f(glGetUniformLocation(program, "uXMin"), xMin);
    glUniform1f(glGetUniformLocation(program, "uXMax"), xMax);
    glUniform1f(glGetUniformLocation(program, "uYMin"), yMin);
    glUniform1f(glGetUniformLocation(program, "uYMax"), yMax);
    glUniform2f(glGetUniformLocation(program, "uPlotPos"), plotPos.x, plotPos.y);
    glUniform2f(glGetUniformLocation(program, "uPlotSize"), plotSize.x, plotSize.y);
    glUniform2f(glGetUniformLocation(program, "uScreen"), screenSize.x, screenSize.y);

    // 🔥 BARU: Uniform untuk mode (0=body, 1=wick)
    GLuint locMode = glGetUniformLocation(program, "uMode");

    // 🔥 BARU: Hitung lebar wick dalam world space (agar fixed pixel, misal 1 px)
    float worldPerPixel = (xMax - xMin) / plotSize.x;
    float wickWidth = worldPerPixel * wickPixelWidth; // wickPixelWidth = 1.0f default
    if (wickWidth < worldPerPixel) wickWidth = worldPerPixel; // Minimal 1 px

    glBindVertexArray(vaoRect);

    // 🔥 BARU: Kirim Settingan User ke Shader GPU
    glUniform4f(glGetUniformLocation(program, "uColorBull"), 
                colorBull.x, colorBull.y, colorBull.z, colorBull.w);

    glUniform4f(glGetUniformLocation(program, "uColorBear"), 
                colorBear.x, colorBear.y, colorBear.z, colorBear.w);
    // 🔥 BARU: Kirim Warna Wick
    glUniform4f(glGetUniformLocation(program, "uWickColorBull"), 
                wickColorBull.x, wickColorBull.y, wickColorBull.z, wickColorBull.w);

    glUniform4f(glGetUniformLocation(program, "uWickColorBear"), 
                wickColorBear.x, wickColorBear.y, wickColorBear.z, wickColorBear.w);

    // Hollow body mode — aktif saat FP style
    glUniform1f(glGetUniformLocation(program, "uHollow"),
                hollowBody ? 1.0f : 0.0f);
    // uBorderFracX tidak dipakai lagi — shader kini hitung sendiri via dFdx/dFdy

    // 🔥 BARU: PASS 1 - RENDER WICK (DULU, AGAR BODY TIMPA NANTI)
    glUniform1f(glGetUniformLocation(program, "uCandleWidth"), wickWidth);
    glUniform1f(locMode, 1.0f); // Mode wick
    glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, instanceCount);

    // 🔥 BARU: PASS 2 - RENDER BODY (DI ATAS WICK)
    glUniform1f(glGetUniformLocation(program, "uCandleWidth"), candleWidth);
    glUniform1f(locMode, 0.0f); // Mode body
    glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, instanceCount);
    
    glBindVertexArray(0);
    glUseProgram(0);
}