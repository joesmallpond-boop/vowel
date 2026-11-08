// vowelworm~ — the VowelWorm engine as a Max/MSP external (used by the Max for Live device).
//
// Inlets : 1 signal L (+ messages), 2 signal R. The stereo input is summed to mono.
// Outlets: 1 signal L, 2 signal R, 3 messages:
//            tele <63 floats>     — ~30 Hz analysis/telemetry for the displays (see Max/README.md)
//            p <param-id> <value> — parameter values pushed back to the UI (presets, calibration)
//
// Messages:
//   param <id> <value>    set a parameter in real units (ids = docs/PARAMETERS.md)
//   paramp <id> <value>   same, value given in percent (divided by 100)
//   note <pitch> <vel>    MIDI note (vel 0 = off)
//   cc <num> <value>      MIDI controller (1 mod wheel, 64 sustain, 120/123 all off)
//   bend <value>          pitch bend, 0..16383 (or 0..127 MSB)
//   allnotesoff
//   preset <index>        load factory preset (0-based) and dump its values
//   calibrate <0|1>       start / stop vowel calibration
//
// Real-time safety: MIDI goes through a lock-free queue (writers serialise among
// themselves; the audio thread never blocks). Parameter floats are word-sized
// writes read once per vector.

#include "ext.h"
#include "ext_obex.h"
#include "z_dsp.h"
#include "ext_critical.h"

#include "Engine/Engine.h"
#include "Engine/FactoryPresets.h"

#include <atomic>
#include <string>
#include <unordered_map>
#include <vector>

namespace
{
struct MidiEv { int type, a, b; }; // 0 note, 1 cc, 2 bend, 3 all off

struct State
{
    vw::Engine engine;
    vw::ParamValues values = vw::defaultParamValues();
    std::vector<float> mono, L, R;
    static constexpr int kQ = 1024;
    MidiEv q[kQ];
    std::atomic<int> head { 0 }, tail { 0 };
    bool calibrating = false;
    int calibWait = 0;
};

const std::unordered_map<std::string, int>& idIndex()
{
    static const std::unordered_map<std::string, int> m = []
    {
        std::unordered_map<std::string, int> r;
        const auto& t = vw::paramTable();
        for (size_t i = 0; i < t.size(); ++i) r[t[i].id] = (int) i;
        return r;
    }();
    return m;
}
} // namespace

typedef struct _vowelworm
{
    t_pxobject ob;
    State* st;
    void* msgOut;
    void* clock;
    t_critical lock;
} t_vowelworm;

static t_class* vw_class = nullptr;

static void vw_push (t_vowelworm* x, int type, int a, int b)
{
    State& s = *x->st;
    critical_enter (x->lock);
    const int t = s.tail.load (std::memory_order_relaxed);
    const int next = (t + 1) % State::kQ;
    if (next != s.head.load (std::memory_order_acquire))
    {
        s.q[t] = { type, a, b };
        s.tail.store (next, std::memory_order_release);
    }
    critical_exit (x->lock);
}

static void vw_setparam (t_vowelworm* x, t_symbol* id, double v, bool percent)
{
    auto it = idIndex().find (id->s_name);
    if (it == idIndex().end()) { object_warn ((t_object*) x, "unknown parameter %s", id->s_name); return; }
    const auto& d = vw::paramTable()[(size_t) it->second];
    float f = (float) (percent ? v / 100.0 : v);
    x->st->values[(size_t) it->second] = vw::clampf (f, d.min, d.max);
}

static void vw_param (t_vowelworm* x, t_symbol*, long argc, t_atom* argv)
{
    if (argc >= 2 && atom_gettype (argv) == A_SYM) vw_setparam (x, atom_getsym (argv), atom_getfloat (argv + 1), false);
}

static void vw_paramp (t_vowelworm* x, t_symbol*, long argc, t_atom* argv)
{
    if (argc >= 2 && atom_gettype (argv) == A_SYM) vw_setparam (x, atom_getsym (argv), atom_getfloat (argv + 1), true);
}

static void vw_note (t_vowelworm* x, long pitch, long vel) { vw_push (x, 0, (int) pitch, (int) vel); }
static void vw_cc (t_vowelworm* x, long num, long val) { vw_push (x, 1, (int) num, (int) val); }
static void vw_bend (t_vowelworm* x, long v) { vw_push (x, 2, v < 128 ? (int) v * 128 : (int) v, 0); }
static void vw_allnotesoff (t_vowelworm* x) { vw_push (x, 3, 0, 0); }

static void vw_dumpparam (t_vowelworm* x, int index)
{
    t_atom a[2];
    atom_setsym (a, gensym (vw::paramTable()[(size_t) index].id.c_str()));
    atom_setfloat (a + 1, x->st->values[(size_t) index]);
    outlet_anything (x->msgOut, gensym ("p"), 2, a);
}

static void vw_preset (t_vowelworm* x, long index)
{
    const auto& f = vw::factoryPresets();
    if (index < 0 || index >= (long) f.size()) return;
    const auto v = vw::presetValues (f[(size_t) index]);
    for (int i = 0; i < vw::P::count; ++i)
    {
        if (i >= vw::P::f1Lo && i <= vw::P::f2Hi) continue;           // calibration belongs to the singer
        if (i >= vw::P::macro0 && i < vw::P::macro0 + vw::kNumMacros) continue; // macros stay where the player left them
        x->st->values[(size_t) i] = v[(size_t) i];
        vw_dumpparam (x, i);
    }
}

static void vw_calibrate (t_vowelworm* x, long on)
{
    x->st->engine.requestCalibration (on != 0);
    x->st->calibrating = on != 0;
    x->st->calibWait = on ? 0 : 1;
}

static void vw_tick (t_vowelworm* x)
{
    State& s = *x->st;
    auto& t = s.engine.telemetry;
    t_atom a[64];
    int n = 0;
    auto F = [&] (float v) { atom_setfloat (a + n, v); ++n; };
    F (t.pitchHz.load()); F (t.confidence.load()); F (t.vowelX.load()); F (t.vowelY.load());
    F (t.f1.load()); F (t.f2.load()); F (t.amplitude.load()); F (t.attack.load());
    F (t.brightness.load()); F (t.breath.load()); F (t.pitchMove.load()); F (t.vibrato.load());
    F (t.inputPeak.exchange (0.0f)); F (t.outputPeak.exchange (0.0f)); F (t.harmonyDelayMs.load()); F ((float) t.numVoices.load());
    for (int i = 0; i < vw::kMaxVoices; ++i) F ((float) t.voiceNote[i].load());
    for (int i = 0; i < vw::kMaxVoices; ++i) F (t.voiceLevel[i].load());
    for (int i = 0; i < (int) vw::ModDest::Count; ++i) F (t.destValue[i].load());
    for (int i = 0; i < vw::kNumRoutes; ++i) F (t.routeValue[i].load());
    for (int i = 0; i < 4; ++i) { const uint32_t m = t.heldNotes[i].load(); F ((float) (m & 0xffffu)); F ((float) (m >> 16)); } // 16-bit halves stay exact as floats
    F (t.lfo1.load()); F (t.lfo2.load());
    outlet_anything (x->msgOut, gensym ("tele"), n, a);

    // finish calibration once the audio thread has stopped collecting
    if (s.calibWait > 0 && ! s.engine.isCalibrating() && ++s.calibWait > 3)
    {
        s.calibWait = 0;
        float f1lo, f1hi, f2lo, f2hi;
        t_atom r;
        if (s.engine.getCalibration (f1lo, f1hi, f2lo, f2hi))
        {
            s.values[vw::P::f1Lo] = f1lo * 1.04f; s.values[vw::P::f1Hi] = f1hi * 0.96f;
            s.values[vw::P::f2Lo] = f2lo * 1.04f; s.values[vw::P::f2Hi] = f2hi * 0.96f;
            for (int i = vw::P::f1Lo; i <= vw::P::f2Hi; ++i) vw_dumpparam (x, i);
            atom_setlong (&r, 1);
        }
        else atom_setlong (&r, 0);
        outlet_anything (x->msgOut, gensym ("calibrated"), 1, &r);
    }
    clock_fdelay (x->clock, 33.0);
}

static void vw_perform64 (t_vowelworm* x, t_object*, double** ins, long numins, double** outs, long numouts, long frames, long, void*)
{
    State& s = *x->st;
    // drain MIDI
    int h = s.head.load (std::memory_order_relaxed);
    const int tl = s.tail.load (std::memory_order_acquire);
    while (h != tl)
    {
        const MidiEv& e = s.q[h];
        if (e.type == 0) { if (e.b > 0) s.engine.noteOn (e.a, (float) e.b / 127.0f); else s.engine.noteOff (e.a); }
        else if (e.type == 1) s.engine.controller (e.a, e.b);
        else if (e.type == 2) s.engine.pitchBend (e.a);
        else s.engine.allNotesOff();
        h = (h + 1) % State::kQ;
    }
    s.head.store (h, std::memory_order_release);

    s.engine.setParameters (s.values);

    for (long start = 0; start < frames; )
    {
        const long n = std::min (frames - start, (long) s.mono.size());
        for (long i = 0; i < n; ++i)
        {
            double v = numins > 0 ? ins[0][start + i] : 0.0;
            if (numins > 1) v = 0.5 * (v + ins[1][start + i]);
            s.mono[(size_t) i] = (float) v;
        }
        s.engine.process (s.mono.data(), s.L.data(), s.R.data(), (int) n);
        for (long i = 0; i < n; ++i)
        {
            if (numouts > 0) outs[0][start + i] = s.L[(size_t) i];
            if (numouts > 1) outs[1][start + i] = s.R[(size_t) i];
        }
        start += n;
    }
}

static void vw_dsp64 (t_vowelworm* x, t_object* dsp64, short*, double samplerate, long maxvectorsize, long)
{
    State& s = *x->st;
    const size_t cap = (size_t) std::max<long> (maxvectorsize, 64);
    s.mono.assign (cap, 0.0f); s.L.assign (cap, 0.0f); s.R.assign (cap, 0.0f);
    s.engine.setParameters (s.values);
    s.engine.prepare (samplerate, (int) cap);
    object_method (dsp64, gensym ("dsp_add64"), x, vw_perform64, 0, nullptr);
}

static void vw_assist (t_vowelworm*, void*, long m, long a, char* s)
{
    if (m == ASSIST_INLET) snprintf_zero (s, 256, a == 0 ? "(signal) vocal L, messages" : "(signal) vocal R");
    else snprintf_zero (s, 256, a == 0 ? "(signal) out L" : (a == 1 ? "(signal) out R" : "tele / p / calibrated messages"));
}

static void* vw_new (t_symbol*, long, t_atom*)
{
    t_vowelworm* x = (t_vowelworm*) object_alloc (vw_class);
    if (! x) return nullptr;
    dsp_setup ((t_pxobject*) x, 2);
    x->msgOut = outlet_new (x, nullptr);
    outlet_new (x, "signal");
    outlet_new (x, "signal");
    x->st = new State();
    critical_new (&x->lock);
    x->clock = clock_new (x, (method) vw_tick);
    clock_fdelay (x->clock, 100.0);
    return x;
}

static void vw_free (t_vowelworm* x)
{
    dsp_free ((t_pxobject*) x);
    if (x->clock) { clock_unset (x->clock); object_free (x->clock); }
    critical_free (x->lock);
    delete x->st;
}

extern "C" void ext_main (void*)
{
    t_class* c = class_new ("vowelworm~", (method) vw_new, (method) vw_free, (long) sizeof (t_vowelworm), 0L, A_GIMME, 0);
    class_addmethod (c, (method) vw_dsp64, "dsp64", A_CANT, 0);
    class_addmethod (c, (method) vw_assist, "assist", A_CANT, 0);
    class_addmethod (c, (method) vw_param, "param", A_GIMME, 0);
    class_addmethod (c, (method) vw_paramp, "paramp", A_GIMME, 0);
    class_addmethod (c, (method) vw_note, "note", A_LONG, A_LONG, 0);
    class_addmethod (c, (method) vw_cc, "cc", A_LONG, A_LONG, 0);
    class_addmethod (c, (method) vw_bend, "bend", A_LONG, 0);
    class_addmethod (c, (method) vw_allnotesoff, "allnotesoff", 0);
    class_addmethod (c, (method) vw_preset, "preset", A_LONG, 0);
    class_addmethod (c, (method) vw_calibrate, "calibrate", A_LONG, 0);
    class_dspinit (c);
    class_register (CLASS_BOX, c);
    vw_class = c;
}
