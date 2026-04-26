// ============================================================
//  JARVIS AI SERVER — Phase 6: Drawing + Swing Analysis + Key Levels
//  Groq API (Llama 3.3) + Function Calling
//  HYBRID: Auto-context (ringan) + Tools (dipanggil AI)
// ============================================================

const express = require('express');
const cors = require('cors');
const fs = require('fs');
const path = require('path');
const Groq = require('groq-sdk');

const app = express();
app.use(cors());
app.use(express.json({ limit: '1mb' }));

// ── Config ──────────────────────────────────────────────────
const CONFIG_PATH = path.join(__dirname, 'ai_config.env');
let API_KEY = '';
let USE_REAL_AI = false;
let groqClient = null;

function loadConfig() {
    try {
        if (fs.existsSync(CONFIG_PATH)) {
            const lines = fs.readFileSync(CONFIG_PATH, 'utf8').split('\n');
            for (const line of lines) {
                const trimmed = line.trim();
                if (!trimmed || trimmed.startsWith('#')) continue;
                const eq = trimmed.indexOf('=');
                if (eq > 0) {
                    const key = trimmed.substring(0, eq).trim();
                    const val = trimmed.substring(eq + 1).trim();
                    if (key === 'GROQ_API_KEY' && val.length > 0 && val !== 'MASUKKAN_KEY_DISINI') {
                        API_KEY = val;
                        USE_REAL_AI = true;
                    }
                }
            }
        }
    } catch (e) {
        console.log('[CONFIG] Gagal baca ai_config.env:', e.message);
    }
}

loadConfig();

// ── Init Groq Client ────────────────────────────────────────
function initGroq() {
    if (!USE_REAL_AI) return false;
    try {
        groqClient = new Groq({
            apiKey: API_KEY,
            dangerouslyAllowBrowser: false
        });
        return true;
    } catch (e) {
        console.log('[GROQ] Gagal init:', e.message);
        return false;
    }
}

// ── Tool Execution (returns {success, message}) ─────────────
function executeTool(name, args) {
    console.log(`[TOOL] Executing: ${name}(${JSON.stringify(args)})`);

    if (name === 'chart_add_symbol') {
        let sym = (args.symbol || '').toUpperCase();
        const map = { BTC:'BTCUSDT', ETH:'ETHUSDT', XAU:'XAUUSD', EUR:'EURUSD', GBP:'GBPUSD',
                      SOL:'SOLUSDT', BNB:'BNBUSDT', DOGE:'DOGEUSDT', ADA:'ADAUSDT' };
        sym = map[sym] || sym;
        return { success: true, message: `Symbol ${sym} berhasil ditambahkan ke chart.` };
    }

    if (name === 'chart_add_indicator') {
        const sym = (args.symbol || '').toUpperCase();
        const ind = (args.indicator || '').toLowerCase();
        const period = args.period || 14;
        return { success: true, message: `Indicator ${ind.toUpperCase()}(${period}) berhasil ditambahkan ke ${sym}.` };
    }

    // READ tools — data sudah ada di tool_data dari C++
    if (name === 'chart_analyze_swing') {
        return { success: true, message: 'swing_analysis', is_read_tool: true };
    }
    if (name === 'chart_get_key_levels') {
        return { success: true, message: 'key_levels', is_read_tool: true };
    }

    // DRAW tools — AI wants to draw shapes on chart
    if (name === 'chart_draw_line') {
        const p0 = args.price0;
        const p1 = args.price1;
        return { success: true, message: `Garis berhasil digambar dari ${p0} ke ${p1}.`, is_draw_tool: true, draw_type: 'LINE', draw_args: args };
    }
    if (name === 'chart_draw_rect') {
        const p0 = args.price0;
        const p1 = args.price1;
        return { success: true, message: `Rectangle berhasil digambar dari ${p0} ke ${p1}.`, is_draw_tool: true, draw_type: 'RECT', draw_args: args };
    }
    if (name === 'chart_draw_fib') {
        const p0 = args.price0;
        const p1 = args.price1;
        return { success: true, message: `Fibonacci berhasil digambar dari ${p0} ke ${p1}.`, is_draw_tool: true, draw_type: 'FIB', draw_args: args };
    }
    if (name === 'chart_draw_text') {
        const txt = args.text || 'Label';
        const p = args.price;
        return { success: true, message: `Teks "${txt}" berhasil ditulis di harga ${p}.`, is_draw_tool: true, draw_type: 'TEXT', draw_args: args };
    }
    if (name === 'chart_draw_elliot') {
        const prices = args.prices || [];
        return { success: true, message: `Elliot Wave (${prices.length} titik) berhasil digambar.`, is_draw_tool: true, draw_type: 'ELLIOT', draw_args: args };
    }

    return { success: false, message: `Unknown tool: ${name}` };
}

// ── Tool Definitions (OpenAI format) ────────────────────────
const tools = [
    {
        type: 'function',
        function: {
            name: 'chart_add_symbol',
            description: 'Ganti atau tambahkan simbol trading ke chart. Gunakan saat user minta buka simbol baru, ganti chart, atau analisa suatu pair.',
            parameters: {
                type: 'object',
                properties: {
                    symbol: {
                        type: 'string',
                        description: 'Simbol trading, contoh: BTCUSDT, ETHUSDT, EURUSD, XAUUSD'
                    }
                },
                required: ['symbol']
            }
        }
    },
    {
        type: 'function',
        function: {
            name: 'chart_add_indicator',
            description: 'Tambah indicator teknikal ke chart. Gunakan saat user minta pasang/masang/tambah indicator.',
            parameters: {
                type: 'object',
                properties: {
                    symbol: {
                        type: 'string',
                        description: 'Simbol target, contoh: BTCUSDT'
                    },
                    indicator: {
                        type: 'string',
                        enum: ['sma', 'ema', 'rsi', 'macd', 'bollinger_bands', 'stochastic', 'atr', 'adx', 'cci', 'williams', 'supertrend', 'mfi', 'roc', 'obv', 'volume'],
                        description: 'Jenis indicator'
                    },
                    period: {
                        type: 'number',
                        description: 'Periode indicator (opsional, default tergantung tipe)'
                    }
                },
                required: ['symbol', 'indicator']
            }
        }
    },
    {
        type: 'function',
        function: {
            name: 'chart_analyze_swing',
            description: 'Analisa swing point mendalam (High/Low, trend structure, Higher High, Lower Low). Gunakan saat user minta analisa teknikal, rekomendasi entry/SL/TP, atau menanyakan kondisi market saat ini.',
            parameters: {
                type: 'object',
                properties: {},
                required: []
            }
        }
    },
    {
        type: 'function',
        function: {
            name: 'chart_get_key_levels',
            description: 'Cari area support dan resistance kuat berdasarkan swing point yang sering di-test. Gunakan saat user tanya dimana pasang SL/TP, area penting, level support/resistance, atau area liquidity.',
            parameters: {
                type: 'object',
                properties: {},
                required: []
            }
        }
    },
    {
        type: 'function',
        function: {
            name: 'chart_draw_line',
            description: 'Gambar garis di chart (trendline, support/resistance line). Gunakan saat user minta gambar trendline, garis support, garis resistance, atau garis diagonal di chart.',
            parameters: {
                type: 'object',
                properties: {
                    price0: { type: 'number', description: 'Harga titik awal garis' },
                    price1: { type: 'number', description: 'Harga titik akhir garis' },
                    time0: { type: 'number', description: 'Index candle titik awal (0=paling kiri). Jika tidak tahu, gunakan 0' },
                    time1: { type: 'number', description: 'Index candle titik akhir. Jika tidak tahu, gunakan -1 (artinya candle terakhir)' },
                    color: { type: 'string', description: 'Warna garis dalam hex. Default: "#FFD700" (gold)', default: '#FFD700' },
                    thickness: { type: 'number', description: 'Ketebalan garis (default: 1.5)', default: 1.5 },
                    extend_left: { type: 'boolean', description: 'Perpanjang garis ke kiri (default: false)', default: false },
                    extend_right: { type: 'boolean', description: 'Perpanjang garis ke kanan (default: false)', default: false },
                    label: { type: 'string', description: 'Label teks opsional di ujung garis' }
                },
                required: ['price0', 'price1']
            }
        }
    },
    {
        type: 'function',
        function: {
            name: 'chart_draw_rect',
            description: 'Gambar rectangle/zone di chart (order block, supply/demand zone, range). Gunakan saat user minta gambar zone, blok, area, rectangle, atau kotak di chart.',
            parameters: {
                type: 'object',
                properties: {
                    price0: { type: 'number', description: 'Harga batas bawah rectangle' },
                    price1: { type: 'number', description: 'Harga batas atas rectangle' },
                    time0: { type: 'number', description: 'Index candle batas kiri. Jika tidak tahu, gunakan 0' },
                    time1: { type: 'number', description: 'Index candle batas kanan. Jika tidak tahu, gunakan -1 (candle terakhir)' },
                    color: { type: 'string', description: 'Warna border. Default: "#4488FF" (biru)', default: '#4488FF' },
                    fill_color: { type: 'string', description: 'Warna fill area. Default: "#4488FF" (biru)', default: '#4488FF' },
                    fill_opacity: { type: 'number', description: 'Transparansi fill 0-1 (default: 0.15)', default: 0.15 },
                    label: { type: 'string', description: 'Teks label di dalam rectangle (opsional). Contoh: "OB", "Demand Zone", "Supply"' }
                },
                required: ['price0', 'price1']
            }
        }
    },
    {
        type: 'function',
        function: {
            name: 'chart_draw_fib',
            description: 'Gambar Fibonacci Retracement di chart. Gunakan saat user minta fib, fibonacci, retracement, atau level fibonacci.',
            parameters: {
                type: 'object',
                properties: {
                    price0: { type: 'number', description: 'Harga titik awal (swing low untuk bullish fib, swing high untuk bearish fib)' },
                    price1: { type: 'number', description: 'Harga titik akhir (swing high untuk bullish fib, swing low untuk bearish fib)' },
                    time0: { type: 'number', description: 'Index candle titik awal. Jika tidak tahu, gunakan 0' },
                    time1: { type: 'number', description: 'Index candle titik akhir. Jika tidak tahu, gunakan -1' },
                    color: { type: 'string', description: 'Warna garis fib. Default: "#FFD700" (gold)', default: '#FFD700' }
                },
                required: ['price0', 'price1']
            }
        }
    },
    {
        type: 'function',
        function: {
            name: 'chart_draw_text',
            description: 'Tulis teks/label di chart. Gunakan saat user minta tulis catatan, label, anotasi, atau teks di chart.',
            parameters: {
                type: 'object',
                properties: {
                    text: { type: 'string', description: 'Isi teks yang akan ditulis' },
                    price: { type: 'number', description: 'Posisi harga untuk teks' },
                    time: { type: 'number', description: 'Index candle posisi teks. Jika tidak tahu, gunakan -1 (candle terakhir)' },
                    color: { type: 'string', description: 'Warna teks. Default: "#FFFFFF" (putih)', default: '#FFFFFF' },
                    font_size: { type: 'number', description: 'Ukuran font (default: 16)', default: 16 }
                },
                required: ['text', 'price']
            }
        }
    },
    {
        type: 'function',
        function: {
            name: 'chart_draw_elliot',
            description: 'Gambar Elliot Wave / pola multi-titik di chart. Gunakan saat user minta gambar wave, elliott wave, pola ABCD, atau struktur harga multi-titik.',
            parameters: {
                type: 'object',
                properties: {
                    prices: { type: 'array', items: { type: 'number' }, description: 'Array harga tiap titik. Contoh: [67000, 65000, 68000, 66000, 70000] untuk 5-point wave' },
                    times: { type: 'array', items: { type: 'number' }, description: 'Array index candle tiap titik (opsional). Jika kosong, titik tersebar merata. Contoh: [10, 30, 50, 70, 90]' },
                    color: { type: 'string', description: 'Warna garis. Default: "#FF9900" (orange)', default: '#FF9900' },
                    thickness: { type: 'number', description: 'Ketebalan garis (default: 1.5)', default: 1.5 }
                },
                required: ['prices']
            }
        }
    }
];

// ── System Prompt ───────────────────────────────────────────
const SYSTEM_PROMPT = `Kamu adalah Jarvis, asisten AI untuk platform Chart Trading.

ATURAN DASAR:
1. Kamu BISA mengontrol chart melalui function calling (tool_use)
2. Jawab dalam bahasa Indonesia yang singkat dan jelas
3. Jangan menjelaskan teknis, langsung eksekusi perintah user
4. Kalau user hanya ngobrol santai, jawab biasa tanpa panggil tools
5. Jangan pernah panggil tool tanpa alasan yang jelas dari user
6. Untuk "ma" atau "moving average", gunakan indicator "ema" dengan period 20
7. Selalu sertakan text response yang menjelaskan apa yang kamu lakukan

ATURAN CHART:
8. Saat user minta analisa simbol → panggil chart_add_symbol dulu, lalu chart_analyze_swing
9. Saat user minta tambah indicator → panggil chart_add_indicator
10. Saat user minta ganti chart/pair → panggil chart_add_symbol
11. Saat user tanya dimana SL/TP, area support/resistance, atau area penting → panggil chart_get_key_levels
12. Saat user tanya analisa teknikal, kondisi market, atau trend → panggil chart_analyze_swing

ATURAN ANALISA:
13. Setelah dapat data swing, analisa: trend (bullish/bearish/sideways), Higher High/Higher Low pattern, area entry/SL/TP
14. Berikan rekomendasi yang spesifik: level angka, bukan "di sekitar sana"
15. Jika swing data menunjukkan resistance kuat (test_count >= 3), peringatkan user tentang area liquidity
16. Selalu sebutkan risk/reward ratio kalau user minta rekomendasi entry

ATURAN MENGGAMBAR (DRAWING):
17. Kamu BISA menggambar langsung di chart! Gunakan tools drawing untuk visualisasi analisa.
18. Saat user minta gambar trendline/garis → panggil chart_draw_line
19. Saat user minta gambar zone/area/order block → panggil chart_draw_rect
20. Saat user minta fibonacci/fib → panggil chart_draw_fib
21. Saat user minta tulis label/catatan → panggil chart_draw_text
22. Saat user minta gambar wave/struktur → panggil chart_draw_elliot
23. CONTOH ALUR ANALISA VISUAL:
    a. User: "analisa BTCUSDT" → chart_analyze_swing + chart_get_key_levels → beri rekomendasi + gambar support/resistance
    b. User: "gambar fib di chart" → chart_draw_fib dari swing high ke swing low terakhir
    c. User: "tandai order block" → chart_draw_rect untuk zone OB
    d. User: "gambar trendline" → chart_draw_line dari swing low ke swing low berikutnya
24. SELALU gunakan data swing/key_levels untuk menentukan koordinat gambar yang akurat
25. Untuk time parameter: gunakan index candle (0=terlama, -1=terbaru). Jika ragu, gunakan 0 dan -1
26. Warna default: gold (#FFD700) untuk garis penting, biru (#4488FF) untuk zone, hijau (#00FF00) untuk bullish, merah (#FF0000) untuk bearish`;

// ── Format swing data untuk LLM ──
function formatSwingForLLM(swingData) {
    if (!swingData || swingData.total_candles === 0) {
        return 'Belum ada data candle.';
    }

    let text = `ANALISA SWING ${swingData.symbol} (${swingData.timeframe}):\n`;
    text += `Total candle: ${swingData.total_candles} (${swingData.first_candle} s/d ${swingData.last_candle})\n`;
    text += `Trend: ${swingData.trend}\n\n`;

    if (swingData.current_candle) {
        const c = swingData.current_candle;
        text += `CANDLE LIVE: O:${c.open} H:${c.high} L:${c.low} C:${c.close}\n\n`;
    }

    if (swingData.swing_highs && swingData.swing_highs.length > 0) {
        text += `SWING HIGHS (${swingData.swing_highs.length} point):\n`;
        for (const sh of swingData.swing_highs) {
            const label = sh.label ? ` [${sh.label}]` : '';
            text += `  [${sh.datetime}] H:${sh.price}${label}\n`;
        }
        text += '\n';
    }

    if (swingData.swing_lows && swingData.swing_lows.length > 0) {
        text += `SWING LOWS (${swingData.swing_lows.length} point):\n`;
        for (const sl of swingData.swing_lows) {
            const label = sl.label ? ` [${sl.label}]` : '';
            text += `  [${sl.datetime}] L:${sl.price}${label}\n`;
        }
    }

    return text;
}

// ── Format key levels untuk LLM ──
function formatKeyLevelsForLLM(keyData) {
    if (!keyData || !keyData.resistances) {
        return 'Belum ada data level.';
    }

    let text = `KEY LEVELS ${keyData.symbol} (${keyData.timeframe}):\n`;
    text += `Trend: ${keyData.trend}\n`;
    text += `Price saat ini: ${keyData.current_price}\n`;
    text += `Nearest Resistance: ${keyData.nearest_resistance}\n`;
    text += `Nearest Support: ${keyData.nearest_support}\n\n`;

    if (keyData.resistances.length > 0) {
        text += `RESISTANCE (di atas harga):\n`;
        for (const r of keyData.resistances) {
            text += `  ${r.price} — ${r.strength} (test ${r.test_count}x, jarak ${r.distance_pct.toFixed(2)}%)\n`;
        }
        text += '\n';
    }

    if (keyData.supports.length > 0) {
        text += `SUPPORT (di bawah harga):\n`;
        for (const s of keyData.supports) {
            text += `  ${s.price} — ${s.strength} (test ${s.test_count}x, jarak ${s.distance_pct.toFixed(2)}%)\n`;
        }
    }

    return text;
}

// ── MOCK MODE (fallback tanpa API key) ─────────────────────
function mockProcess(userMessage, chartStatus) {
    const msg = userMessage.toLowerCase();
    const actions = [];
    let response = '';

    const symbols = [];
    const symbolPatterns = [
        { regex: /\b(btcusdt?)\b/gi, name: 'BTCUSDT' },
        { regex: /\b(ethusdt?)\b/gi, name: 'ETHUSDT' },
        { regex: /\b(eurusd?)\b/gi, name: 'EURUSD' },
        { regex: /\b(xauusdt?)\b/gi, name: 'XAUUSD' },
        { regex: /\b(gbpusdt?)\b/gi, name: 'GBPUSD' },
        { regex: /\b(bnbusdt?)\b/gi, name: 'BNBUSDT' },
        { regex: /\b(solusdt?)\b/gi, name: 'SOLUSDT' },
        { regex: /\b(dogeusdt?)\b/gi, name: 'DOGEUSDT' },
        { regex: /\b(xrpusdt?)\b/gi, name: 'XRPUSDT' },
        { regex: /\b(adausdt?)\b/gi, name: 'ADAUSDT' },
    ];
    for (const sp of symbolPatterns) {
        if (sp.regex.test(msg)) symbols.push(sp.name);
    }

    const indicators = [];
    const indPatterns = [
        { regex: /\brsi\b/gi, name: 'rsi', period: 14 },
        { regex: /\bmacd\b/gi, name: 'macd', period: 12 },
        { regex: /\bsma\b/gi, name: 'sma', period: 20 },
        { regex: /\bema\b/gi, name: 'ema', period: 20 },
        { regex: /\b(atr)\b/gi, name: 'atr', period: 14 },
        { regex: /\bbollinger\b/gi, name: 'bollinger_bands', period: 20 },
        { regex: /\bstochastic\b/gi, name: 'stochastic', period: 14 },
        { regex: /\badx\b/gi, name: 'adx', period: 14 },
        { regex: /\bcci\b/gi, name: 'cci', period: 20 },
        { regex: /\bsupertrend\b/gi, name: 'supertrend', period: 10 },
        { regex: /\bwilliams\b/gi, name: 'williams', period: 14 },
        { regex: /\bmfi\b/gi, name: 'mfi', period: 14 },
        { regex: /\broc\b/gi, name: 'roc', period: 12 },
        { regex: /\bobv\b/gi, name: 'obv', period: 0 },
        { regex: /\bvolume\b/gi, name: 'volume', period: 0 },
    ];
    for (const ip of indPatterns) {
        if (ip.regex.test(msg)) indicators.push(ip);
    }

    const isAnalisa = /\b(analisa|analisis|analyze|check|lihat|review|kondisi|trend|market)\b/i.test(msg);
    const isTambah = /\b(tambah|pasang|add|masang|taro|set|attach)\b/i.test(msg);
    const isGanti = /\b(ganti|switch|pindah|ubah|change|buka)\b/i.test(msg);
    const isLevel = /\b(sl|tp|stoploss|takeprofit|support|resistance|level|area|liquidity|dimana)\b/i.test(msg);
    const isDraw = /\b(gambar|draw|tandai|mark|blok|zone|fib|fibonacci|trendline|wave|elliot|annotasi|catatan)\b/i.test(msg);
    const isFib = /\b(fib|fibonacci|retracement)\b/i.test(msg);
    const isOB = /\b(order.?block|ob|supply|demand|zone|blok)\b/i.test(msg);
    const isTrendline = /\b(trendline|trend.?line|garis)\b/i.test(msg);
    const isWave = /\b(wave|elliot|elliott|abc|abcd)\b/i.test(msg);

    if (isAnalisa || isGanti) {
        if (symbols.length > 0) {
            const result = executeTool('chart_add_symbol', { symbol: symbols[0] });
            actions.push({ tool: 'chart_add_symbol', arguments: { symbol: symbols[0] }, result: result });
            response += `Oke, chart diganti ke **${symbols[0]}**.\n`;
        }
        if (isAnalisa && indicators.length === 0 && symbols.length > 0) {
            const r2 = executeTool('chart_add_indicator', { symbol: symbols[0], indicator: 'rsi', period: 14 });
            const r3 = executeTool('chart_add_indicator', { symbol: symbols[0], indicator: 'macd', period: 12 });
            actions.push({ tool: 'chart_add_indicator', arguments: { symbol: symbols[0], indicator: 'rsi', period: 14 }, result: r2 });
            actions.push({ tool: 'chart_add_indicator', arguments: { symbol: symbols[0], indicator: 'macd', period: 12 }, result: r3 });
            response += `Indicator **RSI(14)** dan **MACD(12)** dipasang otomatis.\n`;
        }
    }

    if (isTambah && indicators.length > 0) {
        const sym = symbols.length > 0 ? symbols[0] : 'BTCUSDT';
        for (const ind of indicators) {
            const result = executeTool('chart_add_indicator', { symbol: sym, indicator: ind.name, period: ind.period });
            actions.push({ tool: 'chart_add_indicator', arguments: { symbol: sym, indicator: ind.name, period: ind.period }, result: result });
            response += `Indicator **${ind.name.toUpperCase()}(${ind.period})** dipasang di **${sym}**.\n`;
        }
    }

    if (isLevel) {
        response += `[MOCK] Fitur analisa level support/resistance membutuhkan AI Groq. Aktifkan API key di ai_config.env.\n`;
    }

    if (actions.length === 0 && symbols.length > 0) {
        const result = executeTool('chart_add_symbol', { symbol: symbols[0] });
        actions.push({ tool: 'chart_add_symbol', arguments: { symbol: symbols[0] }, result: result });
        response += `Oke, chart diganti ke **${symbols[0]}**.\n`;
    }

    if (isFib && chartStatus && chartStatus.last_swing_high && chartStatus.last_swing_low) {
        const result = executeTool('chart_draw_fib', {
            price0: chartStatus.last_swing_low,
            price1: chartStatus.last_swing_high,
            time0: 0, time1: -1
        });
        actions.push({ tool: 'chart_draw_fib', arguments: { price0: chartStatus.last_swing_low, price1: chartStatus.last_swing_high }, result: result });
        response += `Fibonacci digambar dari ${chartStatus.last_swing_low} ke ${chartStatus.last_swing_high}.\n`;
    }

    if (isOB && chartStatus && chartStatus.support && chartStatus.resistance) {
        const result = executeTool('chart_draw_rect', {
            price0: chartStatus.support,
            price1: chartStatus.resistance,
            time0: 0, time1: -1,
            label: 'Zone'
        });
        actions.push({ tool: 'chart_draw_rect', arguments: { price0: chartStatus.support, price1: chartStatus.resistance, label: 'Zone' }, result: result });
        response += `Zone digambar dari ${chartStatus.support} ke ${chartStatus.resistance}.\n`;
    }

    if (actions.length === 0 && response === '') {
        response = 'Maaf, aku belum paham perintahnya. Coba ketik:\n- "analisa BTCUSDT"\n- "pasang RSI di ETHUSDT"\n- "ganti ke XAUUSD"\n- "dimana SL yang aman?"\n- "gambar fib di chart"\n- "tandai order block"\n- "gambar trendline"';
    }

    return { actions, response };
}

// ── Real AI Mode (Groq + Function Calling) ──────────────────
async function groqProcess(userMessage, chatHistory, chartStatus, toolData) {
    if (!groqClient) {
        const ok = initGroq();
        if (!ok) {
            console.log('[GROQ] Fallback ke MOCK mode');
            return mockProcess(userMessage, chartStatus);
        }
    }

    try {
        // Build system prompt with chart auto-context
        let systemPrompt = SYSTEM_PROMPT;
        if (chartStatus && chartStatus.symbol && chartStatus.symbol !== 'NONE') {
            const indList = (chartStatus.indicators && chartStatus.indicators.length > 0)
                ? chartStatus.indicators.join(', ')
                : 'tidak ada';

            systemPrompt += `\n\nKONDISI CHART SAAT INI (dilihat user):\n`;
            systemPrompt += `- Symbol: ${chartStatus.symbol}\n`;
            systemPrompt += `- Timeframe: ${chartStatus.timeframe || '?'}\n`;
            systemPrompt += `- Harga terakhir: ${chartStatus.price || '?'}\n`;
            systemPrompt += `- Trend: ${chartStatus.trend || '?'}\n`;
            systemPrompt += `- Resistance: ${chartStatus.resistance || '?'}\n`;
            systemPrompt += `- Support: ${chartStatus.support || '?'}\n`;
            systemPrompt += `- Last Swing High: ${chartStatus.last_swing_high || '?'}\n`;
            systemPrompt += `- Last Swing Low: ${chartStatus.last_swing_low || '?'}\n`;
            systemPrompt += `- Indicator aktif: ${indList}\n`;
            systemPrompt += `\nATURAN KONTEKS:\n`;
            systemPrompt += `- Jika user minta tambah indicator yang SUDAH ADA, bilang sudah terpasang\n`;
            systemPrompt += `- Jika user minta analisa tanpa sebutkan symbol, gunakan symbol AKTIF (${chartStatus.symbol})\n`;
            systemPrompt += `- Jika user minta tambah indicator tanpa sebutkan symbol, gunakan symbol AKTIF (${chartStatus.symbol})\n`;
            systemPrompt += `- Gunakan info trend/levels di atas untuk jawaban yang lebih tepat\n`;
        }

        const messages = [{ role: 'system', content: systemPrompt }];

        for (const msg of chatHistory) {
            if (msg.role === 'user' || msg.role === 'assistant') {
                messages.push({ role: msg.role, content: msg.content });
            }
        }
        messages.push({ role: 'user', content: userMessage });

        // ── FIRST LLM CALL ──
        const response = await groqClient.chat.completions.create({
            model: 'llama-3.3-70b-versatile',
            messages: messages,
            tools: tools,
            tool_choice: 'auto',
            temperature: 0.3,
            max_tokens: 1024
        });

        const choice = response.choices[0];
        const actions = [];
        let responseText = '';

        if (choice.message.tool_calls && choice.message.tool_calls.length > 0) {
            let hasReadTool = false;
            const toolResults = [];

            for (const tc of choice.message.tool_calls) {
                const toolName = tc.function.name;
                const toolArgs = JSON.parse(tc.function.arguments);
                const result = executeTool(toolName, toolArgs);

                if (result.is_read_tool && toolData) {
                    hasReadTool = true;
                    let dataText = '';

                    if (result.message === 'swing_analysis' && toolData.swing_analysis) {
                        dataText = formatSwingForLLM(toolData.swing_analysis);
                    } else if (result.message === 'key_levels' && toolData.key_levels) {
                        dataText = formatKeyLevelsForLLM(toolData.key_levels);
                    } else {
                        dataText = 'Data belum tersedia.';
                    }

                    toolResults.push({
                        tool_call_id: tc.id,
                        type: 'function',
                        name: toolName,
                        content: dataText
                    });

                    console.log(`[TOOL] ${toolName} -> READ (inject ${dataText.length} chars from C++)`);
                } else if (result.is_read_tool && !toolData) {
                    // Read tool tapi toolData kosong — inject pesan error
                    console.log(`[TOOL] ${toolName} -> READ (NO toolData from C++!)`);
                    toolResults.push({
                        tool_call_id: tc.id,
                        type: 'function',
                        name: toolName,
                        content: 'Data chart belum tersedia. Candle belum dimuat.'
                    });
                } else {
                    actions.push({
                        tool: toolName,
                        arguments: toolArgs,
                        result: result,
                        // Pass draw info to C++ client
                        ...(result.is_draw_tool ? { draw_type: result.draw_type, draw_args: result.draw_args } : {})
                    });
                    console.log(`[TOOL] ${toolName}(${tc.function.arguments}) -> ${result.success ? 'OK' : 'FAIL'}${result.is_draw_tool ? ' [DRAW:' + result.draw_type + ']' : ''}`);

                    toolResults.push({
                        tool_call_id: tc.id,
                        type: 'function',
                        name: toolName,
                        content: result.message
                    });
                }
            }

            // ── SECOND LLM CALL (with tool results) ──
            if (hasReadTool || toolResults.length > 0) {
                const t1 = Date.now();
                const secondMessages = [...messages];
                secondMessages.push(choice.message);

                for (const tr of toolResults) {
                    secondMessages.push({
                        role: 'tool',
                        tool_call_id: tr.tool_call_id,
                        name: tr.name,
                        content: tr.content
                    });
                }

                console.log(`[AI] Second call... (${toolResults.length} tool results)`);

                try {
                    const secondResponse = await groqClient.chat.completions.create({
                        model: 'llama-3.3-70b-versatile',
                        messages: secondMessages,
                        tools: tools,
                        tool_choice: 'auto',
                        temperature: 0.3,
                        max_tokens: 1500
                    });

                    const elapsed2 = Date.now() - t1;
                    const secondChoice = secondResponse.choices[0];
                    responseText = secondChoice.message.content || '';
                    console.log(`[AI] Second call complete (${elapsed2}ms)`);

                    // Handle second-round tool calls (AI wants more tools after seeing data)
                    if (secondChoice.message.tool_calls && secondChoice.message.tool_calls.length > 0) {
                        const round2ToolResults = [];
                        let round2HasRead = false;

                        for (const tc of secondChoice.message.tool_calls) {
                            const toolName = tc.function.name;
                            const toolArgs = JSON.parse(tc.function.arguments);
                            const result = executeTool(toolName, toolArgs);

                            if (result.is_read_tool && toolData) {
                                // FIX: Handle read tools in second round too
                                round2HasRead = true;
                                let dataText = '';
                                if (result.message === 'swing_analysis' && toolData.swing_analysis) {
                                    dataText = formatSwingForLLM(toolData.swing_analysis);
                                } else if (result.message === 'key_levels' && toolData.key_levels) {
                                    dataText = formatKeyLevelsForLLM(toolData.key_levels);
                                } else {
                                    dataText = 'Data belum tersedia.';
                                }
                                round2ToolResults.push({
                                    tool_call_id: tc.id,
                                    type: 'function',
                                    name: toolName,
                                    content: dataText
                                });
                                console.log(`[TOOL-2] ${toolName} -> READ (inject ${dataText.length} chars)`);
                            } else {
                                actions.push({
                                    tool: toolName,
                                    arguments: toolArgs,
                                    result: result,
                                    // Pass draw info to C++ client
                                    ...(result.is_draw_tool ? { draw_type: result.draw_type, draw_args: result.draw_args } : {})
                                });
                                round2ToolResults.push({
                                    tool_call_id: tc.id,
                                    type: 'function',
                                    name: toolName,
                                    content: result.message
                                });
                                console.log(`[TOOL-2] ${toolName}(${tc.function.arguments}) -> ${result.success ? 'OK' : 'FAIL'}`);
                            }
                        }

                        // THIRD LLM CALL if second round had read tools
                        if (round2HasRead) {
                            console.log(`[AI] Third call... (${round2ToolResults.length} results)`);
                            const t2 = Date.now();
                            try {
                                const thirdMessages = [...secondMessages];
                                thirdMessages.push(secondChoice.message);
                                for (const tr of round2ToolResults) {
                                    thirdMessages.push({
                                        role: 'tool',
                                        tool_call_id: tr.tool_call_id,
                                        name: tr.name,
                                        content: tr.content
                                    });
                                }
                                const thirdResponse = await groqClient.chat.completions.create({
                                    model: 'llama-3.3-70b-versatile',
                                    messages: thirdMessages,
                                    tools: tools,
                                    tool_choice: 'none',
                                    temperature: 0.3,
                                    max_tokens: 1500
                                });
                                responseText = thirdResponse.choices[0].message.content || responseText;
                                console.log(`[AI] Third call complete (${Date.now() - t2}ms)`);
                            } catch (e3) {
                                console.log(`[AI] Third call failed: ${e3.message}`);
                            }
                        }
                    }
                } catch (e2) {
                    console.log(`[AI] Second call FAILED: ${e2.message}`);
                    // Fallback: use first call content if available
                    if (choice.message.content) {
                        responseText = choice.message.content;
                    }
                }
            }

            if (!responseText && choice.message.content) {
                responseText = choice.message.content;
            }

            if (!responseText) {
                for (const action of actions) {
                    if (action.tool === 'chart_add_symbol') {
                        responseText += `Chart diganti ke **${action.arguments.symbol}**. `;
                    } else if (action.tool === 'chart_add_indicator') {
                        responseText += `Indicator **${action.arguments.indicator.toUpperCase()}(${action.arguments.period || 'default'})** dipasang. `;
                    }
                }
            }
        } else {
            responseText = choice.message.content || 'Selesai.';
        }

        return { actions, response: responseText };
    } catch (e) {
        console.log('[GROQ] Error:', e.message);
        console.log('[GROQ] Fallback ke MOCK mode');
        return mockProcess(userMessage, chartStatus);
    }
}

// ── API Endpoints ───────────────────────────────────────────

app.get('/health', (req, res) => {
    res.json({
        status: 'ok',
        mode: USE_REAL_AI ? 'GROQ AI' : 'MOCK',
        ai_ready: !!groqClient
    });
});

app.post('/api/chat', async (req, res) => {
    const { message, history, chart_status, tool_data } = req.body;

    if (!message || message.trim().length === 0) {
        return res.json({ response: 'Pesan kosong.', actions: [] });
    }

    const t0 = Date.now();
    const chatHistory = history || [];

    console.log(`\n[CHAT] User: ${message}`);
    console.log(`[CHAT] History: ${chatHistory.length} messages`);

    if (chart_status) {
        console.log(`[CONTEXT] Chart: ${chart_status.symbol} ${chart_status.timeframe} | Trend: ${chart_status.trend} | Price: ${chart_status.price} | R:${chart_status.resistance} S:${chart_status.support}`);
    }
    if (tool_data) {
        const shCount = tool_data.swing_analysis?.swing_highs?.length || 0;
        const slCount = tool_data.swing_analysis?.swing_lows?.length || 0;
        console.log(`[TOOL_DATA] Swing: ${shCount} highs + ${slCount} lows | Levels: ${(tool_data.key_levels?.resistances?.length || 0) + (tool_data.key_levels?.supports?.length || 0)} levels`);
    }

    let result;

    if (USE_REAL_AI) {
        console.log('[AI] Mode: GROQ (real AI)');
        result = await groqProcess(message, chatHistory, chart_status, tool_data);
    } else {
        console.log('[MOCK] Processing locally (no AI key)');
        result = mockProcess(message, chart_status);
    }

    const elapsed = Date.now() - t0;
    console.log(`[${USE_REAL_AI ? 'AI' : 'MOCK'}] Response ready (${elapsed}ms). Actions: ${result.actions.length}`);

    for (const action of result.actions) {
        console.log(`[TOOL] Executing: ${action.tool}(${JSON.stringify(action.arguments)})`);
    }

    res.json({
        response: result.response,
        actions: result.actions
    });
});

// ── Startup ─────────────────────────────────────────────────
const PORT = 3000;

app.listen(PORT, async () => {
    console.log('');
    console.log('========================================');
    console.log('  JARVIS AI SERVER — Phase 6');
    console.log('  Drawing + Swing Analysis + Key Levels');
    console.log('========================================');
    console.log(`  Port: ${PORT}`);
    console.log(`  Mode: ${USE_REAL_AI ? 'GROQ AI (Real)' : 'MOCK (Offline)'}`);

    if (USE_REAL_AI) {
        console.log(`  API Key: ${API_KEY.substring(0, 10)}...`);
        console.log('  Loading Groq model...');
        const ok = initGroq();
        if (ok) {
            console.log('  Groq: READY');
            console.log('  Model: llama-3.3-70b-versatile');
            console.log('  Tools: chart_add_symbol, chart_add_indicator,');
            console.log('         chart_analyze_swing, chart_get_key_levels,');
            console.log('         chart_draw_line, chart_draw_rect,');
            console.log('         chart_draw_fib, chart_draw_text, chart_draw_elliot');
        } else {
            console.log('  Groq: FAILED -> Fallback MOCK');
            USE_REAL_AI = false;
        }
    } else {
        console.log('');
        console.log('  Untuk aktifkan AI asli (GRATIS):');
        console.log('  1. Buka https://console.groq.com/keys');
        console.log('  2. Login pakai GitHub (30 detik)');
        console.log('  3. Create API Key -> Copy');
        console.log('  4. Edit file ai_config.env');
        console.log('  5. Restart server ini');
        console.log('');
    }

    console.log('========================================');
    console.log(`  Health: http://localhost:${PORT}/health`);
    console.log('========================================');
    console.log('');
});
