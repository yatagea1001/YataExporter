console.log("%c[OB-WS] Direct Order Book Engine v2 — Multi-Symbol", "color:#00BFFF;font-weight:bold;background:#0B0E11;padding:4px;");

// =========================================================
// websocket_ob.js  v2 — Multi-Symbol Architecture
//
// BUG v1 yang diperbaiki:
//   1. Race condition: singleton _ws + _intentionallyClosed
//      → onclose lama fire setelah _intentionallyClosed direset
//      → onopen coba kirim ke _ws yang sudah null → crash
//
//   2. Singleton tidak cukup untuk multi OB tab
//      (BTC tab + ETH tab buka bersamaan → saling bunuh)
//
// SOLUSI v2: Map-based, satu OBConnection per symbol
//   _connections: Map<uiSym, OBConnection>
//   Setiap OBConnection punya WS, state, dan timer sendiri.
//   Tidak ada shared mutable state antar symbol.
//
// PUBLIC API (tidak berubah dari v1):
//   window.OB.subscribe("BTCUSDT")
//   window.OB.unsubscribe("BTCUSDT")
//   window.OB.subscribedSymbol()        → symbol pertama aktif (compat)
//   window.OB.isConnected()
//   window.OB.activeSymbols()           → Set semua symbol aktif [BARU]
//   window.OB.debug()                   → console.table semua koneksi [BARU]
//   window.requestOrderBook("BTCUSDT")  → shortcut ke OB.subscribe
// =========================================================

const HL_WS_URL = "wss://api.hyperliquid.xyz/ws";
const TOP_N     = 30;

// UI symbol → Hyperliquid coin name
const UI_TO_HL = {
    "BTCUSDT": "BTC",
    "ETHUSDT": "ETH",
    "SOLUSDT": "SOL",
    "XAUUSD":  "XAU",
    "EURUSD":  "EUR",
    "GBPUSD":  "GBP",
};

// ─────────────────────────────────────────────────────────
// WASM HELPERS (shared, stateless)
// ─────────────────────────────────────────────────────────
function _wasmReady() {
    return typeof isWasmReady !== 'undefined' && isWasmReady
        && typeof Module !== 'undefined' && Module && Module.ccall;
}

function _clearOB(sym) {
    if (!_wasmReady() || typeof Module._wasm_clear_orderbook !== 'function') return;
    Module.ccall('wasm_clear_orderbook', null, ['string'], [sym]);
}

function _pushLevel(sym, price, size, isBid) {
    if (!_wasmReady() || typeof Module._wasm_push_orderbook_level !== 'function') return;
    Module.ccall('wasm_push_orderbook_level', null,
        ['string', 'number', 'number', 'number'],
        [sym, price, size, isBid ? 1 : 0]);
}

function _pushSnapshot(sym, timestamp, imbalance, rise_ratio_60) {
    if (!_wasmReady() || typeof Module._wasm_push_ob_snapshot !== 'function') return;
    Module.ccall('wasm_push_ob_snapshot', null,
        ['string', 'number', 'number', 'number'],
        [sym, timestamp, imbalance, rise_ratio_60]);
}

function _clearSnapshot(sym) {
    if (!_wasmReady() || typeof Module._wasm_clear_ob_snapshot !== 'function') return;
    Module.ccall('wasm_clear_ob_snapshot', null, ['string'], [sym]);
}

// ─────────────────────────────────────────────────────────
// OBConnection — satu instance per symbol
//
// KUNCI ANTI-RACE-CONDITION:
//   Setiap handler (onopen/onclose/onmessage) menyimpan referensi
//   lokal ke WS object pada saat handler dibuat (closure `ws`).
//   Guard `this.ws !== ws` memastikan callback dari WS yang
//   sudah di-replace tidak bisa korupsi state instance.
//
//   destroy() menandai this.dead = true dan null-kan semua handlers
//   SEBELUM close() — memastikan onclose tidak trigger reconnect.
// ─────────────────────────────────────────────────────────
class OBConnection {
    constructor(uiSym, coin) {
        this.uiSym = uiSym;
        this.coin  = coin;
        this.ws    = null;
        this.dead  = false;
        this.timer = null;
        this.snapTimer  = null;  // 1-sec interval for tick snapshot
        this.lastBids   = [];    // stored parsed bids (sorted high→low)
        this.lastAsks   = [];    // stored parsed asks (sorted low→high)
        this.priceHistory = [];  // mid-price history for rise_ratio_60
    }

    start() {
        if (this.dead) return;
        this._openWS();
        // Start 1-second tick snapshot timer
        this.snapTimer = setInterval(() => this._takeSnapshot(), 1000);
    }

    destroy() {
        this.dead = true;           // 1. tandai dead DULU
        clearTimeout(this.timer);   // 2. batalkan reconnect timer
        if (this.snapTimer) { clearInterval(this.snapTimer); this.snapTimer = null; }
        _clearSnapshot(this.uiSym); // clear tick history di WASM
        const ws = this.ws;
        this.ws = null;             // 3. lepas referensi
        if (ws) {
            // 4. null-kan semua handlers → onclose tidak fire reconnect
            ws.onopen    = null;
            ws.onmessage = null;
            ws.onerror   = null;
            ws.onclose   = null;
            if (ws.readyState === WebSocket.OPEN ||
                ws.readyState === WebSocket.CONNECTING) {
                ws.close();
            }
        }
        console.log(`[OB-WS] ${this.coin} destroyed`);
    }

    _openWS() {
        if (this.dead) return;
        console.log(`%c[OB-WS] ${this.coin} connecting...`, "color:#00BFFF");

        const ws = new WebSocket(HL_WS_URL);
        this.ws  = ws;  // simpan SEBELUM set handlers

        ws.onopen = () => {
            // Guard: pastikan ini WS yang masih relevan
            if (this.dead || this.ws !== ws) return;
            console.log(`%c[OB-WS] ${this.coin} ✅ subscribe l2Book`, "color:#00FF88");
            ws.send(JSON.stringify({
                method: "subscribe",
                subscription: { type: "l2Book", coin: this.coin, nSigFigs: 5 }
            }));
        };

        ws.onmessage = (event) => {
            if (this.dead || this.ws !== ws) return;
            try {
                const msg = JSON.parse(event.data);
                if      (msg.channel === "l2Book")             this._handleL2Book(msg.data);
                else if (msg.channel === "subscriptionResponse") console.log(`[OB-WS] ${this.coin} ack ✅`);
                else if (msg.channel === "error")               console.error(`[OB-WS] ${this.coin} error:`, msg.data);
            } catch(e) {
                console.error(`[OB-WS] ${this.coin} parse error:`, e);
            }
        };

        ws.onerror = (e) => {
            if (this.dead || this.ws !== ws) return;
            console.error(`[OB-WS] ${this.coin} WS error`);
        };

        ws.onclose = (e) => {
            // Guard ganda: dead flag DAN referensi WS
            if (this.dead)        return;  // destroy() sudah dipanggil
            if (this.ws !== ws)   return;  // stale callback dari WS lama

            this.ws = null;
            console.warn(`[OB-WS] ${this.coin} closed (${e.code}) → reconnect 3s`);
            clearTimeout(this.timer);
            this.timer = setTimeout(() => {
                if (!this.dead) this._openWS();
            }, 3000);
        };
    }

    _handleL2Book(data) {
        if (this.dead || !data || data.coin !== this.coin) return;

        const levels = data.levels;
        if (!Array.isArray(levels) || levels.length < 2) return;

        _clearOB(this.uiSym);

        const bids = levels[0].slice(0, TOP_N);
        bids.sort((a, b) => parseFloat(b.px) - parseFloat(a.px));

        const asks = levels[1].slice(0, TOP_N);
        asks.sort((a, b) => parseFloat(a.px) - parseFloat(b.px));

        // Store parsed levels for snapshot analytics + push ke WASM
        this.lastBids = [];
        this.lastAsks = [];

        for (const lvl of bids) {
            const px = parseFloat(lvl.px), sz = parseFloat(lvl.sz);
            if (px > 0 && sz > 0) {
                _pushLevel(this.uiSym, px, sz, true);
                this.lastBids.push({ px, sz });
            }
        }
        for (const lvl of asks) {
            const px = parseFloat(lvl.px), sz = parseFloat(lvl.sz);
            if (px > 0 && sz > 0) {
                _pushLevel(this.uiSym, px, sz, false);
                this.lastAsks.push({ px, sz });
            }
        }
    }

    // ── Tick Snapshot Analytics ─────────────────────────────────────────
    // Dipanggil setiap 1 detik oleh snapTimer
    // Menghitung imbalance dari top 3 bid/ask + rise_ratio_60 dari harga
    // ─────────────────────────────────────────────────────────────────────
    _takeSnapshot() {
        if (this.dead) return;
        const bids = this.lastBids;
        const asks = this.lastAsks;

        // Butuh minimal 3 level per sisi untuk imbalance yang akurat
        if (bids.length < 3 || asks.length < 3) return;

        // Best bid (highest) dan best ask (lowest)
        const bestBid = bids[0].px;
        const bestAsk = asks[0].px;
        const midPrice = (bestBid + bestAsk) / 2;

        // ── Imbalance: (bid_top3 - ask_top3) / (bid_top3 + ask_top3) ──
        const bidTotal = bids[0].sz + bids[1].sz + bids[2].sz;
        const askTotal = asks[0].sz + asks[1].sz + asks[2].sz;
        const imbalance = (bidTotal + askTotal) > 0
            ? (bidTotal - askTotal) / (bidTotal + askTotal)
            : 0;

        // ── Rise Ratio 60: perubahan harga vs 60 snapshot lalu (~60 detik) ──
        this.priceHistory.push(midPrice);
        if (this.priceHistory.length > 60) this.priceHistory.shift();

        let riseRatio = 0;
        if (this.priceHistory.length > 1) {
            const oldPrice = this.priceHistory[0];
            riseRatio = oldPrice > 0 ? (midPrice - oldPrice) / oldPrice : 0;
        }

        // Push ke WASM C++ side
        _pushSnapshot(this.uiSym, Date.now(), imbalance, riseRatio);
    }

    isAlive() {
        return !this.dead && this.ws !== null && this.ws.readyState === WebSocket.OPEN;
    }

    // Debug: tampilkan analytics state
    analyticsInfo() {
        const n = this.priceHistory.length;
        const imb = n > 0 && this.lastBids.length >= 3 && this.lastAsks.length >= 3
            ? (() => {
                const bt = this.lastBids[0].sz + this.lastBids[1].sz + this.lastBids[2].sz;
                const at = this.lastAsks[0].sz + this.lastAsks[1].sz + this.lastAsks[2].sz;
                return (bt + at) > 0 ? ((bt - at) / (bt + at)).toFixed(4) : 'N/A';
              })()
            : 'N/A';
        return { coin: this.coin, priceHistoryLen: n, imbalance: imb };
    }
}

// ─────────────────────────────────────────────────────────
// CONNECTION REGISTRY — Map<uiSym, OBConnection>
// ─────────────────────────────────────────────────────────
const _connections = new Map();

// ─────────────────────────────────────────────────────────
// PUBLIC API
// ─────────────────────────────────────────────────────────
window.OB = {

    // Subscribe l2Book untuk symbol. Idempoten — aman dipanggil berkali-kali.
    subscribe(uiSym) {
        if (!uiSym) return;
        const coin = UI_TO_HL[uiSym];
        if (!coin) {
            console.warn(`[OB-WS] subscribe: unknown symbol "${uiSym}"`);
            return;
        }
        if (_connections.has(uiSym)) {
            console.log(`[OB-WS] ${coin} sudah aktif — skip`);
            return;
        }
        const conn = new OBConnection(uiSym, coin);
        _connections.set(uiSym, conn);
        conn.start();
        console.log(`[OB-WS] +${uiSym} | aktif: ${[..._connections.keys()].join(', ')}`);
    },

    // Stop dan destroy koneksi untuk symbol ini. Dipanggil saat tab OB ditutup.
    unsubscribe(uiSym) {
        const conn = _connections.get(uiSym);
        if (!conn) return;
        conn.destroy();
        _connections.delete(uiSym);
        console.log(`[OB-WS] -${uiSym} | sisa: ${[..._connections.keys()].join(', ') || 'none'}`);
    },

    // Semua UI symbol yang sedang aktif
    activeSymbols() {
        return new Set(_connections.keys());
    },

    // Backward compat v1
    subscribedSymbol() {
        return _connections.keys().next().value ?? null;
    },

    isConnected() {
        for (const c of _connections.values()) if (c.isAlive()) return true;
        return false;
    },

    // Debug helper: OB.debug() di console → tabel semua koneksi
    debug() {
        console.table([..._connections.entries()].map(([sym, c]) => ({
            symbol: sym,
            coin:   c.coin,
            state:  c.ws ? ['CONNECTING','OPEN','CLOSING','CLOSED'][c.ws.readyState] : 'null',
            dead:   c.dead
        })));
    }
};

// Backward compat — dipanggil dari C++ / UI_ChartTabs.h
window.requestOrderBook = function(symbol) {
    window.OB.subscribe(symbol);
};

console.log("%c[OB-WS] v2 Multi-Symbol Engine siap ✅", "color:#00BFFF;font-weight:bold");
