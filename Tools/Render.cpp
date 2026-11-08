// vw_render — run the VowelWorm engine offline on a WAV file.
//
// Lets you judge the harmoniser on real vocal recordings without a DAW, and
// dumps the analysis signals for inspection.
//
//   vw_render vocal.wav out.wav --chords "0:55,59,62,67;2.5:57,60,64,69"
//             [--preset "Warm Choir"] [--set blend=0.5 --set glide=80]
//             [--block 128] [--analysis analysis.csv] [--timing]
//
// --chords: semicolon-separated "seconds:note,note,..." changes (previous chord released).
#include "Engine/Engine.h"
#include "Engine/FactoryPresets.h"
#include "WavIO.h"
#include <chrono>
#include <cstdio>
#include <sstream>
#include <algorithm>

using namespace vw;

struct Change { double t; std::vector<int> notes; };

static std::vector<Change> parseChords (const std::string& s)
{
    std::vector<Change> out;
    std::stringstream ss (s);
    std::string item;
    while (std::getline (ss, item, ';'))
    {
        const auto colon = item.find (':');
        if (colon == std::string::npos) continue;
        Change c; c.t = std::stod (item.substr (0, colon));
        std::stringstream ns (item.substr (colon + 1));
        std::string n;
        while (std::getline (ns, n, ',')) if (! n.empty()) c.notes.push_back (std::stoi (n));
        out.push_back (c);
    }
    return out;
}

int main (int argc, char** argv)
{
    if (argc < 3)
    {
        std::printf ("usage: vw_render in.wav out.wav --chords \"0:60,64,67\" [--preset NAME] [--set id=value] [--block N] [--analysis file.csv] [--timing]\n\nPresets:\n");
        for (auto& p : factoryPresets()) std::printf ("  %s\n", p.name.c_str());
        return 1;
    }
    std::string inPath = argv[1], outPath = argv[2], chords = "0:60,64,67", preset, csvPath;
    std::vector<std::pair<std::string, float>> sets;
    int block = 128; bool timing = false;
    for (int i = 3; i < argc; ++i)
    {
        std::string a = argv[i];
        auto next = [&] { return i + 1 < argc ? std::string (argv[++i]) : std::string(); };
        if (a == "--chords") chords = next();
        else if (a == "--preset") preset = next();
        else if (a == "--block") block = std::max (1, std::stoi (next()));
        else if (a == "--analysis") csvPath = next();
        else if (a == "--timing") timing = true;
        else if (a == "--set") { auto kv = next(); auto eq = kv.find ('='); if (eq != std::string::npos) sets.push_back ({ kv.substr (0, eq), std::stof (kv.substr (eq + 1)) }); }
    }

    wav::Audio in;
    if (! wav::read (inPath, in)) { std::printf ("cannot read %s\n", inPath.c_str()); return 1; }
    const size_t n = in.frames();
    std::vector<float> mono (n, 0.0f);
    for (auto& c : in.ch) for (size_t i = 0; i < n; ++i) mono[i] += c[i] / (float) in.ch.size();

    ParamValues pv = defaultParamValues();
    if (! preset.empty())
    {
        bool found = false;
        for (auto& p : factoryPresets()) if (p.name == preset) { pv = presetValues (p); found = true; }
        if (! found) { std::printf ("unknown preset '%s'\n", preset.c_str()); return 1; }
    }
    const auto& table = paramTable();
    for (auto& [id, v] : sets)
    {
        bool ok = false;
        for (size_t i = 0; i < table.size(); ++i) if (table[i].id == id) { pv[i] = clampf (v, table[i].min, table[i].max); ok = true; }
        if (! ok) { std::printf ("unknown parameter '%s'\n", id.c_str()); return 1; }
    }

    Engine e;
    e.setParameters (pv);
    e.prepare (in.sampleRate, block);

    auto changes = parseChords (chords);
    std::vector<int> held;
    size_t ci = 0;

    wav::Audio out; out.sampleRate = in.sampleRate; out.ch.assign (2, std::vector<float> (n));
    FILE* csv = csvPath.empty() ? nullptr : std::fopen (csvPath.c_str(), "w");
    if (csv) std::fprintf (csv, "time,pitch_hz,voicing,amplitude,attack,brightness,breath,pitch_move,vibrato,vowel_x,vowel_y,f1,f2,harmony_delay_ms\n");

    double worstBlockMs = 0;
    std::vector<double> blockMs;
    const auto t0 = std::chrono::steady_clock::now();
    for (size_t pos = 0; pos < n; )
    {
        // next event boundary
        size_t end = std::min (n, pos + (size_t) block);
        if (ci < changes.size())
        {
            const size_t evs = (size_t) (changes[ci].t * in.sampleRate);
            if (evs <= pos)
            {
                for (int h : held) e.noteOff (h);
                held = changes[ci].notes;
                for (int h : held) e.noteOn (h, 0.9f);
                ++ci;
                continue;
            }
            end = std::min (end, evs);
        }
        const auto b0 = std::chrono::steady_clock::now();
        e.process (mono.data() + pos, out.ch[0].data() + pos, out.ch[1].data() + pos, (int) (end - pos));
        const double ms = std::chrono::duration<double, std::milli> (std::chrono::steady_clock::now() - b0).count();
        worstBlockMs = std::max (worstBlockMs, ms);
        if (end - pos == (size_t) block) blockMs.push_back (ms);
        if (csv && (pos / 480) != (end / 480))
        {
            const auto& a = e.analysis(); const auto& t = e.telemetry;
            std::fprintf (csv, "%.3f,%.2f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.0f,%.0f,%.1f\n", (double) end / in.sampleRate,
                          t.pitchHz.load(), a.voicing, a.amplitude, a.attack, a.brightness, a.breath, a.pitchMove, a.vibratoDepth, a.vowelX, a.vowelY, a.f1, a.f2, t.harmonyDelayMs.load());
        }
        pos = end;
    }
    const double secs = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
    if (csv) std::fclose (csv);
    wav::write (outPath, out);
    std::printf ("rendered %.2f s in %.3f s (%.1f%% of real time)\n", (double) n / in.sampleRate, secs, 100.0 * secs * in.sampleRate / (double) n);
    if (timing && ! blockMs.empty())
    {
        std::sort (blockMs.begin(), blockMs.end());
        std::printf ("block time: median %.3f ms, 99.9th pct %.3f ms, worst %.3f ms (budget %.3f ms per %d-sample block)\n",
                     blockMs[blockMs.size() / 2], blockMs[(size_t) (blockMs.size() * 0.999)], worstBlockMs, 1000.0 * block / in.sampleRate, block);
    }
    return 0;
}
