// VowelWorm — top-level real-time engine.
//
// Signal flow per control block (32 samples):
//   mono input -> PitchTracker (YIN + epochs) -> VocalAnalyser (expression signals)
//   -> modulation (8 routes + 4 macros + 2 LFOs) applied on top of host values
//   -> per voice: PSOLA harmony + formant emphasis  |  synth voice
//      blended (equal power), levelled, panned
//   -> + lead vocal (optional) -> output gain -> safety soft-clip
//
// Host-automated parameters are the *base* values. Internal modulation is added
// on the parameter's normalised 0..1 scale and clamped, so automation and the
// matrix combine predictably: automation moves the centre, modulation moves
// around it. All continuous values are smoothed at control rate.
//
// Real-time safety: all memory is allocated in prepare(); process() never
// allocates, locks or does I/O.
#pragma once

#include "Params.h"
#include "PitchTracker.h"
#include "PsolaVoice.h"
#include "VoiceAllocator.h"
#include "VocalAnalyser.h"
#include "SynthVoice.h"
#include <atomic>
#include <memory>

namespace vw
{

struct Telemetry
{
    std::atomic<float> inputPeak { 0 }, outputPeak { 0 };
    std::atomic<float> pitchHz { 0 }, confidence { 0 };
    std::atomic<float> vowelX { 0.5f }, vowelY { 0.5f }, f1 { 0 }, f2 { 0 };
    std::atomic<float> amplitude { 0 }, attack { 0 }, breath { 0 }, brightness { 0 }, pitchMove { 0 }, vibrato { 0 }, vibratoRate { 0 };
    std::atomic<float> lfo1 { 0 }, lfo2 { 0 };
    std::atomic<int>   voiceNote[kMaxVoices] {};
    std::atomic<float> voiceLevel[kMaxVoices] {};
    std::atomic<float> destValue[(int) ModDest::Count] {};   // final normalised values
    std::atomic<float> destOffset[(int) ModDest::Count] {};  // modulation offset (for UI rings)
    std::atomic<float> routeValue[kNumRoutes] {};             // live route contributions
    std::atomic<uint32_t> heldNotes[4] {};
    std::atomic<int>   midiActivity { 0 };
    std::atomic<float> harmonyDelayMs { 0 };
    std::atomic<int>   numVoices { 4 };
};

class Engine
{
public:
    Engine();

    void prepare (double sampleRate, int maxBlock);
    void reset();

    /** Values in real units, indexed by P::. Call once per host block (cheap copy). */
    void setParameters (const ParamValues& v) noexcept { params = v; }

    // MIDI (time = sample offset inside the next process() call is handled by the caller splitting blocks)
    void noteOn (int note, float velocity) noexcept;
    void noteOff (int note) noexcept;
    void controller (int cc, int value) noexcept;
    void pitchBend (int value14) noexcept;
    void allNotesOff() noexcept;

    /** in: mono input (n samples). outL/outR may alias nothing. */
    void process (const float* in, float* outL, float* outR, int n) noexcept;

    // calibration (message thread)
    void requestCalibration (bool on) noexcept { calibRequest.store (on ? 1 : 2); }
    bool getCalibration (float& f1lo, float& f1hi, float& f2lo, float& f2hi) const noexcept
    { return analyser.calibrationResult (f1lo, f1hi, f2lo, f2hi); }
    bool isCalibrating() const noexcept { return analyser.isCalibrating(); }

    Telemetry telemetry;
    const AnalysisFrame& analysis() const noexcept { return analyser.get(); }
    float currentHarmonyDelaySamples() const noexcept { return harmonyDelay; }
    void setDelayMargin (float periods) noexcept { for (auto& p : psola) p.delayMargin = periods; }
    void setGrainCapMs (float ms) noexcept { tracker.setGrainCap (ms <= 0 ? 1.0e9f : (float) (ms * 0.001 * sr)); }

private:
    void controlUpdate (int n) noexcept;
    float modSourceValue (ModSource s) const noexcept;
    float destBaseNorm (ModDest d) const noexcept;
    float destFromNorm (ModDest d, float n) const noexcept;

    double sr = 48000.0;
    int maxBlock = 512;
    ParamValues params {};

    PitchTracker tracker;
    VocalAnalyser analyser;
    VoiceAllocator alloc;
    std::array<PsolaVoice, kMaxVoices> psola;
    std::array<SynthVoice, kMaxVoices> synth;

    struct VoiceState
    {
        uint32_t seenTrigger = 0;
        float glided = 60.0f;      // note (abs) or interval (rel), semitones
        float target = 60.0f;
        bool  active = false;      // PSOLA running
        float env = 0.0f;          // harmony amplitude envelope
        float velGain = 1.0f;
        OnePole level, panL, panR, vocalGain, synthGain, voicedness;
        Svf emph1, emph2;
        float emphAmt = 0.0f;
        float vibPhase = 0.0f;
    };
    std::array<VoiceState, kMaxVoices> vs {};

    // smoothed destination values (normalised) and per-route smoothers
    std::array<float, (int) ModDest::Count> destNorm {};
    std::array<OnePole, (int) ModDest::Count> destSmooth {};
    std::array<OnePole, kNumRoutes> routeSmooth {};
    std::array<OnePole, kNumMacros> macroSmooth {};

    // control signals
    float modWheel = 0.0f, lastVelocity = 0.8f, bend = 0.0f;
    float lfoPhase[2] { 0, 0 }, lfoValue[2] { 0, 0 }, lfoRandA[2] { 0, 0 }, lfoRandB[2] { 0, 0 };
    Noise lfoNoise;
    float harmonyDelay = 0.0f;
    int controlCounter = 0;

    std::atomic<int> calibRequest { 0 };
    uint32_t heldMask[4] {};

    std::vector<float> tmpVoc, tmpSyn;
    float inPeak = 0.0f, outPeak = 0.0f;
    float leadTarget = 0.0f, outTarget = 1.0f, leadPrev = 0.0f, outPrev = 1.0f;
    std::array<std::array<float, 4>, kMaxVoices> prevGain {};
    static constexpr float kHarmonyScale = 0.6f;   // ~-4.4 dB per voice for headroom

    static inline float softClip (float x) noexcept
    {
        const float ax = std::abs (x);
        if (ax <= 0.8f) return x;
        const float y = 0.8f + 0.2f * std::tanh ((ax - 0.8f) * 5.0f);
        return x < 0.0f ? -y : y;
    }
};

} // namespace vw
