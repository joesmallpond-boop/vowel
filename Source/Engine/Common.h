// VowelWorm — shared DSP helpers. Plain C++17, no JUCE dependency so the
// engine can be unit-tested and rendered offline without a plugin host.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <array>

namespace vw
{
constexpr float kPi    = 3.14159265358979323846f;
constexpr float kTwoPi = 6.28318530717958647692f;
constexpr int   kMaxVoices = 8;
constexpr int   kControlBlock = 32;   // modulation / smoothing update interval (samples)

inline float clamp01 (float x) noexcept { return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); }
inline float clampf (float x, float lo, float hi) noexcept { return x < lo ? lo : (x > hi ? hi : x); }
inline float midiToHz (float note) noexcept { return 440.0f * std::exp2 ((note - 69.0f) / 12.0f); }
inline float hzToMidi (float hz) noexcept { return 69.0f + 12.0f * std::log2 (std::max (hz, 1.0f) / 440.0f); }
inline float dbToGain (float db) noexcept { return db <= -95.0f ? 0.0f : std::pow (10.0f, db * 0.05f); }
inline float gainToDb (float g) noexcept { return g <= 1.0e-5f ? -100.0f : 20.0f * std::log10 (g); }
inline float lerp (float a, float b, float t) noexcept { return a + (b - a) * t; }

/** Coefficient for a one-pole smoother with time constant `ms` updated every `interval` samples. */
inline float onePoleCoeff (float ms, double sampleRate, int interval = 1) noexcept
{
    if (ms <= 0.01f) return 1.0f;
    const double samples = (double) ms * 0.001 * sampleRate / (double) interval;
    return (float) (1.0 - std::exp (-1.0 / samples));
}

struct OnePole
{
    float y = 0.0f;
    float process (float x, float coeff) noexcept { y += coeff * (x - y); return y; }
    void reset (float v = 0.0f) noexcept { y = v; }
};

/** Envelope follower with separate attack / release coefficients. */
struct EnvFollower
{
    float y = 0.0f, att = 0.1f, rel = 0.01f;
    void setup (float attackMs, float releaseMs, double sr) noexcept
    {
        att = onePoleCoeff (attackMs, sr);
        rel = onePoleCoeff (releaseMs, sr);
    }
    float process (float x) noexcept
    {
        const float a = std::abs (x);
        y += (a > y ? att : rel) * (a - y);
        return y;
    }
};

/** Glide time is stored as a normalised 0..1 value; this maps to milliseconds (0..1000ms, skewed). */
inline float glideNormToMs (float n) noexcept { n = clamp01 (n); return 1000.0f * n * n * n; }
inline float glideMsToNorm (float ms) noexcept { return std::cbrt (clampf (ms, 0.0f, 1000.0f) / 1000.0f); }

/** 4-point cubic Hermite interpolation. */
inline float hermite (float xm1, float x0, float x1, float x2, float t) noexcept
{
    const float c1 = 0.5f * (x1 - xm1);
    const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
    const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
    return ((c3 * t + c2) * t + c1) * t + x0;
}

/** Cheap deterministic white noise (xorshift32). Real-time safe. */
struct Noise
{
    uint32_t s = 0x9E3779B9u;
    float next() noexcept
    {
        s ^= s << 13; s ^= s >> 17; s ^= s << 5;
        return (float) (int32_t) s * (1.0f / 2147483648.0f);
    }
};

/** Topology-preserving-transform state-variable filter (Simper). */
struct Svf
{
    float ic1 = 0.0f, ic2 = 0.0f, g = 0.0f, k = 1.0f, a1 = 0.0f, a2 = 0.0f, a3 = 0.0f;
    void set (float hz, float q, double sr) noexcept
    {
        hz = clampf (hz, 20.0f, (float) sr * 0.45f);
        g  = std::tan (kPi * hz / (float) sr);
        k  = 1.0f / std::max (q, 0.05f);
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }
    // returns band-pass (constant peak gain variant: bp * k gives unity peak)
    inline void process (float v0, float& lp, float& bp) noexcept
    {
        const float v3 = v0 - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        lp = v2; bp = v1;
    }
    void reset() noexcept { ic1 = ic2 = 0.0f; }
};

/** Direct-form-II-transposed biquad with a Butterworth low-pass designer. */
struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    inline float process (float x) noexcept
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
    void reset() noexcept { z1 = z2 = 0; }
    void lowpass (float fc, float q, double sr) noexcept
    {
        const float w = kTwoPi * fc / (float) sr;
        const float cw = std::cos (w), sw = std::sin (w);
        const float alpha = sw / (2.0f * q);
        const float a0 = 1.0f + alpha;
        b0 = (1.0f - cw) * 0.5f / a0; b1 = (1.0f - cw) / a0; b2 = b0;
        a1 = -2.0f * cw / a0; a2 = (1.0f - alpha) / a0;
    }
};

/** Absolute-time circular buffer of the mono input signal. */
template <int SizePow2>
struct History
{
    static constexpr int kSize = SizePow2;
    static constexpr int kMask = SizePow2 - 1;
    static_assert ((SizePow2 & (SizePow2 - 1)) == 0, "power of two");
    std::array<float, (size_t) SizePow2> buf {};
    int64_t written = 0; // absolute index of the next sample to write

    void clear() noexcept { buf.fill (0.0f); written = 0; }
    void push (float x) noexcept { buf[(size_t) (written & kMask)] = x; ++written; }
    float at (int64_t absIndex) const noexcept
    {
        if (absIndex < written - kSize + 4 || absIndex >= written) return 0.0f;
        return buf[(size_t) (absIndex & kMask)];
    }
    float atUnchecked (int64_t absIndex) const noexcept { return buf[(size_t) (absIndex & kMask)]; }
    float interp (double pos) const noexcept
    {
        const double fl = std::floor (pos);
        const int64_t i = (int64_t) fl;
        const float t = (float) (pos - fl);
        return hermite (at (i - 1), at (i), at (i + 1), at (i + 2), t);
    }
};

} // namespace vw
