#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace vw;

// ---------------------------------------------------------------------------
juce::AudioProcessor::BusesProperties VowelWormProcessor::makeBuses()
{
    if (isInstrument)
        // Instrument: no main input; the vocal arrives on the sidechain (aux) bus.
        return BusesProperties()
            .withInput ("Input", juce::AudioChannelSet::stereo(), false)
            .withInput ("Vocal Sidechain", juce::AudioChannelSet::stereo(), true)
            .withOutput ("Output", juce::AudioChannelSet::stereo(), true);

    return BusesProperties()
        .withInput ("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true);
}

bool VowelWormProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto out = l.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono()) return false;
    for (int b = 0; b < l.inputBuses.size(); ++b)
    {
        const auto in = l.getChannelSet (true, b);
        if (! in.isDisabled() && in != juce::AudioChannelSet::stereo() && in != juce::AudioChannelSet::mono()) return false;
    }
    if (! isInstrument && l.getMainInputChannelSet().isDisabled()) return false;
    return true;
}

juce::AudioProcessorValueTreeState::ParameterLayout VowelWormProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (const auto& d : paramTable())
    {
        const juce::ParameterID id { d.id, 1 };
        const juce::String unit (d.unit);
        const float mn = d.min, mx = d.max;
        const bool isPan = juce::String (d.id).endsWith ("Pan");
        auto toText = [unit, mn, mx, isPan] (float v, int) -> juce::String
        {
            if (isPan) { const int p = juce::roundToInt (v * 100.0f); return p == 0 ? juce::String ("C") : (p < 0 ? "L" : "R") + juce::String (std::abs (p)); }
            if (unit == "dB") return v <= -59.9f ? juce::String ("-inf dB") : (std::abs (v) < 0.05f ? juce::String ("0.0 dB") : juce::String (v, std::abs (v) < 10.0f ? 1 : 0) + " dB");
            if (unit == "ms") return v >= 1000.0f ? juce::String (v / 1000.0f, 2) + " s" : juce::String (v, v < 10.0f ? 1 : 0) + " ms";
            if (unit == "st") return (v > 0.04f ? "+" : "") + juce::String (v, 1) + " st";
            if (unit == "ct") return juce::String (juce::roundToInt (v)) + " ct";
            if (unit == "Hz") return juce::String (v, v < 1.0f ? 2 : (v < 100.0f ? 1 : 0)) + " Hz";
            if (unit == "x") return juce::String (v, 2) + "x";
            if (mn >= 0.0f && mx <= 1.0f) return juce::String (juce::roundToInt (v * 100.0f)) + "%";
            if (mn >= -1.0f && mx <= 1.0f) { const int p = juce::roundToInt (v * 100.0f); return (p > 0 ? "+" : "") + juce::String (p) + "%"; }
            return juce::String (v, 2);
        };
        auto fromText = [unit, mn, mx, isPan] (const juce::String& t) -> float
        {
            auto s = t.trim();
            if (isPan) { const float n = s.retainCharacters ("0123456789.").getFloatValue() / 100.0f; return s.startsWithIgnoreCase ("L") ? -n : n; }
            if (s.containsIgnoreCase ("inf")) return mn;
            float v = s.retainCharacters ("-+0123456789.").getFloatValue();
            if (unit == "ms" && s.endsWithIgnoreCase (" s")) v *= 1000.0f;
            else if (unit.isEmpty() && mx <= 1.0f && s.containsChar ('%')) v /= 100.0f;
            return v;
        };
        const auto attrs = juce::AudioParameterFloatAttributes().withLabel (d.unit).withAutomatable (d.automatable)
                               .withStringFromValueFunction (toText).withValueFromStringFunction (fromText);
        switch (d.type)
        {
            case PType::Float:
            {
                juce::NormalisableRange<float> range (d.min, d.max);
                if (d.skewCentre > d.min && d.skewCentre < d.max) range.setSkewForCentre (d.skewCentre);
                layout.add (std::make_unique<juce::AudioParameterFloat> (id, d.name, range, d.def, attrs));
                break;
            }
            case PType::Int:
                layout.add (std::make_unique<juce::AudioParameterInt> (id, d.name, (int) d.min, (int) d.max, (int) d.def,
                                                                        juce::AudioParameterIntAttributes().withLabel (d.unit)));
                break;
            case PType::Bool:
                layout.add (std::make_unique<juce::AudioParameterBool> (id, d.name, d.def > 0.5f));
                break;
            case PType::Choice:
            {
                juce::StringArray items;
                for (const auto& c : d.choices) items.add (c);
                layout.add (std::make_unique<juce::AudioParameterChoice> (id, d.name, items, (int) d.def));
                break;
            }
        }
    }
    return layout;
}

VowelWormProcessor::VowelWormProcessor()
    : AudioProcessor (makeBuses()),
      apvts (*this, nullptr, "VowelWorm", createLayout())
{
    const auto& t = paramTable();
    for (size_t i = 0; i < t.size(); ++i)
    {
        raw[i] = apvts.getRawParameterValue (t[i].id);
        params[i] = apvts.getParameter (t[i].id);
        jassert (raw[i] != nullptr && params[i] != nullptr);
    }
    values = defaultParamValues();
}

// ---------------------------------------------------------------------------
void VowelWormProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    mono.assign ((size_t) std::max (samplesPerBlock, 2048), 0.0f);
    for (size_t i = 0; i < values.size(); ++i) values[i] = raw[i]->load();
    engine.setParameters (values);
    engine.prepare (sampleRate, samplesPerBlock);
    setLatencySamples (0); // dry lead is not delayed; harmonies trail by ~2-3 pitch periods (see docs)
}

void VowelWormProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (n == 0) return;

    for (size_t i = 0; i < values.size(); ++i) values[i] = raw[i]->load (std::memory_order_relaxed);
    engine.setParameters (values);

    // vocal input: sidechain bus for the instrument build (falls back to the main bus)
    juce::AudioBuffer<float> in;
    if (isInstrument && getBusCount (true) > 1 && getBus (true, 1)->isEnabled())
        in = getBusBuffer (buffer, true, 1);
    else if (getBusCount (true) > 0 && getBus (true, 0)->isEnabled())
        in = getBusBuffer (buffer, true, 0);

    auto out = getBusBuffer (buffer, false, 0);
    float* L = out.getWritePointer (0);
    float* R = out.getNumChannels() > 1 ? out.getWritePointer (1) : nullptr;

    // process in pieces no larger than the mono scratch buffer
    auto midiIt = midi.cbegin();
    for (int start = 0; start < n; )
    {
        const int len = std::min (n - start, (int) mono.size());

        // mono sum of the input (taken before the output overwrites a shared buffer)
        const int inCh = in.getNumChannels();
        if (inCh == 0) std::fill (mono.begin(), mono.begin() + len, 0.0f);
        else
        {
            juce::FloatVectorOperations::copy (mono.data(), in.getReadPointer (0, start), len);
            for (int c = 1; c < inCh; ++c) juce::FloatVectorOperations::add (mono.data(), in.getReadPointer (c, start), len);
            if (inCh > 1) juce::FloatVectorOperations::multiply (mono.data(), 1.0f / (float) inCh, len);
        }

        // split at MIDI events for sample-accurate notes
        int pos = 0;
        float* r = R != nullptr ? R + start : L + start; // mono out: R written into L then averaged below
        while (pos < len)
        {
            int next = len;
            if (midiIt != midi.cend())
            {
                const int evPos = (*midiIt).samplePosition - start;
                if (evPos <= pos)
                {
                    const auto msg = (*midiIt).getMessage();
                    if (msg.isNoteOn()) engine.noteOn (msg.getNoteNumber(), msg.getFloatVelocity());
                    else if (msg.isNoteOff()) engine.noteOff (msg.getNoteNumber());
                    else if (msg.isController()) engine.controller (msg.getControllerNumber(), msg.getControllerValue());
                    else if (msg.isPitchWheel()) engine.pitchBend (msg.getPitchWheelValue());
                    else if (msg.isAllNotesOff() || msg.isAllSoundOff()) engine.allNotesOff();
                    ++midiIt;
                    continue;
                }
                next = std::min (len, evPos);
            }
            if (R != nullptr) engine.process (mono.data() + pos, L + start + pos, r + pos, next - pos);
            else
            {
                // mono output bus: render stereo into scratch, then fold down
                float tmpR[256];
                for (int p = pos; p < next; )
                {
                    const int k = std::min (256, next - p);
                    engine.process (mono.data() + p, L + start + p, tmpR, k);
                    for (int i = 0; i < k; ++i) L[start + p + i] = 0.5f * (L[start + p + i] + tmpR[i]);
                    p += k;
                }
            }
            pos = next;
        }
        start += len;
    }

    // remaining events after the last sample (rare) are applied now
    for (; midiIt != midi.cend(); ++midiIt)
    {
        const auto msg = (*midiIt).getMessage();
        if (msg.isNoteOn()) engine.noteOn (msg.getNoteNumber(), msg.getFloatVelocity());
        else if (msg.isNoteOff()) engine.noteOff (msg.getNoteNumber());
    }

    for (int c = 2; c < out.getNumChannels(); ++c) out.clear (c, 0, n);
    midi.clear();
}

// ---------------------------------------------------------------------------
int VowelWormProcessor::getNumPrograms() { return (int) factoryPresets().size(); }

const juce::String VowelWormProcessor::getProgramName (int index)
{
    const auto& f = factoryPresets();
    return juce::isPositiveAndBelow (index, (int) f.size()) ? juce::String (f[(size_t) index].name) : juce::String();
}

void VowelWormProcessor::setCurrentProgram (int index)
{
    const auto& f = factoryPresets();
    if (! juce::isPositiveAndBelow (index, (int) f.size())) return;
    currentProgram = index;
    applyPreset (f[(size_t) index]);
}

void VowelWormProcessor::setParamValue (int index, float realValue)
{
    auto* p = params[(size_t) index];
    p->setValueNotifyingHost (p->convertTo0to1 (realValue));
}

void VowelWormProcessor::applyPreset (const PresetData& p)
{
    const auto v = presetValues (p);
    const auto& t = paramTable();
    for (size_t i = 0; i < t.size(); ++i)
    {
        // calibration belongs to the singer, not the preset
        if ((int) i >= P::f1Lo && (int) i <= P::f2Hi) continue;
        if ((int) i >= P::macro0 && (int) i < P::macro0 + kNumMacros) { setParamValue ((int) i, 0.0f); continue; }
        setParamValue ((int) i, v[i]);
    }
    currentPresetName = p.name;
}

PresetData VowelWormProcessor::captureCurrent (const juce::String& name) const
{
    PresetData d;
    d.name = name.toStdString();
    const auto& t = paramTable();
    for (size_t i = 0; i < t.size(); ++i)
    {
        if ((int) i >= P::f1Lo && (int) i <= P::f2Hi) continue;
        d.values[t[i].id] = raw[i]->load();
    }
    return d;
}

bool VowelWormProcessor::pollCalibrationResult (bool& succeeded)
{
    if (! calibrationPending) return false;
    if (engine.isCalibrating() || ++calibrationWait < 3) return false; // wait for the audio thread to stop
    calibrationPending = false; calibrationWait = 0;
    float a, b, c, d;
    succeeded = engine.getCalibration (a, b, c, d);
    if (succeeded)
    {
        // small margin so the extremes of the singer's range reach the edges comfortably
        setParamValue (P::f1Lo, a * 1.04f); setParamValue (P::f1Hi, b * 0.96f);
        setParamValue (P::f2Lo, c * 1.04f); setParamValue (P::f2Hi, d * 0.96f);
    }
    return true;
}

// ---------------------------------------------------------------------------
void VowelWormProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("presetName", currentPresetName, nullptr);
    state.setProperty ("program", currentProgram, nullptr);
    state.setProperty ("stateVersion", 1, nullptr);
    state.removeChild (state.getChildWithName ("UI"), nullptr);
    state.appendChild (uiState.createCopy(), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void VowelWormProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        auto tree = juce::ValueTree::fromXml (*xml);
        if (! tree.hasType (apvts.state.getType())) return;
        currentPresetName = tree.getProperty ("presetName", currentPresetName).toString();
        currentProgram = (int) tree.getProperty ("program", 0);
        auto ui = tree.getChildWithName ("UI");
        if (ui.isValid()) { uiState = ui.createCopy(); tree.removeChild (ui, nullptr); }
        apvts.replaceState (tree);

        // replaceState only notifies parameters whose stored value changed; a host
        // that moved a parameter just before restoring could otherwise keep its
        // value. Push every parameter explicitly so restore is always exact.
        const auto& t = paramTable();
        for (size_t i = 0; i < t.size(); ++i)
        {
            float v = t[i].def;
            for (const auto& child : tree)
                if (child.hasType ("PARAM") && child.getProperty ("id").toString() == juce::String (t[i].id))
                { v = (float) child.getProperty ("value", v); break; }
            auto* prm = params[i];
            const float norm = prm->convertTo0to1 (v);
            if (std::abs (prm->getValue() - norm) > 1.0e-6f) prm->setValueNotifyingHost (norm);
        }
    }
}

juce::AudioProcessorEditor* VowelWormProcessor::createEditor() { return new VowelWormEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new VowelWormProcessor(); }
