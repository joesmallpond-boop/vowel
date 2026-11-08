// VowelWorm — factory presets. Each preset is a set of overrides on top of the
// parameter defaults (so every preset is complete and deterministic).
#pragma once

#include "Params.h"
#include <map>
#include <string>
#include <vector>

namespace vw
{

struct PresetData
{
    std::string name;
    std::map<std::string, float> values; // param id -> real value
};

namespace detail
{
inline float S (ModSource s) { return (float) (int) s; }
inline float D (ModDest d) { return (float) (int) d; }
}

inline const std::vector<PresetData>& factoryPresets()
{
    using namespace detail;
    using MS = ModSource;
    using MD = ModDest;
    // clears the four default routes so a preset fully defines its own matrix
    auto clearRoutes = [] (std::map<std::string, float>& m)
    {
        for (int r = 1; r <= kNumRoutes; ++r) { m["mod" + std::to_string (r) + "Src"] = 0; m["mod" + std::to_string (r) + "Dst"] = 0; }
    };
    auto route = [] (std::map<std::string, float>& m, int r, MS s, MD d, float depth, float smoothMs = 20.0f)
    {
        const std::string n = std::to_string (r);
        m["mod" + n + "Src"] = S (s); m["mod" + n + "Dst"] = D (d); m["mod" + n + "Depth"] = depth; m["mod" + n + "Smooth"] = smoothMs;
    };
    auto voices = [] (std::map<std::string, float>& m, const char* what, std::vector<float> v)
    {
        for (size_t i = 0; i < v.size(); ++i) m["v" + std::to_string (i + 1) + what] = v[i];
    };

    static const std::vector<PresetData> presets = [&]
    {
        std::vector<PresetData> p;

        { // 1 — clean, realistic: pure PSOLA voices, gentle ensemble detune
            PresetData d { "Natural Four-Part", {} };
            auto& m = d.values;
            m["voices"] = 4; m["blend"] = 0; m["glide"] = 35; m["spread"] = 0.6f;
            voices (m, "Detune", { -3, 3, -2, 2 });
            p.push_back (d);
        }
        { // 2 — six soft voices, slow attack, varied formants, a little synthetic air
            PresetData d { "Warm Choir", {} };
            auto& m = d.values;
            m["voices"] = 6; m["blend"] = 0.18f; m["glide"] = 80; m["spread"] = 0.85f;
            m["harmonyAttack"] = 60; m["harmonyRelease"] = 700; m["velSens"] = 0.3f;
            m["formantEmphasis"] = 0.12f; m["vibratoDepth"] = 7; m["vibratoRate"] = 5.1f;
            m["oscMorph"] = 0.15f; m["synthBrightness"] = 0.3f; m["synthResonance"] = 0.6f; m["noise"] = 0.08f;
            m["synthAttack"] = 90; m["synthRelease"] = 900;
            voices (m, "Formant", { -0.6f, 0.5f, -0.3f, 0.4f, -0.8f, 0.7f });
            voices (m, "Detune", { -6, 5, -3, 4, -8, 7 });
            voices (m, "Glide", { 0.8f, 1.2f, 1.0f, 1.4f, 0.7f, 1.1f });
            clearRoutes (m);
            route (m, 1, MS::VowelY, MD::SynthBrightness, 0.35f, 60);
            route (m, 2, MS::Breath, MD::Noise, 0.3f, 60);
            route (m, 3, MS::LFO2, MD::FormantShift, 0.015f, 200);
            m["lfo2Rate"] = 0.4f; m["lfo2Shape"] = 2;
            p.push_back (d);
        }
        { // 3 — vocal + saw hybrid; brightness and resonance ride the singer's vowel and attacks
            PresetData d { "Synthetic Gospel", {} };
            auto& m = d.values;
            m["voices"] = 4; m["blend"] = 0.5f; m["glide"] = 60; m["spread"] = 0.7f;
            m["formantEmphasis"] = 0.25f;
            m["oscMorph"] = 0.62f; m["synthBrightness"] = 0.35f; m["synthResonance"] = 0.5f; m["noise"] = 0.04f;
            m["dynamicsFollow"] = 0.9f; m["synthAttack"] = 15; m["synthRelease"] = 380;
            clearRoutes (m);
            route (m, 1, MS::VowelY, MD::SynthBrightness, 0.5f);
            route (m, 2, MS::Attack, MD::SynthResonance, 0.3f, 10);
            route (m, 3, MS::Amplitude, MD::SynthBrightness, 0.3f);
            route (m, 4, MS::PitchMove, MD::OscMorph, 0.15f, 40);
            route (m, 5, MS::Breath, MD::Noise, 0.3f);
            route (m, 6, MS::VowelX, MD::FormantEmphasis, 0.2f, 60);
            p.push_back (d);
        }
        { // 4 — mostly synthetic, sine-ish source through high-Q formants: precise and glassy
            PresetData d { "Glass Voice", {} };
            auto& m = d.values;
            m["voices"] = 4; m["blend"] = 0.78f; m["glide"] = 0; m["expressTransfer"] = 0.3f;
            m["harmonyAttack"] = 5; m["spread"] = 0.75f;
            m["oscMorph"] = 0.05f; m["synthBrightness"] = 0.75f; m["synthResonance"] = 0.88f; m["noise"] = 0;
            m["dynamicsFollow"] = 0.6f; m["synthAttack"] = 6; m["synthRelease"] = 500; m["synthLevel"] = -2;
            clearRoutes (m);
            route (m, 1, MS::VowelX, MD::SynthResonance, 0.12f, 50);
            route (m, 2, MS::VowelY, MD::SynthBrightness, 0.2f, 50);
            route (m, 3, MS::Vibrato, MD::OscMorph, 0.25f, 80);
            p.push_back (d);
        }
        { // 5 — the keyboard plays a sustained formant synth; the voice only shapes its vowel
            PresetData d { "Formant Organ", {} };
            auto& m = d.values;
            m["voices"] = 4; m["blend"] = 1.0f; m["glide"] = 0; m["velSens"] = 0;
            m["oscMorph"] = 0.5f; m["synthBrightness"] = 0.45f; m["synthResonance"] = 0.75f; m["noise"] = 0;
            m["dynamicsFollow"] = 0.0f; m["synthAttack"] = 8; m["synthRelease"] = 140; m["synthLevel"] = -3;
            m["vowelSmooth"] = 90;
            clearRoutes (m);
            route (m, 1, MS::VowelY, MD::SynthBrightness, 0.4f, 40);
            route (m, 2, MS::VowelX, MD::OscMorph, 0.25f, 40);
            route (m, 3, MS::Amplitude, MD::SynthResonance, 0.15f, 30);
            p.push_back (d);
        }
        { // 6 — airy noise-led choir; breathier singing opens the noise and softens the tone
            PresetData d { "Breath Choir", {} };
            auto& m = d.values;
            m["voices"] = 6; m["blend"] = 0.65f; m["glide"] = 120; m["spread"] = 0.9f; m["consonantLevel"] = 0.8f;
            m["harmonyAttack"] = 80; m["harmonyRelease"] = 900;
            m["oscMorph"] = 0.2f; m["synthBrightness"] = 0.35f; m["synthResonance"] = 0.7f; m["noise"] = 0.45f;
            m["dynamicsFollow"] = 1.0f; m["synthAttack"] = 120; m["synthRelease"] = 1100;
            voices (m, "Detune", { -5, 6, -3, 3, -7, 8 });
            clearRoutes (m);
            route (m, 1, MS::Breath, MD::Noise, 0.45f, 50);
            route (m, 2, MS::Breath, MD::SynthBrightness, -0.2f, 80);
            route (m, 3, MS::VowelY, MD::SynthResonance, 0.2f, 60);
            route (m, 4, MS::LFO2, MD::HarmonySpread, 0.1f, 300);
            m["lfo2Rate"] = 0.15f; m["lfo2Shape"] = 2;
            p.push_back (d);
        }
        { // 7 — long, staggered glides; formants and spread stretch with pitch movement
            PresetData d { "Elastic Harmony", {} };
            auto& m = d.values;
            m["voices"] = 4; m["blend"] = 0.15f; m["glide"] = 260; m["formantFollow"] = 0.35f;
            m["vibratoDepth"] = 12; m["spread"] = 0.6f;
            voices (m, "Glide", { 0.5f, 1.0f, 1.6f, 2.2f });
            clearRoutes (m);
            route (m, 1, MS::PitchMove, MD::FormantShift, 0.15f, 30);
            route (m, 2, MS::VowelY, MD::FormantEmphasis, 0.4f, 50);
            route (m, 3, MS::LFO1, MD::HarmonySpread, 0.3f, 100);
            route (m, 4, MS::Attack, MD::Glide, -0.25f, 10);
            m["lfo1Rate"] = 0.35f;
            p.push_back (d);
        }
        { // 8 — octave-down voices + sub synth, dark pulse through resonant formants
            PresetData d { "Dark Resonance", {} };
            auto& m = d.values;
            m["voices"] = 4; m["octave"] = -1; m["synthOctave"] = 0; m["blend"] = 0.6f; m["glide"] = 90;
            m["formantShift"] = -3; m["formantEmphasis"] = 0.3f; m["harmonyLevel"] = -2; m["spread"] = 0.5f;
            m["oscMorph"] = 0.72f; m["synthBrightness"] = 0.15f; m["synthResonance"] = 0.8f; m["noise"] = 0.03f;
            m["synthAttack"] = 60; m["synthRelease"] = 1300;
            clearRoutes (m);
            route (m, 1, MS::Amplitude, MD::SynthBrightness, 0.35f, 40);
            route (m, 2, MS::VowelY, MD::SynthBrightness, 0.2f, 60);
            route (m, 3, MS::LFO1, MD::SynthResonance, 0.15f, 200);
            m["lfo1Rate"] = 0.08f;
            p.push_back (d);
        }
        return p;
    }();
    return presets;
}

/** Full value set for a preset (defaults + overrides). */
inline ParamValues presetValues (const PresetData& p)
{
    ParamValues v = defaultParamValues();
    const auto& t = paramTable();
    for (size_t i = 0; i < t.size(); ++i)
    {
        auto it = p.values.find (t[i].id);
        if (it != p.values.end()) v[i] = clampf (it->second, t[i].min, t[i].max);
    }
    return v;
}

} // namespace vw
