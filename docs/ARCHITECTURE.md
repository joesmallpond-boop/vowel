# VowelWorm architecture and decisions

## Prior prototype
No earlier VowelWorm code was available (the attachment contained only the brief and the repository was
empty), so v0.1 is a from-scratch design. Nothing is inherited.

## 1. Host configuration (Ableton Live 12, VST3)

| Option | Verdict |
|---|---|
| **Audio effect that accepts MIDI** (vocal track + MIDI track "MIDI To" → effect) | **Primary.** This is the documented Ableton workflow for MIDI-controlled vocal effects (iZotope VocalSynth 2's Ableton guide uses exactly this). Live treats a VST3 with MIDI input and `isMidiEffect=false` as an audio effect that can be a MIDI target. JUCE's "MIDI Effect" flag is deliberately **off** (JUCE forum reports Live refuses to load VST3s flagged as pure MIDI effects). |
| Instrument + sidechain | **Shipped as a fallback build** ("VowelWorm Inst"). Live supports sidechain inputs on VST3 instruments. Same source; only the bus layout differs. |
| Separate plug-ins / virtual MIDI / helper app | Rejected: unnecessary complexity. |

Both builds report **0 samples latency**: the dry lead is never delayed, so monitoring is immediate. The
harmonies trail the voice by the PSOLA delay (below). This is reported in the Settings page and measured in tests.

**Unverified:** I could not run Ableton here. The routing above is the standard documented approach, but
both builds must be checked in Live 12 on your Mac (checklist in TESTING.md).

## 2. Pitch shifting: TD-PSOLA, written for this project

Candidates considered:

* **TD-PSOLA (chosen).** Each output grain is two periods of the singer's own voice, centred on a glottal
  epoch and re-spaced at the target period. Because each grain carries the vocal-tract response, **formants are
  preserved by construction**. Cost per voice is about two interpolated reads per output sample, so many voices
  are cheap. Delay is about 3 periods of the *sung* note. Consonants and breath are handled by switching grains
  to identity resynthesis (no pitch shift) when unvoiced, with hysteresis.
* **Phase vocoder / Signalsmith Stretch (MIT) / Rubber Band (GPL or commercial).** These are excellent for
  polyphonic material, but they need 40–120 ms frames for vocal-quality frequency resolution, they smear
  transients, and formant preservation needs a separate envelope stage. They are too slow for live keyboard
  harmonies, and Rubber Band's GPL would force the plug-in to be GPL or need a commercial licence. Not used.
* **WSOLA.** This is a time-scaler that still needs resampling for pitch, which shifts formants. It only suits
  small shifts.

Implementation details:

* **Pitch tracking**: YIN on a ~24 kHz decimated copy (4× cheaper), lag refined on the full-rate signal,
  3-point median, and a stationarity check so frames straddling an onset cannot report a sub-octave.
  Measured accuracy is within 8 cents worst case on synthetic vowels from 82 Hz to 880 Hz at 44.1, 48 and 96 kHz.
* **Epoch marks**: peak of a fundamental-tracking low-pass copy, searched within ±0.3 T of the predicted mark.
* **Formant shift** is independent of pitch. Each grain is resampled by the formant ratio while grain
  *placement* stays at the target period. "Formant Follow Pitch" blends towards classic chipmunk behaviour.
* **Expression transfer** (absolute mode): the singer's vibrato and micro-intonation, measured against a
  slow pitch centre that snaps on real note changes, is added to the MIDI pitch. Deviations taper to 0 by
  1.2 semitones, so scoops and note changes never bend the harmonies.
* **Latency mode**: *Quality* (default) uses full-period grains. *Low Latency* caps grains at 6 ms, which cuts
  delay by about a third for voices below ~165 Hz (98 Hz: 31 → 21 ms) at a measurable fidelity cost
  (unison correlation 0.996 → 0.894 at 98 Hz). Higher voices are unaffected.

## 3. Voice allocation
New notes prefer a voice released within the last 250 ms whose last note is nearest, and glide from it.
On a chord change each voice therefore moves the shortest distance instead of every voice jumping. Free
idle voices start on pitch. Stealing takes the oldest pedal-only voice first. Sustain (CC64), latch (a new
chord replaces the latched one), pitch bend, CC1, all-notes-off.

## 4. Vocal expression analysis
* Amplitude, attack (fast vs. slow envelope), brightness (spectral-centroid estimate), breathiness (YIN aperiodicity, with an
  unvoiced floor), pitch movement, vibrato depth and rate.
* **Formants**: order-12 LPC on a ~12 kHz decimated, pre-emphasised signal every 5 ms. The LPC envelope is evaluated
  on a precomputed table and the first two resonances are picked. A 5-frame median rejects outliers, frames
  only update while confidently voiced, a one-pole filter smooths, and a slew limit (full range in ≥ 120 ms)
  stops any single bad frame from causing a jump.
* **Vowel space**: X = log F2 between the calibrated limits (frontness), Y = log F1 (openness). The vowel
  triangle comes out as oo (0,0.1), ah (0.3,0.9), ee (1,0.1). **Calibration** records your own F1/F2 range
  while you sing oo-oh-ah-eh-ee. Calibration is stored with the project but is not changed by presets.

## 5. Synthesis
One synth voice per harmony voice, on the same pitch (including glide and vibrato):
morphing band-limited oscillator (sine → triangle → PolyBLEP saw → narrowing pulse) + breath noise →
**three-band vowel filter whose F1/F2 are the singer's measured formants** (× formant shift; F3 follows
frontness) → key-tracked brightness low-pass → amplitude = gate envelope × the singer's level (Dynamics
Follow). Human/Synthetic blend is equal-power and level-matched to within 0.4 dB.

## 6. Modulation
* 8 routes: source × bipolar depth × per-route smoothing → destination, added on the destination's
  normalised 0..1 scale on top of the host parameter, then clamped and smoothed (12 ms).
* Sources: Vowel X/Y, Amplitude, Attack, Breathiness, Brightness, Pitch Movement, Vibrato, Velocity, Mod Wheel, 2 LFOs.
* Destinations: Formant Shift, Formant Emphasis, Synth Blend, Synth Brightness, Osc Morph, Noise, Harmony Spread,
  Vibrato Depth, Glide, Voice Balance, Harmony Level, Synth Resonance.
* 4 macros × 3 targets each with a bipolar amount (the target moves base → base + amount).
* The UI shows live modulation as teal rings on the affected knobs and activity bars per route.

## 7. Real-time safety
All buffers are allocated in `prepare()`. The audio path makes no allocations, takes no locks and does no I/O. The UI reads
atomics only. Parameters are read once per block and smoothed per 32-sample control block, with per-sample
gain ramps. MIDI is sample-accurate (blocks are split at events). Output is identical for any host block size (tested).

## 8. Known risks and open questions
1. **Ableton routing on your Mac is unverified.** This is the biggest unknown; both builds exist to hedge it.
2. **Perceived quality on real singing is unverified by ear.** Automated tests use synthetic vowels plus
   CC-BY read speech, not singing. Listening tests on your voice are required (TESTING.md).
3. **Harmony delay is pitch-dependent**: about 3 periods, so 6–16 ms for most singing and 22–31 ms for low male notes.
   Low Latency mode helps low voices. Below ~90 Hz it may feel late.
4. **Large upward shifts** (> +12 st) repeat grains and get thinner. Expect this to be audible at +19 st.
5. **Octave errors** on creaky or very breathy phonation (seen on read speech: 2–19 per 15 s) can cause
   short glitches. Sung tones are much steadier.
6. **LPC formant tracking at high pitch** (> ~400 Hz) latches onto harmonics. The vowel signals stay smooth but
   become less meaningful for high sopranos. Calibration partly compensates.
7. CPU on the 2019 Intel MacBook Pro is unmeasured. Here it is about 4% of one core for 4–8 voices with the synth on.
