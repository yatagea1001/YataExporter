#pragma once
// ===========================================================================
// CReplayManager.h — Realistic Tick Engine + Turbo Mode
// ===========================================================================

#include <vector>
#include <string>
#include <chrono>
#include <functional>
#include <algorithm>
#include <optional>
#include <cmath>
#include <random> 
#include "Candle.h"

struct ReplayState {
    double price = 0;
    double high = 0;
    double low = 0;
    double volume = 0;
    bool isNewCandle = false; 
};

class CReplayManager {
public:
    std::vector<Candle>* source = nullptr; 
    int currentIndex = 0;
    bool active = false;
    bool running = false;
    float speed = 5.0f; 
    // Masukkan ini di dalam class CReplayManager, bagian public:

    void Reset() {
        // 1. Matikan Replay
        running = false;
        active = false;
        
        // 2. Kembalikan Index ke Awal
        currentIndex = 0;
        
        // 3. Reset Progress Tick (Simulasi)
        tickProgress = 0.0f;
        
        // 4. Reset Data State Visual
        currentState.price = 0;
        currentState.high = 0;
        currentState.low = 0;
        currentState.volume = 0;
        currentState.isNewCandle = false;
        
        // 5. (Opsional) Hapus Link Timeframe jika ganti pair total
        // linkedTFs.clear(); 
    }

    // Sinkronisasi Multi Timeframe
    std::vector<std::pair<std::vector<Candle>*, int*>> linkedTFs;

    std::function<void(int)> OnCandleChange = nullptr;
    std::function<void()> OnReplayStart = nullptr;
    std::function<void()> OnReplayEnd = nullptr;

    ReplayState currentState;

    // 🔥 Dipublish agar ReplayDrawParams di OrderFlowRenderer bisa akses
    // Nilai 0.0→1.0 = progress animasi tick dalam satu candle
    float tickProgress = 0.0f;

private:
    std::chrono::steady_clock::time_point lastUpdate;

    double Lerp(double a, double b, float t) {
        return a + (b - a) * t;
    }

public:
    CReplayManager() = default;

  // Update fungsi Init agar 'active' jadi true saat replay dimulai
    void Init(std::vector<Candle>* src, float initSpeed = 1.0f) {
        source = src;
        speed = initSpeed;
        
        // 🔥 Set active jadi TRUE saat inisialisasi
        active = true; 
        running = false; // Mulai dalam keadaan Pause

        currentIndex = (src && !src->empty()) ? (int)src->size() - 1 : 0;
        if (source && !source->empty()) SyncToCurrentIndex();
    }
    // Tambahkan fungsi untuk mematikan mode replay (Exit Replay)
    void StopReplayMode() {
        active = false;
        running = false;
        source = nullptr;
    }
    void LinkTF(std::vector<Candle>* tfVec, int* idxPtr) {
        linkedTFs.push_back({tfVec, idxPtr});
    }

    void Toggle() {
        running = !running;
        lastUpdate = std::chrono::steady_clock::now();
        if (running && OnReplayStart) OnReplayStart();
    }

    void Pause() { running = false; }

    void SetIndex(int idx) {
        if (!source) return;
        currentIndex = std::clamp(idx, 0, (int)source->size() - 1);
        SyncToCurrentIndex();
        EmitCandleChange();
    }

   // ============================================================
    // ⏭ TOMBOL NEXT: "SMART JUMP" (Sesuai Timeframe)
    // ============================================================
    // Default steps = 1 (untuk M1), tapi bisa diisi 5, 15, 60, dst.
    void StepForward(int steps = 1) {
        if (!source) return;
        
        // Target index baru
        int targetIndex = currentIndex + steps;

        // Validasi: Jangan sampai bablas melebihi data terakhir
        if (targetIndex < (int)source->size()) {
            currentIndex = targetIndex; // Langsung loncat ke index target
            tickProgress = 0.0f;        // Reset animasi
            SyncToCurrentIndex();       // Update harga & visual
            EmitCandleChange();         // Kabari chart
        } else {
            // Kalau target melebihi data yang ada, mentok di akhir & stop
            currentIndex = (int)source->size() - 1;
            SyncToCurrentIndex();
            EmitCandleChange();
            
            running = false;
            if (OnReplayEnd) OnReplayEnd();
        }
    }

    // ============================================================
    // ⏮ TOMBOL PREV: MUNDUR 1 CANDLE (Opsional)
    // ============================================================
    void StepBackward() {
        if (currentIndex > 0) {
            currentIndex--;
            tickProgress = 0.0f;
            SyncToCurrentIndex();
            EmitCandleChange();
        }
    }

    // ============================================================
    // ▶ TOMBOL PLAY: ANIMASI "BREATHING" TICK
    // ============================================================
    void Update(bool cutoffBlocking) {
        if (!source || source->empty()) return;
        if (cutoffBlocking || !running) return;

        auto now = std::chrono::steady_clock::now();
        float dt = std::chrono::duration<float>(now - lastUpdate).count();
        lastUpdate = now;

        // --- MODE 1: TURBO / WARP SPEED (Kalau speed digeser mentok kanan) ---
        if (speed > 20.0f) {
            float candlesToMove = speed * dt;
            int steps = (int)candlesToMove;
            if (steps < 1) steps = 1; 
            if (steps > 500) steps = 500; // Safety limit

            for(int i = 0; i < steps; i++) {
                if (currentIndex >= (int)source->size() - 1) {
                    running = false;
                    if (OnReplayEnd) OnReplayEnd();
                    break;
                }
                currentIndex++;
                if (OnCandleChange) OnCandleChange(currentIndex); 
            }
            SyncToCurrentIndex(); 
        }
        
        // --- MODE 2: REALISTIC ANIMATION (Play Biasa) ---
        else {
            // tickProgress naik pelan-pelan (0.0 -> 1.0)
            tickProgress += dt * (speed * 0.2f); 
            
            // Kalau animasi sudah 100% (1.0), berarti candle ini selesai
            if (tickProgress >= 1.0f) {
                StepForward(); // Pindah ke candle berikutnya
            } else {
                UpdateRealisticTick(); // Jalankan animasi naik-turun
            }
        }
    }

private:
    // Logika animasi naik-turun dalam 1 candle
    void UpdateRealisticTick() {
        const Candle& c = source->at(currentIndex);
        double O = c.open, H = c.high, L = c.low, C = c.close;
        double targetPrice = O;
        float t = tickProgress;

        // Pola pergerakan harga buatan (0% -> 30% -> 70% -> 100%)
        if (C >= O) { // Candle Bullish
            if (t < 0.3f) targetPrice = Lerp(O, L, t / 0.3f);            // Turun ke Low dulu
            else if (t < 0.7f) targetPrice = Lerp(L, H, (t - 0.3f) / 0.4f); // Naik ke High
            else targetPrice = Lerp(H, C, (t - 0.7f) / 0.3f);            // Turun dikit ke Close
        } else {      // Candle Bearish
            if (t < 0.3f) targetPrice = Lerp(O, H, t / 0.3f);            // Naik ke High dulu
            else if (t < 0.7f) targetPrice = Lerp(H, L, (t - 0.3f) / 0.4f); // Turun ke Low
            else targetPrice = Lerp(L, C, (t - 0.7f) / 0.3f);            // Naik dikit ke Close
        }

        // Tambah sedikit getaran (noise) biar gak kaku
        double noise = (H - L) * 0.05 * std::sin(t * 20.0f); 
        currentState.price = std::clamp(targetPrice + noise, L, H);

        // Update High/Low memory untuk visualisasi wick yang sedang tumbuh
        if (tickProgress < 0.05f) {
            currentState.high = std::max(O, currentState.price);
            currentState.low = std::min(O, currentState.price);
        } else {
            currentState.high = std::max(currentState.high, currentState.price);
            currentState.low = std::min(currentState.low, currentState.price);
        }
    }

    void SyncToCurrentIndex() {
        if (!source || source->empty()) return;
        const Candle& c = source->at(currentIndex);
        // Reset state visual ke OPEN candle
        currentState.price = c.open;
        currentState.high = c.open;
        currentState.low = c.open;
        currentState.volume = c.volume;
        currentState.isNewCandle = true;
        SyncLinkedTFs();
    }

    void EmitCandleChange() {
        if (OnCandleChange) OnCandleChange(currentIndex);
    }

    // Logic sinkronisasi multi timeframe (M5, H1 ikut gerak)
    void SyncLinkedTFs() {
        if (!source || source->empty()) return;
        double currentTime = source->at(currentIndex).time;
        
        for (auto& p : linkedTFs) {
            std::vector<Candle>* vec = p.first;
            int* idxPtr = p.second;
            if (!vec || vec->empty() || !idxPtr) continue;
            
            // Binary search cari candle yang pas waktunya
            auto it = std::lower_bound(vec->begin(), vec->end(), currentTime, 
                 [](const Candle& c, double t){ return c.time < t; });
            
            int idx = 0;
            if (it != vec->end()) {
                if (it->time > currentTime && it != vec->begin()) it--; 
                idx = (int)std::distance(vec->begin(), it);
            } else {
                idx = (int)vec->size() - 1;
            }
            *idxPtr = std::clamp(idx, 0, (int)vec->size() - 1);
        }
    }
};