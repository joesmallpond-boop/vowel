// Minimal WAV reader/writer for tests and the offline renderer (PCM16/24/32, float32).
#pragma once
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace wav
{
struct Audio
{
    double sampleRate = 48000;
    std::vector<std::vector<float>> ch; // channels
    size_t frames() const { return ch.empty() ? 0 : ch[0].size(); }
};

inline bool read (const std::string& path, Audio& out)
{
    FILE* f = std::fopen (path.c_str(), "rb");
    if (! f) return false;
    auto rd32 = [&] { uint32_t v = 0; std::fread (&v, 4, 1, f); return v; };
    auto rd16 = [&] { uint16_t v = 0; std::fread (&v, 2, 1, f); return v; };
    char id[4];
    if (std::fread (id, 1, 4, f) != 4 || std::memcmp (id, "RIFF", 4) != 0) { std::fclose (f); return false; }
    rd32();
    std::fread (id, 1, 4, f);
    int fmt = 0, chans = 0, bits = 0; uint32_t rate = 0;
    bool ok = false;
    while (std::fread (id, 1, 4, f) == 4)
    {
        uint32_t sz = rd32();
        if (std::memcmp (id, "fmt ", 4) == 0)
        {
            fmt = rd16(); chans = rd16(); rate = rd32(); rd32(); rd16(); bits = rd16();
            if (fmt == 0xFFFE && sz >= 26) { std::fseek (f, 8, SEEK_CUR); fmt = rd16(); std::fseek (f, (long) sz - 26, SEEK_CUR); }
            else std::fseek (f, (long) sz - 16, SEEK_CUR);
        }
        else if (std::memcmp (id, "data", 4) == 0)
        {
            const int bps = bits / 8;
            const size_t n = sz / (size_t) (bps * chans);
            out.sampleRate = rate;
            out.ch.assign ((size_t) chans, std::vector<float> (n));
            std::vector<uint8_t> raw (sz);
            std::fread (raw.data(), 1, sz, f);
            for (size_t i = 0; i < n; ++i)
                for (int c = 0; c < chans; ++c)
                {
                    const uint8_t* p = raw.data() + (i * (size_t) chans + (size_t) c) * (size_t) bps;
                    float v = 0;
                    if (fmt == 3 && bits == 32) std::memcpy (&v, p, 4);
                    else if (bits == 16) v = (float) (int16_t) (p[0] | (p[1] << 8)) / 32768.0f;
                    else if (bits == 24) { int32_t s = (p[0] << 8) | (p[1] << 16) | (p[2] << 24); v = (float) s / 2147483648.0f; }
                    else if (bits == 32) { int32_t s; std::memcpy (&s, p, 4); v = (float) s / 2147483648.0f; }
                    out.ch[(size_t) c][i] = v;
                }
            ok = true;
            break;
        }
        else std::fseek (f, (long) (sz + (sz & 1)), SEEK_CUR);
    }
    std::fclose (f);
    return ok;
}

inline bool write (const std::string& path, const Audio& a)
{
    FILE* f = std::fopen (path.c_str(), "wb");
    if (! f) return false;
    const uint16_t chans = (uint16_t) a.ch.size();
    const uint32_t n = (uint32_t) a.frames();
    const uint32_t dataSize = n * chans * 4;
    auto w32 = [&] (uint32_t v) { std::fwrite (&v, 4, 1, f); };
    auto w16 = [&] (uint16_t v) { std::fwrite (&v, 2, 1, f); };
    std::fwrite ("RIFF", 1, 4, f); w32 (36 + dataSize); std::fwrite ("WAVE", 1, 4, f);
    std::fwrite ("fmt ", 1, 4, f); w32 (16); w16 (3); w16 (chans); w32 ((uint32_t) a.sampleRate);
    w32 ((uint32_t) a.sampleRate * chans * 4); w16 ((uint16_t) (chans * 4)); w16 (32);
    std::fwrite ("data", 1, 4, f); w32 (dataSize);
    for (uint32_t i = 0; i < n; ++i)
        for (uint16_t c = 0; c < chans; ++c) std::fwrite (&a.ch[c][i], 4, 1, f);
    std::fclose (f);
    return true;
}
} // namespace wav
