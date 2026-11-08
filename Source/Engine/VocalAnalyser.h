// VowelWorm — continuous vocal-expression analysis.
//
// Produces smooth, musically useful control signals rather than phoneme labels:
//   amplitude, attack, brightness, breathiness, pitch movement, vibrato depth/rate,
//   formants F1/F2 (LPC on a ~12 kHz decimated signal) and the derived
//   vowel-space position (X = frontness from F2, Y = openness from F1).
//
// Vowel tracking is only updated while the voice is confidently voiced and above
// the gate; outliers are rejected with a 5-frame median and a jump limiter, then
// one-pole smoothed. During consonants and silence the vowel position holds.
#pragma once

#include "PitchTracker.h"
#include <vector>

namespace vw
{

struct AnalysisFrame
{
    float amplitude = 0;    // 0..1 (-60..0 dBFS)
    float attack = 0;       // 0..1 onset intensity
    float brightness = 0;   // 0..1 (log centroid 300 Hz..5 kHz)
    float breath = 0;       // 0..1 breathiness / aperiodicity
    float pitchMove = 0;    // -1..1 (deviation from slow pitch centre, +/-100 cents)
    float vibratoDepth = 0; // 0..1 (0..100 cents)
    float vibratoRate = 0;  // Hz
    float vowelX = 0.5f;    // 0..1 frontness
    float vowelY = 0.5f;    // 0..1 openness
    float f1 = 500, f2 = 1500; // Hz (smoothed)
    float pitchDev = 0;     // semitones, unsmoothed deviation from the slow centre (+/-1.5)
    float pitchSemis = 60;  // sung pitch (MIDI note number, held through unvoiced)
    float voicing = 0;
};

class VocalAnalyser
{
public:
    struct Settings
    {
        float vowelSmoothMs = 60.0f;
        float analysisSmoothMs = 25.0f;
        float sensitivity = 1.0f;  // 0.25..4 scales X/Y excursion around the centre
        float f1Lo = 280, f1Hi = 800;    // calibration ranges
        float f2Lo = 800, f2Hi = 2300;
    };

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        decim = std::max (1, (int) std::lround (sr / 11025.0));
        dsr = sr / decim;
        frameLen = (int) std::lround (dsr * 0.028);
        lpcHop = std::max (1, (int) std::lround (dsr * 0.005));
        dHist.assign ((size_t) 2048, 0.0f);
        win.resize ((size_t) frameLen);
        for (int i = 0; i < frameLen; ++i)
            win[(size_t) i] = 0.54f - 0.46f * std::cos (kTwoPi * (float) i / (float) (frameLen - 1));
        for (int b = 0; b < kBins; ++b)
        {
            const double f = kFLo + (kFHi - kFLo) * b / (kBins - 1);
            const double w = 2.0 * kPi * f / dsr;
            for (int k = 0; k <= kOrder; ++k) { cosT[(size_t) b][(size_t) k] = (float) std::cos (w * k); sinT[(size_t) b][(size_t) k] = (float) std::sin (w * k); }
        }
        fastEnv.setup (1.0f, 40.0f, sr);
        slowEnv.setup (60.0f, 250.0f, sr);
        // anti-alias: two cascaded biquad low-passes at 0.42 * dsr/2
        designLowpass ((float) (0.42 * dsr * 0.5));
        reset();
    }

    void reset()
    {
        std::fill (dHist.begin(), dHist.end(), 0.0f);
        dWrite = 0; decimCount = 0; lpcCount = 0;
        for (auto& b : aa) b.z1 = b.z2 = 0;
        e2 = d2 = 0; prevX = 0; preEmPrev = 0;
        frame = {};
        medN = 0; haveFormants = false;
        f1s.reset (500); f2s.reset (1500);
        pitchCentre = 60; havePitch = false;
        vibBp1 = vibBp2 = 0; vibEnv = 0; lastSign = 0; zcInterval.reset (5.5f); sinceZc = 0;
        attackHold = 0;
        calibrating = false;
    }

    Settings settings;

    /** Push a block of mono input; reads pitch/voicing from the shared tracker. */
    void process (const float* x, int n, const PitchTracker& pt) noexcept
    {
        for (int i = 0; i < n; ++i)
        {
            const float s = x[i];
            fastEnv.process (s);
            slowEnv.process (s);
            const float d = s - prevX; prevX = s;
            e2 += 0.002f * (s * s - e2);
            d2 += 0.002f * (d * d - d2);

            float y = s;
            for (auto& b : aa) y = b.process (y);
            if (++decimCount >= decim)
            {
                decimCount = 0;
                const float pe = y - 0.94f * preEmPrev; preEmPrev = y;
                dHist[(size_t) (dWrite & 2047)] = pe; ++dWrite;
                if (++lpcCount >= lpcHop) { lpcCount = 0; formantFrame (pt); }
            }
        }
        controlUpdate (pt, n);
    }

    const AnalysisFrame& get() const noexcept { return frame; }

    // ---- calibration ----------------------------------------------------------
    void startCalibration() noexcept { calibrating = true; cF1Lo = 1e9f; cF1Hi = 0; cF2Lo = 1e9f; cF2Hi = 0; calibFrames = 0; }
    void stopCalibration() noexcept  { calibrating = false; }
    bool isCalibrating() const noexcept { return calibrating; }
    bool calibrationResult (float& f1lo, float& f1hi, float& f2lo, float& f2hi) const noexcept
    {
        if (calibFrames < 40 || cF1Hi < cF1Lo * 1.3f || cF2Hi < cF2Lo * 1.3f) return false;
        f1lo = cF1Lo; f1hi = cF1Hi; f2lo = cF2Lo; f2hi = cF2Hi; return true;
    }

    float rawF1 = 500, rawF2 = 1500; // last accepted per-frame estimates

private:
    void designLowpass (float fc)
    {
        aa[0].lowpass (fc, 0.5412f, sr);
        aa[1].lowpass (fc, 1.3066f, sr);
    }

    void formantFrame (const PitchTracker& pt) noexcept
    {
        const bool usable = pt.getVoicing() > 0.6f && pt.getRms() > dbToGain (-48.0f);
        if (! usable) return;

        constexpr int order = kOrder;
        float buf[1024];
        const int n = std::min (frameLen, 1024);
        for (int i = 0; i < n; ++i)
            buf[i] = dHist[(size_t) ((dWrite - n + i) & 2047)] * win[(size_t) i];

        double r[order + 1];
        for (int k = 0; k <= order; ++k)
        {
            double s = 0;
            for (int i = k; i < n; ++i) s += (double) buf[i] * buf[i - k];
            r[k] = s;
        }
        if (r[0] < 1.0e-9) return;
        r[0] *= 1.0001; // white-noise floor / lag window
        for (int k = 1; k <= order; ++k) r[k] *= std::exp (-0.5 * std::pow (2.0 * kPi * 40.0 * k / dsr, 2.0));

        double a[order + 1] = { 1.0 }, tmp[order + 1];
        double err = r[0];
        for (int i = 1; i <= order; ++i)
        {
            double acc = r[i];
            for (int j = 1; j < i; ++j) acc += a[j] * r[i - j];
            const double k = -acc / err;
            for (int j = 0; j <= i; ++j) tmp[j] = a[j];
            for (int j = 1; j < i; ++j) a[j] = tmp[j] + k * tmp[i - j];
            a[i] = k;
            err *= (1.0 - k * k);
            if (err <= 0) return;
        }

        // evaluate the LPC envelope 150..3600 Hz and pick resonance peaks
        constexpr int bins = kBins;
        float env[bins];
        const double fLo = kFLo, fHi = kFHi;
        for (int b = 0; b < bins; ++b)
        {
            float re = 0, im = 0;
            for (int k = 0; k <= order; ++k) { re += (float) a[k] * cosT[(size_t) b][(size_t) k]; im -= (float) a[k] * sinT[(size_t) b][(size_t) k]; }
            env[b] = -10.0f * std::log10 (re * re + im * im + 1e-12f);
        }
        float peaks[8]; int np = 0;
        for (int b = 1; b < bins - 1 && np < 8; ++b)
            if (env[b] > env[b - 1] && env[b] >= env[b + 1])
            {
                const float den = env[b - 1] - 2 * env[b] + env[b + 1];
                const float off = std::abs (den) > 1e-6f ? 0.5f * (env[b - 1] - env[b + 1]) / den : 0.0f;
                peaks[np++] = (float) (fLo + (fHi - fLo) * (b + off) / (bins - 1));
            }

        float F1 = -1, F2 = -1;
        for (int p = 0; p < np; ++p)
            if (peaks[p] >= 220 && peaks[p] <= 1100) { F1 = peaks[p]; break; }
        if (F1 < 0) return;
        for (int p = 0; p < np; ++p)
            if (peaks[p] > F1 * 1.25f && peaks[p] > 600 && peaks[p] <= 3200) { F2 = peaks[p]; break; }
        if (F2 < 0) return;

        // 5-frame median
        mF1[(size_t) (medN % 5)] = F1; mF2[(size_t) (medN % 5)] = F2; ++medN;
        const int cnt = std::min (medN, 5);
        F1 = median (mF1.data(), cnt);
        F2 = median (mF2.data(), cnt);

        // jump limiter: big jumps must persist across the median to get through
        rawF1 = F1; rawF2 = F2;
        haveFormants = true;

        if (calibrating)
        {
            ++calibFrames;
            cF1Lo = std::min (cF1Lo, F1); cF1Hi = std::max (cF1Hi, F1);
            cF2Lo = std::min (cF2Lo, F2); cF2Hi = std::max (cF2Hi, F2);
        }
    }

    static float median (const float* v, int n) noexcept
    {
        float t[5] {};
        for (int i = 0; i < n; ++i) t[i] = v[i];
        for (int i = 1; i < n; ++i)
            for (int j = i; j > 0 && t[j - 1] > t[j]; --j) std::swap (t[j - 1], t[j]);
        return t[n / 2];
    }

    void controlUpdate (const PitchTracker& pt, int n) noexcept
    {
        const float aCoef = onePoleCoeff (settings.analysisSmoothMs, sr, n);
        const float vCoef = onePoleCoeff (settings.vowelSmoothMs, sr, n);

        const float ampDb = gainToDb (fastEnv.y * 1.41f);
        const float ampN = clamp01 ((ampDb + 60.0f) / 60.0f);
        frame.amplitude += aCoef * (ampN - frame.amplitude);

        const float onset = clamp01 ((gainToDb (fastEnv.y) - gainToDb (slowEnv.y)) / 18.0f);
        attackHold = std::max (onset, attackHold * std::exp (-(float) n / (float) (0.15 * sr)));
        frame.attack += aCoef * (attackHold - frame.attack);

        const bool present = fastEnv.y > dbToGain (-50.0f);
        if (present && e2 > 1e-10f)
        {
            const float centroid = (float) sr / kTwoPi * std::sqrt (d2 / e2);
            const float bN = clamp01 (std::log (std::max (centroid, 300.0f) / 300.0f) / std::log (5000.0f / 300.0f));
            frame.brightness += aCoef * (bN - frame.brightness);

            float br = clamp01 ((pt.getAperiodicity() - 0.04f) / 0.4f);
            if (! pt.isVoiced()) br = std::max (br, 0.8f);
            frame.breath += aCoef * (br - frame.breath);
        }
        else
        {
            frame.breath += aCoef * (0.0f - frame.breath);
        }

        frame.voicing = pt.getVoicing();

        // pitch movement / vibrato
        if (pt.isVoiced() && pt.getF0() > 0)
        {
            const float semis = hzToMidi (pt.getF0());
            // slow pitch centre; snaps on real note changes so a new sung note is
            // not mistaken for a large expressive deviation
            if (! havePitch || std::abs (semis - pitchCentre) > 1.2f) { pitchCentre = semis; havePitch = true; }
            pitchCentre += onePoleCoeff (150.0f, sr, n) * (semis - pitchCentre);
            frame.pitchSemis = semis;
            {
                // transferable deviation: vibrato / micro-intonation only (tapers to 0 by 1.2 st)
                const float d = semis - pitchCentre, ad = std::abs (d);
                const float w = ad <= 0.6f ? 1.0f : clamp01 ((1.2f - ad) / 0.6f);
                frame.pitchDev = d * w;
            }
            const float devCents = clampf ((semis - pitchCentre) * 100.0f, -150.0f, 150.0f);
            frame.pitchMove += aCoef * (clampf (devCents / 100.0f, -1, 1) - frame.pitchMove);

            // vibrato: band-limit deviation (~3..9 Hz) and follow its envelope
            const float hpC = onePoleCoeff (1000.0f / (kTwoPi * 3.0f), sr, n);
            const float lpC = onePoleCoeff (1000.0f / (kTwoPi * 9.0f), sr, n);
            vibBp1 += hpC * (devCents - vibBp1);
            const float hp = devCents - vibBp1;
            vibBp2 += lpC * (hp - vibBp2);
            vibEnv += onePoleCoeff (120.0f, sr, n) * (std::abs (vibBp2) * 1.57f - vibEnv);
            frame.vibratoDepth = clamp01 (vibEnv / 100.0f);

            const int sign = vibBp2 > 2.0f ? 1 : (vibBp2 < -2.0f ? -1 : lastSign);
            sinceZc += n;
            if (sign != lastSign && lastSign != 0)
            {
                const float hz = (float) sr / (2.0f * (float) sinceZc);
                if (hz > 2.5f && hz < 10.0f) zcInterval.process (hz, 0.3f);
                sinceZc = 0;
            }
            lastSign = sign;
            frame.vibratoRate = zcInterval.y;
        }
        else
        {
            frame.pitchMove += aCoef * (0.0f - frame.pitchMove);
            frame.pitchDev *= 0.9f;
            vibEnv *= 0.98f;
            frame.vibratoDepth = clamp01 (vibEnv / 100.0f);
        }

        // vowel space (holds through unvoiced sections)
        if (haveFormants)
        {
            f1s.process (rawF1, vCoef);
            f2s.process (rawF2, vCoef);
            frame.f1 = f1s.y; frame.f2 = f2s.y;
            const float s = settings.sensitivity;
            const float x = std::log (f2s.y / settings.f2Lo) / std::log (settings.f2Hi / settings.f2Lo);
            const float yv = std::log (f1s.y / settings.f1Lo) / std::log (settings.f1Hi / settings.f1Lo);
            // slew limit: the vowel point can cross the whole space in no less than ~120 ms,
            // so a single unreliable frame can never cause a sudden jump
            const float maxStep = (float) n / (float) (0.12 * sr);
            frame.vowelX += clampf (clamp01 (0.5f + (x - 0.5f) * s) - frame.vowelX, -maxStep, maxStep);
            frame.vowelY += clampf (clamp01 (0.5f + (yv - 0.5f) * s) - frame.vowelY, -maxStep, maxStep);
        }
    }

    static constexpr int kOrder = 12, kBins = 160;
    static constexpr double kFLo = 150.0, kFHi = 3600.0;
    std::array<std::array<float, kOrder + 1>, kBins> cosT {}, sinT {};
    double sr = 48000, dsr = 12000;
    int decim = 4, decimCount = 0, frameLen = 336, lpcHop = 60, lpcCount = 0;
    std::vector<float> dHist, win;
    int64_t dWrite = 0;
    std::array<Biquad, 2> aa {};
    float preEmPrev = 0;

    EnvFollower fastEnv, slowEnv;
    float e2 = 0, d2 = 0, prevX = 0;
    float attackHold = 0;

    std::array<float, 5> mF1 {}, mF2 {};
    int medN = 0;
    bool haveFormants = false;
    OnePole f1s, f2s;

    float pitchCentre = 60; bool havePitch = false;
    float vibBp1 = 0, vibBp2 = 0, vibEnv = 0; int lastSign = 0; int sinceZc = 0; OnePole zcInterval;

    bool calibrating = false; int calibFrames = 0;
    float cF1Lo = 0, cF1Hi = 0, cF2Lo = 0, cF2Hi = 0;

    AnalysisFrame frame;
};

} // namespace vw
