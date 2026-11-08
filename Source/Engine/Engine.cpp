#include "Engine.h"

namespace vw
{

Engine::Engine() { params = defaultParamValues(); }

void Engine::prepare (double sampleRate, int maximumBlock)
{
    sr = sampleRate;
    maxBlock = std::max (1, maximumBlock);
    tracker.prepare (sr);
    analyser.prepare (sr);
    for (int i = 0; i < kMaxVoices; ++i)
    {
        synth[(size_t) i].prepare (sr);
        synth[(size_t) i].seed (0x1234567u * (uint32_t) (i + 3));
    }
    tmpVoc.assign ((size_t) kControlBlock, 0.0f);
    tmpSyn.assign ((size_t) kControlBlock, 0.0f);
    alloc.legatoWindow = (int64_t) (0.25 * sr);
    reset();
}

void Engine::reset()
{
    tracker.reset();
    analyser.reset();
    alloc.reset();
    for (int i = 0; i < kMaxVoices; ++i)
    {
        psola[(size_t) i].reset (0);
        synth[(size_t) i].reset();
        auto& v = vs[(size_t) i];
        v = VoiceState {};
        v.vibPhase = 0.137f * (float) i;
    }
    for (int d = 0; d < (int) ModDest::Count; ++d)
    {
        destNorm[(size_t) d] = destBaseNorm ((ModDest) d);
        destSmooth[(size_t) d].reset (destNorm[(size_t) d]);
    }
    for (auto& r : routeSmooth) r.reset();
    for (auto& m : macroSmooth) m.reset();
    leadPrev = 0.0f; outPrev = dbToGain (params[P::outputLevel]);
    for (auto& g : prevGain) g = {};
    controlCounter = 0;
    std::memset (heldMask, 0, sizeof (heldMask));
}

// ---------------------------------------------------------------- MIDI ----
void Engine::noteOn (int note, float velocity) noexcept
{
    if (velocity <= 0.0f) { noteOff (note); return; }
    lastVelocity = velocity;
    alloc.noteOn (note, velocity, tracker.now());
    heldMask[note >> 5] |= (1u << (note & 31));
    telemetry.midiActivity.fetch_add (1, std::memory_order_relaxed);
}

void Engine::noteOff (int note) noexcept
{
    alloc.noteOff (note, tracker.now());
    heldMask[note >> 5] &= ~(1u << (note & 31));
}

void Engine::controller (int cc, int value) noexcept
{
    if (cc == 1) modWheel = (float) value / 127.0f;
    else if (cc == 64) alloc.setSustain (value >= 64, tracker.now());
    else if (cc == 120 || cc == 123) allNotesOff();
}

void Engine::pitchBend (int value14) noexcept { bend = clampf (((float) value14 - 8192.0f) / 8192.0f, -1.0f, 1.0f); }

void Engine::allNotesOff() noexcept
{
    alloc.allNotesOff (tracker.now());
    std::memset (heldMask, 0, sizeof (heldMask));
}

// ------------------------------------------------------------ modulation ----
float Engine::modSourceValue (ModSource s) const noexcept
{
    const auto& a = analyser.get();
    switch (s)
    {
        case ModSource::VowelX:     return a.vowelX;
        case ModSource::VowelY:     return a.vowelY;
        case ModSource::Amplitude:  return a.amplitude;
        case ModSource::Attack:     return a.attack;
        case ModSource::Breath:     return a.breath;
        case ModSource::Brightness: return a.brightness;
        case ModSource::PitchMove:  return a.pitchMove;
        case ModSource::Vibrato:    return a.vibratoDepth;
        case ModSource::Velocity:   return lastVelocity;
        case ModSource::ModWheel:   return modWheel;
        case ModSource::LFO1:       return lfoValue[0];
        case ModSource::LFO2:       return lfoValue[1];
        case ModSource::None:
        case ModSource::Count:      return 0.0f;
    }
    return 0.0f;
}

static inline float normLin (float v, float lo, float hi) { return clamp01 ((v - lo) / (hi - lo)); }

float Engine::destBaseNorm (ModDest d) const noexcept
{
    switch (d)
    {
        case ModDest::FormantShift:    return normLin (params[P::formantShift], -12.0f, 12.0f);
        case ModDest::FormantEmphasis: return params[P::formantEmphasis];
        case ModDest::SynthBlend:      return params[P::blend];
        case ModDest::SynthBrightness: return params[P::synthBrightness];
        case ModDest::OscMorph:        return params[P::oscMorph];
        case ModDest::Noise:           return params[P::noise];
        case ModDest::HarmonySpread:   return params[P::spread];
        case ModDest::VibratoDepth:    return normLin (params[P::vibratoDepth], 0.0f, 100.0f);
        case ModDest::Glide:           return glideMsToNorm (params[P::glide]);
        case ModDest::VoiceBalance:    return 0.5f;
        case ModDest::HarmonyLevel:    return normLin (params[P::harmonyLevel], -60.0f, 6.0f);
        case ModDest::SynthResonance:  return params[P::synthResonance];
        case ModDest::None:
        case ModDest::Count:           return 0.0f;
    }
    return 0.0f;
}

float Engine::destFromNorm (ModDest d, float n) const noexcept
{
    switch (d)
    {
        case ModDest::FormantShift: return -12.0f + 24.0f * n;
        case ModDest::VibratoDepth: return 100.0f * n;
        case ModDest::Glide:        return glideNormToMs (n);
        case ModDest::VoiceBalance: return 2.0f * n - 1.0f;
        case ModDest::HarmonyLevel: return -60.0f + 66.0f * n;
        case ModDest::FormantEmphasis: case ModDest::SynthBlend: case ModDest::SynthBrightness:
        case ModDest::OscMorph: case ModDest::Noise: case ModDest::HarmonySpread:
        case ModDest::SynthResonance: case ModDest::None: case ModDest::Count: return n;
    }
    return n;
}

// ------------------------------------------------------------ control ------
void Engine::controlUpdate (int n) noexcept
{
    // calibration requests from the message thread
    if (const int req = calibRequest.exchange (0))
    {
        if (req == 1) analyser.startCalibration(); else analyser.stopCalibration();
    }

    tracker.settings.gateDb = params[P::gate];
    // Low Latency caps grains at 6 ms: ~1/3 less delay for voices below ~165 Hz,
    // at some cost in low-voice fidelity (see docs/TESTING.md)
    tracker.setGrainCap (params[P::latencyMode] > 0.5f ? (float) (0.006 * sr) : 1.0e9f);
    auto& as = analyser.settings;
    as.vowelSmoothMs = params[P::vowelSmooth];
    as.analysisSmoothMs = params[P::analysisSmooth];
    as.sensitivity = params[P::vowelSens];
    as.f1Lo = params[P::f1Lo]; as.f1Hi = std::max (params[P::f1Hi], params[P::f1Lo] * 1.2f);
    as.f2Lo = params[P::f2Lo]; as.f2Hi = std::max (params[P::f2Hi], params[P::f2Lo] * 1.2f);

    // LFOs (bipolar)
    for (int l = 0; l < 2; ++l)
    {
        const float rate = params[l == 0 ? P::lfo1Rate : P::lfo2Rate];
        const int shape = (int) params[l == 0 ? P::lfo1Shape : P::lfo2Shape];
        lfoPhase[l] += rate * (float) n / (float) sr;
        if (lfoPhase[l] >= 1.0f)
        {
            lfoPhase[l] -= std::floor (lfoPhase[l]);
            lfoRandA[l] = lfoRandB[l];
            lfoRandB[l] = lfoNoise.next();
        }
        const float p = lfoPhase[l];
        if (shape == 0) lfoValue[l] = std::sin (kTwoPi * p);
        else if (shape == 1) lfoValue[l] = 1.0f - 4.0f * std::abs (p - 0.5f);
        else { const float s = p * p * (3.0f - 2.0f * p); lfoValue[l] = lerp (lfoRandA[l], lfoRandB[l], s); }
    }
    telemetry.lfo1.store (lfoValue[0]); telemetry.lfo2.store (lfoValue[1]);

    // modulation offsets
    std::array<float, (int) ModDest::Count> offset {};
    for (int r = 0; r < kNumRoutes; ++r)
    {
        const auto src = (ModSource) (int) params[(size_t) (P::modSrc0 + r)];
        const auto dst = (ModDest) (int) params[(size_t) (P::modDst0 + r)];
        const float raw = (src == ModSource::None || dst == ModDest::None) ? 0.0f
                        : modSourceValue (src) * params[(size_t) (P::modDepth0 + r)];
        const float v = routeSmooth[(size_t) r].process (raw, onePoleCoeff (params[(size_t) (P::modSmooth0 + r)], sr, n));
        if (dst != ModDest::None) offset[(size_t) dst] += v;
        telemetry.routeValue[r].store (v, std::memory_order_relaxed);
    }
    for (int m = 0; m < kNumMacros; ++m)
    {
        const float mv = macroSmooth[(size_t) m].process (params[(size_t) (P::macro0 + m)], onePoleCoeff (15.0f, sr, n));
        for (int s = 0; s < kMacroSlots; ++s)
        {
            const auto dst = (ModDest) (int) params[(size_t) (P::macroDst0 + m * kMacroSlots + s)];
            if (dst != ModDest::None) offset[(size_t) dst] += mv * params[(size_t) (P::macroAmt0 + m * kMacroSlots + s)];
        }
    }
    const float dCoef = onePoleCoeff (12.0f, sr, n);
    for (int d = 1; d < (int) ModDest::Count; ++d)
    {
        const float target = clamp01 (destBaseNorm ((ModDest) d) + offset[(size_t) d]);
        destNorm[(size_t) d] = destSmooth[(size_t) d].process (target, dCoef);
        telemetry.destValue[d].store (destNorm[(size_t) d], std::memory_order_relaxed);
        telemetry.destOffset[d].store (offset[(size_t) d], std::memory_order_relaxed);
    }
    auto D = [&] (ModDest d) { return destFromNorm (d, destNorm[(size_t) d]); };

    const float formantShift = D (ModDest::FormantShift);
    const float emphasis = D (ModDest::FormantEmphasis);
    const float blendG = D (ModDest::SynthBlend);
    const float spread = D (ModDest::HarmonySpread);
    const float vibDepth = D (ModDest::VibratoDepth);
    const float glideMs = D (ModDest::Glide);
    const float balance = D (ModDest::VoiceBalance);
    const float harmDb = D (ModDest::HarmonyLevel);

    const float leadMul = balance > 0.0f ? 1.0f - balance : 1.0f;
    const float harmMul = balance < 0.0f ? 1.0f + balance : 1.0f;

    const int mode = (int) params[P::mode];
    const int octave = (int) params[P::octave];
    const int nv = (int) params[P::voices];
    alloc.setNumVoices (nv, tracker.now());
    alloc.setLatch (params[P::latch] > 0.5f, tracker.now());
    telemetry.numVoices.store (nv);

    const auto& a = analyser.get();
    const float voicing = tracker.getVoicing();
    const float sung = a.pitchSemis;
    const float transfer = params[P::expressTransfer];
    const float bendSt = bend * params[P::bendRange];
    const float follow = params[P::formantFollow];
    const float cons = params[P::consonantLevel];
    const float velSens = params[P::velSens];
    const float attC = onePoleCoeff (params[P::harmonyAttack], sr, n);
    const float relC = onePoleCoeff (params[P::harmonyRelease], sr, n);
    // singer's dynamics as a linear gain (unity at -12 dBFS), soft-gated below -50 dBFS
    const float ampDb = a.amplitude * 60.0f - 60.0f;
    const float ampLin = clampf (dbToGain (ampDb + 12.0f), 0.0f, 2.0f) * clamp01 ((ampDb + 60.0f) / 10.0f);
    const float dynFollow = params[P::dynamicsFollow];
    const float synthLevel = dbToGain (params[P::synthLevel]);
    const float fTrack = params[P::formantTrack];
    const float vowelF1 = lerp (500.0f, a.f1, fTrack), vowelF2 = lerp (1500.0f, a.f2, fTrack);
    const float vowelF3 = lerp (2600.0f, 2300.0f + 900.0f * a.vowelX, fTrack);
    const float vCoef = onePoleCoeff (8.0f, sr, n);

    for (int i = 0; i < kMaxVoices; ++i)
    {
        auto& slot = alloc.v[(size_t) i];
        auto& v = vs[(size_t) i];
        auto& ps = psola[(size_t) i];
        auto& sy = synth[(size_t) i];

        const bool gate = slot.gate && i < nv;
        if (slot.triggerCount != v.seenTrigger)
        {
            v.seenTrigger = slot.triggerCount;
            const float val = mode == 0 ? (float) (slot.note + 12 * octave) : (float) (slot.note - (int) params[P::relRoot] + 12 * octave);
            v.target = val;
            if (! (slot.legato && (v.active || v.env > 0.01f))) v.glided = val;
            v.velGain = lerp (1.0f, slot.velocity, velSens);
        }
        if (slot.note >= 0)
            v.target = mode == 0 ? (float) (slot.note + 12 * octave) : (float) (slot.note - (int) params[P::relRoot] + 12 * octave);

        const float vg = params[(size_t) (P::vGlide0 + i)];
        const float gc = vg <= 0.001f ? 1.0f : onePoleCoeff (glideMs * vg, sr, n);
        v.glided += gc * (v.target - v.glided);

        v.env += (gate ? attC : relC) * ((gate ? 1.0f : 0.0f) - v.env);
        const bool shouldRun = gate || v.env > 1.0e-4f;
        if (shouldRun && ! v.active) ps.reset (tracker.now() - n);
        v.active = shouldRun;
        sy.gate = gate;

        // pitch
        v.vibPhase += params[P::vibratoRate] * (1.0f + 0.03f * (float) i) * (float) n / (float) sr;
        if (v.vibPhase >= 1.0f) v.vibPhase -= 1.0f;
        float semis = mode == 0 ? v.glided + transfer * a.pitchDev : sung + v.glided;
        semis += bendSt + params[(size_t) (P::vDetune0 + i)] * 0.01f
               + vibDepth * 0.01f * std::sin (kTwoPi * v.vibPhase);
        semis = clampf (semis, 24.0f, 100.0f);
        const float hz = midiToHz (semis);

        // formants
        const float fShift = formantShift + params[(size_t) (P::vFormant0 + i)];
        const float formantSemis = fShift + follow * (semis - sung);
        const float fr = clampf (std::exp2 (formantSemis / 12.0f), 0.5f, 2.0f);

        ps.targetPeriod = (float) sr / hz;
        ps.formantRatio = fr;
        ps.voicedness = v.voicedness.process (voicing, vCoef);

        v.emphAmt = emphasis;
        v.emph1.set (a.f1 * fr, 5.0f, sr);
        v.emph2.set (a.f2 * fr, 6.0f, sr);

        // levels & pan
        const float b = clamp01 (blendG + params[(size_t) (P::vBlend0 + i)]);
        const float vocalW = std::cos (b * kPi * 0.5f);
        const float synthW = std::sin (b * kPi * 0.5f);
        const float lvl = dbToGain (params[(size_t) (P::vLevel0 + i)]) * dbToGain (harmDb) * harmMul * v.velGain;
        const float consMul = lerp (cons, 1.0f, ps.voicedness);
        const float pan = clampf (params[(size_t) (P::vPan0 + i)] * spread, -1.0f, 1.0f);
        const float pl = std::cos ((pan + 1.0f) * kPi * 0.25f), pr = std::sin ((pan + 1.0f) * kPi * 0.25f);
        v.level.y = lvl;               // used below
        v.vocalGain.y = vocalW * consMul * v.env;
        v.synthGain.y = synthW * synthLevel;
        v.panL.y = pl; v.panR.y = pr;

        // synth
        SynthVoice::Controls c;
        c.freq = hz * std::exp2 ((float) (int) params[P::synthOctave]);
        c.morph = D (ModDest::OscMorph);
        c.brightness = D (ModDest::SynthBrightness);
        c.resonance = D (ModDest::SynthResonance);
        c.noise = D (ModDest::Noise);
        const float fsMul = std::exp2 (fShift / 12.0f);
        c.f1 = vowelF1 * fsMul; c.f2 = vowelF2 * fsMul; c.f3 = vowelF3 * fsMul;
        c.amp = std::pow (std::max (ampLin, 1.0e-6f), dynFollow) * (dynFollow > 0.0f ? clamp01 (ampLin * 1.0e4f) : 1.0f) * v.velGain;
        sy.setEnvelope (params[P::synthAttack], params[P::synthRelease]);
        sy.setControls (c);

        telemetry.voiceNote[i].store (gate ? slot.note : -1, std::memory_order_relaxed);
        telemetry.voiceLevel[i].store (v.env * lvl, std::memory_order_relaxed);
    }

    // telemetry
    telemetry.pitchHz.store (tracker.isVoiced() ? tracker.getF0() : 0.0f);
    telemetry.confidence.store (voicing);
    telemetry.vowelX.store (a.vowelX); telemetry.vowelY.store (a.vowelY);
    telemetry.f1.store (a.f1); telemetry.f2.store (a.f2);
    telemetry.amplitude.store (a.amplitude); telemetry.attack.store (a.attack);
    telemetry.breath.store (a.breath); telemetry.brightness.store (a.brightness);
    telemetry.pitchMove.store (a.pitchMove); telemetry.vibrato.store (a.vibratoDepth);
    telemetry.vibratoRate.store (a.vibratoRate);
    for (int k = 0; k < 4; ++k) telemetry.heldNotes[k].store (heldMask[k], std::memory_order_relaxed);

    // global gains (targets; ramped per sample in process)
    leadTarget = params[P::leadOn] > 0.5f ? dbToGain (params[P::leadLevel]) * leadMul : 0.0f;
    outTarget = dbToGain (params[P::outputLevel]);
}

// ------------------------------------------------------------- process -----
void Engine::process (const float* in, float* outL, float* outR, int n) noexcept
{
    int off = 0;
    while (off < n)
    {
        const int c = std::min (kControlBlock, n - off);
        const float* x = in + off;
        float* L = outL + off;
        float* R = outR + off;

        for (int i = 0; i < c; ++i) { tracker.push (x[i]); inPeak = std::max (inPeak, std::abs (x[i])); }
        tracker.updateMarks();
        analyser.process (x, c, tracker);
        controlUpdate (c);

        std::fill (L, L + c, 0.0f);
        std::fill (R, R + c, 0.0f);
        const int64_t t0 = tracker.now() - c;
        const float invC = 1.0f / (float) c;

        float delaySum = 0.0f; int delayN = 0;
        for (int vi = 0; vi < kMaxVoices; ++vi)
        {
            auto& v = vs[(size_t) vi];
            auto& sy = synth[(size_t) vi];
            const bool synthOn = ! sy.isSilent() && v.synthGain.y > 1.0e-4f;
            if (! v.active && ! synthOn) { prevGain[(size_t) vi] = {}; continue; }

            float* voc = tmpVoc.data();
            float* syn = tmpSyn.data();
            std::fill (syn, syn + c, 0.0f);

            const float gVoc = v.vocalGain.y * v.level.y * kHarmonyScale;
            if (v.active)
            {
                psola[(size_t) vi].render (tracker, t0, c, voc);
                delaySum += psola[(size_t) vi].lastDelay; ++delayN;
                if (v.emphAmt > 0.001f)
                {
                    const float e = v.emphAmt * 2.0f;
                    const float k1 = v.emph1.k, k2 = v.emph2.k;
                    for (int i = 0; i < c; ++i)
                    {
                        float lp, b1, b2;
                        v.emph1.process (voc[i], lp, b1);
                        v.emph2.process (voc[i], lp, b2);
                        voc[i] += e * (b1 * k1 + 0.7f * b2 * k2);
                    }
                    const float norm = 1.0f / (1.0f + 0.9f * v.emphAmt);
                    for (int i = 0; i < c; ++i) voc[i] *= norm;
                }
            }
            else std::fill (voc, voc + c, 0.0f);

            if (synthOn) sy.process (syn, c);
            const float gSyn = v.synthGain.y * v.level.y * kHarmonyScale;

            // per-sample linear ramps from the previous block's gains
            auto& pg = prevGain[(size_t) vi];
            const float vl = gVoc * v.panL.y, vr = gVoc * v.panR.y;
            const float sl = gSyn * v.panL.y, sr_ = gSyn * v.panR.y;
            for (int i = 0; i < c; ++i)
            {
                const float t = (float) (i + 1) * invC;
                const float a = lerp (pg[0], vl, t), b = lerp (pg[1], vr, t);
                const float d = lerp (pg[2], sl, t), e = lerp (pg[3], sr_, t);
                L[i] += voc[i] * a + syn[i] * d;
                R[i] += voc[i] * b + syn[i] * e;
            }
            pg = { vl, vr, sl, sr_ };
        }
        if (delayN > 0)
        {
            harmonyDelay = delaySum / (float) delayN;
            telemetry.harmonyDelayMs.store (1000.0f * harmonyDelay / (float) sr);
        }

        // lead vocal + output gain + safety soft clip
        for (int i = 0; i < c; ++i)
        {
            const float t = (float) (i + 1) * invC;
            const float lg = lerp (leadPrev, leadTarget, t);
            const float og = lerp (outPrev, outTarget, t);
            float l = (L[i] + x[i] * lg) * og;
            float r = (R[i] + x[i] * lg) * og;
            l = softClip (l); r = softClip (r);
            L[i] = l; R[i] = r;
            outPeak = std::max (outPeak, std::max (std::abs (l), std::abs (r)));
        }
        leadPrev = leadTarget; outPrev = outTarget;
        off += c;

        if (++controlCounter >= 16)
        {
            controlCounter = 0;
            // UI reads these with exchange(0); keep the max until then
            telemetry.inputPeak.store (std::max (inPeak, telemetry.inputPeak.load (std::memory_order_relaxed)), std::memory_order_relaxed);
            telemetry.outputPeak.store (std::max (outPeak, telemetry.outputPeak.load (std::memory_order_relaxed)), std::memory_order_relaxed);
            inPeak = outPeak = 0.0f;
        }
    }
}

} // namespace vw
