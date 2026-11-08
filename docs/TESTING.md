# Testing

Three kinds of evidence, kept separate:

* **A. Automated, objective** (`vw_tests`): synthetic voice-like signals (glottal source + formant cascade), judged by
  *independent* estimators (autocorrelation pitch, harmonic-envelope comparison) rather than the engine's own.
* **B. Real-voice measurements**: CC-BY LibriSpeech read speech (two male, one female) run through `vw_render`.
  This is real human voice with consonants and natural pitch, but it is **speech, not singing**.
* **C. Subjective listening and Ableton**: **not done yet**. It needs your voice, your Mac and Live 12.

Synthetic signals are not used as proof of vocal quality.

## A. Automated results (Linux x86-64, 48 kHz unless stated) — 265 checks, 0 failures

| Area | Result |
|---|---|
| Pitch tracker | worst error < 8 ct for 82–880 Hz, vowels a/i/u, at 44.1/48/96 kHz; > 95% of frames voiced |
| Harmony tuning (abs MIDI, sung G3) | notes G2…G5 (−12…+24 st): within ±0.2 ct at all three sample rates |
| Relative mode (+4 st) | −0.03 ct |
| Vibrato transfer | ±40 ct sung → −40…+40 ct in the harmony, mean offset −0.7 ct |
| Formant preservation (−7…+12 st) | harmonic-envelope error 1.8–3.5 dB vs. a natively generated vowel at the target pitch (8.5–14.6 dB vs. un-preserved) |
| Independent formant shift ±4, ±7 st | 1.4–3.0 dB from the target envelope |
| Vowel space (synthetic) | oo (0.00, 0.12), oh (0.00, 0.73), ah (0.31, 0.91), eh (0.80, 0.55), ee (0.98, 0.12) |
| Vowel glide oo→ah→ee | largest step 0.005 per 128-sample block |
| Voice allocation | C→F major: C stays, E→F, G→A (nearest-voice glide); sustain, latch, stealing |
| Four-part chord + chord change | all four fundamentals present (13–20 dB above quarter-tone neighbours), no NaN, peak < 1 |
| Synth | pitch exact; 'ah' 2× brighter than 'oo'; 20 dB vocal level change → 16 dB synth change |
| Blend level | human 0.0 dB, hybrid +0.4 dB, synthetic −0.1 dB |
| Clicks | abrupt jumps of 7 parameters: max 2nd-difference around the jump ≤ the steady-state values |
| Robustness | 44.1/48/96 kHz × blocks 37/64/128/256/1024: finite, peak ≤ 1, silence → < −70 dB; output independent of block size |
| Harmony delay (Quality) | 98 Hz 31 ms · 147 Hz 22 ms · 220 Hz 16 ms · 330 Hz 7.7 ms · 494 Hz 6.4 ms; unison correlation ≥ 0.996 |
| Harmony delay (Low Latency) | 98 Hz 21 ms (correlation 0.894); ≥ 220 Hz unchanged |
| CPU (this VM) | 4–8 voices, synth on or off: 3.2–4.6% of one core |
| pluginval strictness 10 | **VowelWorm** and **VowelWorm Inst** VST3: pass (state restore, automation, thread safety, fuzzing, VST3 validator) |

## B. Real voice (LibriSpeech, CC-BY 4.0), chords changed every 3 s, 64-sample blocks

| Clip | median f0 | voiced | octave jumps | harmony delay | block time median / 99.9% |
|---|---|---|---|---|---|
| 198-209-0000 (female) | 202 Hz | 60% | 19 | 16 ms | 0.03 / 0.34 ms |
| 3436-172162-0000 (male) | 140 Hz | 61% | 10 | 22 ms | 0.03 / 0.25 ms |
| 5703-47212-0000 (male) | 78 Hz | 51% | 2 | 39 ms | 0.04 / 0.32 ms |

Budget for a 64-sample block at 48 kHz is 1.33 ms. Occasional single-block outliers of 2–8 ms did not reproduce
between runs and come from scheduling on this shared VM. The vowel point never moved more than 0.089 per 10 ms.
The spectrogram shows the harmony lines locked to the chord during voiced speech and consonants passing through.

## C. On your Mac (please run; nothing here is verified yet)

1. `./build_mac.sh --install`, or install the CI DMG. Rescan in Live.
2. Set up the routing from README. Check that **AUDIO IN**, **MIDI** and **VOICED** light up.
3. Sing a sustained "ah" and hold a 4-note chord: tuning, stability, any metallic or grainy quality.
4. Change chords quickly: smoothness and responsiveness.
5. Sing oo → ah → ee slowly, then run *Vocal Analysis → Calibrate* and repeat: the pad should span the square.
6. Vibrato, a fast run, consonant-heavy lyrics, quiet vs. loud.
7. Map Ableton LFO → Formant Shift, and an Envelope Follower on a drum track → Human/Synthetic.
8. Save the set, close, reopen: parameters, preset name, calibration and UI page restored.
9. Try **VowelWorm Inst** with the sidechain routing as well.
10. CPU: 4 voices at 64 and 128 samples on the 2019 MacBook Pro (Live's CPU meter), Quality and Low Latency modes.

To capture a reproducible case: record the dry vocal, then
`vw_render take.wav out.wav --chords "..." --analysis take.csv --timing` and send me the files.
