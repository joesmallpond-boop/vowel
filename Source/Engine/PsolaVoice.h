// VowelWorm — one TD-PSOLA harmony voice.
//
// Synthesis grains are two analysis periods long (Hann window, half-width Wg),
// taken from the shared input history centred on a PitchTracker epoch mark and
// overlap-added at the target period P. Because each grain carries one whole
// glottal cycle of the singer's own vocal-tract response, the spectral envelope
// (formants) is preserved naturally when only P changes.
//
// Independent formant shift: the grain is resampled by `formantRatio` while its
// placement stays at P, which scales the envelope without moving pitch.
//
// Unvoiced input (consonants, breath): the voice blends towards an identity
// resynthesis (P -> analysis period, formant ratio -> 1), so sibilants and
// breath pass through un-pitched instead of becoming buzzy.
#pragma once

#include "PitchTracker.h"

namespace vw
{

class PsolaVoice
{
public:
    static constexpr int kOut = 1 << 14;
    static constexpr int kOutMask = kOut - 1;

    void reset (int64_t now) noexcept
    {
        acc.fill (0.0f);
        nextSynth = (double) now;
        lastGrainMark = -1;
    }

    /** Controls (updated at control rate). */
    float targetPeriod = 200.0f;   // samples
    float formantRatio = 1.0f;     // >1 raises formants
    float voicedness = 1.0f;       // smoothed voicing 0..1; switches synthesis mode with hysteresis

    /** Render output samples for absolute times [t0, t0 + n). Input up to t0 + n must already be pushed. */
    void render (const PitchTracker& pt, int64_t t0, int n, float* out) noexcept
    {
        if (pt.markCountTotal() < 2) { std::fill (out, out + n, 0.0f); return; }

        for (int i = 0; i < n; ++i)
        {
            const int64_t t = t0 + i;
            if (nextSynth < (double) t - 4.0 * targetPeriod - 8.0)
                nextSynth = (double) t; // resync after a stall / reset

            for (int guard = 0; guard < 16; ++guard)
            {
                const float Wg = currentHalfWidth (pt);
                if (nextSynth - Wg > (double) t) break;
                renderGrain (pt, Wg, t);
            }
            float& a = acc[(size_t) (t & kOutMask)];
            out[i] = a;
            a = 0.0f;
        }
    }

    /** Content-time margin behind the synthesis point, in periods. Smaller = lower delay;
        too small and the nearest mark is often not yet available (falls back to the latest). */
    float delayMargin = 2.0f;

    /** Algorithmic delay estimate in samples (content time to output time). */
    float lastDelay = 0.0f;

private:
    // Pitch is never interpolated between the target and the input period (that
    // would produce wrong intermediate pitches); each grain is either pitch-shifted
    // (voiced) or identity-resynthesised (unvoiced), with hysteresis.
    bool shiftMode() noexcept
    {
        if (voicedness > 0.6f) shifting = true;
        else if (voicedness < 0.35f) shifting = false;
        return shifting;
    }
    float effFormant() const noexcept { return shifting ? formantRatio : 1.0f; }

    float currentHalfWidth (const PitchTracker& pt) const noexcept
    {
        const float T = std::min (pt.latestMark().period, pt.getGrainCap());
        return clampf (T / effFormant(), 16.0f, 3000.0f);
    }

    void renderGrain (const PitchTracker& pt, float Wg, int64_t tNow) noexcept
    {
        const double s = nextSynth;
        const float T = pt.latestMark().period;
        const bool shift = shiftMode();
        const float fr = effFormant();

        // choose the analysis mark nearest to a content time that keeps a stable delay
        const double target = s - (double) Wg - (double) delayMargin * (double) std::min (T, pt.getGrainCap());
        const int avail = std::min (pt.markCountTotal(), 12);
        int bestK = 0; double bestD = 1.0e18;
        for (int k = 0; k < avail; ++k)
        {
            const double d = std::abs ((double) pt.markBack (k).pos - target);
            if (d < bestD) { bestD = d; bestK = k; }
            if ((double) pt.markBack (k).pos < target - 2.0 * T) break;
        }
        const Mark& m = pt.markBack (bestK);
        lastDelay = (float) (s - (double) m.pos);

        // synthesis period, crossfaded to identity when unvoiced
        const float P = std::max (8.0f, shift ? targetPeriod : m.period);

        // Hann half-width Wg at spacing P sums to Wg/P. Full normalisation makes
        // upward shifts too quiet (fewer harmonics under each formant); sqrt is a
        // perceptual compromise between energy and amplitude preservation.
        const float g = P < Wg ? std::sqrt (P / Wg) : 1.0f;

        const auto& h = pt.history();
        const int64_t i0 = std::max (tNow, (int64_t) std::ceil (s - Wg));
        const int64_t i1 = (int64_t) std::floor (s + Wg);
        const float invW = kPi / Wg;
        for (int64_t idx = i0; idx <= i1; ++idx)
        {
            const float u = (float) ((double) idx - s);
            const float w = 0.5f + 0.5f * std::cos (u * invW);
            const float x = h.interp ((double) m.pos + (double) (u * fr));
            acc[(size_t) (idx & kOutMask)] += g * w * x;
        }
        nextSynth += P;
        lastGrainMark = m.pos;
    }

    std::array<float, kOut> acc {};
    double nextSynth = 0.0;
    bool shifting = true;
    int64_t lastGrainMark = -1;
};

} // namespace vw
