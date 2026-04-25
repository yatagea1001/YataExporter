console.log("%c[OFW] V1 - ORDER FLOW ENGINE (FOOTPRINT + TICK_FLOW)", "color: #FF9900; font-weight:bold; background: #0B0E11; padding: 4px;");

// =========================================================
// websocket_orderflow.js
// Tanggung jawab TUNGGAL: semua yang berhubungan dengan Order Flow
//
// Dipisah dari websocket.js agar:
//   1. websocket.js fokus ke data (tick/candle/IDB/ninja)
//   2. order flow bisa di-debug/ganti independen
//   3. tidak ada footprint logic nyasar ke candle handler
//
// Entry points dipanggil dari websocket.js:
//   window.handleOrderFlowMessage(msg)  → ws.onmessage dispatch
//   window.handleOrderFlowBarClose(sym) → tiap bar close
//   window.requestFootprint(sym, count) → request ke server
//
// Dependencies (dari websocket.js, sudah global):
//   wsSend(), isWasmReady, Module, downloadedSymbols, CURRENT_SYMBOL
// =========================================================

// ─────────────────────────────────────────────────────────
// WASM BRIDGE
// ─────────────────────────────────────────────────────────
// Inject satu footprint level ke C++
// time_val = epoch seconds (bar time, bukan tick time)
// price    = harga level (e.g. 70050.5)
// buy_vol  = volume aggressor buy (side "B" Hyperliquid)
// sell_vol = volume aggressor sell (side "A" Hyperliquid)
// bypassGate=1 → data dari IDB saat replay, boleh masuk meski gate aktif
// bypassGate=0 → live feed biasa, diblok saat replay aktif (default)
function notifyWASM_footprint(symbol, time_val, price, buy_vol, sell_vol, bypassGate = 0) {
    if (!isWasmReady || !Module || !Module.ccall) return;
    // 🔥 Gate cek sekarang di C++ — JS tidak perlu cek lagi
    // C++: if (g_replayGateActive && !fromIDB) return;
    // Kita cukup teruskan flag bypassGate ke parameter fromIDB
    Module.ccall('wasm_push_footprint', null,
        ['string', 'number', 'number', 'number', 'number', 'number'],
        [symbol, time_val, price, buy_vol, sell_vol, bypassGate]);
}

// ─────────────────────────────────────────────────────────
// FORMAT HELPER
// ─────────────────────────────────────────────────────────
// 36000000 → "36.0M" | 312000 → "312K" | 850 → "850"
function fmtUSD(val) {
    const abs = Math.abs(val);
    const sign = val < 0 ? '-' : '';
    if (abs >= 1e9) return `${sign}${(abs/1e9).toFixed(1)}B`;
    if (abs >= 1e6) return `${sign}${(abs/1e6).toFixed(1)}M`;
    if (abs >= 1e3) return `${sign}${(abs/1e3).toFixed(0)}K`;
    return `${sign}${abs.toFixed(0)}`;
}

// ─────────────────────────────────────────────────────────
// REQUEST FOOTPRINT
// Dipanggil dari:
//   - SetActiveSymbol (cache hit)
//   - setelah full download selesai
//   - LoadTabSymbol (non-primary tab)
//   - handleOrderFlowBarClose (tiap bar close, count=0 → hanya candle aktif)
// ─────────────────────────────────────────────────────────
// ─────────────────────────────────────────────────────────
// DOWNLOADED FP TRACKING
// Analogi downloadedSymbols di websocket.js — tapi untuk footprint.
//
// Masalah sebelumnya:
//   User klik FP Bar → request 500 FP candles ✅
//   User klik FP Profile → request 500 FP candles LAGI ❌
//   → wasm_push_footprint tidak replace tapi ADD → angka double!
//
// Fix: downloadedFP = Set symbol yang FP-nya sudah ada di WASM.
//   Kalau sudah ada → skip request.
//   Kalau switch symbol baru → hapus dari Set → boleh request lagi.
//   Kalau switch FP style (Bar→Profile) → data sudah ada → tidak request.
// ─────────────────────────────────────────────────────────
const downloadedFP = new Set(); // symbol yang FP-nya sudah di-inject ke WASM

// Panggil ini saat symbol berubah — reset FP state untuk symbol lama
window.clearFPForSymbol = function(symbol) {
    if (symbol) {
        downloadedFP.delete(symbol);
        logInfo(`[OFW] FP cache cleared untuk ${symbol} → akan request ulang saat diperlukan`);
    }
};

// Panggil ini saat startup/reconnect — reset semua FP state
window.clearAllFP = function() {
    downloadedFP.clear();
    logInfo(`[OFW] FP cache cleared semua symbol`);
};

window.requestFootprint = function(symbol, count = 500, bypassGate = 0) {
    if (!symbol) return;

    // 🔥 SMART GUARD: Track bahwa FP pernah aktif di sesi ini
    // Dipakai oleh reloadLiveAfterReplay di websocket.js untuk auto-reload FP
    // setelah replay selesai — trader tidak perlu klik tombol FP lagi
    window._lastFPStyleActive = true;

    // 🔥 GUARD: Skip kalau FP sudah ada di WASM untuk symbol ini
    // KECUALI: bypassGate=1 (dipanggil dari replay) → selalu load ulang dari IDB
    // Ini penting karena saat masuk replay, FP mungkin sudah ada tapi gate membloknya
    if (downloadedFP.has(symbol) && !bypassGate) {
        logInfo(`[OFW] FP skip — ${symbol} sudah ada di WASM (switch style, bukan symbol)`);
        return;
    }

    // 🔥 REPLAY MODE: Load langsung dari IDB, bypass server request
    // Data historis sudah ada di IDB — tidak perlu ke server
    if (bypassGate) {
        logInfo(`[OFW] FP replay load dari IDB: ${symbol} (bypass gate aktif)`);
        loadFPFromIDB(symbol, bypassGate);
        return;
    }

    wsSend({ type: "request_footprint", symbol: symbol, count: count });
    console.log(`%c[OFW] FP requested: ${symbol} (count=${count})`, "color:#FF9900");
};

// ─────────────────────────────────────────────────────────
// LOAD FP FROM IDB (REPLAY MODE)
// Dipanggil saat user pilih FP style di dalam replay.
// Baca candle data dari IDB langsung — tidak ke server.
// bypassGate=1 diteruskan ke notifyWASM_footprint → C++ terima
// ─────────────────────────────────────────────────────────
async function loadFPFromIDB(symbol, bypassGate) {
    if (!symbol) return;

    // Buka IDB — pakai nama DB yang sama dengan websocket.js
    const DB_NAME = 'CandleDB';
    const DB_VERSION = 3;

    let db;
    try {
        db = await new Promise((resolve, reject) => {
            const req = indexedDB.open(DB_NAME, DB_VERSION);
            req.onsuccess = e => resolve(e.target.result);
            req.onerror   = e => reject(e);
        });
    } catch(e) {
        console.error('[OFW] loadFPFromIDB: gagal buka IDB', e);
        return;
    }

    // Timeframes yang perlu di-inject footprint-nya
    const TFS = ['M1', 'M5', 'M15', 'M30', 'H1', 'H4', 'D1'];
    let totalLevels = 0;

    for (const tf of TFS) {
        // Key IDB: primary tab pakai TF saja, non-primary pakai SYMBOL_TF
        // Coba primary dulu, kalau kosong coba non-primary
        const keys = [tf, `${symbol}_${tf}`];

        for (const storeKey of keys) {
            if (!db.objectStoreNames.contains(storeKey)) continue;

            try {
                const candles = await new Promise((resolve, reject) => {
                    const tx  = db.transaction(storeKey, 'readonly');
                    const req = tx.objectStore(storeKey).getAll();
                    req.onsuccess = e => resolve(e.target.result || []);
                    req.onerror   = e => reject(e);
                });

                for (const candle of candles) {
                    if (!candle.footprint || !candle.footprint.length) continue;
                    for (const lvl of candle.footprint) {
                        notifyWASM_footprint(
                            symbol,
                            candle.time,
                            lvl.price,
                            lvl.buyVol  || 0,
                            lvl.sellVol || 0,
                            bypassGate   // 🔥 pass bypass ke C++
                        );
                        totalLevels++;
                    }
                }
            } catch(e) {
                // Store mungkin tidak ada untuk TF ini — lanjut saja
            }
        }
    }

    if (db) db.close();

    // Tandai sudah di-inject (untuk switch style tetap aman)
    downloadedFP.add(symbol);
    console.log(`%c✅ [OFW] FP replay IDB: ${symbol} ${totalLevels} levels injected (bypassGate=${bypassGate})`,
        "color:#00FF88;font-weight:bold");
}

// ─────────────────────────────────────────────────────────
// BAR CLOSE HOOK
// Dipanggil dari websocket.js saat bar close untuk CURRENT_SYMBOL
// Hanya untuk crypto (USDT pairs) karena forex tidak punya tick_flow
// count=0 → server hanya kirim footprint candle aktif saja (lebih ringan)
// ─────────────────────────────────────────────────────────
window.handleOrderFlowBarClose = function(sym) {
   
};

// ─────────────────────────────────────────────────────────
// MAIN MESSAGE HANDLER
// Dipanggil dari ws.onmessage di websocket.js:
//   if (window.handleOrderFlowMessage && window.handleOrderFlowMessage(msg)) return;
//
// Return true  → pesan sudah dihandle, websocket.js stop
// Return false → pesan bukan order flow, websocket.js terus proses
// ─────────────────────────────────────────────────────────
window.handleOrderFlowMessage = function(msg) {

    // ── E. FOOTPRINT_DATA — history order flow ────────────────────
    // Format server v17: { type:"footprint_data", symbol, data:[{time, levels:[{p,b,s}]}] }
    // Dipakai untuk:
    //   - Primary tab: setelah load chart / tiap bar close (count=0)
    //   - Non-primary tab: setelah LoadTabSymbol + rebuild HTF selesai
    if (msg.type === "footprint_data" && msg.symbol) {
        const fpArray = msg.data;
        if (!Array.isArray(fpArray) || !fpArray.length) return true; // handled, data kosong

        const sym = msg.symbol;
        let totalLevels = 0;
        let totalVolUSD = 0;

        for (const fp of fpArray) {
            if (!fp.levels) continue;
            for (const lvl of fp.levels) {
                notifyWASM_footprint(sym, fp.time, lvl.p, lvl.b || 0, lvl.s || 0);
                totalLevels++;
                totalVolUSD += (lvl.b || 0) + (lvl.s || 0);
            }
        }

        // Primary tab → rebuild global HTF (M5/M15/dst) setelah inject selesai
        // Non-primary → tidak perlu, C++ wasm_push_footprint sudah inject ke SYMBOL_TF keys
        if (sym === CURRENT_SYMBOL && Module._wasm_rebuild_all_htfs) {
            Module._wasm_rebuild_all_htfs();
        }

        // 🔥 Tandai FP sudah ada di WASM untuk symbol ini
        // Request berikutnya untuk symbol ini akan di-skip (switch style aman)
        downloadedFP.add(sym);

        console.log(`%c✅ [OFW] FP ${sym}: ${totalLevels} levels | $${fmtUSD(totalVolUSD)} vol`, "color:#00FF88;font-weight:bold");
        return true; // pesan sudah dihandle
    }

    // ── F. TICK_FLOW — footprint real-time per aggressor trade ────
    // Format: { type:"tick_flow", symbol, bar_time, price, buy_vol, sell_vol }
    // Server kirim ini TIAP trade masuk (bukan nunggu bar close)
    // → update footprint candle aktif secara real-time
    //
    // Berlaku untuk semua symbol yang sudah didownload (primary & non-primary)
    if (msg.type === "tick_flow" && msg.symbol) {
        if (!downloadedSymbols.has(msg.symbol)) return true; // skip symbol belum ready
        notifyWASM_footprint(
            msg.symbol,
            msg.bar_time,
            msg.price,
            msg.buy_vol  || 0,
            msg.sell_vol || 0
        );
        return true; // pesan sudah dihandle
    }

    // ── G. ORDER BOOK ─────────────────────────────────────────────────
    // Dihandle oleh websocket_ob.js (direct WS ke Hyperliquid).
    // Pesan "orderbook" dari server tidak akan datang lagi.
    if (msg.type === "orderbook") return true;

    return false; // bukan pesan order flow
};

// ─────────────────────────────────────────────────────────
// REQUEST ORDER BOOK
// window.requestOrderBook sudah di-define ulang di websocket_ob.js
// → window.OB.subscribe(symbol)
// websocket_ob.js juga mengirim tick snapshot analytics (imbalance + rise_ratio)
// via wasm_push_ob_snapshot ke C++ setiap 1 detik
// Definisi ini hanya fallback kalau websocket_ob.js belum load.
// ─────────────────────────────────────────────────────────
if (!window.requestOrderBook) {
    window.requestOrderBook = function(symbol) {
        console.warn(`[OFW] requestOrderBook(${symbol}) dipanggil tapi websocket_ob.js belum load`);
    };
}

// ── OB SNAPSHOT ANALYTICS STATUS ─────────────────────────────────────
// websocket_ob.js menyediakan window.OB.analyticsInfo(sym) untuk debug
// Tick snapshot (imbalance + rise_ratio_60) dikirim ke WASM via:
//   Module.ccall('wasm_push_ob_snapshot', ...)
// Setiap 1 detik per symbol yang aktif
if (!window.OB) {
    console.warn('[OFW] window.OB belum tersedia — websocket_ob.js belum load, snapshot analytics tidak aktif');
}

console.log("%c[OFW] Order Flow Engine siap ✅", "color:#FF9900;font-weight:bold");
