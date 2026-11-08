// VowelWorm — compact voice-following synth voice.
//
//   morphing band-limited oscillator (sine > triangle > saw > narrow pulse)
//   + breath noise
//   -> three-band formant filter whose F1/F2 follow the SINGER's measured
//      formants (scaled by the voice's formant shift), F3 follows frontness
//   -> key-tracked 2-pole brightness low-pass
//   -> amplitude = gate envelope x (optionally) the singer's own dynamics
//
// The relationship to the voice lives in the formant filter and the dynamics:
// a dark "oo" gives a muted, hollow choir; opening to "ah" raises F1 and the
// brightness routes add harmonic richness.
#pragma once

#include "Common.h"

namespace vw
{

class SynthVoice
{
public:
    struct Controls
    {
        float freq = 220;         // Hz
        float morph = 0.3f;       // 0..1
        float brightness = 0.5f;  // 0..1
        float resonance = 0.5f;   // 0..1 formant filter amount/Q
        float noise = 0.0f;       // 0..1
        float f1 = 500, f2 = 1500, f3 = 2600; // Hz, already formant-shifted
        float amp = 1.0f;         // dynamics & velocity (smoothed by caller)
    };

    void prepare (double sampleRate) { sr = sampleRate; reset(); }
    void reset()
    {
        phase = 0; tri = 0; env = 0;
        for (auto& f : bands) f.reset();
        tone.reset();
        ampS.reset (0);
    }

    void setEnvelope (float attackMs, float releaseMs)
    {
        attC = onePoleCoeff (attackMs, sr);
        relC = onePoleCoeff (releaseMs, sr);
    }

    /** Set per control block. */
    void setControls (const Controls& c) noexcept
    {
        ctl = c;
        const float q = 2.5f + 14.0f * c.resonance;
        bands[0].set (c.f1, q, sr);
        bands[1].set (c.f2, q * 1.2f, sr);
        bands[2].set (c.f3, q * 1.4f, sr);
        const float cutoff = clampf (c.freq * std::exp2 (1.0f + 7.0f * c.brightness), 120.0f, 18000.0f);
        tone.set (cutoff, 0.6f + 0.6f * c.resonance * c.brightness, sr);
        dt = clampf (c.freq / (float) sr, 0.0f, 0.45f);
    }

    bool isSilent() const noexcept { return env < 1.0e-5f && ! gate; }

    bool gate = false;
    void seed (uint32_t s) noexcept { noise.s = s | 1u; }

    /** Adds the voice's mono output into `out`. */
    void process (float* out, int n) noexcept
    {
        if (isSilent()) return;
        const float m = ctl.morph * 3.0f;
        const float mixAmt = 0.25f + 0.75f * ctl.resonance;
        const float ampCoef = 0.004f;

        for (int i = 0; i < n; ++i)
        {
            env += (gate ? attC : relC) * ((gate ? 1.0f : 0.0f) - env);
            const float a = ampS.process (ctl.amp, ampCoef);

            phase += dt; if (phase >= 1.0f) phase -= 1.0f;
            float o;
            if (m < 1.0f)
            {
                const float sine = std::sin (kTwoPi * phase);
                const float tr = 1.0f - 4.0f * std::abs (phase - 0.5f);
                o = lerp (sine, tr, m);
            }
            else if (m < 2.0f)
            {
                const float tr = 1.0f - 4.0f * std::abs (phase - 0.5f);
                o = lerp (tr, saw (phase), m - 1.0f);
            }
            else
            {
                const float t = m - 2.0f;
                const float pw = 0.5f - 0.38f * t;
                float p2 = phase + pw; if (p2 >= 1.0f) p2 -= 1.0f;
                const float pulse = saw (phase) - saw (p2);
                o = lerp (saw (phase), pulse, t);
            }

            const float src = o + ctl.noise * 0.9f * noise.next();

            float lp, b1, b2, b3;
            bands[0].process (src, lp, b1);
            bands[1].process (src, lp, b2);
            bands[2].process (src, lp, b3);
            const float k = bands[0].k;
            const float formant = (b1 * 1.0f + b2 * 0.6f + b3 * 0.3f) * k * 1.4f;
            const float shaped = lerp (src, formant, mixAmt);

            float tl, tb;
            tone.process (shaped, tl, tb);
            out[i] += tl * env * a * 0.33f; // level-matched to the PSOLA voices (see testBlendBalance)
        }
    }

private:
    float saw (float p) const noexcept
    {
        float v = 2.0f * p - 1.0f;
        v -= polyBlep (p);
        return v;
    }
    float polyBlep (float t) const noexcept
    {
        if (t < dt) { t /= dt; return t + t - t * t - 1.0f; }
        if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
        return 0.0f;
    }

    double sr = 48000;
    Controls ctl;
    float phase = 0, dt = 0.005f, tri = 0, env = 0;
    float attC = 0.01f, relC = 0.001f;
    std::array<Svf, 3> bands;
    Svf tone;
    Noise noise;
    OnePole ampS;
};

} // namespace vw
