// Synthetic vocal test signals (glottal source + formant cascade) and an
// independent pitch estimator used to judge the engine's output.
//
// These are deliberately "voice-like" rather than sine waves, but they are NOT
// a substitute for listening tests on real recordings (see docs/TESTING.md).
#pragma once
#include <cmath>
#include <complex>
#include <vector>
#include <random>
#include <algorithm>

namespace ts
{
constexpr double kPi = 3.14159265358979323846;

struct Vowel { double f1, f2, f3, f4; };
inline Vowel vowelA() { return { 730, 1090, 2440, 3400 }; }
inline Vowel vowelI() { return { 270, 2290, 3010, 3700 }; }
inline Vowel vowelU() { return { 300, 870, 2240, 3300 }; }
inline Vowel vowelE() { return { 530, 1840, 2480, 3500 }; }
inline Vowel vowelO() { return { 570, 840, 2410, 3400 }; }

struct Resonator
{
    double a1 = 0, a2 = 0, b0 = 1, y1 = 0, y2 = 0;
    void set (double f, double bw, double sr)
    {
        const double r = std::exp (-kPi * bw / sr);
        a1 = 2 * r * std::cos (2 * kPi * f / sr);
        a2 = -r * r;
        b0 = 1 - a1 - a2;
    }
    double process (double x) { const double y = b0 * x + a1 * y1 + a2 * y2; y2 = y1; y1 = y; return y; }
};

/** Glottal (Rosenberg) source through a 4-formant cascade with optional vibrato, jitter and aspiration. */
struct VoiceGen
{
    double sr = 48000, phase = 0, prevG = 0;
    double f0 = 200, vibDepthCents = 0, vibRate = 5.5, vibPhase = 0, breath = 0.0, amp = 0.3;
    Vowel v = vowelA();
    Resonator r[4];
    std::mt19937 rng { 7 };
    std::normal_distribution<double> nd { 0.0, 1.0 };

    void setVowel (Vowel nv)
    {
        v = nv;
        const double bw[4] = { 80, 100, 140, 200 };
        const double f[4] = { v.f1, v.f2, v.f3, v.f4 };
        for (int i = 0; i < 4; ++i) r[i].set (f[i], bw[i], sr);
    }
    double next()
    {
        vibPhase += vibRate / sr;
        const double f = f0 * std::pow (2.0, vibDepthCents / 1200.0 * std::sin (2 * kPi * vibPhase));
        phase += f / sr;
        if (phase >= 1) phase -= 1;
        const double op = 0.6;
        double g = phase < op * 0.66 ? 0.5 * (1 - std::cos (kPi * phase / (op * 0.66)))
                 : (phase < op ? std::cos (kPi * 0.5 * (phase - op * 0.66) / (op * 0.34)) : 0.0);
        double d = g - prevG; prevG = g;           // lip radiation (differentiated flow)
        double x = d * 8.0 + breath * 0.05 * nd (rng) * (0.3 + g);
        for (auto& rr : r) x = rr.process (x);
        return x * amp;
    }
};

inline std::vector<float> render (VoiceGen& g, int n)
{
    std::vector<float> out ((size_t) n);
    for (auto& s : out) s = (float) g.next();
    return out;
}

/** Independent f0 estimate: normalised autocorrelation with parabolic interpolation (not YIN). */
inline double estimateF0 (const float* x, int n, double sr, double fmin = 60, double fmax = 1200)
{
    const int lagMin = (int) (sr / fmax), lagMax = std::min (n / 2, (int) (sr / fmin));
    std::vector<double> nac ((size_t) lagMax + 2, 0.0);
    for (int lag = lagMin; lag <= lagMax + 1; ++lag)
    {
        double xy = 0, xx = 0, yy = 0;
        for (int i = 0; i + lag < n; ++i) { xy += x[i] * x[i + lag]; xx += x[i] * x[i]; yy += x[i + lag] * x[i + lag]; }
        nac[(size_t) lag] = xy / std::sqrt (xx * yy + 1e-20);
    }
    // first peak above 0.9 of the global maximum (avoids sub-octave picks)
    double gmax = 0; for (int l = lagMin + 1; l <= lagMax; ++l) gmax = std::max (gmax, nac[(size_t) l]);
    int best = -1;
    for (int l = lagMin + 1; l <= lagMax; ++l)
        if (nac[(size_t) l] > 0.9 * gmax && nac[(size_t) l] >= nac[(size_t) l - 1] && nac[(size_t) l] >= nac[(size_t) l + 1]) { best = l; break; }
    if (best < 0) return 0;
    const double a = nac[(size_t) best - 1], b = nac[(size_t) best], c = nac[(size_t) best + 1];
    const double den = a - 2 * b + c;
    const double off = std::abs (den) > 1e-12 ? 0.5 * (a - c) / den : 0;
    return sr / (best + off);
}

inline double cents (double a, double b) { return 1200.0 * std::log2 (a / b); }
inline double midiHz (double n) { return 440.0 * std::pow (2.0, (n - 69) / 12.0); }

inline double rms (const float* x, int n) { double s = 0; for (int i = 0; i < n; ++i) s += x[i] * x[i]; return std::sqrt (s / std::max (1, n)); }

/** Simple LPC formant estimate (order 2+sr/1000 on a decimated signal) — independent of the engine's analyser. */
inline void lpcFormants (const float* x, int n, double sr, double& F1, double& F2)
{
    // decimate to ~12 kHz with a crude moving-average low-pass
    const int d = std::max (1, (int) std::lround (sr / 12000.0));
    std::vector<double> y;
    for (int i = 0; i + d <= n; i += d) { double s = 0; for (int k = 0; k < d; ++k) s += x[i + k]; y.push_back (s / d); }
    const double fs = sr / d;
    for (size_t i = y.size() - 1; i > 0; --i) y[i] -= 0.94 * y[i - 1];
    for (size_t i = 0; i < y.size(); ++i) y[i] *= 0.54 - 0.46 * std::cos (2 * kPi * (double) i / (double) (y.size() - 1));
    const int order = 14;
    std::vector<double> r (order + 1);
    for (int k = 0; k <= order; ++k) { double s = 0; for (size_t i = (size_t) k; i < y.size(); ++i) s += y[i] * y[i - (size_t) k]; r[(size_t) k] = s; }
    std::vector<double> a (order + 1, 0.0), t (order + 1);
    a[0] = 1; double e = r[0] * 1.0001;
    for (int i = 1; i <= order; ++i)
    {
        double acc = r[(size_t) i]; for (int j = 1; j < i; ++j) acc += a[(size_t) j] * r[(size_t) (i - j)];
        const double k = -acc / e; t = a;
        for (int j = 1; j < i; ++j) a[(size_t) j] = t[(size_t) j] + k * t[(size_t) (i - j)];
        a[(size_t) i] = k; e *= 1 - k * k;
    }
    std::vector<double> env;
    const int bins = 400;
    for (int b = 0; b < bins; ++b)
    {
        const double f = 150 + 3350.0 * b / bins, w = 2 * kPi * f / fs;
        std::complex<double> z = 0; for (int k = 0; k <= order; ++k) z += a[(size_t) k] * std::polar (1.0, -w * k);
        env.push_back (-20 * std::log10 (std::abs (z) + 1e-12));
    }
    std::vector<double> pk;
    for (int b = 1; b < bins - 1; ++b) if (env[(size_t) b] > env[(size_t) b - 1] && env[(size_t) b] >= env[(size_t) b + 1]) pk.push_back (150 + 3350.0 * b / bins);
    F1 = pk.size() > 0 ? pk[0] : 0; F2 = pk.size() > 1 ? pk[1] : 0;
}

} // namespace ts
