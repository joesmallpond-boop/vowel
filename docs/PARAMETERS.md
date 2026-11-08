# Parameters

All host parameters (VST3/AU) and Live parameters (Max for Live device). IDs are stable and append-only.

| ID | Name | Range | Default |
|---|---|---|---|
| `mode` | Harmony Mode | Absolute MIDI / Relative | Absolute MIDI |
| `voices` | Voices | 1 … 8 | 4 |
| `octave` | Octave | -2 … 2 oct | 0 |
| `relRoot` | Relative Root | 36 … 84 | 60 |
| `latch` | Latch | off / on | off |
| `glide` | Glide | 0 … 1000 ms | 35 |
| `bendRange` | Bend Range | 0 … 12 st | 2 |
| `velSens` | Velocity Sensitivity | 0 … 1 | 0.4 |
| `harmonyLevel` | Harmony Level | -60 … 6 dB | 0 |
| `leadOn` | Lead Vocal | off / on | on |
| `leadLevel` | Lead Level | -60 … 6 dB | 0 |
| `outputLevel` | Output | -24 … 12 dB | 0 |
| `blend` | Human / Synthetic | 0 … 1 | 0 |
| `formantShift` | Formant Shift | -12 … 12 st | 0 |
| `formantFollow` | Formant Follow Pitch | 0 … 1 | 0 |
| `formantEmphasis` | Formant Emphasis | 0 … 1 | 0 |
| `expressTransfer` | Expression Transfer | 0 … 1 | 1 |
| `consonantLevel` | Consonant Level | 0 … 1 | 0.6 |
| `harmonyAttack` | Harmony Attack | 1 … 500 ms | 12 |
| `harmonyRelease` | Harmony Release | 10 … 3000 ms | 160 |
| `spread` | Stereo Spread | 0 … 1 | 0.6 |
| `vibratoDepth` | Added Vibrato | 0 … 100 ct | 0 |
| `vibratoRate` | Vibrato Rate | 2 … 9 Hz | 5.4 |
| `v1Level` | Voice 1 Level | -60 … 6 dB | 0 |
| `v2Level` | Voice 2 Level | -60 … 6 dB | 0 |
| `v3Level` | Voice 3 Level | -60 … 6 dB | 0 |
| `v4Level` | Voice 4 Level | -60 … 6 dB | 0 |
| `v5Level` | Voice 5 Level | -60 … 6 dB | 0 |
| `v6Level` | Voice 6 Level | -60 … 6 dB | 0 |
| `v7Level` | Voice 7 Level | -60 … 6 dB | 0 |
| `v8Level` | Voice 8 Level | -60 … 6 dB | 0 |
| `v1Pan` | Voice 1 Pan | -1 … 1 | -0.55 |
| `v2Pan` | Voice 2 Pan | -1 … 1 | 0.55 |
| `v3Pan` | Voice 3 Pan | -1 … 1 | -0.25 |
| `v4Pan` | Voice 4 Pan | -1 … 1 | 0.25 |
| `v5Pan` | Voice 5 Pan | -1 … 1 | -0.85 |
| `v6Pan` | Voice 6 Pan | -1 … 1 | 0.85 |
| `v7Pan` | Voice 7 Pan | -1 … 1 | -0.1 |
| `v8Pan` | Voice 8 Pan | -1 … 1 | 0.1 |
| `v1Formant` | Voice 1 Formant | -6 … 6 st | 0 |
| `v2Formant` | Voice 2 Formant | -6 … 6 st | 0 |
| `v3Formant` | Voice 3 Formant | -6 … 6 st | 0 |
| `v4Formant` | Voice 4 Formant | -6 … 6 st | 0 |
| `v5Formant` | Voice 5 Formant | -6 … 6 st | 0 |
| `v6Formant` | Voice 6 Formant | -6 … 6 st | 0 |
| `v7Formant` | Voice 7 Formant | -6 … 6 st | 0 |
| `v8Formant` | Voice 8 Formant | -6 … 6 st | 0 |
| `v1Blend` | Voice 1 Synth Blend | -1 … 1 | 0 |
| `v2Blend` | Voice 2 Synth Blend | -1 … 1 | 0 |
| `v3Blend` | Voice 3 Synth Blend | -1 … 1 | 0 |
| `v4Blend` | Voice 4 Synth Blend | -1 … 1 | 0 |
| `v5Blend` | Voice 5 Synth Blend | -1 … 1 | 0 |
| `v6Blend` | Voice 6 Synth Blend | -1 … 1 | 0 |
| `v7Blend` | Voice 7 Synth Blend | -1 … 1 | 0 |
| `v8Blend` | Voice 8 Synth Blend | -1 … 1 | 0 |
| `v1Glide` | Voice 1 Glide Scale | 0 … 4 x | 1 |
| `v2Glide` | Voice 2 Glide Scale | 0 … 4 x | 1 |
| `v3Glide` | Voice 3 Glide Scale | 0 … 4 x | 1 |
| `v4Glide` | Voice 4 Glide Scale | 0 … 4 x | 1 |
| `v5Glide` | Voice 5 Glide Scale | 0 … 4 x | 1 |
| `v6Glide` | Voice 6 Glide Scale | 0 … 4 x | 1 |
| `v7Glide` | Voice 7 Glide Scale | 0 … 4 x | 1 |
| `v8Glide` | Voice 8 Glide Scale | 0 … 4 x | 1 |
| `v1Detune` | Voice 1 Detune | -25 … 25 ct | 0 |
| `v2Detune` | Voice 2 Detune | -25 … 25 ct | 0 |
| `v3Detune` | Voice 3 Detune | -25 … 25 ct | 0 |
| `v4Detune` | Voice 4 Detune | -25 … 25 ct | 0 |
| `v5Detune` | Voice 5 Detune | -25 … 25 ct | 0 |
| `v6Detune` | Voice 6 Detune | -25 … 25 ct | 0 |
| `v7Detune` | Voice 7 Detune | -25 … 25 ct | 0 |
| `v8Detune` | Voice 8 Detune | -25 … 25 ct | 0 |
| `gate` | Input Gate | -80 … -20 dB | -55 |
| `vowelSmooth` | Vowel Smoothing | 5 … 500 ms | 60 |
| `analysisSmooth` | Analysis Smoothing | 1 … 300 ms | 25 |
| `vowelSens` | Vowel Sensitivity | 0.25 … 3 x | 1 |
| `f1Lo` | Calib F1 Low | 150 … 600 Hz | 280 |
| `f1Hi` | Calib F1 High | 450 … 1300 Hz | 800 |
| `f2Lo` | Calib F2 Low | 500 … 1600 Hz | 800 |
| `f2Hi` | Calib F2 High | 1400 … 3400 Hz | 2300 |
| `oscMorph` | Osc Morph | 0 … 1 | 0.4 |
| `synthBrightness` | Synth Brightness | 0 … 1 | 0.4 |
| `synthResonance` | Synth Resonance | 0 … 1 | 0.5 |
| `noise` | Breath Noise | 0 … 1 | 0.05 |
| `dynamicsFollow` | Dynamics Follow | 0 … 1 | 0.8 |
| `synthAttack` | Synth Attack | 1 … 2000 ms | 25 |
| `synthRelease` | Synth Release | 10 … 5000 ms | 350 |
| `synthLevel` | Synth Level | -60 … 6 dB | 0 |
| `synthOctave` | Synth Octave | -2 … 1 oct | 0 |
| `formantTrack` | Synth Vowel Tracking | 0 … 1 | 1 |
| `lfo1Rate` | LFO 1 Rate | 0.02 … 20 Hz | 0.25 |
| `lfo1Shape` | LFO 1 Shape | Sine / Triangle / Drift | Sine |
| `lfo2Rate` | LFO 2 Rate | 0.02 … 20 Hz | 4 |
| `lfo2Shape` | LFO 2 Shape | Sine / Triangle / Drift | Drift |
| `mod1Src` | Mod 1 Source | None / Vowel X (front) / Vowel Y (open) / Amplitude / Attack / Breathiness / Brightness / Pitch Movement / Vibrato Depth / Velocity / Mod Wheel / LFO 1 / LFO 2 | Vowel Y (open) |
| `mod2Src` | Mod 2 Source | None / Vowel X (front) / Vowel Y (open) / Amplitude / Attack / Breathiness / Brightness / Pitch Movement / Vibrato Depth / Velocity / Mod Wheel / LFO 1 / LFO 2 | Vowel X (front) |
| `mod3Src` | Mod 3 Source | None / Vowel X (front) / Vowel Y (open) / Amplitude / Attack / Breathiness / Brightness / Pitch Movement / Vibrato Depth / Velocity / Mod Wheel / LFO 1 / LFO 2 | Breathiness |
| `mod4Src` | Mod 4 Source | None / Vowel X (front) / Vowel Y (open) / Amplitude / Attack / Breathiness / Brightness / Pitch Movement / Vibrato Depth / Velocity / Mod Wheel / LFO 1 / LFO 2 | Amplitude |
| `mod5Src` | Mod 5 Source | None / Vowel X (front) / Vowel Y (open) / Amplitude / Attack / Breathiness / Brightness / Pitch Movement / Vibrato Depth / Velocity / Mod Wheel / LFO 1 / LFO 2 | None |
| `mod6Src` | Mod 6 Source | None / Vowel X (front) / Vowel Y (open) / Amplitude / Attack / Breathiness / Brightness / Pitch Movement / Vibrato Depth / Velocity / Mod Wheel / LFO 1 / LFO 2 | None |
| `mod7Src` | Mod 7 Source | None / Vowel X (front) / Vowel Y (open) / Amplitude / Attack / Breathiness / Brightness / Pitch Movement / Vibrato Depth / Velocity / Mod Wheel / LFO 1 / LFO 2 | None |
| `mod8Src` | Mod 8 Source | None / Vowel X (front) / Vowel Y (open) / Amplitude / Attack / Breathiness / Brightness / Pitch Movement / Vibrato Depth / Velocity / Mod Wheel / LFO 1 / LFO 2 | None |
| `mod1Dst` | Mod 1 Destination | None / Formant Shift / Formant Emphasis / Synth Blend / Synth Brightness / Osc Morph / Noise Amount / Harmony Spread / Vibrato Depth / Glide / Voice Balance / Harmony Level / Synth Resonance | Synth Brightness |
| `mod2Dst` | Mod 2 Destination | None / Formant Shift / Formant Emphasis / Synth Blend / Synth Brightness / Osc Morph / Noise Amount / Harmony Spread / Vibrato Depth / Glide / Voice Balance / Harmony Level / Synth Resonance | Osc Morph |
| `mod3Dst` | Mod 3 Destination | None / Formant Shift / Formant Emphasis / Synth Blend / Synth Brightness / Osc Morph / Noise Amount / Harmony Spread / Vibrato Depth / Glide / Voice Balance / Harmony Level / Synth Resonance | Noise Amount |
| `mod4Dst` | Mod 4 Destination | None / Formant Shift / Formant Emphasis / Synth Blend / Synth Brightness / Osc Morph / Noise Amount / Harmony Spread / Vibrato Depth / Glide / Voice Balance / Harmony Level / Synth Resonance | Synth Brightness |
| `mod5Dst` | Mod 5 Destination | None / Formant Shift / Formant Emphasis / Synth Blend / Synth Brightness / Osc Morph / Noise Amount / Harmony Spread / Vibrato Depth / Glide / Voice Balance / Harmony Level / Synth Resonance | None |
| `mod6Dst` | Mod 6 Destination | None / Formant Shift / Formant Emphasis / Synth Blend / Synth Brightness / Osc Morph / Noise Amount / Harmony Spread / Vibrato Depth / Glide / Voice Balance / Harmony Level / Synth Resonance | None |
| `mod7Dst` | Mod 7 Destination | None / Formant Shift / Formant Emphasis / Synth Blend / Synth Brightness / Osc Morph / Noise Amount / Harmony Spread / Vibrato Depth / Glide / Voice Balance / Harmony Level / Synth Resonance | None |
| `mod8Dst` | Mod 8 Destination | None / Formant Shift / Formant Emphasis / Synth Blend / Synth Brightness / Osc Morph / Noise Amount / Harmony Spread / Vibrato Depth / Glide / Voice Balance / Harmony Level / Synth Resonance | None |
| `mod1Depth` | Mod 1 Depth | -1 … 1 | 0.45 |
| `mod2Depth` | Mod 2 Depth | -1 … 1 | 0.3 |
| `mod3Depth` | Mod 3 Depth | -1 … 1 | 0.45 |
| `mod4Depth` | Mod 4 Depth | -1 … 1 | 0.25 |
| `mod5Depth` | Mod 5 Depth | -1 … 1 | 0 |
| `mod6Depth` | Mod 6 Depth | -1 … 1 | 0 |
| `mod7Depth` | Mod 7 Depth | -1 … 1 | 0 |
| `mod8Depth` | Mod 8 Depth | -1 … 1 | 0 |
| `mod1Smooth` | Mod 1 Smoothing | 0 … 1000 ms | 20 |
| `mod2Smooth` | Mod 2 Smoothing | 0 … 1000 ms | 20 |
| `mod3Smooth` | Mod 3 Smoothing | 0 … 1000 ms | 20 |
| `mod4Smooth` | Mod 4 Smoothing | 0 … 1000 ms | 20 |
| `mod5Smooth` | Mod 5 Smoothing | 0 … 1000 ms | 20 |
| `mod6Smooth` | Mod 6 Smoothing | 0 … 1000 ms | 20 |
| `mod7Smooth` | Mod 7 Smoothing | 0 … 1000 ms | 20 |
| `mod8Smooth` | Mod 8 Smoothing | 0 … 1000 ms | 20 |
| `macro1` | Macro 1 | 0 … 1 | 0 |
| `macro2` | Macro 2 | 0 … 1 | 0 |
| `macro3` | Macro 3 | 0 … 1 | 0 |
| `macro4` | Macro 4 | 0 … 1 | 0 |
| `macro1Dst1` | Macro 1 Target 1 | None / Formant Shift / Formant Emphasis / Synth Blend / Synth Brightness / Osc Morph / Noise Amount / Harmony Spread / Vibrato Depth / Glide / Voice Balance / Harmony Level / Synth Resonance | Synth Blend |
| `macro1Dst2` | Macro 1 Target 2 | None / Formant Shift / Formant Emphasis / Synth Blend / Synth Brightness / Osc Morph / Noise Amount / Harmony Spread / Vibrato Depth / Glide / Voice Balance / Harmony Level / Synth Resonance | Synth Resonance |
| `macro1Dst3` | Macro 1 Target 3 | None / Formant Shift / Formant Emphasis / Synth Blend / Synth Brightness / Osc Morph / Noise Amount / Harmony Spread / Vibrato Depth / Glide / Voice Balance / Harmony Level / Synth Resonance | Harmony Level |
| `macro2Dst1` | Macro 2 Target 1 | None / Formant Shift / Formant Emphasis / Synth Blend / Synth Brightness / Osc Morph / Noise Amount / Harmony Spread / Vibrato Depth / Glide / Voice Balance / Harmony Level / Synth Resonance | Synth Brightness |
| `macro2Dst2` | Macro 2 Target 2 | None / Formant Shift / Formant Emphasis / Synth Blend / Synth Brightness / Osc Morph / Noise Amount / Harmony Spread / Vibrato Depth / Glide / Voice Balance / Harmony Level / Synth Resonance | Formant Emphasis |
| `macro2Dst3` | Macro 2 Target 3 | None / Formant Shift / Formant Emphasis / Synth Blend / Synth Brightness / Osc Morph / Noise Amount / Harmony Spread / Vibrato Depth / Glide / Voice Balance / Harmony Level / Synth Resonance | Osc Morph |
| `macro3Dst1` | Macro 3 Target 1 | None / Formant Shift / Formant Emphasis / Synth Blend / Synth Brightness / Osc Morph / Noise Amount / Harmony Spread / Vibrato Depth / Glide / Voice Balance / Harmony Level / Synth Resonance | Harmony Spread |
| `macro3Dst2` | Macro 3 Target 2 | None / Formant Shift / Formant Emphasis / Synth Blend / Synth Brightness / Osc Morph / Noise Amount / Harmony Spread / Vibrato Depth / Glide / Voice Balance / Harmony Level / Synth Resonance | Vibrato Depth |
| `macro3Dst3` | Macro 3 Target 3 | None / Formant Shift / Formant Emphasis / Synth Blend / Synth Brightness / Osc Morph / Noise Amount / Harmony Spread / Vibrato Depth / Glide / Voice Balance / Harmony Level / Synth Resonance | Glide |
| `macro4Dst1` | Macro 4 Target 1 | None / Formant Shift / Formant Emphasis / Synth Blend / Synth Brightness / Osc Morph / Noise Amount / Harmony Spread / Vibrato Depth / Glide / Voice Balance / Harmony Level / Synth Resonance | Formant Shift |
| `macro4Dst2` | Macro 4 Target 2 | None / Formant Shift / Formant Emphasis / Synth Blend / Synth Brightness / Osc Morph / Noise Amount / Harmony Spread / Vibrato Depth / Glide / Voice Balance / Harmony Level / Synth Resonance | None |
| `macro4Dst3` | Macro 4 Target 3 | None / Formant Shift / Formant Emphasis / Synth Blend / Synth Brightness / Osc Morph / Noise Amount / Harmony Spread / Vibrato Depth / Glide / Voice Balance / Harmony Level / Synth Resonance | None |
| `macro1Amt1` | Macro 1 Amount 1 | -1 … 1 | 0.8 |
| `macro1Amt2` | Macro 1 Amount 2 | -1 … 1 | 0.3 |
| `macro1Amt3` | Macro 1 Amount 3 | -1 … 1 | -0.15 |
| `macro2Amt1` | Macro 2 Amount 1 | -1 … 1 | 0.5 |
| `macro2Amt2` | Macro 2 Amount 2 | -1 … 1 | 0.4 |
| `macro2Amt3` | Macro 2 Amount 3 | -1 … 1 | 0.25 |
| `macro3Amt1` | Macro 3 Amount 1 | -1 … 1 | 0.4 |
| `macro3Amt2` | Macro 3 Amount 2 | -1 … 1 | 0.25 |
| `macro3Amt3` | Macro 3 Amount 3 | -1 … 1 | 0.3 |
| `macro4Amt1` | Macro 4 Amount 1 | -1 … 1 | 0.25 |
| `macro4Amt2` | Macro 4 Amount 2 | -1 … 1 | 0 |
| `macro4Amt3` | Macro 4 Amount 3 | -1 … 1 | 0 |
| `latencyMode` | Latency Mode | Quality / Low Latency | Quality |

Factory presets: Natural Four-Part, Warm Choir, Synthetic Gospel, Glass Voice, Formant Organ, Breath Choir, Elastic Harmony, Dark Resonance
