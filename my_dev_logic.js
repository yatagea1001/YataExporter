// =========================================================
// my_dev_logic.js - OTAK TAMBAHAN (SUNTIKAN TANPA COMPILE)
// =========================================================

console.log("💉 Meloading Logika Suntikan Developer...");

// Kita "Hook" (cegat) saat mesin C++ sudah siap jalan
// Teknik ini aman, tidak menimpa loading screen yang ada di HTML
var originalOnInit = Module.onRuntimeInitialized;

Module.onRuntimeInitialized = function() {
    // 1. Jalankan fungsi asli dulu (Loading screen hilang, dll)
    if (originalOnInit) originalOnInit();

    console.log("🔥 Mesin C++ Siap! Mengecek Mode Dewa...");

    // 2. CEK URL: Apakah ada ?mode=dewa ?
    const urlParams = new URLSearchParams(window.location.search);
    const mode = urlParams.get('mode');

    if (mode === 'dewa') {
        console.log("🔓 MODE DEWA AKTIF! Membuka Panel Creator...");

        // A. PANGGIL FUNGSI C++ UNTUK BUKA PANEL
        // Pastikan fungsi ToggleCreatorMode sudah di-export di main.cpp
        if (Module._ToggleCreatorMode) {
            Module._ToggleCreatorMode(true);
        } else {
            console.error("⚠️ Fungsi _ToggleCreatorMode tidak ditemukan di WASM!");
        }

        // B. CONTOH SUNTIK TOMBOL BARU DARI JS
        // Tombol ini akan muncul di tab "Live Features" di panel creator
        if (Module._InjectFeature) {
            
            Module._InjectFeature(
                "Tes Tombol JS",       // Nama
                "JS Injection",        // Kategori
                "alert('Halo! Ini logika dari file JS tanpa compile!');" // Aksi
            );

            Module._InjectFeature(
                "Ubah Background", 
                "UI Tweaks", 
                "document.body.style.backgroundColor = '#222';"
            );
        }

    } else {
        console.log("🔒 Mode User Biasa (Panel Creator Disembunyikan)");
        // Pastikan panel tertutup buat user biasa
        if (Module._ToggleCreatorMode) {
            Module._ToggleCreatorMode(false);
        }
    }
};