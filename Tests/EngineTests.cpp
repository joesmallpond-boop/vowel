// VowelWorm engine tests — run headless: ./vw_tests [--write-wavs]
// Automated, objective checks only. Subjective quality needs real vocals (docs/TESTING.md).
#include "../Source/Engine/Engine.h"
#include "../Tools/WavIO.h"
#include "TestSignals.h"
#include <chrono>
#include <cstdio>
#include <functional>
#include <map>
#include <string>

using namespace vw;

static int failures = 0, checks = 0;
static bool writeWavs = false;
#define CHECK(cond, ...) do { ++checks; if (! (cond)) { ++failures; std::printf ("  FAIL: " __VA_ARGS__); std::printf ("\n"); } } while (0)

struct Event { int sample; std::function<void (Engine&)> fn; };

struct Rig
{
    double sr; int block;
    std::unique_ptr<Engine> e = std::make_unique<Engine>();
    ParamValues p = defaultParamValues();
    Rig (double s = 48000, int b = 128) : sr (s), block (b)
    {
        p[P::leadOn] = 0; p[P::spread] = 0; p[P::consonantLevel] = 1; p[P::velSens] = 0;
        p[P::harmonyAttack] = 5; p[P::glide] = 0;
        e->setParameters (p); e->prepare (sr, b);
    }
    void apply() { e->setParameters (p); }
    // returns mono (L+R)/2 * sqrt2 (centre pan compensation)
    std::vector<float> run (const std::vector<float>& in, std::vector<Event> ev = {}, std::vector<float>* right = nullptr)
    {
        std::vector<float> L (in.size()), R (in.size());
        size_t evi = 0;
        std::sort (ev.begin(), ev.end(), [] (auto& a, auto& b) { return a.sample < b.sample; });
        for (size_t pos = 0; pos < in.size(); pos += (size_t) block)
        {
            const int n = (int) std::min ((size_t) block, in.size() - pos);
            int done = 0;
            while (done < n)
            {
                while (evi < ev.size() && ev[evi].sample <= (int) pos + done) { ev[evi].fn (*e); ++evi; }
                int end = n;
                if (evi < ev.size() && ev[evi].sample < (int) pos + n) end = ev[evi].sample - (int) pos;
                e->setParameters (p);
                e->process (in.data() + pos + (size_t) done, L.data() + pos + (size_t) done, R.data() + pos + (size_t) done, end - done);
                done = end;
            }
        }
        std::vector<float> m (in.size());
        for (size_t i = 0; i < in.size(); ++i) m[i] = (L[i] + R[i]) * 0.70710678f;
        if (right) *right = R;
        return m;
    }
};

static void saveWav (const std::string& name, const std::vector<float>& x, double sr)
{
    if (! writeWavs) return;
    wav::Audio a; a.sampleRate = sr; a.ch = { x };
    wav::write ("test_out_" + name + ".wav", a);
}

static Event on (int s, int note, float vel = 1.0f) { return { s, [=] (Engine& e) { e.noteOn (note, vel); } }; }
static Event off (int s, int note) { return { s, [=] (Engine& e) { e.noteOff (note); } }; }

// --------------------------------------------------------------------------
static void testPitchTracker()
{
    std::printf ("[pitch tracker] YIN accuracy on synthetic vowels\n");
    for (double sr : { 44100.0, 48000.0, 96000.0 })
        for (double f0 : { 82.0, 110.0, 196.0, 330.0, 523.0, 880.0 })
            for (int vw = 0; vw < 3; ++vw)
            {
                ts::VoiceGen g; g.sr = sr; g.f0 = f0; g.setVowel (vw == 0 ? ts::vowelA() : vw == 1 ? ts::vowelI() : ts::vowelU());
                auto x = ts::render (g, (int) (sr * 0.6));
                PitchTracker pt; pt.prepare (sr);
                double worst = 0; int voicedHops = 0, hops = 0;
                for (size_t i = 0; i < x.size(); ++i)
                {
                    pt.push (x[i]);
                    if (i > (size_t) (sr * 0.12) && i % 256 == 0)
                    {
                        ++hops;
                        if (pt.isVoiced()) { ++voicedHops; worst = std::max (worst, std::abs (ts::cents (pt.getF0(), f0))); }
                    }
                }
                CHECK (worst < 8.0, "sr %.0f f0 %.0f vowel %d: worst error %.1f cents", sr, f0, vw, worst);
                CHECK (voicedHops > hops * 0.95, "sr %.0f f0 %.0f vowel %d: voiced %d/%d", sr, f0, vw, voicedHops, hops);
            }
}

static double measureOutF0 (const std::vector<float>& y, double sr, double t0, double t1)
{
    const int a = (int) (t0 * sr), b = (int) (t1 * sr);
    std::vector<double> ests;
    for (int s = a; s + 4096 <= b; s += 2048) ests.push_back (ts::estimateF0 (y.data() + s, 4096, sr));
    std::sort (ests.begin(), ests.end());
    return ests.empty() ? 0 : ests[ests.size() / 2];
}

static void testHarmonyTuning()
{
    std::printf ("[harmoniser] single-voice tuning (absolute MIDI), input G3 'ah'\n");
    for (double sr : { 44100.0, 48000.0, 96000.0 })
        for (int note : { 43, 48, 52, 55, 60, 64, 67, 72, 79 })
        {
            Rig r (sr, 128);
            ts::VoiceGen g; g.sr = sr; g.f0 = 196.0; g.setVowel (ts::vowelA());
            auto x = ts::render (g, (int) (sr * 1.0));
            auto y = r.run (x, { on (0, note) });
            const double f = measureOutF0 (y, sr, 0.3, 1.0);
            const double err = ts::cents (f, ts::midiHz (note));
            CHECK (std::abs (err) < 6.0, "sr %.0f note %d: out %.2f Hz, error %.1f cents", sr, note, f, err);
            const double gin = ts::rms (x.data() + (int) (0.3 * sr), (int) (0.6 * sr));
            const double gout = ts::rms (y.data() + (int) (0.3 * sr), (int) (0.6 * sr));
            const double gainDb = 20 * std::log10 (gout / gin);
            CHECK (gainDb > -12 && gainDb < 3, "sr %.0f note %d: level %.1f dB re input", sr, note, gainDb);
            if (sr == 48000.0) std::printf ("    note %d: %.2f Hz (%+.2f ct), level %+.1f dB\n", note, f, err, gainDb);
            if (sr == 48000.0 && (note == 64 || note == 43)) saveWav ("single_" + std::to_string (note), y, sr);
        }
}

static void testRelativeMode()
{
    std::printf ("[harmoniser] relative mode follows the singer\n");
    Rig r; r.p[P::mode] = 1; r.p[P::relRoot] = 60; r.apply();
    ts::VoiceGen g; g.f0 = 220.0; g.setVowel (ts::vowelO());
    auto x = ts::render (g, 48000);
    auto y = r.run (x, { on (0, 64) }); // +4 semitones
    const double f = measureOutF0 (y, 48000, 0.3, 1.0);
    const double err = ts::cents (f, 220.0 * std::pow (2.0, 4.0 / 12.0));
    CHECK (std::abs (err) < 6.0, "relative +4: error %.1f cents", err);
    std::printf ("    +4 st: %.2f Hz (%+.2f ct)\n", f, err);
}

static void testExpressionTransfer()
{
    std::printf ("[harmoniser] vibrato transfer into absolute-mode harmonies\n");
    Rig r;
    ts::VoiceGen g; g.f0 = 196.0; g.vibDepthCents = 40; g.vibRate = 5.5; g.setVowel (ts::vowelA());
    auto x = ts::render (g, 96000);
    auto y = r.run (x, { on (0, 64) });
    // f0 track of output, short frames
    std::vector<double> c;
    for (int s = 24000; s + 1024 < 96000; s += 256) { const double f = ts::estimateF0 (y.data() + s, 1024, 48000, 200, 600); if (f > 0) c.push_back (ts::cents (f, ts::midiHz (64))); }
    double mn = 1e9, mx = -1e9, mean = 0; for (double v : c) { mn = std::min (mn, v); mx = std::max (mx, v); mean += v; }
    mean /= std::max<size_t> (1, c.size());
    std::printf ("    output deviation range %.1f..%.1f ct (mean %.1f), input vibrato +/-40 ct\n", mn, mx, mean);
    CHECK (mx - mn > 40 && mx - mn < 120, "vibrato excursion %.1f cents", mx - mn);
    CHECK (std::abs (mean) < 8, "mean pitch offset %.1f cents", mean);
}

// harmonic-amplitude envelope (dB, mean removed) up to 3.6 kHz
static std::vector<double> harmonicEnvelope (const std::vector<float>& y, double f0, double sr, int s0 = 30000)
{
    const int N = 16384;
    std::vector<double> env;
    for (int h = 1; h * f0 < 3600; ++h)
    {
        double re = 0, im = 0;
        for (int i = 0; i < N; ++i) { const double w = 0.5 - 0.5 * std::cos (2 * ts::kPi * i / N); const double ph = 2 * ts::kPi * f0 * h * i / sr; re += w * y[(size_t) (s0 + i)] * std::cos (ph); im += w * y[(size_t) (s0 + i)] * std::sin (ph); }
        env.push_back (20 * std::log10 (std::sqrt (re * re + im * im) + 1e-9));
    }
    double m = 0; for (double v : env) m += v; m /= (double) env.size();
    for (double& v : env) v = std::max (v - m, -40.0);
    return env;
}
static double envDistance (const std::vector<double>& a, const std::vector<double>& b)
{
    double s = 0; const size_t n = std::min (a.size(), b.size());
    for (size_t i = 0; i < n; ++i) s += (a[i] - b[i]) * (a[i] - b[i]);
    return std::sqrt (s / (double) n);
}

static void testFormants()
{
    std::printf ("[harmoniser] formant preservation & independent formant shift\n");
    // Each output is compared, harmonic by harmonic, with the same synthetic vowel
    // generated natively at the output pitch ("natural") and with the vowel whose
    // formants are scaled by the pitch ratio ("chipmunk"). Preserved output must be
    // close to natural and clearly closer to it than to chipmunk.
    const double sr = 48000;
    auto natural = [&] (double f0, double fscale)
    {
        ts::VoiceGen g; g.f0 = f0; ts::Vowel v = ts::vowelA(); v.f1 *= fscale; v.f2 *= fscale; v.f3 *= fscale; v.f4 *= fscale; g.setVowel (v);
        return ts::render (g, 72000);
    };
    ts::VoiceGen g; g.f0 = ts::midiHz (50); g.setVowel (ts::vowelA());
    auto x = ts::render (g, 72000);
    auto shifted = [&] (int note, float fshift, float follow)
    {
        Rig r; r.p[P::formantShift] = fshift; r.p[P::formantFollow] = follow; r.apply();
        auto y = r.run (x, { on (0, note) });
        if (note == 62 && fshift == 0 && follow == 0) saveWav ("formant_preserved_+12", y, sr);
        if (note == 62 && follow == 1) saveWav ("formant_follow_+12", y, sr);
        return y;
    };
    for (int shift : { -7, -3, 5, 9, 12 })
    {
        const double f0 = ts::midiHz (50 + shift), ratio = std::pow (2.0, shift / 12.0);
        auto y = shifted (50 + shift, 0, 0);
        const auto e = harmonicEnvelope (y, f0, sr);
        const double dNat = envDistance (e, harmonicEnvelope (natural (f0, 1.0), f0, sr));
        const double dChip = envDistance (e, harmonicEnvelope (natural (f0, ratio), f0, sr));
        std::printf ("    pitch %+3d st, preserved: envelope error %.1f dB vs natural, %.1f dB vs chipmunk\n", shift, dNat, dChip);
        CHECK (dNat < 4.5 && dNat < dChip * 0.7, "formants not preserved at %+d st", shift);
    }
    for (float fs : { 4.0f, -4.0f, 7.0f, -7.0f })
    {
        const double f0 = ts::midiHz (55), k = std::pow (2.0, fs / 12.0);
        auto y = shifted (55, fs, 0);
        const auto e = harmonicEnvelope (y, f0, sr);
        const double dTarget = envDistance (e, harmonicEnvelope (natural (f0, k), f0, sr));
        const double dUnshift = envDistance (e, harmonicEnvelope (natural (f0, 1.0), f0, sr));
        std::printf ("    formant %+.0f st (pitch +5): %.1f dB vs target envelope, %.1f dB vs unshifted\n", fs, dTarget, dUnshift);
        CHECK (dTarget < 5.0 && dTarget < dUnshift * 0.7, "formant shift %+.0f st inaccurate", fs);
    }
    {
        const double f0 = ts::midiHz (57), ratio = std::pow (2.0, 7 / 12.0);
        auto y = shifted (57, 0, 1.0f);
        const auto e = harmonicEnvelope (y, f0, sr);
        const double dChip = envDistance (e, harmonicEnvelope (natural (f0, ratio), f0, sr));
        const double dNat = envDistance (e, harmonicEnvelope (natural (f0, 1.0), f0, sr));
        std::printf ("    follow-pitch +7 st: %.1f dB vs chipmunk, %.1f dB vs preserved\n", dChip, dNat);
        CHECK (dChip < 5.0 && dChip < dNat, "formant-follow mode inaccurate");
    }
}

static void testVowelSpace()
{
    std::printf ("[analysis] vowel-space position for u / a / i / e / o\n");
    const double sr = 48000;
    Engine e; auto p = defaultParamValues(); e.setParameters (p); e.prepare (sr, 128);
    std::map<char, std::pair<float, float>> pos;
    std::vector<float> L (128), R (128);
    float maxJump = 0;
    for (char c : { 'u', 'a', 'i', 'e', 'o' })
    {
        ts::VoiceGen g; g.f0 = 160; g.setVowel (c == 'u' ? ts::vowelU() : c == 'a' ? ts::vowelA() : c == 'i' ? ts::vowelI() : c == 'e' ? ts::vowelE() : ts::vowelO());
        auto x = ts::render (g, 24000);
        float px = -1, py = -1;
        for (int s = 0; s < 24000; s += 128)
        {
            e.process (x.data() + s, L.data(), R.data(), 128);
            const auto& a = e.analysis();
            if (s > 12000)
            {
                if (px >= 0) maxJump = std::max (maxJump, std::max (std::abs (a.vowelX - px), std::abs (a.vowelY - py)));
                px = a.vowelX; py = a.vowelY;
            }
        }
        pos[c] = { e.analysis().vowelX, e.analysis().vowelY };
        std::printf ("    %c: X %.2f Y %.2f  (F1 %.0f F2 %.0f)\n", c, pos[c].first, pos[c].second, e.analysis().f1, e.analysis().f2);
    }
    CHECK (pos['a'].second > pos['u'].second + 0.35f && pos['a'].second > pos['i'].second + 0.35f, "openness: a should be well above u and i");
    CHECK (pos['i'].first > pos['a'].first + 0.35f && pos['i'].first > pos['u'].first + 0.5f, "frontness: i should be well right of a and u");
    CHECK (pos['e'].first > pos['o'].first + 0.25f, "frontness: e right of o");
    CHECK (maxJump < 0.03f, "steady-vowel jitter %.3f per 128-sample block", maxJump);
}

static void testVowelGlide()
{
    std::printf ("[analysis] continuous oo -> ah -> ee glide stays smooth\n");
    const double sr = 48000;
    Engine e; e.setParameters (defaultParamValues()); e.prepare (sr, 128);
    ts::VoiceGen g; g.f0 = 180; g.setVowel (ts::vowelU());
    std::vector<float> L (128), R (128), blk (128);
    float prevX = -1, prevY = -1, maxStep = 0;
    const int total = (int) (sr * 2.0);
    for (int s = 0; s < total; s += 128)
    {
        const double t = (double) s / total;
        ts::Vowel v;
        auto mix = [] (ts::Vowel a, ts::Vowel b, double k) { return ts::Vowel { a.f1 + (b.f1 - a.f1) * k, a.f2 + (b.f2 - a.f2) * k, a.f3 + (b.f3 - a.f3) * k, a.f4 + (b.f4 - a.f4) * k }; };
        v = t < 0.5 ? mix (ts::vowelU(), ts::vowelA(), t * 2) : mix (ts::vowelA(), ts::vowelI(), (t - 0.5) * 2);
        g.setVowel (v);
        for (auto& b : blk) b = (float) g.next();
        e.process (blk.data(), L.data(), R.data(), 128);
        if (s > 9600)
        {
            const auto& a = e.analysis();
            if (prevX >= 0) maxStep = std::max (maxStep, std::max (std::abs (a.vowelX - prevX), std::abs (a.vowelY - prevY)));
            prevX = a.vowelX; prevY = a.vowelY;
        }
    }
    std::printf ("    largest per-block step %.3f\n", maxStep);
    CHECK (maxStep < 0.04f, "vowel glide step %.3f", maxStep);
}

static void testAllocation()
{
    std::printf ("[allocation] chord changes, sustain, latch\n");
    VoiceAllocator a; a.numVoices = 4; a.legatoWindow = 12000;
    auto noteOf = [&] (int n) { for (int i = 0; i < 4; ++i) if (a.v[(size_t) i].gate && a.v[(size_t) i].note == n) return i; return -1; };
    a.noteOn (60, 1, 0); a.noteOn (64, 1, 1); a.noteOn (67, 1, 2);
    const int vC = noteOf (60), vE = noteOf (64), vG = noteOf (67);
    // C major -> F major (C F A): C stays, E->F, G->A
    a.noteOff (60, 1000); a.noteOff (64, 1000); a.noteOff (67, 1000);
    a.noteOn (60, 1, 1010); a.noteOn (65, 1, 1011); a.noteOn (69, 1, 1012);
    CHECK (noteOf (60) == vC, "C should stay on its voice");
    CHECK (noteOf (65) == vE, "F should take E's voice (nearest)");
    CHECK (noteOf (69) == vG, "A should take G's voice (nearest)");
    CHECK (a.v[(size_t) vE].legato, "reassigned voice should glide");
    // sustain
    a.allNotesOff (2000);
    a.setSustain (true, 2000); a.noteOn (62, 1, 2001); a.noteOff (62, 2100);
    CHECK (noteOf (62) >= 0, "sustain should hold released note");
    a.setSustain (false, 2200);
    CHECK (noteOf (62) < 0, "sustain release should end note");
    // latch
    a.setLatch (true, 3000); a.noteOn (60, 1, 3001); a.noteOn (64, 1, 3002); a.noteOff (60, 3100); a.noteOff (64, 3100);
    CHECK (noteOf (60) >= 0 && noteOf (64) >= 0, "latch should hold chord");
    a.noteOn (62, 1, 3200);
    CHECK (noteOf (60) < 0 && noteOf (62) >= 0, "new chord replaces latched chord");
    // stealing: 5 notes into 4 voices
    VoiceAllocator b; b.numVoices = 4;
    for (int i = 0; i < 5; ++i) b.noteOn (60 + i, 1, i);
    int count = 0; for (int i = 0; i < 4; ++i) count += b.v[(size_t) i].gate ? 1 : 0;
    CHECK (count == 4, "4 voices sounding after steal");
    bool has64 = false; for (int i = 0; i < 4; ++i) has64 |= b.v[(size_t) i].note == 64;
    CHECK (has64, "newest note must sound");
}

static void testFourPartChord()
{
    std::printf ("[harmoniser] four-part chord + chord change\n");
    const double sr = 48000;
    Rig r; r.p[P::glide] = 40; r.apply();
    ts::VoiceGen g; g.f0 = 196.0; g.vibDepthCents = 20; g.setVowel (ts::vowelA());
    auto x = ts::render (g, (int) (sr * 2.0));
    std::vector<Event> ev { on (0, 55), on (0, 59), on (0, 62), on (0, 67),
        off (48000, 55), off (48000, 59), off (48000, 62), off (48000, 67),
        on (48010, 57), on (48010, 60), on (48010, 64), on (48010, 69) };
    auto y = r.run (x, ev);
    bool finite = true; double peak = 0, maxDelta = 0;
    for (size_t i = 1; i < y.size(); ++i) { finite &= std::isfinite (y[i]); peak = std::max (peak, (double) std::abs (y[i])); maxDelta = std::max (maxDelta, (double) std::abs (y[i] - y[i - 1])); }
    CHECK (finite, "non-finite output");
    CHECK (peak < 1.0, "peak %.2f", peak);
    int active = 0; for (int i = 0; i < kMaxVoices; ++i) active += r.e->telemetry.voiceNote[i].load() >= 0;
    CHECK (active == 4, "4 voices active (got %d)", active);
    // spectral check of the second chord: energy at each fundamental
    const int N = 16384, s0 = (int) (1.4 * sr);
    auto bandEnergy = [&] (double f) { double re = 0, im = 0; for (int i = 0; i < N; ++i) { const double w = 0.5 - 0.5 * std::cos (2 * ts::kPi * i / N); re += w * y[(size_t) (s0 + i)] * std::cos (2 * ts::kPi * f * i / sr); im += w * y[(size_t) (s0 + i)] * std::sin (2 * ts::kPi * f * i / sr); } return std::sqrt (re * re + im * im); };
    for (int n : { 57, 60, 64, 69 })
    {
        const double f = ts::midiHz (n);
        const double on_ = bandEnergy (f), offB = 0.5 * (bandEnergy (f * std::pow (2, 0.5 / 12)) + bandEnergy (f * std::pow (2, -0.5 / 12)));
        std::printf ("    note %d: peak/quarter-tone-off ratio %.1f dB\n", n, 20 * std::log10 (on_ / offB));
        CHECK (on_ > offB * 2.8, "note %d fundamental missing in chord", n);
    }
    saveWav ("chord", y, sr);
}

static void testRobustness()
{
    std::printf ("[robustness] sample rates x buffer sizes, silence, noise, NaN guard\n");
    for (double sr : { 44100.0, 48000.0, 96000.0 })
        for (int bs : { 64, 128, 256, 37, 1024 })
        {
            Rig r (sr, bs); r.p[P::blend] = 0.5f; r.p[P::leadOn] = 1; r.apply();
            ts::VoiceGen g; g.sr = sr; g.f0 = 140; g.breath = 0.5; g.setVowel (ts::vowelE());
            const int n = (int) (sr * 1.5);
            auto x = ts::render (g, n);
            std::mt19937 rng (3); std::normal_distribution<float> nd (0, 0.1f);
            for (int i = (int) (sr * 0.5); i < (int) (sr * 0.7); ++i) x[(size_t) i] = nd (rng);    // "ssss"
            for (int i = (int) (sr * 1.0); i < n; ++i) x[(size_t) i] = 0;                          // silence
            auto y = r.run (x, { on (0, 60), on (0, 64), on (0, 67), on (100, 71) });
            bool finite = true; double peak = 0;
            for (float v : y) { finite &= std::isfinite (v); peak = std::max (peak, (double) std::abs (v)); }
            const double tail = ts::rms (y.data() + (int) (sr * 1.3), (int) (sr * 0.2));
            CHECK (finite, "sr %.0f bs %d non-finite", sr, bs);
            CHECK (peak <= 1.0, "sr %.0f bs %d peak %.2f", sr, bs, peak);
            CHECK (20 * std::log10 (tail + 1e-12) < -70, "sr %.0f bs %d silence leaks %.1f dB", sr, bs, 20 * std::log10 (tail + 1e-12));
        }
    // block-size independence: same output for 64 vs 256 (sample-accurate MIDI, deterministic)
    ts::VoiceGen g; g.f0 = 200; g.setVowel (ts::vowelA());
    auto x = ts::render (g, 48000);
    Rig a (48000, 64), b (48000, 256);
    auto ya = a.run (x, { on (1000, 64) }), yb = b.run (x, { on (1000, 64) });
    double diff = 0; for (size_t i = 0; i < ya.size(); ++i) diff = std::max (diff, (double) std::abs (ya[i] - yb[i]));
    CHECK (diff < 1e-4, "output depends on host block size (max diff %.2e)", diff);
}

static double maxSecondDiff (const std::vector<float>& y, int a, int b)
{
    double m = 0; for (int i = a + 2; i < b; ++i) m = std::max (m, (double) std::abs (y[(size_t) i] - 2 * y[(size_t) i - 1] + y[(size_t) i - 2])); return m;
}


static void testBlendBalance()
{
    std::printf ("[synth] loudness across the human -> synthetic blend\n");
    ts::VoiceGen g; g.f0 = 196; g.setVowel (ts::vowelA());
    auto x = ts::render (g, 48000);
    double lv[3]; int k = 0;
    for (float b : { 0.0f, 0.5f, 1.0f })
    {
        Rig r; r.p[P::blend] = b; r.apply();
        auto y = r.run (x, { on (0, 59), on (0, 62), on (0, 67) });
        lv[k] = 20 * std::log10 (ts::rms (y.data() + 20000, 24000) / ts::rms (x.data() + 20000, 24000));
        std::printf ("    blend %.1f: %+.1f dB re input\n", b, lv[k]); ++k;
    }
    CHECK (std::abs (lv[2] - lv[0]) < 3.0, "synth level %.1f dB vs vocal harmonies %.1f dB", lv[2], lv[0]);
    CHECK (std::abs (lv[1] - lv[0]) < 3.0, "hybrid level dips/peaks %.1f dB", lv[1] - lv[0]);
}

static void testClicks()
{
    std::printf ("[clicks] abrupt host-parameter jumps stay smooth\n");
    ts::VoiceGen g; g.f0 = 196; g.setVowel (ts::vowelA());
    auto x = ts::render (g, 96000);
    struct J { int id; float a, b; const char* name; };
    for (J j : { J { P::formantShift, 0, 8, "formant shift" }, J { P::blend, 0, 1, "blend" }, J { P::harmonyLevel, 0, -30, "harmony level" },
                 J { P::outputLevel, 0, 9, "output" }, J { P::spread, 0, 1, "spread" }, J { P::macro0, 0, 1, "macro 1" }, J { P::oscMorph, 0, 1, "osc morph" } })
    {
        Rig r; r.p[P::blend] = j.id == P::blend ? 0.0f : 0.5f; r.p[j.id] = j.a; r.apply();
        auto ev = std::vector<Event> { on (0, 64), { 48000, [&r, j] (Engine&) { r.p[j.id] = j.b; } } };
        auto y = r.run (x, ev);
        const double before = maxSecondDiff (y, 30000, 47000);
        const double after = maxSecondDiff (y, 60000, 90000);
        const double jump = maxSecondDiff (y, 47990, 49500);
        std::printf ("    %-14s before %.4f  after %.4f  around jump %.4f\n", j.name, before, after, jump);
        CHECK (jump < std::max (before, after) * 1.5 + 0.002, "%s jump causes a click", j.name);
    }
}

static void testIdentityAndLatency (float margin = -1.0f)
{
    std::printf ("[latency] harmony delay vs sung pitch; unison transparency%s\n", margin > 0 ? " (margin experiment)" : "");
    if (margin > 0) std::printf ("    grain cap %.1f ms\n", margin);
    for (double f0 : { 98.0, 147.0, 220.0, 330.0, 494.0 })
    {
        const double sr = 48000;
        Rig r;
        if (margin > 0) r.p[P::latencyMode] = 1;
        ts::VoiceGen g; g.f0 = f0; g.setVowel (ts::vowelA());
        auto x = ts::render (g, 72000);
        // random syllable-like amplitude envelope makes the delay unambiguous
        std::mt19937 rng (11); std::uniform_real_distribution<float> ud (0.1f, 1.0f);
        float a = 0.5f, target = 0.5f;
        for (size_t i = 0; i < x.size(); ++i) { if (i % 2400 == 0) target = ud (rng); a += 0.002f * (target - a); x[i] *= a; }
        const double noteF = 12 * std::log2 (f0 / 440.0) + 69; const int note = (int) std::lround (noteF);
        r.p[P::vDetune0] = (float) (100.0 * (noteF - note)); r.apply();
        auto y = r.run (x, { on (0, note) });
        auto env = [] (const std::vector<float>& s) { std::vector<float> e (s.size()); float v = 0; for (size_t i = 0; i < s.size(); ++i) { v += 0.01f * (std::abs (s[i]) - v); e[i] = v; } return e; };
        auto ex = env (x), ey = env (y);
        int bestLag = 0; double best = -1;
        for (int lag = 0; lag < 2400; lag += 4)
        {
            double xy = 0, xx = 0, yy = 0;
            for (int i = 12000; i < 60000; i += 4) { const double p = ex[(size_t) i] - 0.3, q = ey[(size_t) (i + lag)] - 0.15; xy += p * q; xx += p * p; yy += q * q; }
            const double c = xy / std::sqrt (xx * yy + 1e-20);
            if (c > best) { best = c; bestLag = lag; }
        }
        // waveform correlation at that lag (unison = identity resynthesis quality)
        int fine = bestLag; double wbest = -1;
        for (int lag = std::max (0, bestLag - 400); lag < bestLag + 400; ++lag)
        {
            double xy = 0, xx = 0, yy = 0;
            for (int i = 20000; i < 50000; i += 2) { xy += x[(size_t) i] * y[(size_t) (i + lag)]; xx += x[(size_t) i] * x[(size_t) i]; yy += y[(size_t) (i + lag)] * y[(size_t) (i + lag)]; }
            const double c = xy / std::sqrt (xx * yy + 1e-20);
            if (c > wbest) { wbest = c; fine = lag; }
        }
        const double ms = 1000.0 * fine / sr;
        std::printf ("    f0 %3.0f Hz: harmony delay %.1f ms (engine estimate %.1f ms), unison waveform correlation %.3f\n", f0, ms, r.e->telemetry.harmonyDelayMs.load(), wbest);
        CHECK (wbest > 0.9, "unison resynthesis correlation %.3f at %.0f Hz", wbest, f0);
        CHECK (ms < 3.6 * 1000.0 / f0 + 3.0, "delay %.1f ms too long at %.0f Hz", ms, f0);
    }
}

static void testSynth()
{
    std::printf ("[synth] tracks MIDI, follows vowel and dynamics\n");
    const double sr = 48000;
    auto runSynth = [&] (ts::Vowel v, float amp, double& f, double& rmsOut, double& centroid)
    {
        Rig r; r.p[P::blend] = 1.0f; r.apply();
        ts::VoiceGen g; g.f0 = 180; g.amp = amp; g.setVowel (v);
        auto x = ts::render (g, 48000);
        auto y = r.run (x, { on (0, 62) });
        f = measureOutF0 (y, sr, 0.4, 1.0);
        rmsOut = ts::rms (y.data() + 24000, 20000);
        double d2 = 0, e2 = 0; for (int i = 24001; i < 44000; ++i) { const double d = y[(size_t) i] - y[(size_t) i - 1]; d2 += d * d; e2 += (double) y[(size_t) i] * y[(size_t) i]; }
        centroid = sr / (2 * ts::kPi) * std::sqrt (d2 / (e2 + 1e-20));
    };
    double f, r1, c1, r2, c2, r3, c3;
    runSynth (ts::vowelU(), 0.3f, f, r1, c1);
    CHECK (std::abs (ts::cents (f, ts::midiHz (62))) < 6, "synth pitch error %.1f ct", ts::cents (f, ts::midiHz (62)));
    runSynth (ts::vowelA(), 0.3f, f, r2, c2);
    runSynth (ts::vowelA(), 0.03f, f, r3, c3);
    std::printf ("    'oo' centroid %.0f Hz, 'ah' centroid %.0f Hz; loud %.1f dB vs quiet %.1f dB\n", c1, c2, 20 * std::log10 (r2), 20 * std::log10 (r3));
    CHECK (c2 > c1 * 1.3, "synth should brighten when the singer opens the vowel");
    CHECK (r2 > r3 * 3, "synth should follow vocal dynamics");
}

static void testCpu()
{
    std::printf ("[cpu] real-time factor (this machine; not the target Intel Mac)\n");
    for (int voices : { 4, 6, 8 })
        for (float blend : { 0.0f, 0.5f })
        {
            constexpr double sr = 48000;
            Rig r (sr, 128); r.p[P::voices] = (float) voices; r.p[P::blend] = blend; r.p[P::leadOn] = 1; r.apply();
            ts::VoiceGen g; g.f0 = 196; g.vibDepthCents = 30; g.setVowel (ts::vowelA());
            auto x = ts::render (g, (int) (sr * 10));
            std::vector<Event> ev; for (int k = 0; k < voices; ++k) ev.push_back (on (0, 55 + 4 * k));
            const auto t0 = std::chrono::steady_clock::now();
            auto y = r.run (x, ev);
            const double secs = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
            std::printf ("    %d voices, blend %.1f: %.2f%% of one core\n", voices, blend, 100.0 * secs / 10.0);
        }
}

int main (int argc, char** argv)
{
    for (int i = 1; i < argc; ++i) if (std::string (argv[i]) == "--write-wavs") writeWavs = true;
    const bool cpuOnly = argc > 1 && std::string (argv[1]) == "--cpu";
    if (argc > 2 && std::string (argv[1]) == "--margin") { testIdentityAndLatency (std::stof (argv[2])); return 0; }
    if (! cpuOnly)
    {
        testPitchTracker();
        testHarmonyTuning();
        testRelativeMode();
        testExpressionTransfer();
        testFormants();
        testVowelSpace();
        testVowelGlide();
        testAllocation();
        testFourPartChord();
        testSynth();
        testBlendBalance();
        testClicks();
        testIdentityAndLatency();
        testRobustness();
    }
    testCpu();
    std::printf ("\n%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
