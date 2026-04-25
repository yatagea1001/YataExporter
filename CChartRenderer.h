#pragma once

#include "imgui.h"
#include "implot.h"
#include <vector>
#include "Candle.h"

// Untuk Desktop pakai GLAD, untuk WEB otomatis pakai WebGL2 loader Emscripten
#ifndef __EMSCRIPTEN__
#include <glad/glad.h>
#endif


#include <string>
#include <iostream>

// ======================================================================
// ⭐ WEB+DESKTOP COMPATIBLE: CChartRenderer ⭐
// ======================================================================
class CChartRenderer {
private:
    GLuint fbo = 0;
    GLuint texture = 0;
    GLuint shaderProgram = 0;
    GLuint vao = 0, vbo = 0;

    int textureWidth  = 2048;
    int textureHeight = 1080;

    std::vector<float> vboData;
    size_t lastCandleCount = 0;

    // ==========================================================
    // GLSL ES SHADER untuk WEB + DESKTOP
    // (Desktop menerima #version 300 es)
    // ==========================================================
    const char* vertexShaderSource = R"(
        #version 300 es
        precision mediump float;

        layout (location = 0) in vec2 aPos;
        layout (location = 1) in vec4 aColor;

        uniform mat4 projection;
        out vec4 FragColor_VS;

        void main() {
            gl_Position = projection * vec4(aPos.xy, 0.0, 1.0);
            FragColor_VS = aColor;
        }
    )";

    const char* fragmentShaderSource = R"(
        #version 300 es
        precision mediump float;

        in vec4 FragColor_VS;
        out vec4 FragColor;

        void main() {
            FragColor = FragColor_VS;
        }
    )";

    // ----------------------------------------------------------------------
    GLuint CompileShader(GLenum type, const char* src) {
        GLuint s = glCreateShader(type);
        glShaderSource(s, 1, &src, NULL);
        glCompileShader(s);
        GLint ok;
        glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
        if (!ok) {
            char buf[512];
            glGetShaderInfoLog(s, 512, NULL, buf);
            printf("Shader error: %s\n", buf);
        }
        return s;
    }

    GLuint CreateShaderProgram() {
        GLuint vs = CompileShader(GL_VERTEX_SHADER, vertexShaderSource);
        GLuint fs = CompileShader(GL_FRAGMENT_SHADER, fragmentShaderSource);

        GLuint prog = glCreateProgram();
        glAttachShader(prog, vs);
        glAttachShader(prog, fs);
        glLinkProgram(prog);

        GLint ok;
        glGetProgramiv(prog, GL_LINK_STATUS, &ok);
        if (!ok) {
            char buf[512];
            glGetProgramInfoLog(prog, 512, NULL, buf);
            printf("Link error: %s\n", buf);
        }

        glDeleteShader(vs);
        glDeleteShader(fs);
        return prog;
    }

    // ----------------------------------------------------------------------
    void SetOrtho(GLuint program, float left, float right, float bottom, float top) {
        float n = -1.0f, f = 1.0f;
        float m[4][4] = {
            {  2.f/(right-left), 0, 0, 0 },
            { 0, 2.f/(top-bottom), 0, 0 },
            { 0, 0, -2.f/(f-n), 0 },
            { -(right+left)/(right-left),
              -(top+bottom)/(top-bottom),
              -(f+n)/(f-n),
              1 }
        };

        glUseProgram(program);
        GLint loc = glGetUniformLocation(program, "projection");
        glUniformMatrix4fv(loc, 1, GL_FALSE, &m[0][0]);
    }

public:
    // ======================================================================
    CChartRenderer() = default;

    void Init(int w, int h) {
        textureWidth  = w;
        textureHeight = h;

        shaderProgram = CreateShaderProgram();

        // FBO
        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);

        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, textureWidth, textureHeight,
                     0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                               GL_TEXTURE_2D, texture, 0);

        // VAO/VBO
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);

        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);

        glVertexAttribPointer(
            0, 2, GL_FLOAT, GL_FALSE,
            6*sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);

        glVertexAttribPointer(
            1, 4, GL_FLOAT, GL_FALSE,
            6*sizeof(float), (void*)(2*sizeof(float)));
        glEnableVertexAttribArray(1);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void Shutdown() {
        glDeleteFramebuffers(1, &fbo);
        glDeleteTextures(1, &texture);
        glDeleteProgram(shaderProgram);
        glDeleteBuffers(1, &vbo);
        glDeleteVertexArrays(1, &vao);
    }

    // ======================================================================
    void RenderCandlesToTexture(
        const std::vector<Candle>& candles,
        double x0, double x1,
        double y0, double y1,
        float zoom
    ) {
        if (candles.empty()) return;

        vboData.clear();

        int start_idx = std::max(0, (int)x0 - 10);
        int end_idx   = std::min((int)candles.size(), (int)x1 + 10);

        double ppc = (double)textureWidth / (x1 - x0);
        int step = 1;
        if (ppc < 0.5) step = 15;
        else if (ppc < 2.0) step = 5;

        float zoom_pct = 1.0f - std::clamp((zoom-5.f)/(1000.f-5.f),0.f,1.f);
        float width_pct = 0.1f + zoom_pct*(0.8f-0.1f);

        vboData.reserve((end_idx-start_idx)/step * 4 * 6);

        for (int i = start_idx; i < end_idx; i += step) {
            int end = std::min(i+step, end_idx);
            if (end <= i) continue;

            const Candle& a = candles[i];
            const Candle& b = candles[end-1];

            double o = a.open;
            double c = b.close;
            double H = a.high;
            double L = a.low;

            for (int j=i; j<end; j++) {
                H = std::max(H, candles[j].high);
                L = std::min(L, candles[j].low);
            }

            bool bull = (c >= o);
            float r = bull ? 0.1f : 0.9f;
            float g = bull ? 0.85f : 0.2f;
            float b = bull ? 0.2f  : 0.2f;

            double x = (double)i;

            // wick
            vboData.insert(vboData.end(), {
                (float)x, (float)H, r,g,b,1.0f,
                (float)x, (float)L, r,g,b,1.0f
            });

            double left = x - width_pct * (step>1?step*0.5:0.5);
            double right= x + width_pct * (step>1?step*0.5:0.5);

            float top = std::max(o,c);
            float bot = std::min(o,c);

            vboData.insert(vboData.end(), {
                (float)left,  top, r,g,b,1.0f,
                (float)right, bot, r,g,b,1.0f
            });
        }

        // ==================================================
        // RENDER ke TEXTURE
        // ==================================================
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glViewport(0,0,textureWidth,textureHeight);
        glClearColor(0,0,0,0);
        glClear(GL_COLOR_BUFFER_BIT);

        SetOrtho(shaderProgram, (float)x0,(float)x1,(float)y0,(float)y1);

        glBindVertexArray(vao);

        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER,
                     vboData.size()*sizeof(float),
                     vboData.data(),
                     GL_STREAM_DRAW);

        glDrawArrays(GL_LINES, 0, (GLsizei)(vboData.size()/6));

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    ImTextureID GetTextureID() {
        return (void*)(intptr_t)texture;
    }
};
