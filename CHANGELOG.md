# Changelog

## 0.1.0 — 2026-11-08 (first implementation: Milestones 1–3, most of 4)

### Harmoniser
- TD-PSOLA harmony voices (4 by default, up to 8) with epoch marking and identity resynthesis for consonants and breath.
- YIN pitch tracker (decimated, full-rate lag refinement, onset stationarity check).
- Absolute MIDI and Relative modes, octave, relative root, latch, sustain pedal, pitch bend, glide.
- Nearest-voice reassignment on chord changes; voice stealing.
- Independent formant shift, Formant Follow Pitch, Formant Emphasis.
- Vibrato and micro-intonation transfer (tapered so note changes are not transferred).
- Per-voice level, pan, formant offset, synth blend, glide scale, detune.
- Latency mode: Quality / Low Latency (6 ms grain cap for low voices).

### Vocal analysis
- Amplitude, attack, brightness, breathiness, pitch movement, vibrato depth and rate.
- LPC formants → continuous vowel-space X (frontness) / Y (openness) with median, smoothing and slew limiting.
- Voice calibration (oo-oh-ah-eh-ee sweep).

### Synth
- Per-voice morphing PolyBLEP oscillator + noise → vowel filter following the singer's formants → brightness → dynamics follow.
- Equal-power, level-matched Human / Synthetic blend.

### Modulation and host
- 8-route modulation matrix, 2 LFOs, 4 macros × 3 targets; host values are the base, modulation adds on top.
- ~170 stable host parameters (IDs are append-only), smoothed and click-free.
- Two builds: VowelWorm (effect + MIDI in) and VowelWorm Inst (instrument + sidechain).
- State save/restore, including explicit per-parameter restore (pluginval strictness 10 passes).
- 8 factory presets and user presets (~/Music/VowelWorm/Presets).

### UI
- Main view: input/output meters, sung pitch and confidence, MIDI key strip, vowel pad with trail, voice cells,
  blend, sound knobs, expression meters, 4 macros, status pills (audio, MIDI, voiced).
- Pages: Harmony, Vocal Analysis (with calibration), Synthesis (live vowel-filter response), Modulation, Settings.
- Resizable, with live modulation rings.

### Tooling
- `vw_tests` (265 checks), `vw_render` (offline renderer + analysis CSV + block timing), `build_mac.sh`,
  GitHub Actions macOS universal build with tests, pluginval and a DMG.
