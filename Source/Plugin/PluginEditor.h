#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

namespace ui
{
namespace col
{
    // "soil & candy": a worm's-eye palette — dark earth, cream text, worm pink (voice), mint (synth)
    const juce::Colour bg       { 0xff1e1611 };
    const juce::Colour panel    { 0xff2b2019 };
    const juce::Colour panelHi  { 0xff3b2c22 };
    const juce::Colour edge     { 0xff54402f };
    const juce::Colour text     { 0xfff7ead7 };
    const juce::Colour dim      { 0xffb7a08a };
    const juce::Colour human    { 0xffff7d9c };   // worm pink: the natural voice
    const juce::Colour synth    { 0xff6fe3c4 };   // mint: synthetic
    const juce::Colour warn     { 0xffffc94a };   // mustard
    const juce::Colour ok       { 0xffa8d96b };   // leaf
    const juce::Colour clip     { 0xffff5a4a };
    const juce::Colour wormDark { 0xffd65a7c };
}

/** Rounded "pebble" panel with a soft top highlight and an optional chunky title. */
void drawPebble (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& title = {});

class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    LookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float minPos, float maxPos, juce::Slider::SliderStyle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool over, bool down) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override { return juce::FontOptions (13.0f); }
    juce::Font getLabelFont (juce::Label& l) override { return l.getFont(); }
};

/** Rotary knob bound to a host parameter, with an optional modulation ring. */
class Knob : public juce::Component
{
public:
    Knob (VowelWormProcessor& p, int paramIndex, const juce::String& label = {}, int modDest = -1, bool large = false);
    void resized() override;
    void paintOverChildren (juce::Graphics&) override;
    void refreshMod();
    juce::Slider slider;
    juce::Label name;
    bool compact = false; // no title, value only while hovering/dragging
private:
    VowelWormProcessor& proc;
    int dest;
    bool big;
    float lastOffset = 0.0f;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
};

class Choice : public juce::Component
{
public:
    Choice (VowelWormProcessor& p, int paramIndex, const juce::String& label = {});
    void resized() override;
    juce::ComboBox box;
    juce::Label name;
private:
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> att;
};

class Toggle : public juce::Component
{
public:
    Toggle (VowelWormProcessor& p, int paramIndex, const juce::String& label = {});
    void resized() override { button.setBounds (getLocalBounds()); }
    juce::ToggleButton button;
private:
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> att;
};

/** A titled panel that lays its children out in a simple flowing grid. */
class Section : public juce::Component
{
public:
    explicit Section (juce::String t) : title (std::move (t)) {}
    void paint (juce::Graphics&) override;
    void resized() override;
    template <typename T> T* add (std::unique_ptr<T> c) { auto* r = c.get(); addAndMakeVisible (*r); items.push_back (std::move (c)); return r; }
    int cellW = 78, cellH = 92;
    juce::String title;
    std::vector<std::unique_ptr<juce::Component>> items;
};

// ---- live displays --------------------------------------------------------------
class LevelMeter : public juce::Component
{
public:
    explicit LevelMeter (juce::String l) : label (std::move (l)) {}
    void setLevel (float peak);
    void paint (juce::Graphics&) override;
private:
    juce::String label;
    float level = 0.0f, hold = 0.0f; int holdFrames = 0; bool clipped = false;
};

class PitchDisplay : public juce::Component
{
public:
    void update (float hz, float conf);
    void paint (juce::Graphics&) override;
private:
    float hz = 0, conf = 0;
};

class KeyStrip : public juce::Component
{
public:
    void update (const vw::Telemetry& t);
    void paint (juce::Graphics&) override;
private:
    std::array<uint32_t, 4> held {};
    std::array<int, vw::kMaxVoices> voiceNotes {};
};

/** The vowel space, inhabited by a worm: its body is your recent vowel path,
    its mouth takes the shape of the vowel you are singing. */
class VowelPad : public juce::Component
{
public:
    void update (float x, float y, float conf, float f1, float f2);
    void paint (juce::Graphics&) override;
private:
    float px = 0.5f, py = 0.5f, conf = 0, f1 = 0, f2 = 0;
    std::array<juce::Point<float>, 40> trail {};
    int trailPos = 0, trailCount = 0;
    int frame = 0;
    juce::Point<float> look { 1, 0 };
};

/** Lower-case wordmark whose two 'o's are little mouths that sing along. */
class WormTitle : public juce::Component
{
public:
    void update (float openness, float frontness, float level);
    void paint (juce::Graphics&) override;
private:
    float open = 0, front = 0.5f, lvl = 0; int t = 0;
};

class VoiceStrip : public juce::Component
{
public:
    void update (const vw::Telemetry& t, float blend);
    void paint (juce::Graphics&) override;
private:
    std::array<int, vw::kMaxVoices> notes {};
    std::array<float, vw::kMaxVoices> levels {};
    int numVoices = 4; float blend = 0;
};

class ExpressionMeters : public juce::Component
{
public:
    void update (const vw::Telemetry& t);
    void paint (juce::Graphics&) override;
private:
    std::array<float, 7> v {};
};

class StatusPills : public juce::Component
{
public:
    void update (float inPeak, int midiCount, int heldCount, bool voiced);
    void paint (juce::Graphics&) override;
private:
    bool audio = false, voiced = false; int midiFlash = 0, held = 0, lastMidi = 0;
};

// ---- pages ------------------------------------------------------------------------
class Page : public juce::Component
{
public:
    virtual ~Page() override = default;
    virtual void tick() {}
    float inPeak = 0.0f, outPeak = 0.0f; // set by the editor each frame
};

class MainPage;
class ModulationPage;
class AnalysisPage;

} // namespace ui

class VowelWormEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit VowelWormEditor (VowelWormProcessor&);
    ~VowelWormEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int baseW = 1040, baseH = 680;
    void showPage (int index);
    void tickForSnapshot() { timerCallback(); }

private:
    void timerCallback() override;
    void refreshPresetList();
    void loadPresetFromMenu (int id);
    void savePresetDialog();
    juce::File userPresetDir() const;

    VowelWormProcessor& proc;
    ui::LookAndFeel lnf;
    juce::Component content;
    ui::WormTitle title;
    juce::Image soil;
    juce::ComboBox presetBox;
    juce::TextButton prevPreset { "<" }, nextPreset { ">" }, savePreset { "Save" };
    juce::OwnedArray<juce::TextButton> tabs;
    std::vector<std::unique_ptr<ui::Page>> pages;
    ui::StatusPills status;
    int currentPage = 0;
    juce::Array<juce::File> userPresets;
    std::unique_ptr<juce::AlertWindow> saveWindow;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VowelWormEditor)
};
