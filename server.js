// ============================================================
// Jarvis AI Chat Backend Server
// Jembatan antara WASM (C++ ImGui) dan z.ai SDK
// Phase 1: 2 tools (chart_add_symbol + chart_add_indicator)
// ============================================================

const express = require('express');
const cors = require('cors');
const ZAI = require('z-ai-web-dev-sdk').default;

const app = express();
app.use(cors());
app.use(express.json({ limit: '1mb' }));

// ============================================================
// SYSTEM PROMPT — Persona Jarvis
// ============================================================
const SYSTEM_PROMPT = `Kamu adalah Jarvis, AI Assistant untuk platform Chart Trading.
Kamu bisa mengontrol chart dengan memanggil tools yang tersedia.

ATURAN:
1. Selalu analisis apa yang user minta SEBELUM memanggil tool
2. Jika user tidak menyebutkan symbol, TANYA dulu
3. Panggil tool yang paling relevan berdasarkan permintaan
4. Setelah tools selesai, jelaskan apa yang sudah dilakukan
5. Bisa berbahasa Indonesia atau English sesuai user
6. Jika ada error, jelaskan dengan jelas dan sarankan solusi
7. Jawab singkat dan to the point, jangan terlalu panjang

TOOLS YANG TERSEDIA:
- chart_add_symbol: Tambah symbol ke chart (BTCUSDT, ETHUSDT, XAUUSD, EURUSD, GBPUSD)
- chart_add_indicator: Tambah technical indicator (SMA, EMA, RSI, MACD, BB, Stochastic, ATR, ADX, CCI, Williams, Supertrend)

CONTOH:
User: "pasang RSI BTC" -> chart_add_indicator(symbol="BTCUSDT", indicator="RSI", period=14)
User: "tambah ETH" -> chart_add_symbol(symbol="ETHUSDT")
User: "analisa ETH" -> chart_add_symbol("ETHUSDT") + chart_add_indicator("ETHUSDT", "RSI", 14) + chart_add_indicator("ETHUSDT", "MACD")`;

// ============================================================
// TOOL DEFINITIONS (OpenAI Function Calling format)
// ============================================================
const TOOLS = [
    {
        type: "function",
        function: {
            name: "chart_add_symbol",
            description: "Tambahkan symbol trading ke chart. Gunakan format lengkap seperti BTCUSDT, ETHUSDT, XAUUSD, EURUSD, GBPUSD. Ini adalah langkah pertama sebelum melakukan operasi lain di chart.",
            parameters: {
                type: "object",
                properties: {
                    symbol: {
                        type: "string",
                        description: "Ticker symbol lengkap. Contoh: BTCUSDT, ETHUSDT, XAUUSD, EURUSD, GBPUSD"
                    }
                },
                required: ["symbol"]
            }
        }
    },
    {
        type: "function",
        function: {
            name: "chart_add_indicator",
            description: "Tambahkan technical indicator ke chart. Tersedia: SMA, EMA, RSI, MACD, Bollinger Bands (BB), Stochastic, ATR, ADX, CCI, Williams %R, Supertrend. Pastikan symbol sudah ada di chart sebelum menambah indicator.",
            parameters: {
                type: "object",
                properties: {
                    symbol: {
                        type: "string",
                        description: "Symbol target (harus sudah ada di chart). Contoh: BTCUSDT, ETHUSDT"
                    },
                    indicator: {
                        type: "string",
                        description: "Nama indicator (lowercase)",
                        enum: ["sma", "ema", "rsi", "macd", "bb", "stochastic", "atr", "adx", "cci", "williams", "supertrend"]
                    },
                    period: {
                        type: "integer",
                        description: "Period/lookback. Default: 14. RSI biasanya 14, SMA bisa 20/50/200"
                    },
                    source: {
                        type: "string",
                        description: "Price source: close, open, high, low. Default: close"
                    }
                },
                required: ["symbol", "indicator"]
            }
        }
    }
];

// ============================================================
// TOOL EXECUTION (simulasi — nanti bisa dihubungkan ke DB/ws)
// ============================================================
function executeTool(name, args) {
    console.log(`[TOOL] Executing: ${name}(${JSON.stringify(args)})`);

    switch (name) {
        case "chart_add_symbol": {
            const symbol = (args.symbol || "").toUpperCase();
            if (!symbol) {
                return { success: false, message: "Symbol tidak boleh kosong" };
            }
            // Normalisasi format symbol
            let normalized = symbol;
            if (symbol === "BTC") normalized = "BTCUSDT";
            else if (symbol === "ETH") normalized = "ETHUSDT";

            return {
                success: true,
                message: `Symbol ${normalized} berhasil ditambahkan ke chart.`,
                data: { symbol: normalized }
            };
        }

        case "chart_add_indicator": {
            const symbol = (args.symbol || "").toUpperCase();
            const indicator = (args.indicator || "").toLowerCase();
            const period = args.period || 14;
            const source = args.source || "close";

            if (!symbol || !indicator) {
                return { success: false, message: "Symbol dan indicator wajib diisi" };
            }

            // Normalisasi symbol
            let normalized = symbol;
            if (symbol === "BTC") normalized = "BTCUSDT";
            else if (symbol === "ETH") normalized = "ETHUSDT";

            const validIndicators = ["sma", "ema", "rsi", "macd", "bb", "stochastic", "atr", "adx", "cci", "williams", "supertrend"];
            if (!validIndicators.includes(indicator)) {
                return { success: false, message: `Indicator '${indicator}' tidak valid. Tersedia: ${validIndicators.join(", ")}` };
            }

            return {
                success: true,
                message: `Indicator ${indicator.toUpperCase()}(${period}) berhasil ditambahkan ke ${normalized}.`,
                data: { symbol: normalized, indicator, period, source }
            };
        }

        default:
            return { success: false, message: `Unknown tool: ${name}` };
    }
}

// ============================================================
// Z.AI SDK INITIALIZATION
// ============================================================
let zaiInstance = null;

async function getZAI() {
    if (!zaiInstance) {
        try {
            zaiInstance = await ZAI.create();
            console.log("[Z.AI] SDK initialized successfully");
        } catch (err) {
            console.error("[Z.AI] Failed to initialize:", err.message);
            throw new Error("Gagal inisialisasi z.ai SDK: " + err.message);
        }
    }
    return zaiInstance;
}

// ============================================================
// CHAT API — Main endpoint
// POST /api/chat
// ============================================================
app.post('/api/chat', async (req, res) => {
    const startTime = Date.now();

    try {
        const { message, history } = req.body;

        // Validasi input
        if (!message || typeof message !== 'string') {
            return res.status(400).json({
                error: "Message wajib diisi",
                response: "Maaf, terjadi kesalahan. Silakan coba lagi.",
                actions: []
            });
        }

        console.log(`[CHAT] User: ${message}`);
        console.log(`[CHAT] History: ${history ? history.length : 0} messages`);

        // 1. Bangun messages array untuk LLM
        const messages = [
            { role: "system", content: SYSTEM_PROMPT }
        ];

        // Tambahkan history (dari client)
        if (Array.isArray(history)) {
            for (const msg of history) {
                if (msg.role === "user" || msg.role === "assistant") {
                    messages.push({ role: msg.role, content: msg.content });
                }
            }
        }

        // Tambahkan pesan user saat ini
        messages.push({ role: "user", content: message });

        // 2. Inisialisasi z.ai SDK
        const zai = await getZAI();

        // 3. TOOL CALLING LOOP (max 5 ronde)
        const MAX_ROUNDS = 5;
        const allActions = []; // Kumpulan semua tool calls untuk dikirim ke client

        for (let round = 0; round < MAX_ROUNDS; round++) {
            console.log(`[LOOP] Round ${round + 1}/${MAX_ROUNDS}`);

            // Kirim ke AI
            const completion = await zai.chat.completions.create({
                messages: messages,
                tools: TOOLS,
                temperature: 0.3,
                max_tokens: 1024
            });

            const choice = completion.choices[0];
            const assistantMsg = choice.message;

            // Tambahkan response assistant ke messages
            messages.push(assistantMsg);

            // Cek apakah AI minta panggil tool
            if (assistantMsg.tool_calls && assistantMsg.tool_calls.length > 0) {
                console.log(`[LOOP] AI requests ${assistantMsg.tool_calls.length} tool call(s)`);

                // Execute setiap tool
                for (const toolCall of assistantMsg.tool_calls) {
                    const toolName = toolCall.function.name;
                    let toolArgs = {};

                    try {
                        toolArgs = JSON.parse(toolCall.function.arguments);
                    } catch (e) {
                        console.error(`[TOOL] Failed to parse args: ${toolCall.function.arguments}`);
                        toolArgs = {};
                    }

                    // Execute tool
                    const result = executeTool(toolName, toolArgs);
                    console.log(`[TOOL] ${toolName} -> ${result.success ? "OK" : "FAIL"}: ${result.message}`);

                    // Simpan action untuk dikirim ke client
                    allActions.push({
                        tool: toolName,
                        arguments: toolArgs,
                        result: {
                            success: result.success,
                            message: result.message
                        }
                    });

                    // Tambahkan tool result ke messages (untuk AI baca)
                    messages.push({
                        role: "tool",
                        tool_call_id: toolCall.id,
                        content: JSON.stringify(result)
                    });
                }

                // Lanjut loop — AI akan baca hasil tool dan bisa panggil lagi / jawab text
                continue;
            }

            // Tidak ada tool calls = AI sudah jawab text final
            console.log(`[LOOP] AI responded with text (no more tool calls)`);
            break;
        }

        // 4. Ambil response text terakhir dari AI
        let responseText = "";
        for (let i = messages.length - 1; i >= 0; i--) {
            if (messages[i].role === "assistant" && messages[i].content) {
                responseText = messages[i].content;
                break;
            }
        }

        // Fallback kalau AI tidak kasih text
        if (!responseText && allActions.length > 0) {
            const actionMsgs = allActions
                .filter(a => a.result.success)
                .map(a => {
                    const args = a.arguments;
                    if (a.tool === "chart_add_symbol") return `Added ${args.symbol}`;
                    if (a.tool === "chart_add_indicator") return `Added ${args.indicator}(${args.period || 14}) to ${args.symbol}`;
                    return `Executed ${a.tool}`;
                });
            responseText = "Done! " + actionMsgs.join(", ") + ". Ada yang lain?";
        }

        if (!responseText) {
            responseText = "Maaf, saya tidak bisa memproses permintaan tersebut. Coba ulangi dengan kalimat yang lebih jelas.";
        }

        const elapsed = Date.now() - startTime;
        console.log(`[CHAT] Response ready (${elapsed}ms). Actions: ${allActions.length}`);

        // 5. Kirim response ke client WASM
        res.json({
            response: responseText,
            actions: allActions,
            history: messages
                .filter(m => m.role === "user" || (m.role === "assistant" && !m.tool_calls))
                .map(m => ({ role: m.role, content: m.content }))
        });

    } catch (err) {
        console.error("[CHAT] Error:", err);

        res.status(500).json({
            error: err.message,
            response: `Maaf, terjadi error: ${err.message}. Silakan coba lagi.`,
            actions: []
        });
    }
});

// ============================================================
// HEALTH CHECK
// ============================================================
app.get('/api/health', (req, res) => {
    res.json({ status: "ok", timestamp: new Date().toISOString() });
});

// ============================================================
// START SERVER
// ============================================================
const PORT = process.env.PORT || 3000;

app.listen(PORT, () => {
    console.log("=".repeat(50));
    console.log("  JARVIS AI BACKEND SERVER");
    console.log("=".repeat(50));
    console.log(`  Port: ${PORT}`);
    console.log(`  API:  http://localhost:${PORT}/api/chat`);
    console.log(`  Health: http://localhost:${PORT}/api/health`);
    console.log("=".repeat(50));
    console.log("  Phase 1 Tools:");
    console.log("    - chart_add_symbol");
    console.log("    - chart_add_indicator");
    console.log("=".repeat(50));
});
