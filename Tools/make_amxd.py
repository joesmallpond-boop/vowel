#!/usr/bin/env python3
"""Generate the VowelWorm Max for Live device (VowelWorm.amxd) from the engine's parameter table.

    ./build/vw_paramdump > build/params.json
    python3 Tools/make_amxd.py build/params.json Max/package/VowelWorm.amxd

Every engine parameter becomes exactly one native Live parameter (live.dial / live.numbox /
live.menu / live.tab / live.text), so the device automates, maps and stores like a stock device.
The front panel holds the performance controls; an "Expand" button opens a larger floating window
(like EQ Eight's expanded view) with big displays and all the deeper controls.

Wiring per control:  control -> [prepend param <id>] -> [s ---vwin] -> vowelworm~
Back channel (presets, calibration): vowelworm~ "p <id> <v>" -> [route ids...] -> [prepend set] -> [s ---set_<id>] -> control
"""
import json, math, struct, sys

params_path = sys.argv[1] if len(sys.argv) > 1 else "build/params.json"
out_path = sys.argv[2] if len(sys.argv) > 2 else "Max/package/VowelWorm.amxd"
spec = json.load(open(params_path))
P = {p["id"]: p for p in spec["params"]}
PRESETS = spec["presets"]

# ------------------------------------------------------------------ helpers
class Patch:
    def __init__(self):
        self.boxes, self.lines, self.n = [], [], 0
        self.y = 20
    def box(self, **kw):
        self.n += 1
        bid = f"obj-{self.n}"
        b = {"id": bid}
        b.update(kw)
        if "patching_rect" not in b:
            b["patching_rect"] = [20.0 + (self.n % 6) * 160.0, 20.0 + (self.n // 6) * 30.0, 150.0, 22.0]
        self.boxes.append({"box": b})
        return bid
    def obj(self, text, ins=1, outs=1, outtypes=None, rect=None):
        kw = dict(maxclass="newobj", text=text, numinlets=ins, numoutlets=outs, outlettype=outtypes or [""] * outs)
        if rect: kw["patching_rect"] = rect
        return self.box(**kw)
    def line(self, a, ao, b, bi):
        self.lines.append({"patchline": {"source": [a, ao], "destination": [b, bi]}})

def is_percent(p):
    return p["type"] == "float" and p["unit"] == "" and p["min"] >= -1.0 and p["max"] <= 1.0

SHORT = {
    "glide": "Glide", "formantShift": "Formant", "formantEmphasis": "Emphasis", "spread": "Spread",
    "expressTransfer": "Expression", "harmonyLevel": "Harmony", "leadLevel": "Lead", "harmonyAttack": "Attack",
    "harmonyRelease": "Release", "consonantLevel": "Consonant", "blend": "Synth Blend", "oscMorph": "Morph",
    "synthBrightness": "Bright", "synthResonance": "Reson", "noise": "Noise", "dynamicsFollow": "Dynamics",
    "synthLevel": "Synth Lvl", "outputLevel": "Output", "voices": "Voices", "octave": "Octave",
    "synthOctave": "Syn Oct", "relRoot": "Rel Root", "bendRange": "Bend", "velSens": "Velocity",
    "vibratoDepth": "Vibrato", "vibratoRate": "Vib Rate", "formantFollow": "Fmt Follow", "gate": "Gate",
    "analysisSmooth": "Ana Smooth", "vowelSmooth": "Vow Smooth", "vowelSens": "Vow Sens",
    "f1Lo": "F1 Low", "f1Hi": "F1 High", "f2Lo": "F2 Low", "f2Hi": "F2 High", "formantTrack": "Vowel Trk",
    "synthAttack": "Syn Att", "synthRelease": "Syn Rel", "lfo1Rate": "LFO1 Rate", "lfo2Rate": "LFO2 Rate",
    "lfo1Shape": "LFO1", "lfo2Shape": "LFO2", "latencyMode": "Latency", "mode": "Mode", "latch": "Latch", "leadOn": "Lead On",
}

def valueof(p, kind):
    v = {"parameter_longname": p["name"], "parameter_shortname": SHORT.get(p["id"], p["name"][:10]),
         "parameter_initial_enable": 1}
    t = p["type"]
    if t in ("choice", "bool"):
        enum = p["choices"] if t == "choice" else ["off", "on"]
        if p["id"] == "mode": enum = ["Absolute", "Relative"]
        if p["id"] == "latencyMode": enum = ["Quality", "Low Latency"]
        v.update(parameter_type=2, parameter_enum=enum, parameter_mmax=len(enum) - 1, parameter_initial=[p["def"]])
        return v
    if t == "int":
        v.update(parameter_type=1, parameter_mmin=p["min"], parameter_mmax=p["max"], parameter_initial=[p["def"]],
                 parameter_unitstyle=8 if p["id"] == "relRoot" else 0)
        return v
    lo, hi, d = p["min"], p["max"], p["def"]
    if is_percent(p):
        lo, hi, d = lo * 100, hi * 100, d * 100
        v.update(parameter_unitstyle=5)
    else:
        u = p["unit"]
        style = {"dB": 4, "ms": 2, "Hz": 3, "st": 7}.get(u)
        if style is not None: v["parameter_unitstyle"] = style
        elif u == "ct": v.update(parameter_unitstyle=9, parameter_units="%0.f ct")
        elif u == "x": v.update(parameter_unitstyle=9, parameter_units="%0.2f x")
        else: v["parameter_unitstyle"] = 1
    v.update(parameter_type=0, parameter_mmin=lo, parameter_mmax=hi, parameter_initial=[d])
    sk = p["skew"]
    if sk > p["min"] and sk < p["max"]:
        v["parameter_exponent"] = round(math.log((sk - p["min"]) / (p["max"] - p["min"])) / math.log(0.5), 3)
    return v

placed = set()
param_index = {}   # box path -> [longname, shortname, 0] for the patcher-level "parameters" table

def control(pt, sendbox, pid, rect, kind=None, extra=None):
    """Create one Live parameter control bound to engine parameter `pid`."""
    p = P[pid]
    placed.add(pid)
    if kind is None:
        kind = {"float": "live.dial", "int": "live.numbox", "choice": "live.menu", "bool": "live.text"}[p["type"]]
    attrs = {"maxclass": kind, "parameter_enable": 1, "presentation": 1, "presentation_rect": rect,
             "varname": pid, "saved_attribute_attributes": {"valueof": valueof(p, kind)}}
    if kind == "live.dial": attrs.update(numinlets=1, numoutlets=2, outlettype=["", "float"])
    elif kind == "live.numbox": attrs.update(numinlets=1, numoutlets=2, outlettype=["", "float"])
    elif kind in ("live.menu", "live.tab"): attrs.update(numinlets=1, numoutlets=3, outlettype=["", "", "float"])
    elif kind == "live.text":
        attrs.update(numinlets=1, numoutlets=2, outlettype=["", ""], mode=1, text=SHORT.get(pid, p["name"]), texton=SHORT.get(pid, p["name"]))
    if extra: attrs.update(extra)
    attrs["patching_rect"] = [rect[0] + 1400, rect[1] + 40, rect[2], rect[3]]
    cid = pt.box(**attrs)
    vo = attrs["saved_attribute_attributes"]["valueof"]
    param_index[(id(pt), cid)] = [vo["parameter_longname"], vo["parameter_shortname"], 0]
    pre = pt.obj(("prepend paramp " if is_percent(p) else "prepend param ") + pid, rect=[rect[0] + 1400, rect[1] + 100, 160, 22])
    pt.line(cid, 0, pre, 0)
    pt.line(pre, 0, sendbox, 0)
    rcv = pt.obj("r ---set_" + pid, 0, 1, rect=[rect[0] + 1400, rect[1] + 10, 120, 22])
    pt.line(rcv, 0, cid, 0)
    return cid

def label(pt, text, rect, size=9.5):
    return pt.box(maxclass="live.comment", text=text, numinlets=1, numoutlets=0, presentation=1,
                  presentation_rect=rect, patching_rect=[rect[0] + 1400, rect[1] + 600, rect[2], rect[3]], fontsize=size)

def vline(pt, x, y0=4, y1=165):
    return pt.box(maxclass="live.line", justification=1, numinlets=1, numoutlets=0, presentation=1,
                  presentation_rect=[x, y0, 5, y1 - y0], patching_rect=[x + 1400, 700, 5, 100])

# ------------------------------------------------------------------ expanded window (subpatcher)
ex = Patch()
ex_in = ex.obj("inlet", 0, 1, rect=[20, 20, 30, 22]); ex.boxes[-1]["box"]["maxclass"] = "inlet"; ex.boxes[-1]["box"].pop("text")
ex_send = ex.obj("s ---vwin", 1, 0, rect=[20, 60, 80, 22])
ex_tele = ex.obj("r ---vwtele", 0, 1, rect=[20, 100, 90, 22])
big = ex.box(maxclass="jsui", filename="vw_display.js", jsarguments=["expanded"], numinlets=1, numoutlets=1, outlettype=[""],
             parameter_enable=0, presentation=1, presentation_rect=[8, 8, 1024, 330], patching_rect=[20, 140, 512, 165])
ex.line(ex_tele, 0, big, 0)

# deeper controls as a flowing grid of dials (left)
deep = ["relRoot", "bendRange", "velSens", "vibratoDepth", "vibratoRate", "formantFollow", "formantTrack", "synthAttack",
        "synthRelease", "synthOctave", "gate", "analysisSmooth", "vowelSmooth", "vowelSens", "f1Lo", "f1Hi", "f2Lo", "f2Hi",
        "lfo1Rate", "lfo2Rate"]
label(ex, "VOICE, ANALYSIS, SYNTH & LFO", [8, 344, 300, 16])
x0, y = 8, 362
for i, pid in enumerate(deep):
    col, row = i % 10, i // 10
    kind = "live.numbox" if P[pid]["type"] == "int" else "live.dial"
    if kind == "live.numbox":
        label(ex, SHORT[pid], [x0 + col * 51, y + row * 64, 50, 14])
        control(ex, ex_send, pid, [x0 + col * 51 + 2, y + row * 64 + 18, 44, 16], kind)
    else:
        control(ex, ex_send, pid, [x0 + col * 51, y + row * 64, 48, 58], kind)
label(ex, "LFO 1", [8, 494, 40, 14]); control(ex, ex_send, "lfo1Shape", [48, 492, 90, 16])
label(ex, "LFO 2", [146, 494, 40, 14]); control(ex, ex_send, "lfo2Shape", [186, 492, 90, 16])

# modulation matrix (right)
label(ex, "MODULATION MATRIX   source > destination   depth   smoothing", [530, 344, 500, 16])
for r in range(8):
    ry = 362 + r * 19
    label(ex, str(r + 1), [530, ry, 14, 16])
    control(ex, ex_send, f"mod{r+1}Src", [546, ry, 150, 16])
    control(ex, ex_send, f"mod{r+1}Dst", [702, ry, 150, 16])
    control(ex, ex_send, f"mod{r+1}Depth", [858, ry, 70, 16], "live.numbox")
    control(ex, ex_send, f"mod{r+1}Smooth", [934, ry, 70, 16], "live.numbox")

# per-voice ensemble grid (bottom left)
label(ex, "PER-VOICE ENSEMBLE", [8, 520, 200, 16])
rows = [("Level", "Level"), ("Pan", "Pan"), ("Formant", "Formant"), ("Synth", "Blend"), ("Glide x", "Glide"), ("Detune", "Detune")]
for v in range(8):
    label(ex, str(v + 1), [70 + v * 56, 536, 50, 14])
for r, (nm, key) in enumerate(rows):
    ry = 552 + r * 19
    label(ex, nm, [8, ry, 60, 16])
    for v in range(8):
        control(ex, ex_send, f"v{v+1}{key}", [70 + v * 56, ry, 52, 16], "live.numbox")

# macro assignments (bottom right)
label(ex, "MACRO ASSIGNMENTS   target   amount", [530, 520, 400, 16])
for m in range(4):
    mx = 530 + m * 126
    label(ex, f"Macro {m+1}", [mx, 536, 120, 14])
    for s in range(3):
        sy = 552 + s * 38
        control(ex, ex_send, f"macro{m+1}Dst{s+1}", [mx, sy, 120, 16])
        control(ex, ex_send, f"macro{m+1}Amt{s+1}", [mx, sy + 18, 60, 16], "live.numbox")

expanded_patcher = {
    "fileversion": 1, "appversion": {"major": 8, "minor": 6, "revision": 0, "architecture": "x64", "modernui": 1},
    "classnamespace": "box", "rect": [100.0, 100.0, 1040.0, 680.0], "openinpresentation": 1,
    "default_fontsize": 10.0, "default_fontname": "Arial Bold", "gridonopen": 1, "gridsize": [8.0, 8.0],
    "toolbarvisible": 0, "lefttoolbarpinned": 0, "toptoolbarpinned": 0, "righttoolbarpinned": 0, "bottomtoolbarpinned": 0,
    "statusbarvisible": 0, "enablehscroll": 0, "enablevscroll": 0, "title": "VowelWorm - expanded",
    "boxes": ex.boxes, "lines": ex.lines,
}

# ------------------------------------------------------------------ main device
pt = Patch()
plugin = pt.obj("plugin~", 2, 2, ["signal", "signal"], rect=[20, 20, 60, 22])
vw = pt.obj("vowelworm~", 2, 3, ["signal", "signal", ""], rect=[20, 120, 120, 22])
plugout = pt.obj("plugout~", 2, 0, rect=[20, 220, 60, 22])
pt.line(plugin, 0, vw, 0); pt.line(plugin, 1, vw, 1)
pt.line(vw, 0, plugout, 0); pt.line(vw, 1, plugout, 1)
rin = pt.obj("r ---vwin", 0, 1, rect=[160, 60, 80, 22]); pt.line(rin, 0, vw, 0)
send = pt.obj("s ---vwin", 1, 0, rect=[160, 300, 80, 22])

# MIDI: routed from a MIDI track with "MIDI To" -> this device
midiin = pt.obj("midiin", 1, 1, ["int"], rect=[300, 20, 50, 22])
mparse = pt.obj("midiparse", 1, 8, ["", "", "", "int", "int", "", "int", ""], rect=[300, 50, 120, 22])
pt.line(midiin, 0, mparse, 0)
for outlet, sel in ((0, "note"), (2, "cc"), (5, "bend")):
    pre = pt.obj("prepend " + sel, rect=[300 + outlet * 40, 90, 80, 22])
    pt.line(mparse, outlet, pre, 0); pt.line(pre, 0, send, 0)

# back channel
router = pt.obj("route tele p calibrated", 1, 4, rect=[460, 160, 160, 22])
pt.line(vw, 2, router, 0)
pre_tele = pt.obj("prepend tele", rect=[460, 200, 80, 22]); pt.line(router, 0, pre_tele, 0)
s_tele = pt.obj("s ---vwtele", 1, 0, rect=[460, 240, 90, 22]); pt.line(pre_tele, 0, s_tele, 0)
ids = [p["id"] for p in spec["params"]]
proute = pt.obj("route " + " ".join(ids), 1, len(ids) + 1, rect=[640, 200, 600, 22]); pt.line(router, 1, proute, 0)
for i, pid in enumerate(ids):
    src = proute
    if is_percent(P[pid]):
        mul = pt.obj("* 100.", 2, 1, rect=[640 + (i % 12) * 60, 240 + (i // 12) * 90, 50, 22]); pt.line(proute, i, mul, 0); src, so = mul, 0
    else:
        so = i
    ps = pt.obj("prepend set", rect=[640 + (i % 12) * 60, 270 + (i // 12) * 90, 70, 22])
    pt.line(src, so if src == proute else 0, ps, 0)
    s = pt.obj("s ---set_" + pid, 1, 0, rect=[640 + (i % 12) * 60, 300 + (i // 12) * 90, 100, 22])
    pt.line(ps, 0, s, 0)

# ---- front panel (169 px tall, Live device conventions) ----
disp = pt.box(maxclass="jsui", filename="vw_display.js", jsarguments=["compact"], numinlets=1, numoutlets=1, outlettype=[""],
              parameter_enable=0, presentation=1, presentation_rect=[4, 4, 152, 118], patching_rect=[20, 400, 152, 118])
rt = pt.obj("r ---vwtele", 0, 1, rect=[20, 370, 90, 22]); pt.line(rt, 0, disp, 0)

pmenu = pt.box(maxclass="live.menu", numinlets=1, numoutlets=3, outlettype=["", "", "float"], parameter_enable=0,
               items=PRESETS, presentation=1, presentation_rect=[4, 126, 152, 16], patching_rect=[200, 400, 152, 16], varname="presetMenu")
# live.menu without a parameter needs its items set directly
pt.boxes[-1]["box"]["items"] = sum([[name, ","] for name in PRESETS], [])[:-1]
ppre = pt.obj("prepend preset", rect=[200, 430, 100, 22]); pt.line(pmenu, 0, ppre, 0); pt.line(ppre, 0, send, 0)

expand = pt.box(maxclass="live.text", text="Expand", texton="Expand", mode=0, parameter_enable=0, numinlets=1, numoutlets=2,
                outlettype=["", ""], presentation=1, presentation_rect=[4, 146, 74, 18], patching_rect=[380, 400, 60, 18], varname="expand")
tb = pt.obj("t b", rect=[380, 430, 40, 22]); pt.line(expand, 0, tb, 0)
openmsg = pt.box(maxclass="message", text="open", numinlets=2, numoutlets=1, outlettype=[""], patching_rect=[380, 460, 40, 22])
pt.line(tb, 0, openmsg, 0)
pc = pt.obj("pcontrol", rect=[380, 490, 60, 22]); pt.line(openmsg, 0, pc, 0)
sub = pt.box(maxclass="newobj", text="p expanded", numinlets=1, numoutlets=0, patching_rect=[380, 520, 80, 22], patcher=expanded_patcher)
pt.line(pc, 0, sub, 0)

calib = pt.box(maxclass="live.text", text="Calibrate", texton="Listening...", mode=1, parameter_enable=0, numinlets=1, numoutlets=2,
               outlettype=["", ""], presentation=1, presentation_rect=[82, 146, 74, 18], patching_rect=[480, 400, 60, 18], varname="calibrate")
cpre = pt.obj("prepend calibrate", rect=[480, 430, 110, 22]); pt.line(calib, 0, cpre, 0); pt.line(cpre, 0, send, 0)
cdone = pt.box(maxclass="message", text="set 0", numinlets=2, numoutlets=1, outlettype=[""], patching_rect=[600, 400, 40, 22])
pt.line(router, 2, cdone, 0); pt.line(cdone, 0, calib, 0)

vline(pt, 160)
# harmony
label(pt, "Harmony", [166, 4, 60, 14])
control(pt, send, "mode", [166, 20, 120, 16], "live.tab")
label(pt, "Voices", [292, 4, 44, 14]); control(pt, send, "voices", [292, 20, 44, 16])
label(pt, "Octave", [340, 4, 44, 14]); control(pt, send, "octave", [340, 20, 44, 16])
for i, pid in enumerate(["glide", "formantShift", "formantEmphasis", "spread", "expressTransfer"]):
    control(pt, send, pid, [166 + i * 45, 40, 44, 50])
for i, pid in enumerate(["harmonyLevel", "leadLevel", "harmonyAttack", "harmonyRelease", "consonantLevel"]):
    control(pt, send, pid, [166 + i * 45, 92, 44, 50])
control(pt, send, "latch", [166, 148, 50, 16]); control(pt, send, "leadOn", [220, 148, 56, 16])
control(pt, send, "latencyMode", [280, 148, 110, 16], "live.tab")
vline(pt, 394)
# synth
label(pt, "Voice > Synth", [400, 4, 90, 14])
control(pt, send, "blend", [404, 30, 56, 66])
for i, pid in enumerate(["oscMorph", "synthBrightness", "synthResonance"]):
    control(pt, send, pid, [468 + i * 45, 20, 44, 50])
for i, pid in enumerate(["noise", "dynamicsFollow", "synthLevel"]):
    control(pt, send, pid, [468 + i * 45, 92, 44, 50])
label(pt, "Synth Oct", [400, 132, 60, 14]); control(pt, send, "synthOctave", [400, 148, 50, 16])
vline(pt, 604)
# macros
label(pt, "Macros", [610, 4, 60, 14])
for m in range(4):
    control(pt, send, f"macro{m+1}", [610 + m * 50, 40, 48, 58])
label(pt, "Assign targets in the expanded view", [610, 148, 200, 14], 8.5)
vline(pt, 812)
# output
label(pt, "Out", [818, 4, 40, 14])
control(pt, send, "outputLevel", [816, 20, 44, 50])
ml = pt.box(maxclass="live.meter~", numinlets=1, numoutlets=1, outlettype=["list"], presentation=1,
            presentation_rect=[824, 80, 8, 84], patching_rect=[80, 260, 8, 60])
mr = pt.box(maxclass="live.meter~", numinlets=1, numoutlets=1, outlettype=["list"], presentation=1,
            presentation_rect=[836, 80, 8, 84], patching_rect=[95, 260, 8, 60])
pt.line(vw, 0, ml, 0); pt.line(vw, 1, mr, 0)

parameters = {}
for (owner, bid), v in param_index.items():
    parameters[(sub + "::" + bid) if owner == id(ex) else bid] = v
parameters["inherited_shortname"] = 1

missing = [p["id"] for p in spec["params"] if p["id"] not in placed]
if missing:
    sys.exit("parameters without a control: " + ", ".join(missing))

device = {"patcher": {
    "fileversion": 1, "appversion": {"major": 8, "minor": 6, "revision": 0, "architecture": "x64", "modernui": 1},
    "classnamespace": "box", "rect": [80.0, 80.0, 1300.0, 800.0], "openrect": [0.0, 0.0, 0.0, 169.0],
    "openinpresentation": 1, "default_fontsize": 10.0, "default_fontname": "Arial Bold", "gridonopen": 1,
    "gridsize": [8.0, 8.0], "devicewidth": 866.0, "description": "VowelWorm - MIDI vocal harmoniser and voice-following synth",
    "boxes": pt.boxes, "lines": pt.lines, "parameters": parameters, "dependency_cache": [
        {"name": "vowelworm~.mxo", "type": "iLaX"}, {"name": "vw_display.js", "type": "TEXT", "implicit": 1}],
    "latency": 0, "is_mpe": 0, "minimum_live_version": "", "minimum_max_version": "", "platform_compatibility": 0,
    "autosave": 0,
}}

body = json.dumps(device, indent=1).encode("utf-8") + b"\n\x00"
header = b"ampf" + struct.pack("<I", 4) + b"aaaa" + b"meta" + struct.pack("<I", 4) + struct.pack("<I", 1) + b"ptch" + struct.pack("<I", len(body))
open(out_path, "wb").write(header + body)
print(f"wrote {out_path}: {len(pt.boxes)} boxes main, {len(ex.boxes)} in expanded view, {len(placed)} Live parameters")
