// VowelWorm — input history, YIN pitch tracking and pitch-synchronous epoch marking.
//
// One PitchTracker is shared by every harmony voice. It owns the absolute-time
// history of the mono input, estimates f0 with YIN every hop (~2.7 ms), and
// places analysis epochs ("pitch marks") one period apart, aligned to the
// positive peak of a fundamental-tracking low-pass copy of the signal. PSOLA
// voices read grains centred on these marks.
#pragma once

#include "Common.h"
#include <vector>

namespace vw
{

struct Mark
{
    int64_t pos = 0;      // absolute sample index (input timeline)
    float   period = 0;   // local period in samples
    float   voicing = 0;  // 0..1 voicing confidence when the mark was placed
};

class PitchTracker
{
public:
    static constexpr int kHistory = 1 << 15;  // 32768 samples (0.34 s at 96 kHz)
    static constexpr int kMarkCap = 512;

    struct Settings
    {
        float minHz = 65.0f;
        float maxHz = 1100.0f;
        float yinThreshold = 0.15f;
        float gateDb = -55.0f;
    };

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        // YIN runs on a ~22-24 kHz decimated copy (4x cheaper), then the lag is
        // refined on the full-rate signal so accuracy is not lost.
        decim = std::max (1, (int) std::lround (sr / 24000.0));
        asr = sr / decim;
        hop = std::max (16, (int) std::lround (asr / 300.0));   // ~3.3 ms
        aa[0].lowpass ((float) (asr * 0.42), 0.5412f, sr);
        aa[1].lowpass ((float) (asr * 0.42), 1.3066f, sr);
        tauMax = (int) std::ceil (asr / settings.minHz) + 2;
        tauMin = std::max (2, (int) std::floor (asr / settings.maxHz));
        window = tauMax;
        frame.assign ((size_t) (window + tauMax + 4), 0.0f);
        diff.assign ((size_t) (tauMax + 2), 0.0f);
        cmnd.assign ((size_t) (tauMax + 2), 1.0f);
        reset();
    }

    void reset()
    {
        hist.clear();
        lpHist.clear();
        aHist.clear();
        lp1 = lp2 = 0; aa[0].reset(); aa[1].reset();
        decimCount = 0; hopCount = 0;
        f0 = 0; period = (float) (sr / 200.0); voicing = 0; aperiodicity = 1; rms = 0;
        lastMarkPos = -1; markCount = 0;
        med[0] = med[1] = med[2] = 0;
        energyAcc = 0;
    }

    void setSettings (const Settings& s) { settings = s; }
    /** Maximum grain half-width in samples (low-latency mode); default = unlimited (one period). */
    void setGrainCap (float samples) noexcept { grainCap = samples; }
    float getGrainCap() const noexcept { return grainCap; }

    /** Push one input sample. Call `updateMarks()` after a run of samples. */
    inline void push (float x) noexcept
    {
        hist.push (x);

        // fundamental-tracking 2-pole low-pass for stable epoch alignment
        lp1 += lpCoeff * (x - lp1);
        lp2 += lpCoeff * (lp1 - lp2);
        lpHist.push (lp2);

        energyAcc += x * x;

        if (decim == 1)
        {
            aHist.push (x);
            if (++hopCount >= hop) { hopCount = 0; analyse(); }
        }
        else
        {
            const float y = aa[1].process (aa[0].process (x));
            if (++decimCount >= decim)
            {
                decimCount = 0;
                aHist.push (y);
                if (++hopCount >= hop) { hopCount = 0; analyse(); }
            }
        }
    }

    /** Place any epoch marks that now have a full period of data after them. */
    void updateMarks() noexcept
    {
        const int64_t now = hist.written;
        const float T = period;
        const bool voiced = voicing > 0.5f;

        if (lastMarkPos < 0 || lastMarkPos < now - (int64_t) (4.0f * T) - 64)
        {
            // (re)start the mark chain just inside the available region
            int64_t start = now - (int64_t) (2.0f * T);
            if (voiced) start = findPeak (start - (int64_t) (0.5f * T), start + (int64_t) (0.5f * T));
            pushMark (start, T);
        }

        for (int guard = 0; guard < 64; ++guard)
        {
            const float sh = voiced ? 0.3f * T : 0.0f;
            const int64_t pred = lastMarkPos + (int64_t) std::lround (T);
            // a mark is usable once the grain's right half exists
            if ((double) pred + sh + std::min (T, grainCap) > (double) now) break;

            int64_t m = pred;
            if (voiced)
            {
                m = findPeak (pred - (int64_t) sh, pred + (int64_t) sh);
                // avoid degenerate spacing (keeps marks at least 0.5 T apart)
                if (m - lastMarkPos < (int64_t) (0.5f * T)) m = pred;
            }
            pushMark (m, T);
        }
    }

    // ---- results ---------------------------------------------------------------
    float getF0() const noexcept           { return f0; }        // Hz (last voiced estimate, held)
    float getPeriod() const noexcept       { return period; }    // samples at full rate
    float getVoicing() const noexcept      { return voicing; }   // 0..1
    float getAperiodicity() const noexcept { return aperiodicity; }
    float getRms() const noexcept          { return rms; }
    bool  isVoiced() const noexcept        { return voicing > 0.5f; }
    int64_t now() const noexcept           { return hist.written; }

    const History<kHistory>& history() const noexcept { return hist; }

    int markCountTotal() const noexcept { return markCount; }
    const Mark& latestMark() const noexcept { return marks[(size_t) ((markCount - 1) & (kMarkCap - 1))]; }
    /** k = 0 is the latest mark, 1 the one before, ... */
    const Mark& markBack (int k) const noexcept { return marks[(size_t) ((markCount - 1 - k) & (kMarkCap - 1))]; }

    Settings settings;

private:
    void pushMark (int64_t pos, float T) noexcept
    {
        Mark& m = marks[(size_t) (markCount & (kMarkCap - 1))];
        m.pos = pos; m.period = T; m.voicing = voicing;
        ++markCount;
        lastMarkPos = pos;
    }

    int64_t findPeak (int64_t a, int64_t b) const noexcept
    {
        a = std::max (a, lpHist.written - lpHist.kSize + 8);
        b = std::min (b, lpHist.written - 1);
        int64_t best = a; float bv = -1.0e9f;
        for (int64_t i = a; i <= b; ++i)
        {
            const float v = lpHist.atUnchecked (i);
            if (v > bv) { bv = v; best = i; }
        }
        return best;
    }

    void analyse() noexcept
    {
        // energy gate over the analysis hop
        const float hopRms = std::sqrt (energyAcc / (float) (hop * decim));
        energyAcc = 0.0f;
        rms = hopRms;

        const int n = window + tauMax;
        const int64_t start = aHist.written - n;
        for (int i = 0; i < n; ++i) frame[(size_t) i] = aHist.at (start + i);

        // YIN difference function
        const float* x = frame.data();
        diff[0] = 0.0f;
        for (int tau = 1; tau <= tauMax; ++tau)
        {
            const float* y = x + tau;
            float s = 0.0f;
            for (int j = 0; j < window; ++j) { const float d = x[j] - y[j]; s += d * d; }
            diff[(size_t) tau] = s;
        }

        // cumulative mean normalised difference
        cmnd[0] = 1.0f;
        float running = 0.0f;
        for (int tau = 1; tau <= tauMax; ++tau)
        {
            running += diff[(size_t) tau];
            cmnd[(size_t) tau] = running > 1.0e-12f ? diff[(size_t) tau] * (float) tau / running : 1.0f;
        }

        int best = -1;
        for (int tau = tauMin; tau < tauMax; ++tau)
        {
            if (cmnd[(size_t) tau] < settings.yinThreshold)
            {
                while (tau + 1 < tauMax && cmnd[(size_t) tau + 1] < cmnd[(size_t) tau]) ++tau;
                best = tau;
                break;
            }
        }
        if (best < 0)
        {
            float mv = 1.0e9f;
            for (int tau = tauMin; tau < tauMax; ++tau)
                if (cmnd[(size_t) tau] < mv) { mv = cmnd[(size_t) tau]; best = tau; }
        }

        float ap = cmnd[(size_t) best];
        float tauF = (float) best;
        if (best > tauMin && best < tauMax - 1)
        {
            const float a = cmnd[(size_t) best - 1], b = cmnd[(size_t) best], c = cmnd[(size_t) best + 1];
            const float den = a - 2.0f * b + c;
            if (std::abs (den) > 1.0e-9f) tauF += 0.5f * (a - c) / den;
        }

        // stationarity: both halves of the analysis frame must carry energy, so a
        // frame straddling a note onset (silence + voice) cannot report a sub-octave
        float e1 = 0.0f, e2 = 0.0f;
        const int half = n / 2;
        for (int i = 0; i < half; ++i) e1 += x[i] * x[i];
        for (int i = half; i < n; ++i) e2 += x[i] * x[i];
        const bool stationary = std::min (e1, e2) > 0.12f * std::max (e1, e2);

        const float gate = dbToGain (settings.gateDb);
        const bool voicedNow = ap < 0.25f && hopRms > gate && stationary;

        aperiodicity += 0.5f * (clamp01 (ap) - aperiodicity);
        const float vTarget = voicedNow ? clamp01 (1.4f - 2.0f * ap) : 0.0f;
        voicing += (vTarget > voicing ? 0.6f : 0.25f) * (vTarget - voicing);

        if (voicedNow)
        {
            float lagFull = tauF * (float) decim;
            if (decim > 1) lagFull = refineLag (lagFull);
            const float hz = (float) sr / lagFull;
            // 3-point median removes isolated octave blips
            med[0] = med[1]; med[1] = med[2]; med[2] = hz;
            float m = hz;
            if (med[0] > 0.0f && med[1] > 0.0f)
                m = std::max (std::min (med[0], med[1]), std::min (std::max (med[0], med[1]), med[2]));
            f0 = m;
            period = (float) (sr / f0);
            // track the fundamental with the epoch low-pass (~1.6 x f0)
            const float fc = clampf (1.6f * f0, 60.0f, 2000.0f);
            lpCoeff = 1.0f - std::exp (-kTwoPi * fc / (float) sr);
        }
    }

    /** Full-rate difference function around the decimated estimate + parabolic interpolation. */
    float refineLag (float lag) const noexcept
    {
        const int c = (int) std::lround (lag);
        const int w = std::min ((int) (lag * 2.0f), 2048);
        const int64_t base = hist.written - w - c - 8;
        float d[9];
        for (int k = -4; k <= 4; ++k)
        {
            const int tau = c + k;
            float s = 0.0f;
            for (int j = 0; j < w; ++j) { const float e = hist.atUnchecked (base + j) - hist.atUnchecked (base + j + tau); s += e * e; }
            d[k + 4] = s;
        }
        int bi = 4;
        for (int k = 1; k < 8; ++k) if (d[k] < d[bi]) bi = k;
        float off = 0.0f;
        const float den = d[bi - 1] - 2.0f * d[bi] + d[bi + 1];
        if (std::abs (den) > 1.0e-12f) off = 0.5f * (d[bi - 1] - d[bi + 1]) / den;
        return (float) (c + bi - 4) + clampf (off, -0.5f, 0.5f);
    }

    double sr = 48000.0, asr = 48000.0;
    int decim = 1, decimCount = 0, hop = 128, hopCount = 0;
    int tauMin = 40, tauMax = 740, window = 740;
    std::vector<float> frame, diff, cmnd;

    History<kHistory> hist, lpHist, aHist;
    float lp1 = 0, lp2 = 0, lpCoeff = 0.05f;
    std::array<Biquad, 2> aa {};
    float energyAcc = 0;

    float f0 = 0, period = 240, voicing = 0, aperiodicity = 1, rms = 0;
    float med[3] {};

    std::array<Mark, kMarkCap> marks {};
    int markCount = 0;
    int64_t lastMarkPos = -1;
    float grainCap = 1.0e9f;
};

} // namespace vw
