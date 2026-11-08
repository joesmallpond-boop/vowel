// VowelWorm — displays for the Max for Live device (jsui + mgraphics, ES5 for Max 8/9).
// jsarguments[1]: "compact" (device panel) or "expanded" (big window, EQ Eight-style).
// Input: "tele" lists from vowelworm~ (layout documented in Max/README.md).

mgraphics.init();
mgraphics.relative_coords = 0;
mgraphics.autofill = 0;

var mode = jsarguments.length > 1 ? String(jsarguments[1]) : "compact";
var T = [];
for (var i = 0; i < 63; i++) T[i] = 0;
var trail = [];          // recent vowel positions
var pitchHist = [];      // [hz, conf] history for the pitch trace
var TRAIL = mode == "compact" ? 30 : 60;
var HIST = 240;

// Live-ish palette (neutral display greys, orange "voice", blue "synth")
var C_BG = [0.11, 0.11, 0.11, 1], C_GRID = [0.22, 0.22, 0.22, 1], C_TXT = [0.62, 0.62, 0.62, 1];
var C_ORANGE = [1.0, 0.62, 0.24, 1], C_BLUE = [0.36, 0.72, 0.94, 1], C_GREEN = [0.55, 0.85, 0.35, 1], C_DIM = [0.35, 0.35, 0.35, 1];
var NOTE = ["C","C#","D","D#","E","F","F#","G","G#","A","A#","B"];

function tele()
{
    for (var i = 0; i < arguments.length && i < 63; i++) T[i] = arguments[i];
    if (T[1] > 0.5) { trail.push([T[2], T[3]]); if (trail.length > TRAIL) trail.shift(); }
    pitchHist.push([T[0], T[1]]); if (pitchHist.length > HIST) pitchHist.shift();
    mgraphics.redraw();
}

function col(c, a) { mgraphics.set_source_rgba(c[0], c[1], c[2], a === undefined ? c[3] : a); }
function txt(s, x, y, size) { mgraphics.select_font_face("Arial"); mgraphics.set_font_size(size || 9); mgraphics.move_to(x, y); mgraphics.show_text(s); }
function noteName(n) { return n < 0 ? "-" : NOTE[n % 12] + (Math.floor(n / 12) - 1); }
function hzToMidi(hz) { return 69 + 12 * Math.log(Math.max(hz, 1) / 440) / Math.LN2; }

function panel(x, y, w, h, title)
{
    col(C_BG); mgraphics.rectangle(x, y, w, h); mgraphics.fill();
    if (title) { col(C_TXT); txt(title, x + 6, y + 12, 9); }
}

// ---- vowel space: trail + head with a tiny mouth (the worm, Live-style) -------------
function drawVowel(x, y, w, h, big)
{
    panel(x, y, w, h, big ? "VOWEL SPACE" : "");
    var px = x + (big ? 26 : 8), py = y + (big ? 20 : 6), pw = w - (big ? 40 : 16), ph = h - (big ? 40 : 14);
    col(C_GRID); mgraphics.set_line_width(1);
    for (var i = 0; i <= 4; i++)
    {
        mgraphics.move_to(px + pw * i / 4, py); mgraphics.line_to(px + pw * i / 4, py + ph); mgraphics.stroke();
        mgraphics.move_to(px, py + ph * i / 4); mgraphics.line_to(px + pw, py + ph * i / 4); mgraphics.stroke();
    }
    var marks = [["oo", 0.05, 0.1], ["oh", 0.08, 0.62], ["ah", 0.33, 0.92], ["eh", 0.78, 0.55], ["ee", 0.95, 0.1]];
    col(C_TXT);
    for (var m = 0; m < marks.length; m++) txt(marks[m][0], px + marks[m][1] * pw - 5, py + (1 - marks[m][2]) * ph + 3, big ? 10 : 8);
    if (big) { txt("back", px, py + ph + 12, 8); txt("front", px + pw - 22, py + ph + 12, 8); txt("open", x + 4, py + 8, 8); txt("closed", x + 2, py + ph, 8); }

    // trail, fading from tail to head
    for (var k = 1; k < trail.length; k++)
    {
        var a = k / trail.length;
        col(C_ORANGE, 0.15 + 0.75 * a);
        mgraphics.set_line_width((big ? 2 : 1) + (big ? 6 : 3) * a);
        mgraphics.move_to(px + trail[k - 1][0] * pw, py + (1 - trail[k - 1][1]) * ph);
        mgraphics.line_to(px + trail[k][0] * pw, py + (1 - trail[k][1]) * ph);
        mgraphics.stroke();
    }
    var hx = px + T[2] * pw, hy = py + (1 - T[3]) * ph, r = big ? 9 : 5;
    var awake = T[1] > 0.5;
    col(awake ? C_ORANGE : C_DIM);
    mgraphics.ellipse(hx - r, hy - r, r * 2, r * 2); mgraphics.fill();
    // the mouth is the vowel
    var mw = (big ? 3 : 2) + (big ? 9 : 5) * T[2], mh = 1 + (big ? 8 : 4) * T[3];
    col(C_BG);
    if (awake) { mgraphics.ellipse(hx - mw / 2, hy + r * 0.15, mw, mh); mgraphics.fill(); }
    if (big) { col(C_TXT); txt("F1 " + Math.round(T[4]) + "  F2 " + Math.round(T[5]), x + w - 92, y + 12, 9); }
}

// ---- pitch trace with held harmony notes (like a tuner history) ---------------------
function drawPitch(x, y, w, h)
{
    panel(x, y, w, h, "PITCH & HARMONY");
    var lo = 36, hi = 84;
    var mid = 60;
    for (var j = pitchHist.length - 1; j >= 0; j--) if (pitchHist[j][0] > 0) { mid = hzToMidi(pitchHist[j][0]); break; }
    lo = Math.round(mid) - 18; hi = lo + 36;
    function yOf(n) { return y + h - 6 - (n - lo) / (hi - lo) * (h - 22); }
    col(C_GRID); mgraphics.set_line_width(1);
    for (var n = lo; n <= hi; n++) if (n % 12 == 0) { mgraphics.move_to(x + 28, yOf(n)); mgraphics.line_to(x + w - 4, yOf(n)); mgraphics.stroke(); col(C_TXT); txt("C" + (n / 12 - 1), x + 4, yOf(n) + 3, 8); col(C_GRID); }
    // harmony voices as horizontal bars
    for (var v = 0; v < 8; v++)
    {
        var note = T[16 + v];
        if (note >= 0 && note >= lo && note <= hi) { col(C_BLUE, 0.35 + 0.6 * Math.min(1, T[24 + v])); mgraphics.rectangle(x + 28, yOf(note) - 1.5, w - 32, 3); mgraphics.fill(); }
    }
    // sung pitch line
    col(C_ORANGE); mgraphics.set_line_width(2);
    var started = false;
    for (var i = 0; i < pitchHist.length; i++)
    {
        var px = x + 28 + (w - 32) * i / (HIST - 1);
        if (pitchHist[i][0] > 0 && pitchHist[i][1] > 0.5)
        {
            var py = yOf(hzToMidi(pitchHist[i][0]));
            if (!started) { mgraphics.move_to(px, py); started = true; } else mgraphics.line_to(px, py);
        }
        else if (started) { mgraphics.stroke(); started = false; }
    }
    if (started) mgraphics.stroke();
    col(C_TXT);
    var hz = T[0];
    txt(hz > 0 ? noteName(Math.round(hzToMidi(hz))) + "  " + hz.toFixed(1) + " Hz   delay " + T[14].toFixed(1) + " ms" : "-", x + w - 170, y + 12, 9);
}

// ---- expression meters --------------------------------------------------------------
function drawMeters(x, y, w, h)
{
    panel(x, y, w, h, "EXPRESSION");
    var names = ["Amp", "Attack", "Bright", "Breath", "Pitch", "Vibrato", "Voiced"];
    var vals = [T[6], T[7], T[8], T[9], 0.5 + 0.5 * T[10], T[11], T[1]];
    var rh = (h - 20) / names.length;
    for (var i = 0; i < names.length; i++)
    {
        var yy = y + 18 + rh * i;
        col(C_TXT); txt(names[i], x + 6, yy + rh * 0.6, 9);
        col(C_GRID); mgraphics.rectangle(x + 56, yy + rh * 0.25, w - 64, rh * 0.5); mgraphics.fill();
        col(i == 4 ? C_BLUE : C_ORANGE);
        if (i == 4) { var c = x + 56 + (w - 64) / 2, e = x + 56 + (w - 64) * vals[i]; mgraphics.rectangle(Math.min(c, e), yy + rh * 0.25, Math.abs(e - c) + 1, rh * 0.5); }
        else mgraphics.rectangle(x + 56, yy + rh * 0.25, (w - 64) * Math.max(0, Math.min(1, vals[i])), rh * 0.5);
        mgraphics.fill();
    }
}

// ---- synth vowel filter response (EQ Eight-style curve) -----------------------------
function drawFilter(x, y, w, h)
{
    panel(x, y, w, h, "SYNTH VOWEL FILTER");
    var fs = Math.pow(2, (-12 + 24 * T[32 + 1]) / 12);
    var f = [Math.max(150, T[4]) * fs, Math.max(400, T[5]) * fs, (2300 + 900 * T[2]) * fs];
    var res = T[32 + 12], q = 2.5 + 14 * res, mix = 0.25 + 0.75 * res;
    var g = [1, 0.6, 0.3], qs = [q, q * 1.2, q * 1.4];
    var lo = Math.log(100), hi = Math.log(6000);
    col(C_GRID); mgraphics.set_line_width(1);
    var grid = [200, 500, 1000, 2000, 5000];
    for (var k = 0; k < grid.length; k++) { var gx = x + w * (Math.log(grid[k]) - lo) / (hi - lo); mgraphics.move_to(gx, y + 16); mgraphics.line_to(gx, y + h - 4); mgraphics.stroke(); }
    col(C_BLUE); mgraphics.set_line_width(1.5);
    for (var i = 0; i <= 120; i++)
    {
        var hz = Math.exp(lo + (hi - lo) * i / 120), re = 1 - mix, im = 0;
        for (var b = 0; b < 3; b++)
        {
            var wn = hz / f[b], dr = 1 - wn * wn, di = wn / qs[b], nr = 0, ni = wn / qs[b];
            var den = dr * dr + di * di;
            re += mix * g[b] * 1.4 * (nr * dr + ni * di) / den;
            im += mix * g[b] * 1.4 * (ni * dr - nr * di) / den;
        }
        var db = Math.max(-36, Math.min(12, 20 * Math.log(Math.sqrt(re * re + im * im) + 1e-6) / Math.LN10));
        var px = x + w * i / 120, py = y + h - 4 - (db + 36) / 48 * (h - 22);
        if (i == 0) mgraphics.move_to(px, py); else mgraphics.line_to(px, py);
    }
    mgraphics.stroke();
}

// ---- voices -------------------------------------------------------------------------
function drawVoices(x, y, w, h)
{
    panel(x, y, w, h, "VOICES");
    var n = Math.max(1, T[15]);
    var cw = (w - 12) / 8;
    var blend = T[32 + 3];
    for (var v = 0; v < 8; v++)
    {
        var cx = x + 6 + cw * v, on = T[16 + v] >= 0, lv = Math.min(1, T[24 + v]);
        col(v < n ? C_GRID : [0.15, 0.15, 0.15, 1]); mgraphics.rectangle(cx + 2, y + 18, cw - 4, h - 34); mgraphics.fill();
        if (on)
        {
            var bh = (h - 34) * (0.1 + 0.9 * lv);
            col(C_ORANGE); mgraphics.rectangle(cx + 2, y + h - 16 - bh, (cw - 4) * (1 - blend), bh); mgraphics.fill();
            col(C_BLUE); mgraphics.rectangle(cx + 2 + (cw - 4) * (1 - blend), y + h - 16 - bh, (cw - 4) * blend, bh); mgraphics.fill();
        }
        col(on ? [0.85, 0.85, 0.85, 1] : C_DIM);
        txt(on ? noteName(T[16 + v]) : String(v + 1), cx + 4, y + h - 4, 9);
    }
}

function paint()
{
    var w = box.rect[2] - box.rect[0], h = box.rect[3] - box.rect[1];
    if (mode == "compact")
    {
        drawVowel(0, 0, w, h - 18, false);
        // pitch + status strip
        col(C_BG); mgraphics.rectangle(0, h - 16, w, 16); mgraphics.fill();
        var voiced = T[1] > 0.5, hz = T[0];
        col(voiced ? C_ORANGE : C_DIM); txt(voiced ? noteName(Math.round(hzToMidi(hz))) : "--", 4, h - 4, 10);
        col(T[12] > 0.003 ? C_GREEN : C_DIM); mgraphics.ellipse(w - 46, h - 12, 7, 7); mgraphics.fill();
        var held = 0; for (var k = 53; k < 61; k++) if (T[k] > 0) held++;
        col(held > 0 ? C_ORANGE : C_DIM); mgraphics.ellipse(w - 34, h - 12, 7, 7); mgraphics.fill();
        col(voiced ? C_BLUE : C_DIM); mgraphics.ellipse(w - 22, h - 12, 7, 7); mgraphics.fill();
        col(C_TXT); txt("in  midi  voice", w - 120, h - 4, 7);
        return;
    }
    // expanded layout
    var gap = 6, colW = Math.floor((w - gap) / 2);
    drawVowel(0, 0, colW, Math.floor(h * 0.62), true);
    drawPitch(colW + gap, 0, w - colW - gap, Math.floor(h * 0.62));
    var y2 = Math.floor(h * 0.62) + gap, h2 = h - y2;
    var w3 = Math.floor((w - 2 * gap) / 3);
    drawVoices(0, y2, w3, h2);
    drawMeters(w3 + gap, y2, w3, h2);
    drawFilter(2 * (w3 + gap), y2, w - 2 * (w3 + gap), h2);
}

function onresize() { mgraphics.redraw(); }
