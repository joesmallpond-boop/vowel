// Prints the parameter table as JSON (used by Tools/make_amxd.py and docs generation).
#include "Engine/Params.h"
#include "Engine/FactoryPresets.h"
#include <cstdio>
#include <string>

static std::string esc (const std::string& s) { std::string o; for (char c : s) { if (c == '"' || c == '\\') o += '\\'; o += c; } return o; }

int main()
{
    const auto& t = vw::paramTable();
    std::printf ("{\"params\":[\n");
    for (size_t i = 0; i < t.size(); ++i)
    {
        const auto& d = t[i];
        const char* type = d.type == vw::PType::Float ? "float" : d.type == vw::PType::Int ? "int" : d.type == vw::PType::Bool ? "bool" : "choice";
        std::printf ("  {\"index\":%zu,\"id\":\"%s\",\"name\":\"%s\",\"unit\":\"%s\",\"type\":\"%s\",\"min\":%g,\"max\":%g,\"def\":%g,\"skew\":%g,\"choices\":[",
                     i, esc (d.id).c_str(), esc (d.name).c_str(), esc (d.unit).c_str(), type, d.min, d.max, d.def, d.skewCentre);
        for (size_t c = 0; c < d.choices.size(); ++c) std::printf ("%s\"%s\"", c ? "," : "", esc (d.choices[c]).c_str());
        std::printf ("]}%s\n", i + 1 < t.size() ? "," : "");
    }
    std::printf ("],\"presets\":[");
    const auto& p = vw::factoryPresets();
    for (size_t i = 0; i < p.size(); ++i) std::printf ("%s\"%s\"", i ? "," : "", esc (p[i].name).c_str());
    std::printf ("]}\n");
}
