# Licensing

* **JUCE 8** (downloaded at build time, not vendored) is dual-licensed: **AGPLv3** or a commercial JUCE licence
  (Starter is free below a revenue threshold; Indie/Pro above it). Distributing VowelWorm binaries requires
  either releasing the VowelWorm source under AGPLv3 or holding a JUCE licence that covers you. Check the current
  terms at juce.com before distributing.
* **VST3 SDK** (bundled with JUCE) is MIT-licensed since 2025 (previously GPLv3 or Steinberg proprietary). "VST" is a
  Steinberg trademark, so follow their logo and naming guidelines if you publish.
* **AU / CoreAudio**: Apple SDK terms.
* **DSP**: all pitch tracking, PSOLA, analysis and synthesis code in `Source/Engine` was written for this project.
  No third-party DSP library is used. Rubber Band (GPL/commercial) and Signalsmith Stretch (MIT) were evaluated
  and not used (see ARCHITECTURE.md).
* **Test data**: the LibriSpeech clips used for real-voice measurements (CC-BY 4.0, openslr.org/12) are not
  included in the repository.
* **pluginval** (GPLv3) is only downloaded and run in CI as a test tool.
