// Renders each editor page to PNG with live engine data (development tool).
#include <juce_gui_basics/juce_gui_basics.h>
#include "Plugin/PluginProcessor.h"
#include "Plugin/PluginEditor.h"
#include "../Tests/TestSignals.h"

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const juce::String outDir = argc > 1 ? argv[1] : ".";
    VowelWormProcessor proc;
    proc.setCurrentProgram (argc > 2 ? juce::String (argv[2]).getIntValue() : 2);
    proc.prepareToPlay (48000, 256);
    juce::AudioBuffer<float> buf (2, 256);
    juce::MidiBuffer midi;
    ts::VoiceGen g; g.f0 = 196; g.vibDepthCents = 25; g.setVowel (ts::vowelA());
    std::unique_ptr<VowelWormEditor> ed (static_cast<VowelWormEditor*> (proc.createEditor()));
    ed->setVisible (true);
    for (int page = 0; page < 6; ++page)
    {
        ed->showPage (page);
        for (int b = 0; b < 400; ++b)
        {
            const double t = b / 400.0;
            ts::Vowel v = ts::vowelA();
            if (t > 0.6) { const double k = (t - 0.6) / 0.4; v = { 730 + (530 - 730) * k, 1090 + (1840 - 1090) * k, 2440, 3400 }; }
            g.setVowel (v);
            for (int i = 0; i < 256; ++i) { const float s = (float) g.next(); buf.setSample (0, i, s); buf.setSample (1, i, s); }
            midi.clear();
            if (b == 2) for (int n : { 55, 59, 62, 67 }) midi.addEvent (juce::MidiMessage::noteOn (1, n, (juce::uint8) 100), 0);
            proc.processBlock (buf, midi);
            if (b % 8 == 0) ed->tickForSnapshot();
        }
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        juce::File f (outDir + "/page" + juce::String (page) + ".png");
        f.deleteFile();
        juce::FileOutputStream os (f);
        juce::PNGImageFormat().writeImageToStream (img, os);
    }
    ed.reset();
    return 0;
}
