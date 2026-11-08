# VowelWorm

A MIDI-controlled vocal harmoniser and voice-following synthesiser for macOS: **VST3**, **AU**, a **Standalone** app
and a **Max for Live device**, all built from one C++ engine (JUCE + CMake for the plug-ins, the Max SDK for the device).

Sing into it and play chords: each held note becomes a pitch-shifted copy of your live voice, formants
preserved. A small synth voice follows every harmony note, and its vowel filter, brightness and dynamics
are driven by how you sing. Everything is exposed as stable host parameters for Ableton's LFO, Shaper,
Envelope Follower, Max for Live devices, rack macros and clip automation.

> Status: **v0.1, Milestones 1–3 implemented and mostly done on Milestone 4.** The engine, both plug-in
> builds and the UI are working and pass automated tests and pluginval (strictness 10) on Linux.
> **Not yet tested in Ableton Live or on a Mac.** The macOS binaries are built by GitHub Actions (see
> below); please treat them as untested until you have run through `docs/TESTING.md` § "On your Mac".

## Ableton Live setup (primary: audio effect with MIDI input)

1. **Vocal track** (audio): *Audio From* = your microphone input, *Monitor* = **In**. Insert **VowelWorm**.
2. **MIDI track**: *MIDI From* = your keyboard, *Monitor* = **In** (or arm the track).
3. On the MIDI track set **MIDI To** = the vocal track, then pick **VowelWorm** in the second menu.
4. Put any Ableton effects *after* VowelWorm on the vocal track. Record the vocal track's output by
   resampling, or route it to another audio track (*Audio From* = vocal track, *Post FX*).

The top-right status pills light when **AUDIO IN** and **MIDI** are arriving and when your voice is **VOICED**.

**Fallback: instrument with sidechain.** If MIDI does not reach the effect, use **VowelWorm Inst**: put it on
the MIDI track and, in its device sidechain section, choose *Audio From* = the vocal track. Same engine,
same parameters, same presets.

**Ableton modulation.** Click *Map* on LFO / Shaper / Envelope Follower (or a M4L modulator) and then click any
VowelWorm control, or use *Configure* to map rack macros. Host values are the base; VowelWorm's internal
matrix and macros add on top and the result is clamped, so the two always combine predictably. All
changes are smoothed (no zipper noise, verified by the click tests).

## Max for Live device

`Max/` contains the `vowelworm~` external and a generated `VowelWorm.amxd` styled like a stock Live device, with an
**Expand** window (EQ Eight-style) for big displays and the deeper controls. See `Max/README.md`. The CI download
includes it in the "Max for Live" folder with an installer.

## Building

```bash
./build_mac.sh               # Release VST3 + AU + Standalone for this Mac, then runs the tests
./build_mac.sh --install     # ...and copies them to ~/Library/Audio/Plug-Ins
./build_mac.sh --universal   # Intel + Apple Silicon
```

The script checks for the Xcode Command Line Tools, CMake ≥ 3.22 (it downloads a private copy into
`.tools/` if you don't have one, so no Homebrew) and JUCE (downloaded automatically, or set `JUCE_DIR`).

Every push to `main` is built by GitHub Actions on macOS. It builds a universal binary, runs the tests and
pluginval, and attaches a DMG plus zip under *Actions → macOS build → Artifacts*. Those bundles are ad-hoc signed;
after downloading, run `xattr -dr com.apple.quarantine <bundle>` (see `docs/INSTALL_MAC.txt`).

Offline tools (any platform): `vw_tests` (engine test suite) and `vw_render` (runs the engine on a WAV file
with a chord script and optional analysis CSV, so you can judge real vocals without a DAW):

```bash
./build/vw_render vocal.wav out.wav --chords "0:55,59,62,67;2.5:57,60,64,69" --preset "Warm Choir" --analysis a.csv --timing
```

## Documentation

* `docs/ARCHITECTURE.md` covers the design decisions: routing, pitch-shifting choice, analysis, synthesis, modulation, risks.
* `docs/TESTING.md` has measured results, what is automated vs. subjective, and the checklist for your Mac and Ableton.
* `docs/PARAMETERS.md` lists every host parameter.
* `docs/LICENSING.md` covers JUCE and other licences.
* `CHANGELOG.md`
