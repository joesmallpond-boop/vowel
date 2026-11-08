#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "../Engine/Engine.h"
#include "../Engine/FactoryPresets.h"

/** VowelWorm plug-in.
 *
 *  Built twice from the same source:
 *   - VowelWorm       (VW_INSTRUMENT=0): audio effect with MIDI input — goes on the
 *                      vocal track; a MIDI track's "MIDI To" targets it. Primary.
 *   - VowelWorm Inst  (VW_INSTRUMENT=1): instrument with a sidechain audio input —
 *                      goes on the MIDI track; its sidechain listens to the vocal track.
 *                      Fallback for hosts that will not send MIDI to an effect.
 */
class VowelWormProcessor : public juce::AudioProcessor
{
public:
    VowelWormProcessor();
    ~VowelWormProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 1.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ---- presets (message thread) ----
    void applyPreset (const vw::PresetData& p);
    vw::PresetData captureCurrent (const juce::String& name) const;
    juce::String getCurrentPresetName() const { return currentPresetName; }
    void setCurrentPresetName (const juce::String& n) { currentPresetName = n; }

    // ---- calibration (message thread) ----
    void startCalibration() { engine.requestCalibration (true); }
    void stopCalibration() { engine.requestCalibration (false); calibrationPending = true; }
    /** Call from a timer after stopCalibration(); returns true once the result was applied. */
    bool pollCalibrationResult (bool& succeeded);

    juce::RangedAudioParameter* param (int index) const { return params[(size_t) index]; }

    static constexpr bool isInstrument = VW_INSTRUMENT != 0;

    juce::AudioProcessorValueTreeState apvts;
    vw::Engine engine;
    juce::ValueTree uiState { "UI" };

private:
    static BusesProperties makeBuses();
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void setParamValue (int index, float realValue);

    std::array<std::atomic<float>*, (size_t) vw::P::count> raw {};
    std::array<juce::RangedAudioParameter*, (size_t) vw::P::count> params {};
    vw::ParamValues values {};
    std::vector<float> mono;

    int currentProgram = 0;
    juce::String currentPresetName { "Natural Four-Part" };
    bool calibrationPending = false;
    int calibrationWait = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VowelWormProcessor)
};
