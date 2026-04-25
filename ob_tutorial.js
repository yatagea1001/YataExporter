// ================================================================
// ob_tutorial.js — Order Book Tutorial Overlay
// ================================================================
// Dipanggil dari C++ via EM_ASM saat user klik tombol "?"
// Membuat div HTML yang overlay persis di atas window ImGui OB
//
// Cara kerja:
//   C++ tahu posisi window → kirim (x,y,w,h) ke JS
//   JS buat div di posisi tersebut → tampil di atas canvas
//   Klik lagi → div hilang (toggle)
// ================================================================

(function() {

// ─────────────────────────────────────────────────────────────────
// STATE
// ─────────────────────────────────────────────────────────────────
let tutorialEl   = null;
let isVisible    = false;
let currentStep  = 0;

// ─────────────────────────────────────────────────────────────────
// KONTEN TUTORIAL — 5 langkah interaktif
// ─────────────────────────────────────────────────────────────────
const steps = [
    {
        title: "📖 Apa itu Order Book?",
        icon:  "📋",
        color: "#3a7bd5",
        content: `
            <p>Order Book adalah daftar <strong>semua order aktif</strong> yang menunggu di pasar.</p>
            <div class="ob-split">
                <div class="ob-ask-ex">
                    <span>ASK (Jual)</span>
                    <small>Seller siap jual di harga ini</small>
                </div>
                <div class="ob-spread-ex">SPREAD</div>
                <div class="ob-bid-ex">
                    <span>BID (Beli)</span>
                    <small>Buyer siap beli di harga ini</small>
                </div>
            </div>
            <p class="ob-tip">💡 Semakin besar bar → semakin banyak order di harga itu</p>
        `
    },
    {
        title: "⚖️ Imbalance Bar",
        icon:  "⚖️",
        color: "#f39c12",
        content: `
            <p>Bar di bagian <strong>atas OB</strong> menunjukkan dominasi pasar saat ini.</p>
            <div style="background:#12151e;border-radius:6px;padding:10px;margin:8px 0">
                <div style="display:flex;height:18px;border-radius:4px;overflow:hidden;margin-bottom:8px">
                    <div style="width:67%;background:rgba(39,174,96,0.8);display:flex;align-items:center;padding:0 6px">
                        <span style="font-size:10px;color:#6bff9a;font-weight:bold">B 67%</span>
                    </div>
                    <div style="width:33%;background:rgba(192,57,43,0.8);display:flex;align-items:center;justify-content:flex-end;padding:0 6px">
                        <span style="font-size:10px;color:#ff8a80;font-weight:bold">A 33%</span>
                    </div>
                </div>
                <div style="text-align:center;font-size:11px;color:#6bff9a;font-weight:bold">BUY PRESSURE ↑</div>
            </div>
            <div style="display:grid;grid-template-columns:1fr 1fr;gap:6px;margin:8px 0;font-size:10px">
                <div style="background:rgba(39,174,96,0.1);border:1px solid rgba(39,174,96,0.3);border-radius:4px;padding:6px">
                    <strong style="color:#6bff9a">BID > 60%</strong><br>
                    <span style="color:#5a8a6a">Pembeli dominan<br>→ potensi naik</span>
                </div>
                <div style="background:rgba(192,57,43,0.1);border:1px solid rgba(192,57,43,0.3);border-radius:4px;padding:6px">
                    <strong style="color:#ff8a80">ASK > 60%</strong><br>
                    <span style="color:#8a5a5a">Penjual dominan<br>→ potensi turun</span>
                </div>
            </div>
            <p class="ob-tip">💡 Update otomatis tiap 2 detik dari data real Hyperliquid</p>
        `
    },
    {
        title: "🔴 Membaca ASK (Merah)",
        icon:  "🔴",
        color: "#c0392b",
        content: `
            <p>Bar <strong style="color:#ff6b6b">merah</strong> = ASK = penjual yang menunggu.</p>
            <div class="ob-demo">
                <div class="ob-row ask-row big-bar">
                    <span class="ob-price">66481.0</span>
                    <span class="ob-size">13.698</span>
                    <div class="bar ask-bar" style="width:90%"></div>
                </div>
                <div class="ob-row ask-row">
                    <span class="ob-price">66483.0</span>
                    <span class="ob-size">3.932</span>
                    <div class="bar ask-bar" style="width:25%"></div>
                </div>
            </div>
            <p>Bar <strong style="color:#f39c12">⚠️ WALL</strong> = order sangat besar = <strong>resistance kuat</strong></p>
            <p class="ob-tip">💡 Harga sulit naik melewati WALL ask yang besar</p>
        `
    },
    {
        title: "🟢 Membaca BID (Hijau)",
        icon:  "🟢",
        color: "#27ae60",
        content: `
            <p>Bar <strong style="color:#6bff9a">hijau</strong> = BID = pembeli yang menunggu.</p>
            <div class="ob-demo">
                <div class="ob-row bid-row">
                    <span class="ob-price">66468.0</span>
                    <span class="ob-size">6.439</span>
                    <div class="bar bid-bar" style="width:40%"></div>
                </div>
                <div class="ob-row bid-row big-bar">
                    <span class="ob-price">66458.0</span>
                    <span class="ob-size">4.706</span>
                    <div class="bar bid-bar" style="width:30%"></div>
                </div>
            </div>
            <p>Bid besar di bawah harga = <strong style="color:#6bff9a">support kuat</strong></p>
            <p class="ob-tip">💡 Harga cenderung mantul naik dari WALL bid yang besar</p>
        `
    },
    {
        title: "📊 Spread = Biaya Masuk",
        icon:  "📊",
        color: "#8e44ad",
        content: `
            <p><strong>Spread</strong> = selisih antara ASK terbaik dan BID terbaik.</p>
            <div class="spread-demo">
                <div style="color:#ff6b6b">ASK terbaik: <strong>66481.0</strong></div>
                <div class="spread-row">↕ Spread: <strong style="color:#f1c40f">1.0</strong></div>
                <div style="color:#6bff9a">BID terbaik: <strong>66480.0</strong></div>
            </div>
            <p>Spread <strong>kecil</strong> = market liquid = mudah entry/exit</p>
            <p>Spread <strong>besar</strong> = market sepi = hati-hati slippage</p>
            <p class="ob-tip">💡 BTC biasanya spread 0.5–2.0 = sangat liquid</p>
        `
    },
    {
        title: "🎯 Strategi dari Order Book",
        icon:  "🎯",
        color: "#e67e22",
        content: `
            <div class="strategy-list">
                <div class="strategy-item">
                    <span class="s-icon">🏰</span>
                    <div>
                        <strong>Wall Detection</strong>
                        <p>WALL ask besar = target resistance. WALL bid besar = zona support.</p>
                    </div>
                </div>
                <div class="strategy-item">
                    <span class="s-icon">⚖️</span>
                    <div>
                        <strong>Imbalance</strong>
                        <p>Bid jauh lebih besar dari ask → tekanan beli → harga cenderung naik.</p>
                    </div>
                </div>
                <div class="strategy-item">
                    <span class="s-icon">🌊</span>
                    <div>
                        <strong>Thin Book</strong>
                        <p>Sedikit order di satu sisi → harga bisa bergerak cepat ke sana.</p>
                    </div>
                </div>
            </div>
            <p class="ob-tip">💡 Gabungkan OB dengan footprint untuk analisa terlengkap!</p>
        `
    }
];

// ─────────────────────────────────────────────────────────────────
// CSS — inject sekali
// ─────────────────────────────────────────────────────────────────
function injectCSS() {
    if (document.getElementById('ob-tut-style')) return;
    const style = document.createElement('style');
    style.id = 'ob-tut-style';
    style.textContent = `
        #ob-tutorial {
            position: fixed;
            background: linear-gradient(160deg, #0d1117 0%, #0f1923 100%);
            border: 1px solid #1e3a5f;
            border-radius: 10px;
            box-shadow: 0 8px 32px rgba(0,0,0,0.8), 0 0 0 1px rgba(58,123,213,0.15);
            font-family: 'Segoe UI', system-ui, sans-serif;
            font-size: 13px;
            color: #c8d6e5;
            z-index: 9999;
            overflow: hidden;
            animation: ob-tut-in 0.2s ease-out;
            width: 300px;
        }
        @keyframes ob-tut-in {
            from { opacity:0; transform: translateY(-8px) scale(0.97); }
            to   { opacity:1; transform: translateY(0) scale(1); }
        }
        #ob-tut-header {
            padding: 12px 14px 10px;
            border-bottom: 1px solid #1a2a3a;
            display: flex;
            align-items: center;
            gap: 8px;
        }
        #ob-tut-header h3 {
            margin: 0;
            font-size: 13px;
            font-weight: 600;
            color: #e0eeff;
            flex: 1;
        }
        #ob-tut-close {
            background: none;
            border: none;
            color: #4a6a8a;
            cursor: pointer;
            font-size: 16px;
            padding: 0 4px;
            line-height: 1;
        }
        #ob-tut-close:hover { color: #ff6b6b; }
        #ob-tut-body {
            padding: 14px;
            min-height: 160px;
        }
        #ob-tut-body p { margin: 6px 0; line-height: 1.5; }
        .ob-tip {
            background: rgba(58,123,213,0.1);
            border-left: 3px solid #3a7bd5;
            padding: 6px 10px;
            border-radius: 4px;
            margin-top: 10px !important;
            font-size: 12px;
        }
        .ob-split {
            display: flex;
            flex-direction: column;
            gap: 4px;
            margin: 10px 0;
        }
        .ob-ask-ex {
            background: rgba(192,57,43,0.2);
            border: 1px solid rgba(192,57,43,0.4);
            padding: 6px 10px;
            border-radius: 4px;
            display: flex;
            justify-content: space-between;
            color: #ff8a80;
        }
        .ob-spread-ex {
            text-align: center;
            color: #f1c40f;
            font-size: 11px;
            padding: 2px;
        }
        .ob-bid-ex {
            background: rgba(39,174,96,0.2);
            border: 1px solid rgba(39,174,96,0.4);
            padding: 6px 10px;
            border-radius: 4px;
            display: flex;
            justify-content: space-between;
            color: #6bff9a;
        }
        .ob-demo { margin: 10px 0; }
        .ob-row {
            display: flex;
            align-items: center;
            padding: 3px 0;
            gap: 6px;
            position: relative;
        }
        .ob-price { font-size: 12px; width: 80px; font-family: monospace; }
        .ob-size  { font-size: 12px; width: 50px; text-align: right; font-family: monospace; }
        .bar {
            height: 14px;
            border-radius: 2px;
            transition: width 0.3s;
        }
        .ask-bar { background: rgba(192,57,43,0.7); }
        .bid-bar { background: rgba(39,174,96,0.7); }
        .big-bar .ask-bar { background: rgba(220,50,50,0.9); box-shadow: 0 0 6px rgba(255,100,50,0.4); }
        .big-bar .bid-bar { background: rgba(50,200,100,0.9); box-shadow: 0 0 6px rgba(50,255,100,0.4); }
        .ask-row .ob-price, .ask-row .ob-size { color: #ff8a80; }
        .bid-row .ob-price, .bid-row .ob-size { color: #6bff9a; }
        .spread-demo {
            background: rgba(142,68,173,0.1);
            border: 1px solid rgba(142,68,173,0.3);
            border-radius: 6px;
            padding: 10px;
            margin: 10px 0;
            text-align: center;
        }
        .spread-row {
            padding: 4px 0;
            color: #f1c40f;
            font-size: 14px;
        }
        .strategy-list { display: flex; flex-direction: column; gap: 8px; }
        .strategy-item {
            display: flex;
            gap: 10px;
            align-items: flex-start;
            background: rgba(255,255,255,0.03);
            border-radius: 6px;
            padding: 8px;
        }
        .s-icon { font-size: 18px; flex-shrink: 0; }
        .strategy-item p { margin: 2px 0; font-size: 12px; color: #8aa; }
        .strategy-item strong { color: #e0eeff; }
        #ob-tut-nav {
            display: flex;
            align-items: center;
            padding: 10px 14px;
            border-top: 1px solid #1a2a3a;
            gap: 8px;
        }
        .ob-dot {
            width: 6px; height: 6px;
            border-radius: 50%;
            background: #2a3a4a;
            cursor: pointer;
            transition: background 0.2s;
        }
        .ob-dot.active { background: #3a7bd5; }
        #ob-tut-nav button {
            background: rgba(58,123,213,0.2);
            border: 1px solid rgba(58,123,213,0.4);
            color: #7ab0e0;
            border-radius: 5px;
            padding: 5px 12px;
            cursor: pointer;
            font-size: 12px;
            transition: all 0.15s;
        }
        #ob-tut-nav button:hover {
            background: rgba(58,123,213,0.4);
            color: #e0eeff;
        }
        #ob-tut-nav button:disabled {
            opacity: 0.3;
            cursor: default;
        }
        #ob-tut-counter {
            flex: 1;
            text-align: center;
            font-size: 11px;
            color: #4a6a8a;
        }
        .step-indicator {
            display: flex;
            gap: 4px;
            justify-content: center;
            margin-bottom: 6px;
        }
    `;
    document.head.appendChild(style);
}

// ─────────────────────────────────────────────────────────────────
// BUILD HTML
// ─────────────────────────────────────────────────────────────────
function buildTutorial() {
    const el = document.createElement('div');
    el.id = 'ob-tutorial';

    el.innerHTML = `
        <div id="ob-tut-header">
            <span style="font-size:16px">${steps[currentStep].icon}</span>
            <h3 id="ob-tut-title">${steps[currentStep].title}</h3>
            <button id="ob-tut-close" title="Tutup">✕</button>
        </div>
        <div id="ob-tut-body">${steps[currentStep].content}</div>
        <div id="ob-tut-nav">
            <button id="ob-prev" ${currentStep===0?'disabled':''}>← Prev</button>
            <div id="ob-tut-counter">
                <div class="step-indicator">
                    ${steps.map((_,i) =>
                        `<div class="ob-dot ${i===currentStep?'active':''}" data-step="${i}"></div>`
                    ).join('')}
                </div>
            </div>
            <button id="ob-next" ${currentStep===steps.length-1?'disabled':''}>Next →</button>
        </div>
    `;

    // Events
    el.querySelector('#ob-tut-close').onclick = hide;
    el.querySelector('#ob-prev').onclick = () => goStep(currentStep - 1);
    el.querySelector('#ob-next').onclick = () => goStep(currentStep + 1);
    el.querySelectorAll('.ob-dot').forEach(dot => {
        dot.onclick = () => goStep(parseInt(dot.dataset.step));
    });

    return el;
}

// ─────────────────────────────────────────────────────────────────
// NAVIGASI STEP
// ─────────────────────────────────────────────────────────────────
function goStep(n) {
    currentStep = Math.max(0, Math.min(steps.length-1, n));
    if (!tutorialEl) return;

    // Update konten dengan animasi
    const body = tutorialEl.querySelector('#ob-tut-body');
    body.style.opacity = '0';
    setTimeout(() => {
        body.innerHTML = steps[currentStep].content;
        tutorialEl.querySelector('#ob-tut-title').textContent = steps[currentStep].title;
        tutorialEl.querySelector('#ob-tut-title').previousElementSibling.textContent = steps[currentStep].icon;
        tutorialEl.querySelector('#ob-prev').disabled = currentStep === 0;
        tutorialEl.querySelector('#ob-next').disabled = currentStep === steps.length - 1;
        tutorialEl.querySelectorAll('.ob-dot').forEach((d,i) => {
            d.classList.toggle('active', i === currentStep);
        });
        // Accent color sesuai step
        tutorialEl.querySelector('#ob-tut-header').style.borderBottom =
            `1px solid ${steps[currentStep].color}40`;
        body.style.opacity = '1';
        body.style.transition = 'opacity 0.15s';
    }, 100);
}

// ─────────────────────────────────────────────────────────────────
// POSISIKAN div RELATIF ke window ImGui
// ─────────────────────────────────────────────────────────────────
function position(wx, wy, ww, wh) {
    if (!tutorialEl) return;

    const canvas = document.querySelector('canvas');
    const canvasRect = canvas ? canvas.getBoundingClientRect() : {left:0, top:0};

    // Skala: ImGui pakai pixel koordinat canvas, browser pakai CSS pixel
    const scaleX = canvas ? canvas.getBoundingClientRect().width  / canvas.width  : 1;
    const scaleY = canvas ? canvas.getBoundingClientRect().height / canvas.height : 1;

    // Posisi: coba di kanan window OB, kalau tidak muat → di kiri
    const tutW = 300;
    let left = canvasRect.left + (wx + ww) * scaleX + 8;
    let top  = canvasRect.top  + wy * scaleY;

    // Kalau melewati kanan layar → pindah ke kiri
    if (left + tutW > window.innerWidth - 10) {
        left = canvasRect.left + wx * scaleX - tutW - 8;
    }
    // Jangan keluar dari bawah layar
    const maxTop = window.innerHeight - tutorialEl.offsetHeight - 10;
    if (top > maxTop) top = maxTop;
    top = Math.max(top, 10);

    tutorialEl.style.left = left + 'px';
    tutorialEl.style.top  = top  + 'px';
}

// ─────────────────────────────────────────────────────────────────
// SHOW / HIDE / TOGGLE
// ─────────────────────────────────────────────────────────────────
function hide() {
    if (tutorialEl) {
        tutorialEl.style.animation = 'ob-tut-in 0.15s ease-in reverse';
        setTimeout(() => {
            if (tutorialEl) {
                tutorialEl.remove();
                tutorialEl = null;
            }
        }, 140);
    }
    isVisible = false;
}

window.toggleOBTutorial = function(wx, wy, ww, wh) {
    injectCSS();

    if (isVisible && tutorialEl) {
        hide();
        return;
    }

    currentStep = 0;
    tutorialEl = buildTutorial();
    document.body.appendChild(tutorialEl);
    isVisible = true;

    // Posisikan setelah DOM render
    requestAnimationFrame(() => position(wx, wy, ww, wh));
};

// Juga expose fungsi hide untuk pakai dari luar
window.hideOBTutorial = hide;

console.log('%c[OB Tutorial] ob_tutorial.js loaded ✅', 'color:#3a7bd5;font-weight:bold');

})(); // IIFE — tidak pollute global scope
