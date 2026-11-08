// VowelWorm — single source of truth for every host parameter.
//
// The plugin builds its AudioProcessorValueTreeState from this table and the
// engine reads plain float values indexed by the same enum. Parameter IDs are
// part of saved Ableton sets: never rename or reorder IDs once released (append only).
#pragma once

#include "Common.h"
#include <string>
#include <vector>

namespace vw
{

enum class ModSource : int
{
    None = 0, VowelX, VowelY, Amplitude, Attack, Breath, Brightness, PitchMove, Vibrato,
    Velocity, ModWheel, LFO1, LFO2, Count
};

enum class ModDest : int
{
    None = 0, FormantShift, FormantEmphasis, SynthBlend, SynthBrightness, OscMorph, Noise,
    HarmonySpread, VibratoDepth, Glide, VoiceBalance, HarmonyLevel, SynthResonance, Count
};

inline const std::vector<std::string>& modSourceNames()
{
    static const std::vector<std::string> n { "None", "Vowel X (front)", "Vowel Y (open)", "Amplitude", "Attack",
        "Breathiness", "Brightness", "Pitch Movement", "Vibrato Depth", "Velocity", "Mod Wheel", "LFO 1", "LFO 2" };
    return n;
}

inline const std::vector<std::string>& modDestNames()
{
    static const std::vector<std::string> n { "None", "Formant Shift", "Formant Emphasis", "Synth Blend",
        "Synth Brightness", "Osc Morph", "Noise Amount", "Harmony Spread", "Vibrato Depth", "Glide",
        "Voice Balance", "Harmony Level", "Synth Resonance" };
    return n;
}

enum class PType { Float, Int, Bool, Choice };

struct ParamDesc
{
    std::string id, name, unit;
    PType type;
    float min, max, def;
    float skewCentre;               // < 0 = linear; otherwise value at the slider midpoint
    std::vector<std::string> choices;
    bool automatable = true;
};

constexpr int kNumRoutes = 8;
constexpr int kNumMacros = 4;
constexpr int kMacroSlots = 3;

// --- parameter indices --------------------------------------------------------
namespace P
{
enum : int
{
    mode, voices, octave, relRoot, latch, glide, bendRange, velSens,
    harmonyLevel, leadOn, leadLevel, outputLevel, blend,
    formantShift, formantFollow, formantEmphasis,
    expressTransfer, consonantLevel, harmonyAttack, harmonyRelease, spread,
    vibratoDepth, vibratoRate,
    // per voice (6 x 8)
    vLevel0, vPan0 = vLevel0 + kMaxVoices, vFormant0 = vPan0 + kMaxVoices, vBlend0 = vFormant0 + kMaxVoices,
    vGlide0 = vBlend0 + kMaxVoices, vDetune0 = vGlide0 + kMaxVoices,
    // analysis
    gate = vDetune0 + kMaxVoices, vowelSmooth, analysisSmooth, vowelSens, f1Lo, f1Hi, f2Lo, f2Hi,
    // synth
    oscMorph, synthBrightness, synthResonance, noise, dynamicsFollow, synthAttack, synthRelease,
    synthLevel, synthOctave, formantTrack,
    // lfos
    lfo1Rate, lfo1Shape, lfo2Rate, lfo2Shape,
    // mod matrix (4 x 8)
    modSrc0, modDst0 = modSrc0 + kNumRoutes, modDepth0 = modDst0 + kNumRoutes, modSmooth0 = modDepth0 + kNumRoutes,
    // macros
    macro0 = modSmooth0 + kNumRoutes,
    macroDst0 = macro0 + kNumMacros,                          // [macro][slot]
    macroAmt0 = macroDst0 + kNumMacros * kMacroSlots,
    // appended after v0.1 layout (IDs are append-only)
    latencyMode = macroAmt0 + kNumMacros * kMacroSlots,
    count
};
} // namespace P

inline const std::vector<ParamDesc>& paramTable()
{
    static const std::vector<ParamDesc> table = []
    {
        std::vector<ParamDesc> t ((size_t) P::count);
        auto F = [&] (int i, const char* id, const char* name, float mn, float mx, float df, const char* unit = "", float skew = -1.0f)
        { t[(size_t) i] = { id, name, unit, PType::Float, mn, mx, df, skew, {}, true }; };
        auto I = [&] (int i, const char* id, const char* name, int mn, int mx, int df, const char* unit = "")
        { t[(size_t) i] = { id, name, unit, PType::Int, (float) mn, (float) mx, (float) df, -1.0f, {}, true }; };
        auto B = [&] (int i, const char* id, const char* name, bool df)
        { t[(size_t) i] = { id, name, "", PType::Bool, 0.0f, 1.0f, df ? 1.0f : 0.0f, -1.0f, {}, true }; };
        auto C = [&] (int i, const char* id, const char* name, std::vector<std::string> ch, int df)
        { t[(size_t) i] = { id, name, "", PType::Choice, 0.0f, (float) (ch.size() - 1), (float) df, -1.0f, ch, true }; };

        C (P::mode, "mode", "Harmony Mode", { "Absolute MIDI", "Relative" }, 0);
        I (P::voices, "voices", "Voices", 1, kMaxVoices, 4);
        I (P::octave, "octave", "Octave", -2, 2, 0, "oct");
        I (P::relRoot, "relRoot", "Relative Root", 36, 84, 60);
        B (P::latch, "latch", "Latch", false);
        F (P::glide, "glide", "Glide", 0.0f, 1000.0f, 35.0f, "ms", 120.0f);
        F (P::bendRange, "bendRange", "Bend Range", 0.0f, 12.0f, 2.0f, "st");
        F (P::velSens, "velSens", "Velocity Sensitivity", 0.0f, 1.0f, 0.4f);
        F (P::harmonyLevel, "harmonyLevel", "Harmony Level", -60.0f, 6.0f, 0.0f, "dB", -12.0f);
        B (P::leadOn, "leadOn", "Lead Vocal", true);
        F (P::leadLevel, "leadLevel", "Lead Level", -60.0f, 6.0f, 0.0f, "dB", -12.0f);
        F (P::outputLevel, "outputLevel", "Output", -24.0f, 12.0f, 0.0f, "dB");
        F (P::blend, "blend", "Human / Synthetic", 0.0f, 1.0f, 0.0f);
        F (P::formantShift, "formantShift", "Formant Shift", -12.0f, 12.0f, 0.0f, "st");
        F (P::formantFollow, "formantFollow", "Formant Follow Pitch", 0.0f, 1.0f, 0.0f);
        F (P::formantEmphasis, "formantEmphasis", "Formant Emphasis", 0.0f, 1.0f, 0.0f);
        F (P::expressTransfer, "expressTransfer", "Expression Transfer", 0.0f, 1.0f, 1.0f);
        F (P::consonantLevel, "consonantLevel", "Consonant Level", 0.0f, 1.0f, 0.6f);
        F (P::harmonyAttack, "harmonyAttack", "Harmony Attack", 1.0f, 500.0f, 12.0f, "ms", 50.0f);
        F (P::harmonyRelease, "harmonyRelease", "Harmony Release", 10.0f, 3000.0f, 160.0f, "ms", 300.0f);
        F (P::spread, "spread", "Stereo Spread", 0.0f, 1.0f, 0.6f);
        F (P::vibratoDepth, "vibratoDepth", "Added Vibrato", 0.0f, 100.0f, 0.0f, "ct");
        F (P::vibratoRate, "vibratoRate", "Vibrato Rate", 2.0f, 9.0f, 5.4f, "Hz");

        static const float pans[kMaxVoices]  = { -0.55f, 0.55f, -0.25f, 0.25f, -0.85f, 0.85f, -0.1f, 0.1f };
        static const float forms[kMaxVoices] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
        for (int v = 0; v < kMaxVoices; ++v)
        {
            const std::string n = std::to_string (v + 1);
            t[(size_t) (P::vLevel0 + v)]   = { "v" + n + "Level", "Voice " + n + " Level", "dB", PType::Float, -60.0f, 6.0f, 0.0f, -12.0f, {}, true };
            t[(size_t) (P::vPan0 + v)]     = { "v" + n + "Pan", "Voice " + n + " Pan", "", PType::Float, -1.0f, 1.0f, pans[v], -1.0f, {}, true };
            t[(size_t) (P::vFormant0 + v)] = { "v" + n + "Formant", "Voice " + n + " Formant", "st", PType::Float, -6.0f, 6.0f, forms[v], -1.0f, {}, true };
            t[(size_t) (P::vBlend0 + v)]   = { "v" + n + "Blend", "Voice " + n + " Synth Blend", "", PType::Float, -1.0f, 1.0f, 0.0f, -1.0f, {}, true };
            t[(size_t) (P::vGlide0 + v)]   = { "v" + n + "Glide", "Voice " + n + " Glide Scale", "x", PType::Float, 0.0f, 4.0f, 1.0f, 1.0f, {}, true };
            t[(size_t) (P::vDetune0 + v)]  = { "v" + n + "Detune", "Voice " + n + " Detune", "ct", PType::Float, -25.0f, 25.0f, 0.0f, -1.0f, {}, true };
        }

        F (P::gate, "gate", "Input Gate", -80.0f, -20.0f, -55.0f, "dB");
        F (P::vowelSmooth, "vowelSmooth", "Vowel Smoothing", 5.0f, 500.0f, 60.0f, "ms", 80.0f);
        F (P::analysisSmooth, "analysisSmooth", "Analysis Smoothing", 1.0f, 300.0f, 25.0f, "ms", 40.0f);
        F (P::vowelSens, "vowelSens", "Vowel Sensitivity", 0.25f, 3.0f, 1.0f, "x", 1.0f);
        F (P::f1Lo, "f1Lo", "Calib F1 Low", 150.0f, 600.0f, 280.0f, "Hz");
        F (P::f1Hi, "f1Hi", "Calib F1 High", 450.0f, 1300.0f, 800.0f, "Hz");
        F (P::f2Lo, "f2Lo", "Calib F2 Low", 500.0f, 1600.0f, 800.0f, "Hz");
        F (P::f2Hi, "f2Hi", "Calib F2 High", 1400.0f, 3400.0f, 2300.0f, "Hz");

        F (P::oscMorph, "oscMorph", "Osc Morph", 0.0f, 1.0f, 0.4f);
        F (P::synthBrightness, "synthBrightness", "Synth Brightness", 0.0f, 1.0f, 0.4f);
        F (P::synthResonance, "synthResonance", "Synth Resonance", 0.0f, 1.0f, 0.5f);
        F (P::noise, "noise", "Breath Noise", 0.0f, 1.0f, 0.05f);
        F (P::dynamicsFollow, "dynamicsFollow", "Dynamics Follow", 0.0f, 1.0f, 0.8f);
        F (P::synthAttack, "synthAttack", "Synth Attack", 1.0f, 2000.0f, 25.0f, "ms", 100.0f);
        F (P::synthRelease, "synthRelease", "Synth Release", 10.0f, 5000.0f, 350.0f, "ms", 500.0f);
        F (P::synthLevel, "synthLevel", "Synth Level", -60.0f, 6.0f, 0.0f, "dB", -12.0f);
        I (P::synthOctave, "synthOctave", "Synth Octave", -2, 1, 0, "oct");
        F (P::formantTrack, "formantTrack", "Synth Vowel Tracking", 0.0f, 1.0f, 1.0f);

        static const std::vector<std::string> shapes { "Sine", "Triangle", "Drift" };
        F (P::lfo1Rate, "lfo1Rate", "LFO 1 Rate", 0.02f, 20.0f, 0.25f, "Hz", 1.0f);
        C (P::lfo1Shape, "lfo1Shape", "LFO 1 Shape", shapes, 0);
        F (P::lfo2Rate, "lfo2Rate", "LFO 2 Rate", 0.02f, 20.0f, 4.0f, "Hz", 1.0f);
        C (P::lfo2Shape, "lfo2Shape", "LFO 2 Shape", shapes, 2);

        // default routes: make the instrument expressive out of the box
        const int dSrc[kNumRoutes]   = { (int) ModSource::VowelY, (int) ModSource::VowelX, (int) ModSource::Breath, (int) ModSource::Amplitude, 0, 0, 0, 0 };
        const int dDst[kNumRoutes]   = { (int) ModDest::SynthBrightness, (int) ModDest::OscMorph, (int) ModDest::Noise, (int) ModDest::SynthBrightness, 0, 0, 0, 0 };
        const float dDep[kNumRoutes] = { 0.45f, 0.3f, 0.45f, 0.25f, 0, 0, 0, 0 };
        for (int r = 0; r < kNumRoutes; ++r)
        {
            const std::string n = std::to_string (r + 1);
            t[(size_t) (P::modSrc0 + r)]    = { "mod" + n + "Src", "Mod " + n + " Source", "", PType::Choice, 0.0f, (float) ((int) ModSource::Count - 1), (float) dSrc[r], -1.0f, modSourceNames(), true };
            t[(size_t) (P::modDst0 + r)]    = { "mod" + n + "Dst", "Mod " + n + " Destination", "", PType::Choice, 0.0f, (float) ((int) ModDest::Count - 1), (float) dDst[r], -1.0f, modDestNames(), true };
            t[(size_t) (P::modDepth0 + r)]  = { "mod" + n + "Depth", "Mod " + n + " Depth", "", PType::Float, -1.0f, 1.0f, dDep[r], -1.0f, {}, true };
            t[(size_t) (P::modSmooth0 + r)] = { "mod" + n + "Smooth", "Mod " + n + " Smoothing", "ms", PType::Float, 0.0f, 1000.0f, 20.0f, 100.0f, {}, true };
        }

        // macros: default 1 = "go synthetic", 2 = "open up", 3 = "wide & moving", 4 = "formant character"
        const int mD[kNumMacros][kMacroSlots] = {
            { (int) ModDest::SynthBlend, (int) ModDest::SynthResonance, (int) ModDest::HarmonyLevel },
            { (int) ModDest::SynthBrightness, (int) ModDest::FormantEmphasis, (int) ModDest::OscMorph },
            { (int) ModDest::HarmonySpread, (int) ModDest::VibratoDepth, (int) ModDest::Glide },
            { (int) ModDest::FormantShift, (int) ModDest::None, (int) ModDest::None } };
        const float mA[kNumMacros][kMacroSlots] = {
            { 0.8f, 0.3f, -0.15f }, { 0.5f, 0.4f, 0.25f }, { 0.4f, 0.25f, 0.3f }, { 0.25f, 0.0f, 0.0f } };
        for (int m = 0; m < kNumMacros; ++m)
        {
            const std::string n = std::to_string (m + 1);
            t[(size_t) (P::macro0 + m)] = { "macro" + n, "Macro " + n, "", PType::Float, 0.0f, 1.0f, 0.0f, -1.0f, {}, true };
            for (int s = 0; s < kMacroSlots; ++s)
            {
                const std::string sn = std::to_string (s + 1);
                t[(size_t) (P::macroDst0 + m * kMacroSlots + s)] = { "macro" + n + "Dst" + sn, "Macro " + n + " Target " + sn, "", PType::Choice, 0.0f, (float) ((int) ModDest::Count - 1), (float) mD[m][s], -1.0f, modDestNames(), true };
                t[(size_t) (P::macroAmt0 + m * kMacroSlots + s)] = { "macro" + n + "Amt" + sn, "Macro " + n + " Amount " + sn, "", PType::Float, -1.0f, 1.0f, mA[m][s], -1.0f, {}, true };
            }
        }
        C (P::latencyMode, "latencyMode", "Latency Mode", { "Quality", "Low Latency" }, 0);
        return t;
    }();
    return table;
}

using ParamValues = std::array<float, (size_t) P::count>;

inline ParamValues defaultParamValues()
{
    ParamValues v {};
    const auto& t = paramTable();
    for (size_t i = 0; i < t.size(); ++i) v[i] = t[i].def;
    return v;
}

} // namespace vw
