// =============================================================================
// data_check.js — IDB Data Availability Checker + Download Popup
// =============================================================================
//
// FLOW:
//   C++ (EM_ASM) → window.checkDataAvailable(symbol, timestamp)
//                           ↓
//               Cek IDB: ada candles symbol X sekitar waktu Y?
//                           ↓
//          TIDAK ADA → showDataMissingPopup(symbol, date)
//          ADA       → return true (C++ lanjut normal)
//
// CARA PAKAI DI C++ (main.cpp):
//   EM_ASM({
//       var sym  = UTF8ToString($0);
//       var ts   = $1;  // epoch seconds
//       window.checkDataAvailable(sym, ts);
//   }, symbol.c_str(), (int)timestamp);
//
// =============================================================================

(function() {

// ── Konstanta IDB (sama dengan websocket.js) ──────────────────────────────
const DB_NAME = 'TradingAppDB';
const DB_VER  = 2;
const STORE   = 'multi_candles';

// ── Toleransi waktu: candle dianggap "ada" kalau dalam ±1 jam dari target ─
const TIME_TOLERANCE_SEC = 3600; // 1 jam

// =============================================================================
// CORE: Cek apakah symbol + waktu tersedia di IDB
// =============================================================================
async function idbHasDataAround(symbol, targetTimeSec) {
    return new Promise((resolve) => {
        const req = indexedDB.open(DB_NAME, DB_VER);
        req.onerror = () => resolve(false);
        req.onsuccess = (e) => {
            const db = e.target.result;
            if (!db.objectStoreNames.contains(STORE)) { resolve(false); return; }

            const tx    = db.transaction([STORE], 'readonly');
            const store = tx.objectStore(STORE);

            // Query: cari candle dalam range [target-tolerance, target+tolerance]
            const lo    = targetTimeSec - TIME_TOLERANCE_SEC;
            const hi    = targetTimeSec + TIME_TOLERANCE_SEC;
            const range = IDBKeyRange.bound([symbol, lo], [symbol, hi]);
            const r     = store.getAll(range);

            r.onsuccess = () => {
                resolve(r.result && r.result.length > 0);
            };
            r.onerror = () => resolve(false);
        };
    });
}

// Cek berapa banyak candles total untuk symbol ini (buat info popup)
async function idbCountForSymbol(symbol) {
    return new Promise((resolve) => {
        const req = indexedDB.open(DB_NAME, DB_VER);
        req.onerror = () => resolve(0);
        req.onsuccess = (e) => {
            const db = e.target.result;
            if (!db.objectStoreNames.contains(STORE)) { resolve(0); return; }
            const tx    = db.transaction([STORE], 'readonly');
            const store = tx.objectStore(STORE);
            const index = store.index('symbol_idx');
            const r     = index.count(IDBKeyRange.only(symbol));
            r.onsuccess = () => resolve(r.result || 0);
            r.onerror   = () => resolve(0);
        };
    });
}

// =============================================================================
// INTERNAL: Batalkan replay mode + reload live
// Dipanggil otomatis saat popup data missing muncul
// =============================================================================
function cancelReplayAndGoLive() {
    // 1. Reset C++ replay flags via exported function (kalau ada)
    //    wasm_cancel_replay() adalah fungsi C++ baru — lihat PATCH_main_cpp
    //    Kalau belum ada, fallback ke wasm_set_replay_mode(0)
    if (Module && Module._wasm_cancel_replay) {
        Module._wasm_cancel_replay();
    } else if (Module && Module._wasm_set_replay_mode) {
        Module._wasm_set_replay_mode(0);
    }

    // 2. JS side: clear WASM → rebuild IDB → reload FP → gate open
    if (typeof window.reloadLiveAfterReplay === 'function') {
        window.reloadLiveAfterReplay();
        console.log('%c[DATA_CHECK] Replay dibatalkan → reload live', 'color:#00AAFF;font-weight:bold');
    }
}

// =============================================================================
// UI: Popup "Data Tidak Tersedia"
// =============================================================================
function showDataMissingPopup(symbol, targetTimeSec, candleCount) {
    // Hapus popup lama kalau masih ada
    const old = document.getElementById('yata-data-missing-popup');
    if (old) old.remove();

    // 🔥 Batalkan replay + reload live SEGERA saat popup muncul
    // Tidak perlu tunggu user klik tombol — replay langsung dibatalkan
    cancelReplayAndGoLive();

    // Format tanggal/waktu target
    const dt     = new Date(targetTimeSec * 1000);
    const dateStr = dt.toLocaleDateString('id-ID', { day:'2-digit', month:'short', year:'numeric' });
    const timeStr = dt.toLocaleTimeString('id-ID', { hour:'2-digit', minute:'2-digit' });

    // Buat overlay + popup
    const overlay = document.createElement('div');
    overlay.id    = 'yata-data-missing-popup';
    overlay.style.cssText = `
        position: fixed; inset: 0;
        background: rgba(0,0,0,0.65);
        z-index: 99999;
        display: flex; align-items: center; justify-content: center;
        font-family: 'Segoe UI', sans-serif;
        animation: fadeInOverlay 0.2s ease;
    `;

    const box = document.createElement('div');
    box.style.cssText = `
        background: #111820;
        border: 1px solid #2a3a5a;
        border-radius: 12px;
        padding: 28px 32px;
        width: 380px;
        max-width: 92vw;
        box-shadow: 0 8px 40px rgba(0,0,0,0.7);
        animation: slideUp 0.25s cubic-bezier(0.34,1.56,0.64,1);
        position: relative;
    `;

    // ── Ikon warning ──
    const icon = document.createElement('div');
    icon.style.cssText = `
        width: 48px; height: 48px;
        background: rgba(239,83,80,0.15);
        border: 2px solid #ef5350;
        border-radius: 50%;
        display: flex; align-items: center; justify-content: center;
        margin: 0 auto 16px;
        font-size: 22px;
    `;
    icon.textContent = '⚠';
    icon.style.color = '#ef5350';

    // ── Judul ──
    const title = document.createElement('div');
    title.style.cssText = `
        color: #c8d6e5; font-size: 15px; font-weight: 600;
        text-align: center; margin-bottom: 8px; letter-spacing: 0.5px;
    `;
    title.textContent = 'Data Tidak Tersedia';

    // ── Info simbol + waktu ──
    const info = document.createElement('div');
    info.style.cssText = `
        background: rgba(58,123,213,0.1);
        border: 1px solid rgba(58,123,213,0.3);
        border-radius: 8px;
        padding: 12px 16px;
        margin: 12px 0 16px;
        text-align: center;
    `;
    info.innerHTML = `
        <div style="color:#3a7bd5;font-size:18px;font-weight:700;letter-spacing:1px">${symbol}</div>
        <div style="color:#8fa8c8;font-size:13px;margin-top:4px">${dateStr} &nbsp;·&nbsp; ${timeStr}</div>
    `;

    // ── Pesan detail ──
    const msg = document.createElement('div');
    msg.style.cssText = `color: #6a8aaa; font-size: 12.5px; text-align: center; line-height: 1.6; margin-bottom: 20px;`;

    if (candleCount > 0) {
        // Ada data tapi bukan di rentang waktu itu
        const countStr = candleCount >= 1000
            ? `${(candleCount/1000).toFixed(1)}K`
            : `${candleCount}`;
        msg.innerHTML = `IDB punya <b style="color:#c8d6e5">${countStr} candles</b> untuk ${symbol},<br>
            tapi tidak ada data di rentang waktu ini.<br>
            <span style="color:#4a6a8a;font-size:11px">Coba download data untuk periode tersebut.</span>`;
    } else {
        // Sama sekali tidak ada
        msg.innerHTML = `Tidak ada data <b style="color:#c8d6e5">${symbol}</b> di IndexedDB.<br>
            Silakan download data terlebih dahulu.<br>
            <span style="color:#4a6a8a;font-size:11px">Data dibutuhkan untuk mode Replay & Analisis.</span>`;
    }

    // ── Tombol Download ──
    const btnDownload = document.createElement('button');
    btnDownload.style.cssText = `
        width: 100%; padding: 11px;
        background: #3a7bd5; color: white;
        border: none; border-radius: 7px;
        font-size: 13px; font-weight: 600;
        cursor: pointer; letter-spacing: 0.5px;
        transition: background 0.15s;
        margin-bottom: 8px;
    `;
    btnDownload.textContent = '⬇  Download Data ' + symbol;
    btnDownload.onmouseenter = () => btnDownload.style.background = '#4a8be5';
    btnDownload.onmouseleave = () => btnDownload.style.background = '#3a7bd5';
    btnDownload.onclick = () => {
        overlay.remove();
        // Trigger download — panggil fungsi yang sudah ada di websocket.js
        // requestHistoryDownload adalah fungsi yang bisa kamu sesuaikan
        if (typeof window.requestHistoryDownload === 'function') {
            window.requestHistoryDownload(symbol, targetTimeSec);
        } else if (typeof wsSend === 'function') {
            // Fallback: request langsung ke server
            wsSend({
                type: 'request_candles',
                symbol: symbol,
                before_time: targetTimeSec + 86400, // minta 1 hari setelah target
                limit: 1500
            });
            console.log(`[DATA_CHECK] Download requested: ${symbol} around ${dateStr}`);
        } else {
            alert(`Silakan download data ${symbol} dari server.`);
        }
    };

    // ── Tombol Tutup ──
    const btnClose = document.createElement('button');
    btnClose.style.cssText = `
        width: 100%; padding: 9px;
        background: transparent; color: #6a8aaa;
        border: 1px solid #2a3a5a; border-radius: 7px;
        font-size: 12px; cursor: pointer;
        transition: all 0.15s;
    `;
    btnClose.textContent = 'Tutup';
    btnClose.onmouseenter = () => { btnClose.style.background = '#1a2535'; btnClose.style.color = '#c8d6e5'; };
    btnClose.onmouseleave = () => { btnClose.style.background = 'transparent'; btnClose.style.color = '#6a8aaa'; };
    btnClose.onclick = () => overlay.remove();

    // ── Tombol X di pojok ──
    const btnX = document.createElement('button');
    btnX.style.cssText = `
        position: absolute; top: 12px; right: 14px;
        background: none; border: none; color: #4a6a8a;
        font-size: 18px; cursor: pointer; line-height: 1;
        padding: 2px 6px; border-radius: 4px;
    `;
    btnX.textContent = '×';
    btnX.onmouseenter = () => btnX.style.color = '#c8d6e5';
    btnX.onmouseleave = () => btnX.style.color = '#4a6a8a';
    btnX.onclick = () => overlay.remove();

    // Klik di luar box → tutup
    overlay.onclick = (e) => { if (e.target === overlay) overlay.remove(); };

    // Rakit elemen
    box.appendChild(btnX);
    box.appendChild(icon);
    box.appendChild(title);
    box.appendChild(info);
    box.appendChild(msg);
    box.appendChild(btnDownload);
    box.appendChild(btnClose);
    overlay.appendChild(box);

    // Inject CSS animasi (sekali saja)
    if (!document.getElementById('yata-datacss')) {
        const style = document.createElement('style');
        style.id = 'yata-datacss';
        style.textContent = `
            @keyframes fadeInOverlay { from { opacity:0 } to { opacity:1 } }
            @keyframes slideUp { from { opacity:0; transform:translateY(20px) scale(0.96) } to { opacity:1; transform:translateY(0) scale(1) } }
        `;
        document.head.appendChild(style);
    }

    document.body.appendChild(overlay);
}

// =============================================================================
// PUBLIC API — dipanggil dari C++ via EM_ASM atau dari JS manapun
// =============================================================================

/**
 * Cek apakah data symbol + waktu tersedia di IDB.
 * Kalau tidak ada → tampilkan popup download.
 *
 * @param {string} symbol     - "XAUUSD", "BTCUSDT", dll
 * @param {number} timestamp  - epoch seconds (Unix timestamp)
 * @returns {Promise<boolean>} - true = data ada, false = tidak ada (popup muncul)
 *
 * CONTOH PANGGIL DARI C++ (main.cpp):
 *   EM_ASM({
 *       var sym = UTF8ToString($0);
 *       var ts  = $1;
 *       window.checkDataAvailable(sym, ts).then(function(ok) {
 *           // ok = true → data ada, lanjut
 *           // ok = false → popup sudah muncul, batalkan aksi
 *           if (ok) {
 *               // lanjutkan replay / navigasi
 *           }
 *       });
 *   }, symbol.c_str(), (int)targetTimestamp);
 */
window.checkDataAvailable = async function(symbol, timestamp) {
    try {
        const hasData = await idbHasDataAround(symbol, timestamp);

        if (!hasData) {
            // Cek apakah ada data symbol ini tapi di waktu lain
            const count = await idbCountForSymbol(symbol);
            showDataMissingPopup(symbol, timestamp, count);
            console.warn(`[DATA_CHECK] ❌ No data: ${symbol} @ ${new Date(timestamp*1000).toISOString()}`);
            return false;
        }

        console.log(`[DATA_CHECK] ✅ Data OK: ${symbol} @ ${new Date(timestamp*1000).toISOString()}`);
        return true;

    } catch (err) {
        console.error('[DATA_CHECK] Error:', err);
        return true; // fail-open: kalau error, jangan blokir user
    }
};

/**
 * Versi synchronous — return langsung tanpa nunggu.
 * Cek dimulai di background, popup muncul kalau kosong.
 * Cocok buat panggil dari C++ tanpa await.
 *
 * CONTOH DARI C++:
 *   EM_ASM({
 *       window.checkDataAvailableAsync(UTF8ToString($0), $1);
 *   }, symbol.c_str(), (int)targetTimestamp);
 */
window.checkDataAvailableAsync = function(symbol, timestamp) {
    window.checkDataAvailable(symbol, timestamp); // fire and forget
};

console.log('[DATA_CHECK] ✅ data_check.js loaded');

})(); // end IIFE
