#pragma once
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

// =============================================================
// 🧭 CViewWindowManager — Mesin Pengatur Tampilan Chart (Index-based)
// =============================================================
// Fitur utama:
// - Hitung otomatis start & end index dari zoom + posisi center
// - AutoFollow cerdas (nyala hanya jika di ujung kanan)
// - Density adaptif untuk axis tick label
// - Auto zoom halus (pakai targetZoom)
// =============================================================
class CViewWindowManager {
public:
    int start = 0;
    int end = 0;
    float zoomLevel = 150.0f;
    float targetZoom = 150.0f;
    int totalCandlesVisible = 0;
    int viewCenterIndex = 0;
    bool autoFollow = true;   // Default: aktif saat pertama kali
    bool userDragged = false; // Deteksi user geser manual

    // =========================================================
    // 🔹 Update view window (dipanggil setiap frame)
    // =========================================================
    void Update(int totalCandles) {
        if (totalCandles <= 0) {
            start = 0;
            end = 100; // Default view untuk chart kosong
            return;
        }

        totalCandlesVisible = (int)zoomLevel;
        totalCandlesVisible = std::clamp(totalCandlesVisible, 20, 20000);

        // ⭐️ FIX: Hitung 'end' dan 'start' murni dari 'viewCenterIndex'
        // JANGAN clamp 'end' ke totalCandles - 1.

        int halfView = totalCandlesVisible / 2;

        // Hitung 'end' dulu, bebas tanpa clamp atas
        end = viewCenterIndex + halfView;
        
        // Hitung 'start' berdasarkan 'end'
        start = end - totalCandlesVisible;
        
        // Satu-satunya clamp yang kita butuhkan adalah agar 'start' tidak negatif.
        if (start < 0) {
            start = 0;
            end = totalCandlesVisible; // (re-kalkulasi end jika start mentok)
        }
    }

    // =========================================================
    // 🔹 Zoom handler (smooth + clamp)
    // =========================================================
    void HandleZoom(float mouseWheel) {
        if (fabs(mouseWheel) > 0.0f) {
            targetZoom -= mouseWheel * (zoomLevel * 0.1f);
            targetZoom = std::clamp(targetZoom, 5.0f, 20000.0f);
        }
        zoomLevel += (targetZoom - zoomLevel) * 0.15f;
    }

    // =========================================================
    // 🔹 Hitung density tick label berdasarkan zoom
    // =========================================================
    float GetDensity() const {
        float density = 3.0f;
        if (zoomLevel > 100) density = 10;
        if (zoomLevel > 500) density = 25;
        if (zoomLevel > 1000) density = 50;
        if (zoomLevel > 2000) density = 100;
        if (zoomLevel > 4000) density = 250;
        if (zoomLevel > 8000) density = 500;
        return density;
    }

    // =========================================================
    // 🔹 AutoFollow Live Candle (versi pintar)
    // =========================================================
    void AutoFollowLive(int totalCandles) {
        if (totalCandles <= 0) return;

        int lastCandleIndex = totalCandles - 1;
        
        // ⭐️ FIX: Ini adalah posisi 'center' yang ideal agar candle terakhir
        // ada di KANAN, tapi dengan sedikit margin (misal 5% dari lebar zoom)
        int targetCenterForLive = lastCandleIndex - (int)(zoomLevel * 0.05f);
        
        // Jika autoFollow aktif, paksa viewCenterIndex ke posisi live
        if (autoFollow) {
            viewCenterIndex = targetCenterForLive;
        }

        // ⭐️ FIX: Cek jika user sudah scroll kembali ke ujung
        // 'end' sekarang BISA lebih besar dari lastCandleIndex
        bool atRightEdge = (end >= lastCandleIndex - 5); // Toleransi 5 candle

        // Jika user scroll lagi ke ujung kanan -> aktifkan auto-follow lagi
        if (atRightEdge && !autoFollow) {
            autoFollow = true;
            printf("📍 AutoFollow: ON (reached right edge)\n");
        }
        
        // Catatan: Logika mematikan autoFollow dipindah ke HandleChartDrag
        //          saat user menggeser chart secara manual.
    }

    // =========================================================
    // 🔹 Snap ke ujung kanan (misal saat ganti TF)
    // =========================================================
    void SnapToEnd(int totalCandles) {
        if (totalCandles <= 0) return;
        viewCenterIndex = std::max(0, totalCandles - (int)(zoomLevel * 0.5f));
    }
};