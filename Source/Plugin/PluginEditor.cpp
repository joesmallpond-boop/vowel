#include "PluginEditor.h"
#include <complex>

using namespace vw;

namespace ui
{
static juce::String noteName (int n)
{
    static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    if (n < 0 || n > 127) return "-";
    return juce::String (names[n % 12]) + juce::String (n / 12 - 1);
}

static juce::Font font (float size, bool bold = false)
{
    return juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain);
}

/** Maps a parameter to the modulation destination it is the base value of (for rings). */
static int destForParam (int p)
{
    switch (p)
    {
        case P::formantShift: return (int) ModDest::FormantShift;
        case P::formantEmphasis: return (int) ModDest::FormantEmphasis;
        case P::blend: return (int) ModDest::SynthBlend;
        case P::synthBrightness: return (int) ModDest::SynthBrightness;
        case P::oscMorph: return (int) ModDest::OscMorph;
        case P::noise: return (int) ModDest::Noise;
        case P::spread: return (int) ModDest::HarmonySpread;
        case P::vibratoDepth: return (int) ModDest::VibratoDepth;
        case P::glide: return (int) ModDest::Glide;
        case P::harmonyLevel: return (int) ModDest::HarmonyLevel;
        case P::synthResonance: return (int) ModDest::SynthResonance;
        default: return -1;
    }
}

// =========================================================================== shared drawing
void drawPebble (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& title)
{
    g.setColour (juce::Colours::black.withAlpha (0.25f));
    g.fillRoundedRectangle (r.translated (0, 2.0f), 14.0f);
    juce::ColourGradient grad (col::panel.brighter (0.06f), 0, r.getY(), col::panel.darker (0.08f), 0, r.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (r, 14.0f);
    g.setColour (juce::Colours::white.withAlpha (0.05f));
    g.drawRoundedRectangle (r.reduced (1.0f), 13.0f, 1.2f);
    if (title.isNotEmpty())
    {
        auto tr = r.reduced (14, 7).removeFromTop (17);
        g.setColour (col::human);
        g.fillEllipse (tr.getX(), tr.getCentreY() - 3.5f, 7.0f, 7.0f);
        g.setColour (col::dim);
        g.setFont (font (12.0f, true));
        g.drawText (title.toLowerCase(), tr.withTrimmedLeft (12), juce::Justification::centredLeft);
    }
}

// =========================================================================== LookAndFeel
LookAndFeel::LookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, col::bg);
    setColour (juce::Label::textColourId, col::text);
    setColour (juce::Slider::textBoxTextColourId, col::text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxHighlightColourId, col::human.withAlpha (0.4f));
    setColour (juce::ComboBox::backgroundColourId, col::panelHi);
    setColour (juce::ComboBox::textColourId, col::text);
    setColour (juce::ComboBox::outlineColourId, col::edge);
    setColour (juce::ComboBox::arrowColourId, col::dim);
    setColour (juce::PopupMenu::backgroundColourId, col::panelHi);
    setColour (juce::PopupMenu::textColourId, col::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, col::human.withAlpha (0.35f));
    setColour (juce::PopupMenu::headerTextColourId, col::dim);
    setColour (juce::TextButton::buttonColourId, col::panelHi);
    setColour (juce::TextButton::buttonOnColourId, col::human);
    setColour (juce::TextButton::textColourOffId, col::text);
    setColour (juce::TextButton::textColourOnId, col::bg);
    setColour (juce::ToggleButton::textColourId, col::text);
    setColour (juce::TextEditor::backgroundColourId, col::panelHi);
    setColour (juce::TextEditor::textColourId, col::text);
    setColour (juce::TextEditor::outlineColourId, col::edge);
    setColour (juce::AlertWindow::backgroundColourId, col::panel);
    setColour (juce::AlertWindow::textColourId, col::text);
    setColour (juce::AlertWindow::outlineColourId, col::edge);
}

void LookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& s)
{
    // "worm-segment" knob: the arc is a row of beads that fill like a worm growing round the dial
    const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (3.0f);
    const float r = std::min (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto c = bounds.getCentre();
    const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
    const juce::Colour accent ((juce::uint32) (juce::int64) s.getProperties().getWithDefault ("accent", (juce::int64) col::human.getARGB()));
    const float zeroPos = bipolar ? (float) ((0.0 - s.getMinimum()) / (s.getMaximum() - s.getMinimum())) : 0.0f;

    const int beads = juce::jlimit (9, 21, (int) (r * 0.55f));
    const float beadR = juce::jlimit (1.6f, 3.6f, r * 0.11f);
    for (int i = 0; i < beads; ++i)
    {
        const float t = (float) i / (float) (beads - 1);
        const float a = a0 + t * (a1 - a0);
        const auto p = c.getPointOnCircumference (r - beadR, a);
        const bool lit = bipolar ? (t >= std::min (zeroPos, pos) - 1e-4f && t <= std::max (zeroPos, pos) + 1e-4f) : t <= pos + 1e-4f;
        const float br = lit ? beadR * (0.85f + 0.3f * std::sin (t * 9.0f)) : beadR * 0.7f;
        g.setColour (lit ? (s.isEnabled() ? accent : col::dim) : col::edge);
        g.fillEllipse (p.x - br, p.y - br, br * 2, br * 2);
    }

    // soft round cap
    const float inner = r - beadR * 2.0f - 3.0f;
    juce::ColourGradient grad (col::panelHi.brighter (0.25f), c.x, c.y - inner, col::panel.darker (0.2f), c.x, c.y + inner, false);
    g.setGradientFill (grad);
    g.fillEllipse (c.x - inner, c.y - inner, inner * 2, inner * 2);
    g.setColour (juce::Colours::black.withAlpha (0.25f));
    g.drawEllipse (c.x - inner, c.y - inner, inner * 2, inner * 2, 1.0f);

    // pointer: a fat little stub with a cream tip
    const float angle = a0 + pos * (a1 - a0);
    const auto from = c.getPointOnCircumference (inner * 0.2f, angle);
    const auto to = c.getPointOnCircumference (inner * 0.78f, angle);
    g.setColour (col::text);
    g.drawLine (juce::Line<float> (from, to), juce::jmax (2.0f, inner * 0.16f));
    g.fillEllipse (to.x - inner * 0.1f, to.y - inner * 0.1f, inner * 0.2f, inner * 0.2f);
}

void LookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float, juce::Slider::SliderStyle style, juce::Slider& s)
{
    const bool horiz = style == juce::Slider::LinearHorizontal || style == juce::Slider::LinearBar;
    auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    const bool blendStyle = s.getProperties().contains ("blend");
    if (! horiz) { LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, pos, 0, 0, style, s); return; }

    if (blendStyle)
    {
        // a wiggly track from pink (human) to mint (synth) with the worm's head as the thumb
        juce::Path wave;
        const float cy = b.getCentreY();
        for (float xx = b.getX(); xx <= b.getRight(); xx += 3.0f)
        {
            const float t = (xx - b.getX()) / b.getWidth();
            const float yy = cy + std::sin (t * 22.0f) * 3.0f * (0.3f + t);
            if (xx == b.getX()) wave.startNewSubPath (xx, yy); else wave.lineTo (xx, yy);
        }
        juce::ColourGradient grad (col::human, b.getX(), 0, col::synth, b.getRight(), 0, false);
        g.setGradientFill (grad);
        g.strokePath (wave, juce::PathStrokeType (7.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        const float kr = 13.0f;
        const float bt = juce::jlimit (0.0f, 1.0f, (pos - b.getX()) / juce::jmax (1.0f, b.getWidth()));
        g.setColour (bt < 0.5f ? col::human : col::synth);
        g.fillEllipse (pos - kr, cy - kr, kr * 2, kr * 2);
        g.setColour (col::bg.withAlpha (0.4f));
        g.drawEllipse (pos - kr, cy - kr, kr * 2, kr * 2, 2.0f);
        // eyes
        for (float ex : { -4.5f, 4.5f })
        {
            g.setColour (juce::Colours::white); g.fillEllipse (pos + ex - 3.2f, cy - 6.0f, 6.4f, 7.0f);
            g.setColour (col::bg); g.fillEllipse (pos + ex - 1.4f, cy - 3.6f, 2.8f, 3.2f);
        }
        g.setColour (col::bg); g.fillEllipse (pos - 3.0f, cy + 3.0f, 6.0f, 4.0f);
        return;
    }

    const float th = 6.0f;
    auto track = b.withSizeKeepingCentre (b.getWidth(), th);
    g.setColour (col::edge); g.fillRoundedRectangle (track, th * 0.5f);
    const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
    const float zero = bipolar ? track.getCentreX() : track.getX();
    g.setColour (pos >= zero ? col::human : col::synth);
    g.fillRoundedRectangle (juce::Rectangle<float> (std::min (zero, pos), track.getY(), std::abs (pos - zero), th), th * 0.5f);
    const float kr = 6.5f;
    g.setColour (col::text);
    g.fillEllipse (pos - kr, b.getCentreY() - kr, kr * 2, kr * 2);
}

void LookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const bool on = b.getToggleState();
    const float rad = r.getHeight() * 0.5f;
    g.setColour (on ? col::human : (down ? col::edge : (over ? col::panelHi.brighter (0.12f) : col::panelHi)));
    g.fillRoundedRectangle (r, rad);
    if (! on)
    {
        g.setColour (juce::Colours::white.withAlpha (0.06f));
        g.fillRoundedRectangle (r.withHeight (r.getHeight() * 0.45f).reduced (3, 1), rad);
    }
}

void LookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool over, bool)
{
    auto r = b.getLocalBounds().toFloat().reduced (2.0f);
    auto box = r.removeFromLeft (r.getHeight()).reduced (3.0f);
    g.setColour (b.getToggleState() ? col::human : (over ? col::edge.brighter (0.2f) : col::edge));
    g.fillRoundedRectangle (box, 4.0f);
    if (b.getToggleState())
    {
        g.setColour (col::bg);
        g.drawLine (box.getX() + box.getWidth() * 0.25f, box.getCentreY(), box.getCentreX(), box.getBottom() - box.getHeight() * 0.28f, 2.0f);
        g.drawLine (box.getCentreX(), box.getBottom() - box.getHeight() * 0.28f, box.getRight() - box.getWidth() * 0.2f, box.getY() + box.getHeight() * 0.25f, 2.0f);
    }
    g.setColour (col::text);
    g.setFont (font (13.0f));
    g.drawText (b.getButtonText(), r.withTrimmedLeft (6), juce::Justification::centredLeft);
}

void LookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& cb)
{
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    g.setColour (cb.findColour (juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (col::edge);
    g.drawRoundedRectangle (r, 5.0f, 1.0f);
    juce::Path arrow;
    const float ax = (float) w - 14.0f, ay = (float) h * 0.5f;
    arrow.addTriangle (ax - 4, ay - 2, ax + 4, ay - 2, ax, ay + 3);
    g.setColour (col::dim);
    g.fillPath (arrow);
}

// =========================================================================== controls
Knob::Knob (VowelWormProcessor& p, int paramIndex, const juce::String& label, int modDest, bool large)
    : proc (p), dest (modDest), big (large)
{
    const auto& d = paramTable()[(size_t) paramIndex];
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 16);
    slider.setPopupDisplayEnabled (false, false, nullptr);
    slider.setColour (juce::Slider::textBoxTextColourId, col::dim);
    addAndMakeVisible (slider);
    name.setText (label.isNotEmpty() ? label : juce::String (d.name), juce::dontSendNotification);
    name.setJustificationType (juce::Justification::centred);
    name.setFont (font (big ? 14.0f : 12.0f, big));
    name.setColour (juce::Label::textColourId, col::text);
    name.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (name);
    att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (p.apvts, d.id, slider);
    if (d.type == PType::Int && ! d.unit.empty()) slider.setTextValueSuffix (" " + juce::String (d.unit));
    if (dest < 0) dest = destForParam (paramIndex);
}

void Knob::resized()
{
    auto r = getLocalBounds();
    if (compact)
    {
        name.setVisible (false);
        slider.setSliderStyle (juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        slider.setPopupDisplayEnabled (true, true, getTopLevelComponent());
        slider.setBounds (r);
        return;
    }
    name.setBounds (r.removeFromTop (big ? 20 : 16));
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, r.getWidth(), 15);
    slider.setBounds (r);
}

void Knob::refreshMod()
{
    if (dest < 0) return;
    const float off = proc.engine.telemetry.destOffset[dest].load (std::memory_order_relaxed);
    if (std::abs (off - lastOffset) > 0.002f) { lastOffset = off; repaint(); }
}

void Knob::paintOverChildren (juce::Graphics& g)
{
    if (compact || dest < 0 || std::abs (lastOffset) < 0.004f) return;
    auto sb = slider.getBounds().toFloat();
    sb = sb.withTrimmedBottom (compact ? 0 : 15).reduced (4.0f);
    const float r = std::min (sb.getWidth(), sb.getHeight()) * 0.5f + 1.0f;
    const auto c = sb.getCentre();
    const auto rp = slider.getRotaryParameters();
    const float base = (float) slider.valueToProportionOfLength (slider.getValue());
    const float tgt = juce::jlimit (0.0f, 1.0f, base + lastOffset);
    const float aA = rp.startAngleRadians + base * (rp.endAngleRadians - rp.startAngleRadians);
    const float aB = rp.startAngleRadians + tgt * (rp.endAngleRadians - rp.startAngleRadians);
    juce::Path arc; arc.addCentredArc (c.x, c.y, r, r, 0, std::min (aA, aB), std::max (aA, aB), true);
    g.setColour (col::synth);
    g.strokePath (arc, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    juce::Point<float> dot (c.x + std::sin (aB) * r, c.y - std::cos (aB) * r);
    g.fillEllipse (dot.x - 3, dot.y - 3, 6, 6);
}

Choice::Choice (VowelWormProcessor& p, int paramIndex, const juce::String& label)
{
    const auto& d = paramTable()[(size_t) paramIndex];
    int id = 1;
    for (const auto& c : d.choices) box.addItem (c, id++);
    addAndMakeVisible (box);
    name.setText (label.isNotEmpty() ? label : juce::String (d.name), juce::dontSendNotification);
    name.setFont (font (12.0f));
    name.setColour (juce::Label::textColourId, col::dim);
    if (label != " ") addAndMakeVisible (name);
    att = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, d.id, box);
}

void Choice::resized()
{
    auto r = getLocalBounds();
    if (name.isVisible()) name.setBounds (r.removeFromTop (16));
    box.setBounds (r.withSizeKeepingCentre (r.getWidth(), std::min (r.getHeight(), 26)));
}

Toggle::Toggle (VowelWormProcessor& p, int paramIndex, const juce::String& label)
{
    const auto& d = paramTable()[(size_t) paramIndex];
    button.setButtonText (label.isNotEmpty() ? label : juce::String (d.name));
    addAndMakeVisible (button);
    att = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, d.id, button);
}

void Section::paint (juce::Graphics& g)
{
    drawPebble (g, getLocalBounds().toFloat(), title);
}

void Section::resized()
{
    auto r = getLocalBounds().reduced (10);
    if (title.isNotEmpty()) r.removeFromTop (18);
    int x = r.getX(), y = r.getY();
    for (auto& c : items)
    {
        const bool wide = dynamic_cast<Choice*> (c.get()) != nullptr || dynamic_cast<Toggle*> (c.get()) != nullptr;
        const int w = wide ? cellW * 2 - 8 : cellW;
        const int h = wide ? 46 : cellH;
        if (x + w > r.getRight()) { x = r.getX(); y += cellH + 4; }
        c->setBounds (x, y + (wide ? (cellH - h) / 2 : 0), w, h);
        x += w + 6;
    }
}

// =========================================================================== displays
void LevelMeter::setLevel (float peak)
{
    const float db = gainToDb (peak);
    const float n = juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f);
    level = std::max (n, level * 0.86f);
    if (n >= hold) { hold = n; holdFrames = 30; } else if (--holdFrames < 0) hold = std::max (0.0f, hold - 0.01f);
    if (peak >= 0.999f) clipped = true;
    repaint();
}

void LevelMeter::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    auto lab = r.removeFromBottom (16);
    g.setColour (col::dim); g.setFont (font (11.0f, true));
    g.drawText (label.toLowerCase(), lab, juce::Justification::centred);
    auto bar = r.withSizeKeepingCentre (std::min (16.0f, r.getWidth()), r.getHeight());
    // segmented "worm" meter: beads stacked from the bottom
    const int segs = 24;
    const float sh = bar.getHeight() / (float) segs;
    for (int i = 0; i < segs; ++i)
    {
        const float t = (float) (i + 1) / (float) segs;
        auto seg = juce::Rectangle<float> (bar.getX(), bar.getBottom() - sh * (float) (i + 1), bar.getWidth(), sh).reduced (1.0f, 1.0f);
        const bool lit = t <= level + 1e-4f;
        const juce::Colour c = t > 0.95f ? col::clip : (t > 0.8f ? col::warn : col::ok);
        g.setColour (lit ? c : col::edge.withAlpha (0.6f));
        g.fillRoundedRectangle (seg, seg.getHeight() * 0.5f);
    }
    const float hy = bar.getBottom() - bar.getHeight() * hold;
    g.setColour (col::text); g.fillRoundedRectangle (bar.getX() - 2, hy - 1, bar.getWidth() + 4, 2.0f, 1.0f);
    if (clipped) { g.setColour (col::clip); g.fillEllipse (bar.getCentreX() - 4, bar.getY() - 10, 8, 8); }
}

void PitchDisplay::update (float h, float c)
{
    if (std::abs (h - hz) > 0.05f || std::abs (c - conf) > 0.02f) { hz = h; conf = c; repaint(); }
}

void PitchDisplay::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (col::panelHi); g.fillRoundedRectangle (r, 12.0f);
    r = r.reduced (12, 7);
    g.setColour (col::dim); g.setFont (font (11.5f, true));
    g.drawText ("you're singing", r.removeFromTop (15), juce::Justification::centredLeft);
    const bool voiced = hz > 0;
    const float midi = voiced ? hzToMidi (hz) : 0;
    const int nn = (int) std::lround (midi);
    const float cents = (midi - (float) nn) * 100.0f;
    g.setColour (voiced ? col::text : col::dim);
    g.setFont (font (34.0f, true));
    g.drawText (voiced ? noteName (nn) : juce::String ("hmm?"), r.removeFromTop (38), juce::Justification::centredLeft);
    // tuning: a little worm sliding along a rail, centred = in tune
    auto rail = r.removeFromTop (16);
    g.setColour (col::edge); g.fillRoundedRectangle (rail.withSizeKeepingCentre (rail.getWidth(), 4.0f), 2.0f);
    g.setColour (col::ok.withAlpha (0.5f)); g.fillRect (rail.getCentreX() - 1.0f, rail.getY() + 2, 2.0f, rail.getHeight() - 4);
    if (voiced)
    {
        const float x = rail.getCentreX() + juce::jlimit (-1.0f, 1.0f, cents / 50.0f) * rail.getWidth() * 0.5f;
        g.setColour (std::abs (cents) < 15 ? col::ok : col::warn);
        g.fillEllipse (x - 6, rail.getCentreY() - 6, 12, 12);
    }
    g.setColour (col::dim); g.setFont (font (10.5f));
    g.drawText (voiced ? juce::String (cents >= 0 ? "+" : "") + juce::String ((int) std::lround (cents)) + " ct  ·  " + juce::String (hz, 1) + " Hz" : juce::String ("(no clear pitch)"),
                r.removeFromTop (14), juce::Justification::centredLeft);
    r.removeFromTop (3);
    auto bar = r.removeFromTop (5);
    g.setColour (col::bg); g.fillRoundedRectangle (bar, 2.5f);
    g.setColour (col::human.interpolatedWith (col::ok, conf)); g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * conf), 2.5f);
}

void KeyStrip::update (const Telemetry& t)
{
    std::array<uint32_t, 4> h; for (int i = 0; i < 4; ++i) h[(size_t) i] = t.heldNotes[i].load();
    std::array<int, kMaxVoices> v; for (int i = 0; i < kMaxVoices; ++i) v[(size_t) i] = t.voiceNote[i].load();
    if (h != held || v != voiceNotes) { held = h; voiceNotes = v; repaint(); }
}

void KeyStrip::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    drawPebble (g, r);
    r = r.reduced (10, 7);
    auto lab = r.removeFromLeft (64);
    g.setColour (col::dim); g.setFont (font (12.0f, true));
    g.drawText ("midi in", lab, juce::Justification::centredLeft);
    const int lo = 24, hi = 108; // C1..C8
    int whites = 0; for (int n = lo; n <= hi; ++n) if (! juce::MidiMessage::isMidiNoteBlack (n)) ++whites;
    const float ww = r.getWidth() / (float) whites;
    auto isHeld = [&] (int n) { return (held[(size_t) (n >> 5)] >> (n & 31)) & 1u; };
    auto isVoice = [&] (int n) { for (int v : voiceNotes) if (v == n) return true; return false; };
    int wi = 0;
    std::vector<std::pair<juce::Rectangle<float>, int>> blacks;
    for (int n = lo; n <= hi; ++n)
    {
        if (juce::MidiMessage::isMidiNoteBlack (n)) { blacks.push_back ({ { r.getX() + wi * ww - ww * 0.32f, r.getY(), ww * 0.64f, r.getHeight() * 0.6f }, n }); continue; }
        juce::Rectangle<float> k (r.getX() + wi * ww + 0.5f, r.getY(), ww - 1.5f, r.getHeight());
        g.setColour (isHeld (n) ? col::human : juce::Colour (0xfff2e2ca));
        g.fillRoundedRectangle (k, 3.0f);
        if (isVoice (n)) { g.setColour (col::synth.darker (0.15f)); g.fillEllipse (k.getCentreX() - 3.5f, k.getBottom() - 10, 7, 7); }
        if (n % 12 == 0) { g.setColour (col::dim.darker (0.3f)); g.setFont (font (8.5f)); g.drawText ("C" + juce::String (n / 12 - 1), k.withTrimmedTop (k.getHeight() - 22).withTrimmedBottom (10), juce::Justification::centred); }
        ++wi;
    }
    for (auto& [k, n] : blacks)
    {
        g.setColour (isHeld (n) ? col::wormDark : col::bg);
        g.fillRoundedRectangle (k, 2.5f);
        if (isVoice (n)) { g.setColour (col::synth); g.fillEllipse (k.getCentreX() - 2.5f, k.getBottom() - 8, 5, 5); }
    }
}

void VowelPad::update (float x, float y, float c, float a, float b)
{
    ++frame;
    const bool moved = std::abs (x - px) > 0.001f || std::abs (y - py) > 0.001f;
    if (moved)
    {
        const juce::Point<float> d (x - px, py - y);
        const float len = d.getDistanceFromOrigin();
        if (len > 1e-4f) look = look * 0.7f + (d / len) * 0.3f;
    }
    px = x; py = y; conf = c; f1 = a; f2 = b;
    if (c > 0.5f || trailCount == 0)
    {
        trail[(size_t) trailPos] = { x, y };
        trailPos = (trailPos + 1) % (int) trail.size();
        trailCount = std::min (trailCount + 1, (int) trail.size());
    }
    repaint(); // the worm breathes, so it repaints every frame (one small component)
}

void VowelPad::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    drawPebble (g, r, "vowel space");
    g.setColour (col::dim); g.setFont (font (10.5f));
    g.drawText ("F1 " + juce::String ((int) f1) + "   F2 " + juce::String ((int) f2), r.reduced (14, 7).removeFromTop (17), juce::Justification::centredRight);

    auto pad = r.reduced (30, 32).withTrimmedTop (4);
    // soil strata instead of a grid
    for (int i = 0; i < 5; ++i)
    {
        const float yy = pad.getY() + pad.getHeight() * (0.1f + 0.2f * (float) i);
        juce::Path strata;
        for (float xx = pad.getX(); xx <= pad.getRight(); xx += 6.0f)
        {
            const float w = std::sin (xx * 0.035f + (float) i * 1.7f) * 3.0f;
            if (xx == pad.getX()) strata.startNewSubPath (xx, yy + w); else strata.lineTo (xx, yy + w);
        }
        g.setColour (col::edge.withAlpha (0.45f));
        g.strokePath (strata, juce::PathStrokeType (1.0f));
    }
    auto toPt = [&] (float x, float y) { return juce::Point<float> (pad.getX() + x * pad.getWidth(), pad.getBottom() - y * pad.getHeight()); };
    struct L { const char* t; float x, y; };
    g.setFont (font (14.0f, true));
    for (L l : { L { "oo", 0.05f, 0.1f }, L { "oh", 0.08f, 0.62f }, L { "ah", 0.33f, 0.92f }, L { "eh", 0.78f, 0.55f }, L { "ee", 0.95f, 0.1f } })
    {
        const auto p = toPt (l.x, l.y);
        g.setColour (col::panelHi); g.fillEllipse (p.x - 15, p.y - 10, 30, 20);
        g.setColour (col::dim.withAlpha (0.8f));
        g.drawText (l.t, juce::Rectangle<float> (30, 20).withCentre (p), juce::Justification::centred);
    }
    g.setColour (col::dim); g.setFont (font (10.5f));
    g.drawText ("back", juce::Rectangle<float> (pad.getX(), pad.getBottom() + 6, 60, 14), juce::Justification::centredLeft);
    g.drawText ("front", juce::Rectangle<float> (pad.getRight() - 60, pad.getBottom() + 6, 60, 14), juce::Justification::centredRight);
    g.saveState();
    g.addTransform (juce::AffineTransform::rotation (-juce::MathConstants<float>::halfPi, pad.getX() - 16, pad.getCentreY()));
    g.drawText ("closed  -  open", juce::Rectangle<float> (pad.getX() - 16 - 60, pad.getCentreY() - 7, 120, 14), juce::Justification::centred);
    g.restoreState();

    // ---- the worm ------------------------------------------------------------
    const bool awake = conf > 0.5f;
    const float breathe = std::sin ((float) frame * 0.12f);
    const int n = trailCount;
    // continuous tapering body: stroke each segment with a width that grows towards the head
    auto ptAt = [&] (int back) { const int idx = (trailPos - 1 - back + (int) trail.size() * 2) % (int) trail.size(); return toPt (trail[(size_t) idx].x, trail[(size_t) idx].y); };
    for (int pass = 0; pass < 2; ++pass)
        for (int i = n - 1; i >= 1; --i)
        {
            const float t = 1.0f - (float) i / (float) n;   // 0 tail .. 1 head
            const float w = 5.0f + 19.0f * t + (pass == 0 ? 3.0f : 0.0f);
            const float a = awake ? 0.45f + 0.55f * t : 0.3f + 0.3f * t;
            g.setColour ((pass == 0 ? col::wormDark : (i % 5 == 0 ? col::wormDark.brighter (0.2f) : col::human)).withAlpha (a));
            g.drawLine (juce::Line<float> (ptAt (i), ptAt (i - 1)), w);
            g.fillEllipse (ptAt (i).x - w * 0.5f, ptAt (i).y - w * 0.5f, w, w);
        }
    const auto head = toPt (px, py);
    const float hr = 17.0f + (awake ? breathe * 0.8f : 0.0f);
    g.setColour (juce::Colours::black.withAlpha (0.25f));
    g.fillEllipse (head.x - hr + 2, head.y - hr + 3, hr * 2, hr * 2);
    g.setColour (awake ? col::human : col::human.withSaturation (0.3f));
    g.fillEllipse (head.x - hr, head.y - hr, hr * 2, hr * 2);
    g.setColour (juce::Colours::white.withAlpha (0.25f));
    g.fillEllipse (head.x - hr * 0.6f, head.y - hr * 0.8f, hr * 0.7f, hr * 0.45f);

    // eyes look where the voice is heading; blink now and then
    const bool blink = (frame % 97) < 3;
    for (float side : { -1.0f, 1.0f })
    {
        const juce::Point<float> e (head.x + side * hr * 0.38f, head.y - hr * 0.3f);
        if (! awake || blink)
        {
            g.setColour (col::bg); g.drawLine (e.x - 3.5f, e.y, e.x + 3.5f, e.y, 1.8f);
            continue;
        }
        g.setColour (juce::Colours::white); g.fillEllipse (e.x - 4.5f, e.y - 5.0f, 9.0f, 10.0f);
        const auto pupil = e + look * 1.8f;
        g.setColour (col::bg); g.fillEllipse (pupil.x - 2.2f, pupil.y - 2.4f, 4.4f, 4.8f);
    }
    // the mouth is the vowel: wide for "ee", round for "oo", tall for "ah"
    const float mw = awake ? 4.0f + 13.0f * px : 6.0f;
    const float mh = awake ? 3.0f + 12.0f * py : 1.5f;
    const juce::Point<float> m (head.x, head.y + hr * 0.38f);
    g.setColour (col::bg);
    g.fillEllipse (m.x - mw * 0.5f, m.y - mh * 0.5f, mw, mh);
    if (awake && mh > 7.0f) { g.setColour (col::clip.withAlpha (0.8f)); g.fillEllipse (m.x - mw * 0.3f, m.y + mh * 0.05f, mw * 0.6f, mh * 0.4f); }
    if (! awake)
    {
        g.setColour (col::dim); g.setFont (font (13.0f, true));
        const float zz = (float) (frame % 60) / 60.0f;
        g.drawText ("z", juce::Rectangle<float> (head.x + hr * 0.6f, head.y - hr - 12 - zz * 10, 14, 14), juce::Justification::centred);
        g.setFont (font (10.0f, true));
        g.drawText ("z", juce::Rectangle<float> (head.x + hr * 1.1f, head.y - hr - 22 - zz * 10, 12, 12), juce::Justification::centred);
    }
}

void VoiceStrip::update (const Telemetry& t, float b)
{
    bool changed = std::abs (b - blend) > 0.01f;
    blend = b;
    const int nv = t.numVoices.load();
    if (nv != numVoices) { numVoices = nv; changed = true; }
    for (int i = 0; i < kMaxVoices; ++i)
    {
        const int n = t.voiceNote[i].load(); const float l = t.voiceLevel[i].load();
        if (n != notes[(size_t) i] || std::abs (l - levels[(size_t) i]) > 0.01f) changed = true;
        notes[(size_t) i] = n; levels[(size_t) i] = l;
    }
    if (changed) repaint();
}

void VoiceStrip::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    drawPebble (g, r, "the choir");
    auto cells = r.reduced (12, 8).withTrimmedTop (20);
    const float cw = cells.getWidth() / (float) kMaxVoices;
    for (int i = 0; i < kMaxVoices; ++i)
    {
        auto c = juce::Rectangle<float> (cells.getX() + cw * (float) i, cells.getY(), cw, cells.getHeight()).reduced (3.0f, 0);
        const bool enabled = i < numVoices;
        const bool on = notes[(size_t) i] >= 0;
        const float lv = juce::jlimit (0.0f, 1.0f, levels[(size_t) i]);
        auto face = c.withHeight (c.getWidth()).withCentre ({ c.getCentreX(), c.getY() + c.getWidth() * 0.5f });
        if (! enabled)
        {
            g.setColour (col::edge.withAlpha (0.5f)); g.fillEllipse (face.reduced (face.getWidth() * 0.25f));
            continue;
        }
        // a round singer: pink half (voice) and mint half (synth), split by the blend
        g.saveState();
        juce::Path circle; circle.addEllipse (face);
        g.reduceClipRegion (circle);
        g.setColour (on ? col::human : col::panelHi); g.fillRect (face);
        if (on) { g.setColour (col::synth); g.fillRect (face.withTrimmedLeft (face.getWidth() * (1.0f - blend))); }
        g.restoreState();
        g.setColour (col::bg.withAlpha (0.6f));
        // eyes
        for (float sx : { -0.2f, 0.2f })
            g.fillEllipse (face.getCentreX() + sx * face.getWidth() - 1.8f, face.getY() + face.getHeight() * 0.33f - 2.0f, 3.6f, on ? 4.0f : 1.5f);
        // mouth opens with the voice's level
        const float mh = on ? 2.0f + face.getHeight() * 0.32f * lv : 1.5f;
        const float mw = face.getWidth() * 0.32f;
        g.setColour (col::bg);
        g.fillEllipse (face.getCentreX() - mw * 0.5f, face.getY() + face.getHeight() * 0.62f - mh * 0.3f, mw, mh);
        g.setColour (on ? col::text : col::dim);
        g.setFont (font (on ? 12.5f : 11.0f, on));
        g.drawText (on ? noteName (notes[(size_t) i]) : juce::String (i + 1), c.withTop (face.getBottom() + 1), juce::Justification::centredTop);
    }
}

void ExpressionMeters::update (const Telemetry& t)
{
    std::array<float, 7> n { t.amplitude.load(), t.attack.load(), t.brightness.load(), t.breath.load(),
                             0.5f + 0.5f * t.pitchMove.load(), t.vibrato.load(), t.confidence.load() };
    bool ch = false; for (size_t i = 0; i < n.size(); ++i) ch |= std::abs (n[i] - v[i]) > 0.005f;
    if (ch) { v = n; repaint(); }
}

void ExpressionMeters::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    drawPebble (g, r, "how you are singing");
    auto rows = r.reduced (14, 8).withTrimmedTop (20);
    static const char* names[] = { "loudness", "attack", "brightness", "breath", "pitch wobble", "vibrato", "voicing" };
    const float rh = rows.getHeight() / 7.0f;
    for (int i = 0; i < 7; ++i)
    {
        auto row = rows.removeFromTop (rh);
        g.setColour (col::text); g.setFont (font (12.0f));
        g.drawText (names[i], row.removeFromLeft (92), juce::Justification::centredLeft);
        auto bar = row.withSizeKeepingCentre (row.getWidth(), 8.0f);
        g.setColour (col::bg.withAlpha (0.7f)); g.fillRoundedRectangle (bar, 4.0f);
        g.setColour (i == 4 ? col::synth : (i == 3 ? col::warn : col::human));
        if (i == 4)
        {
            const float c = bar.getCentreX(), x = bar.getX() + bar.getWidth() * v[(size_t) i];
            g.fillRoundedRectangle (juce::Rectangle<float> (std::min (c, x) - 4, bar.getY(), std::abs (x - c) + 8, 8.0f), 4.0f);
        }
        else g.fillRoundedRectangle (bar.withWidth (juce::jmax (8.0f, bar.getWidth() * juce::jlimit (0.0f, 1.0f, v[(size_t) i]))), 4.0f);
    }
}

void StatusPills::update (float inPeak, int midiCount, int heldCount, bool v)
{
    const bool a = inPeak > dbToGain (-50.0f);
    if (midiCount != lastMidi) { midiFlash = 6; lastMidi = midiCount; }
    else if (midiFlash > 0) --midiFlash;
    if (a != audio || v != voiced || heldCount != held || midiFlash >= 5 || midiFlash == 0) { audio = a; voiced = v; held = heldCount; repaint(); }
}

void StatusPills::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    const float w = (r.getWidth() - 12) / 3.0f;
    auto pill = [&] (juce::Rectangle<float> b, const juce::String& t, bool on, juce::Colour c)
    {
        g.setColour (on ? c : col::panelHi); g.fillRoundedRectangle (b, b.getHeight() * 0.5f);
        g.setColour (on ? col::bg : col::dim); g.setFont (font (11.0f, true));
        g.drawText (t, b, juce::Justification::centred);
    };
    pill (r.removeFromLeft (w), audio ? "hearing you" : "no audio", audio, col::ok); r.removeFromLeft (6);
    pill (r.removeFromLeft (w), held > 0 ? juce::String (held) + (held == 1 ? " note" : " notes") : juce::String ("no midi"), midiFlash > 0 || held > 0, col::human); r.removeFromLeft (6);
    pill (r.removeFromLeft (w), voiced ? "singing" : "not singing", voiced, col::synth);
}

void WormTitle::update (float openness, float frontness, float level)
{
    ++t;
    open += 0.35f * (openness - open);
    front += 0.35f * (frontness - front);
    lvl += 0.3f * (level - lvl);
    repaint();
}

void WormTitle::paint (juce::Graphics& g)
{
    // "vowelworm" with both o's drawn as little mouths that sing the vowel you sing
    const auto f = font (30.0f, true);
    g.setFont (f);
    const juce::String parts[] = { "v", "o", "welw", "o", "rm" };
    float x = 2.0f;
    const float baseY = (float) getHeight() * 0.5f;
    int letter = 0;
    for (int i = 0; i < 5; ++i)
    {
        const bool mouth = (i == 1 || i == 3);
        if (mouth)
        {
            const float w = juce::GlyphArrangement::getStringWidth (f, "o");
            const float bob = std::sin ((float) t * 0.25f + (float) letter) * 2.5f * lvl;
            const juce::Point<float> c (x + w * 0.5f, baseY + 3.0f + bob);
            const float R = w * 0.52f;
            g.setColour (col::human);
            g.fillEllipse (c.x - R, c.y - R, R * 2, R * 2);
            const float mw = R * (0.5f + 0.9f * front) * (0.4f + 0.6f * juce::jlimit (0.0f, 1.0f, lvl * 1.5f));
            const float mh = R * (0.25f + 1.0f * open) * (0.3f + 0.7f * juce::jlimit (0.0f, 1.0f, lvl * 1.5f));
            g.setColour (col::bg);
            g.fillEllipse (c.x - mw * 0.5f, c.y - mh * 0.5f, mw, mh);
            x += w; ++letter;
            continue;
        }
        for (auto ch : parts[i])
        {
            const juce::String one = juce::String::charToString (ch);
            const float w = juce::GlyphArrangement::getStringWidth (f, one);
            const float bob = std::sin ((float) t * 0.25f + (float) letter) * 2.5f * lvl;
            g.setColour (col::text);
            g.drawText (one, juce::Rectangle<float> (x, bob, w + 2, (float) getHeight()), juce::Justification::centredLeft);
            x += w; ++letter;
        }
    }
}

// =========================================================================== pages
class MainPage : public Page
{
public:
    explicit MainPage (VowelWormProcessor& p) : proc (p)
    {
        for (auto* c : std::initializer_list<juce::Component*> { &inMeter, &outMeter, &pitch, &keys, &pad, &voices, &expr, &blendPanel })
            addAndMakeVisible (c);
        blend.setSliderStyle (juce::Slider::LinearHorizontal);
        blend.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        blend.getProperties().set ("blend", true);
        blendPanel.addAndMakeVisible (blend);
        blendAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (p.apvts, "blend", blend);
        knobs.push_back (std::make_unique<Knob> (p, P::formantShift, "Formant", -1, true));
        knobs.push_back (std::make_unique<Knob> (p, P::harmonyLevel, "Harmony", -1, true));
        knobs.push_back (std::make_unique<Knob> (p, P::glide, "Glide", -1, true));
        knobs.push_back (std::make_unique<Knob> (p, P::outputLevel, "Output", -1, true));
        for (auto& k : knobs) addAndMakeVisible (*k);
        for (int m = 0; m < kNumMacros; ++m)
        {
            macros.push_back (std::make_unique<Knob> (p, P::macro0 + m, "MACRO " + juce::String (m + 1), -1, true));
            macros.back()->slider.getProperties().set ("accent", (juce::int64) col::synth.getARGB());
            addAndMakeVisible (*macros.back());
            auto l = std::make_unique<juce::Label>();
            l->setJustificationType (juce::Justification::centredTop);
            l->setFont (font (10.5f));
            l->setColour (juce::Label::textColourId, col::dim);
            addAndMakeVisible (*l);
            macroLabels.push_back (std::move (l));
        }
        voicesChoice = std::make_unique<Choice> (p, P::mode, "Mode");
        addAndMakeVisible (*voicesChoice);
    }

    void paint (juce::Graphics& g) override
    {
        drawPebble (g, leftPanel.toFloat(), "input");
        drawPebble (g, knobPanel.toFloat(), "sound");
        drawPebble (g, macroPanel.toFloat(), "performance macros");
        auto b = blendPanel.getBounds().toFloat();
        drawPebble (g, b);
        g.setFont (font (13.0f, true));
        auto lab = b.reduced (16, 8).removeFromTop (17);
        g.setColour (col::human); g.drawText ("human", lab, juce::Justification::centredLeft);
        g.setColour (col::text.interpolatedWith (col::synth, 0.5f)); g.drawText ("hybrid", lab, juce::Justification::centred);
        g.setColour (col::synth); g.drawText ("synthetic", lab, juce::Justification::centredRight);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (12);
        auto bottom = r.removeFromBottom (168);
        macroPanel = bottom;
        r.removeFromBottom (8);
        auto keyRow = r.removeFromBottom (56);
        keys.setBounds (keyRow);
        r.removeFromBottom (8);

        leftPanel = r.removeFromLeft (210);
        {
            auto lp = leftPanel.reduced (10).withTrimmedTop (20);
            auto meters = lp.removeFromLeft (84);
            inMeter.setBounds (meters.removeFromLeft (42));
            outMeter.setBounds (meters);
            lp.removeFromLeft (6);
            pitch.setBounds (lp.removeFromTop (124));
            lp.removeFromTop (10);
            voicesChoice->setBounds (lp.removeFromTop (46));
        }
        r.removeFromLeft (8);
        auto centre = r.removeFromLeft (360);
        pad.setBounds (centre.removeFromTop (centre.getHeight() - 92));
        centre.removeFromTop (8);
        voices.setBounds (centre);
        r.removeFromLeft (8);

        blendPanel.setBounds (r.removeFromTop (74));
        blend.setBounds (blendPanel.getLocalBounds().reduced (14, 8).withTrimmedTop (22));
        r.removeFromTop (8);
        knobPanel = r.removeFromTop (128);
        {
            auto kp = knobPanel.reduced (10).withTrimmedTop (16);
            const int kw = kp.getWidth() / (int) knobs.size();
            for (auto& k : knobs) k->setBounds (kp.removeFromLeft (kw).reduced (4, 0));
        }
        r.removeFromTop (8);
        expr.setBounds (r);

        auto mp = macroPanel.reduced (10).withTrimmedTop (16);
        const int mw = mp.getWidth() / kNumMacros;
        for (int m = 0; m < kNumMacros; ++m)
        {
            auto cell = mp.removeFromLeft (mw).reduced (8, 0);
            macroLabels[(size_t) m]->setBounds (cell.removeFromBottom (44));
            macros[(size_t) m]->setBounds (cell.withSizeKeepingCentre (std::min (cell.getWidth(), 120), cell.getHeight()));
        }
    }

    void tick() override
    {
        auto& t = proc.engine.telemetry;
        inMeter.setLevel (inPeak);
        outMeter.setLevel (outPeak);
        pitch.update (t.pitchHz.load(), t.confidence.load());
        keys.update (t);
        pad.update (t.vowelX.load(), t.vowelY.load(), t.confidence.load(), t.f1.load(), t.f2.load());
        voices.update (t, t.destValue[(int) ModDest::SynthBlend].load());
        expr.update (t);
        for (auto& k : knobs) k->refreshMod();

        if (++labelTick % 10 == 0)
            for (int m = 0; m < kNumMacros; ++m)
            {
                juce::StringArray parts;
                for (int s = 0; s < kMacroSlots; ++s)
                {
                    const int d = (int) proc.apvts.getRawParameterValue (paramTable()[(size_t) (P::macroDst0 + m * kMacroSlots + s)].id)->load();
                    const float a = proc.apvts.getRawParameterValue (paramTable()[(size_t) (P::macroAmt0 + m * kMacroSlots + s)].id)->load();
                    if (d > 0 && std::abs (a) > 0.001f)
                        parts.add (juce::String (modDestNames()[(size_t) d]) + " " + (a > 0 ? "+" : "") + juce::String ((int) std::lround (a * 100)) + "%");
                }
                macroLabels[(size_t) m]->setText (parts.isEmpty() ? juce::String ("(unassigned)") : parts.joinIntoString ("\n"), juce::dontSendNotification);
            }
    }

private:
    VowelWormProcessor& proc;
    LevelMeter inMeter { "IN" }, outMeter { "OUT" };
    PitchDisplay pitch;
    KeyStrip keys;
    VowelPad pad;
    VoiceStrip voices;
    ExpressionMeters expr;
    juce::Component blendPanel;
    juce::Slider blend;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> blendAtt;
    std::vector<std::unique_ptr<Knob>> knobs, macros;
    std::vector<std::unique_ptr<juce::Label>> macroLabels;
    std::unique_ptr<Choice> voicesChoice;
    juce::Rectangle<int> leftPanel, knobPanel, macroPanel;
    int labelTick = 0;
};

/** Generic page of sections; each section lists parameter indices. */
class GridPage : public Page
{
public:
    struct Spec { juce::String title; std::vector<int> params; int x, y, w, h; };
    GridPage (VowelWormProcessor& p, std::vector<Spec> specs) : proc (p)
    {
        for (auto& s : specs)
        {
            auto sec = std::make_unique<Section> (s.title);
            for (int pi : s.params)
            {
                const auto& d = paramTable()[(size_t) pi];
                if (d.type == PType::Choice) sec->add (std::make_unique<Choice> (p, pi));
                else if (d.type == PType::Bool) sec->add (std::make_unique<Toggle> (p, pi));
                else knobs.push_back (sec->add (std::make_unique<Knob> (p, pi)));
            }
            addAndMakeVisible (*sec);
            layout.push_back ({ s.x, s.y, s.w, s.h });
            sections.push_back (std::move (sec));
        }
    }
    void resized() override
    {
        for (size_t i = 0; i < sections.size(); ++i) sections[i]->setBounds (layout[i].translated (12, 12));
    }
    void tick() override { for (auto* k : knobs) k->refreshMod(); }

protected:
    VowelWormProcessor& proc;
    std::vector<std::unique_ptr<Section>> sections;
    std::vector<juce::Rectangle<int>> layout;
    std::vector<Knob*> knobs;
};

class HarmonyPage : public GridPage
{
public:
    explicit HarmonyPage (VowelWormProcessor& p)
        : GridPage (p, {
            { "Harmony", { P::mode, P::latch, P::voices, P::octave, P::relRoot, P::glide, P::bendRange, P::velSens }, 0, 0, 500, 214 },
            { "Levels & Envelope", { P::leadOn, P::harmonyLevel, P::leadLevel, P::spread, P::harmonyAttack, P::harmonyRelease, P::consonantLevel }, 508, 0, 508, 214 },
            { "Formant & Expression", { P::formantShift, P::formantFollow, P::formantEmphasis, P::expressTransfer, P::vibratoDepth, P::vibratoRate }, 0, 220, 1016, 122 } })
    {
        perVoice = std::make_unique<Section> ("Per-voice (ensemble)");
        static const int rows[] = { P::vLevel0, P::vPan0, P::vFormant0, P::vBlend0, P::vGlide0, P::vDetune0 };
        static const char* rowNames[] = { "Level", "Pan", "Formant", "Synth", "Glide x", "Detune" };
        for (int r = 0; r < 6; ++r)
        {
            auto l = std::make_unique<juce::Label> (juce::String(), rowNames[r]);
            l->setFont (font (12.0f)); l->setColour (juce::Label::textColourId, col::dim);
            perVoice->addAndMakeVisible (*l);
            rowLabels.push_back (std::move (l));
            for (int v = 0; v < kMaxVoices; ++v)
            {
                auto k = std::make_unique<Knob> (p, rows[r] + v, " ");
                k->compact = true;
                k->slider.setTooltip (paramTable()[(size_t) (rows[r] + v)].name);
                k->name.setVisible (false);
                perVoice->addAndMakeVisible (*k);
                small.push_back (std::move (k));
            }
        }
        for (int v = 0; v < kMaxVoices; ++v)
        {
            auto l = std::make_unique<juce::Label> (juce::String(), juce::String (v + 1));
            l->setJustificationType (juce::Justification::centred);
            l->setFont (font (12.0f, true)); l->setColour (juce::Label::textColourId, col::text);
            perVoice->addAndMakeVisible (*l);
            colLabels.push_back (std::move (l));
        }
        addAndMakeVisible (*perVoice);
    }
    void resized() override
    {
        GridPage::resized();
        perVoice->setBounds (12, 12 + 348, 1016, 232);
        auto r = perVoice->getLocalBounds().reduced (10).withTrimmedTop (18);
        auto head = r.removeFromTop (16);
        head.removeFromLeft (70);
        const int cw = head.getWidth() / kMaxVoices;
        // knobs: name label hidden, value text hidden -> pure dial, value shown on hover/drag
        for (int v = 0; v < kMaxVoices; ++v) colLabels[(size_t) v]->setBounds (head.removeFromLeft (cw));
        const int rh = r.getHeight() / 6;
        for (int row = 0; row < 6; ++row)
        {
            auto rr = r.removeFromTop (rh);
            rowLabels[(size_t) row]->setBounds (rr.removeFromLeft (70));
            for (int v = 0; v < kMaxVoices; ++v)
                small[(size_t) (row * kMaxVoices + v)]->setBounds (rr.removeFromLeft (cw).reduced (10, 2));
        }
    }
private:
    std::unique_ptr<Section> perVoice;
    std::vector<std::unique_ptr<Knob>> small;
    std::vector<std::unique_ptr<juce::Label>> rowLabels, colLabels;
};

class AnalysisPage : public GridPage
{
public:
    explicit AnalysisPage (VowelWormProcessor& p)
        : GridPage (p, {
            { "Tracking", { P::gate, P::analysisSmooth, P::vowelSmooth, P::vowelSens }, 0, 0, 500, 130 },
            { "Calibration (formant ranges)", { P::f1Lo, P::f1Hi, P::f2Lo, P::f2Hi }, 0, 138, 500, 200 } })
    {
        calib.setButtonText ("Calibrate");
        calib.setClickingTogglesState (true);
        calib.onClick = [this] { toggleCalibration(); };
        sections[1]->addAndMakeVisible (calib);
        calibInfo.setFont (font (12.0f));
        calibInfo.setColour (juce::Label::textColourId, col::dim);
        calibInfo.setJustificationType (juce::Justification::topLeft);
        calibInfo.setText ("Press Calibrate, then sing a slow 'oo - oh - ah - eh - ee' a few times "
                           "(about 6 seconds) and press again. The vowel pad will then span your voice.", juce::dontSendNotification);
        sections[1]->addAndMakeVisible (calibInfo);
        addAndMakeVisible (pad); addAndMakeVisible (expr); addAndMakeVisible (pitch);
    }
    void resized() override
    {
        GridPage::resized();
        auto& s = *sections[1];
        calib.setBounds (s.getWidth() - 130, 30, 116, 30);
        calibInfo.setBounds (12, 128, s.getWidth() - 24, 64);
        pad.setBounds (520, 12, 508, 330);
        pitch.setBounds (12, 360, 200, 124);
        expr.setBounds (224, 360, 804, 250);
    }
    void tick() override
    {
        GridPage::tick();
        auto& t = proc.engine.telemetry;
        pad.update (t.vowelX.load(), t.vowelY.load(), t.confidence.load(), t.f1.load(), t.f2.load());
        expr.update (t);
        pitch.update (t.pitchHz.load(), t.confidence.load());
        bool ok = false;
        if (proc.pollCalibrationResult (ok))
            calibInfo.setText (ok ? "Calibrated to your voice. Re-run any time; presets do not change calibration."
                                  : "Not enough vowel variety was heard - try again, moving slowly between oo, ah and ee.", juce::dontSendNotification);
        if (calib.getToggleState())
        {
            const int secs = (int) ((juce::Time::getMillisecondCounter() - calibStart) / 1000);
            calib.setButtonText ("Stop (" + juce::String (secs) + "s)");
        }
    }
private:
    void toggleCalibration()
    {
        if (calib.getToggleState()) { proc.startCalibration(); calibStart = juce::Time::getMillisecondCounter(); calibInfo.setText ("Listening... sing oo - oh - ah - eh - ee.", juce::dontSendNotification); }
        else { proc.stopCalibration(); calib.setButtonText ("Calibrate"); }
    }
    juce::TextButton calib;
    juce::Label calibInfo;
    VowelPad pad;
    ExpressionMeters expr;
    PitchDisplay pitch;
    juce::uint32 calibStart = 0;
};

/** Live response of the synth's vowel filter (follows the singer's formants). */
class FilterView : public juce::Component
{
public:
    explicit FilterView (VowelWormProcessor& p) : proc (p) {}
    void tick()
    {
        const auto& t = proc.engine.telemetry;
        auto rawP = [&] (int i) { return proc.apvts.getRawParameterValue (paramTable()[(size_t) i].id)->load(); };
        const float track = rawP (P::formantTrack);
        const float fsMul = std::exp2 ((-12.0f + 24.0f * t.destValue[(int) ModDest::FormantShift].load()) / 12.0f);
        const float nf1 = lerp (500.0f, t.f1.load(), track) * fsMul;
        const float nf2 = lerp (1500.0f, t.f2.load(), track) * fsMul;
        const float nf3 = lerp (2600.0f, 2300.0f + 900.0f * t.vowelX.load(), track) * fsMul;
        const float res = t.destValue[(int) ModDest::SynthResonance].load();
        if (std::abs (nf1 - f[0]) + std::abs (nf2 - f[1]) + std::abs (nf3 - f[2]) > 2.0f || std::abs (res - r) > 0.005f)
        { f = { nf1, nf2, nf3 }; r = res; repaint(); }
    }
    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        drawPebble (g, b, "synth vowel filter (follows your formants)");
        auto plot = b.reduced (16, 12).withTrimmedTop (20).withTrimmedBottom (14);
        const float lo = std::log (100.0f), hi = std::log (6000.0f);
        auto xOf = [&] (float hz) { return plot.getX() + plot.getWidth() * (std::log (hz) - lo) / (hi - lo); };
        g.setColour (col::edge.withAlpha (0.6f)); g.setFont (font (9.5f));
        for (float hz : { 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f })
        {
            g.drawVerticalLine ((int) xOf (hz), plot.getY(), plot.getBottom());
            g.setColour (col::dim); g.drawText (hz >= 1000 ? juce::String (hz / 1000, 0) + "k" : juce::String ((int) hz), juce::Rectangle<float> (xOf (hz) - 20, plot.getBottom() + 1, 40, 12), juce::Justification::centred);
            g.setColour (col::edge.withAlpha (0.6f));
        }
        const float q = 2.5f + 14.0f * r, mix = 0.25f + 0.75f * r;
        const float gains[3] = { 1.0f, 0.6f, 0.3f }, qs[3] = { q, q * 1.2f, q * 1.4f };
        juce::Path path;
        for (int i = 0; i <= 200; ++i)
        {
            const float hz = std::exp (lo + (hi - lo) * (float) i / 200.0f);
            std::complex<float> sum (0.0f, 0.0f);
            for (int k = 0; k < 3; ++k)
            {
                const float w = hz / f[(size_t) k];
                const std::complex<float> h (0.0f, w / qs[k]);
                sum += gains[k] * 1.4f * h / std::complex<float> (1.0f - w * w, w / qs[k]);
            }
            const float mag = std::abs ((1.0f - mix) + mix * sum);
            const float db = juce::jlimit (-36.0f, 12.0f, 20.0f * std::log10 (mag + 1e-6f));
            const float y = plot.getBottom() - plot.getHeight() * (db + 36.0f) / 48.0f;
            if (i == 0) path.startNewSubPath (xOf (hz), y); else path.lineTo (xOf (hz), y);
        }
        juce::Path fill (path);
        fill.lineTo (plot.getRight(), plot.getBottom()); fill.lineTo (plot.getX(), plot.getBottom()); fill.closeSubPath();
        g.setColour (col::synth.withAlpha (0.15f)); g.fillPath (fill);
        g.setColour (col::synth); g.strokePath (path, juce::PathStrokeType (2.0f));
        g.setFont (font (11.0f, true));
        for (int k = 0; k < 3; ++k)
        {
            g.setColour (col::text);
            g.drawText ("F" + juce::String (k + 1), juce::Rectangle<float> (xOf (f[(size_t) k]) - 15, plot.getY(), 30, 14), juce::Justification::centred);
        }
    }
private:
    VowelWormProcessor& proc;
    std::array<float, 3> f { 500, 1500, 2600 };
    float r = 0.5f;
};

class SynthPage : public GridPage
{
public:
    explicit SynthPage (VowelWormProcessor& p)
        : GridPage (p, {
            { "Blend", { P::blend, P::synthLevel, P::synthOctave }, 0, 0, 300, 130 },
            { "Source & Vowel Filter", { P::oscMorph, P::noise, P::synthBrightness, P::synthResonance, P::formantTrack }, 308, 0, 708, 130 },
            { "Articulation (voice-driven)", { P::dynamicsFollow, P::synthAttack, P::synthRelease, P::velSens }, 0, 138, 500, 130 } })
    {
        help.setFont (font (13.0f));
        help.setColour (juce::Label::textColourId, col::dim);
        help.setJustificationType (juce::Justification::topLeft);
        help.setText ("Each harmony voice has a synth partner on the same note. Its vowel filter follows the formants "
                      "you sing, so a dark 'oo' gives a muted, hollow tone and opening to 'ah' raises F1.\n\n"
                      "Dynamics Follow: at 1.0 the synth rises and falls with your level; at 0 it plays like a keyboard.\n\n"
                      "Default routes: Vowel Y + Amplitude brighten, Vowel X morphs the oscillator, Breathiness adds noise.", juce::dontSendNotification);
        addAndMakeVisible (help);
        addAndMakeVisible (filter);
    }
    void resized() override
    {
        GridPage::resized();
        help.setBounds (532, 152, 484, 140);
        filter.setBounds (12, 290, 1016, 290);
    }
    void tick() override { GridPage::tick(); filter.tick(); }
private:
    juce::Label help;
    FilterView filter { proc };
};

class ModulationPage : public Page
{
public:
    explicit ModulationPage (VowelWormProcessor& p) : proc (p)
    {
        for (int r = 0; r < kNumRoutes; ++r)
        {
            Row row;
            row.src = std::make_unique<Choice> (p, P::modSrc0 + r, " ");
            row.dst = std::make_unique<Choice> (p, P::modDst0 + r, " ");
            row.depth = std::make_unique<juce::Slider> (juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight);
            row.depth->setTextBoxStyle (juce::Slider::TextBoxRight, false, 50, 18);
            row.depth->setColour (juce::Slider::textBoxTextColourId, col::dim);
            row.depth->setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
            row.depth->setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
            row.depthAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (p.apvts, paramTable()[(size_t) (P::modDepth0 + r)].id, *row.depth);
            row.smooth = std::make_unique<juce::Slider> (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox);
            row.smooth->setTooltip ("Smoothing (ms)");
            row.smoothAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (p.apvts, paramTable()[(size_t) (P::modSmooth0 + r)].id, *row.smooth);
            for (juce::Component* c : std::initializer_list<juce::Component*> { row.src.get(), row.dst.get(), row.depth.get(), row.smooth.get() }) addAndMakeVisible (c);
            rows.push_back (std::move (row));
        }
        lfo = std::make_unique<Section> ("LFOs");
        lfo->add (std::make_unique<Knob> (p, P::lfo1Rate, "LFO 1 Rate"));
        lfo->add (std::make_unique<Choice> (p, P::lfo1Shape, "LFO 1 Shape"));
        lfo->add (std::make_unique<Knob> (p, P::lfo2Rate, "LFO 2 Rate"));
        lfo->add (std::make_unique<Choice> (p, P::lfo2Shape, "LFO 2 Shape"));
        addAndMakeVisible (*lfo);
        for (int m = 0; m < kNumMacros; ++m)
        {
            Macro mc;
            mc.knob = std::make_unique<Knob> (p, P::macro0 + m, "Macro " + juce::String (m + 1));
            mc.knob->slider.getProperties().set ("accent", (juce::int64) col::synth.getARGB());
            addAndMakeVisible (*mc.knob);
            for (int s = 0; s < kMacroSlots; ++s)
            {
                mc.dst.push_back (std::make_unique<Choice> (p, P::macroDst0 + m * kMacroSlots + s, " "));
                auto sl = std::make_unique<juce::Slider> (juce::Slider::LinearHorizontal, juce::Slider::NoTextBox);
                mc.att.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (p.apvts, paramTable()[(size_t) (P::macroAmt0 + m * kMacroSlots + s)].id, *sl));
                addAndMakeVisible (*mc.dst.back()); addAndMakeVisible (*sl);
                mc.amt.push_back (std::move (sl));
            }
            macros.push_back (std::move (mc));
        }
    }
    void paint (juce::Graphics& g) override
    {
        auto panel = [&] (juce::Rectangle<int> r, const juce::String& t) { drawPebble (g, r.toFloat(), t); };
        panel (matrixArea, "MODULATION MATRIX   source  >  destination   depth   smoothing   activity");
        panel (macroArea, "MACRO ASSIGNMENTS   (amount = how far the target moves when the macro is fully up)");
        for (size_t r = 0; r < rows.size(); ++r)
        {
            auto b = rows[r].src->getBounds();
            g.setColour (col::dim); g.setFont (font (12.0f, true));
            g.drawText (juce::String ((int) r + 1), b.withX (matrixArea.getX() + 8).withWidth (16), juce::Justification::centred);
            g.drawText (">", b.withX (b.getRight() + 2).withWidth (14), juce::Justification::centred);
            // activity bar
            auto act = rows[r].activity.toFloat();
            g.setColour (col::bg); g.fillRoundedRectangle (act, 3.0f);
            const float v = juce::jlimit (-1.0f, 1.0f, activity[r]);
            const float cx = act.getCentreX();
            g.setColour (v >= 0 ? col::synth : col::human);
            g.fillRoundedRectangle (juce::Rectangle<float> (v >= 0 ? cx : cx + v * act.getWidth() * 0.5f, act.getY(), std::abs (v) * act.getWidth() * 0.5f, act.getHeight()), 3.0f);
        }
    }
    void resized() override
    {
        matrixArea = juce::Rectangle<int> (12, 12, 640, 340);
        auto r = matrixArea.reduced (10).withTrimmedTop (22);
        const int rh = r.getHeight() / kNumRoutes;
        for (auto& row : rows)
        {
            auto rr = r.removeFromTop (rh).reduced (0, 5);
            rr.removeFromLeft (20);
            row.src->setBounds (rr.removeFromLeft (150));
            rr.removeFromLeft (18);
            row.dst->setBounds (rr.removeFromLeft (150));
            rr.removeFromLeft (8);
            row.depth->setBounds (rr.removeFromLeft (170));
            row.smooth->setBounds (rr.removeFromLeft (38));
            rr.removeFromLeft (6);
            row.activity = rr.withSizeKeepingCentre (rr.getWidth(), 8);
        }
        lfo->setBounds (660, 12, 368, 340);
        lfo->cellW = 80;
        macroArea = juce::Rectangle<int> (12, 360, 1016, 222);
        auto m = macroArea.reduced (10).withTrimmedTop (22);
        const int mw = m.getWidth() / kNumMacros;
        for (auto& mc : macros)
        {
            auto cell = m.removeFromLeft (mw).reduced (6, 0);
            mc.knob->setBounds (cell.removeFromLeft (80).withSizeKeepingCentre (80, 96));
            for (int s = 0; s < kMacroSlots; ++s)
            {
                auto line = cell.removeFromTop (cell.getHeight() / (kMacroSlots - s));
                mc.dst[(size_t) s]->setBounds (line.removeFromTop (28).reduced (2, 2));
                mc.amt[(size_t) s]->setBounds (line.removeFromTop (22));
            }
        }
    }
    void tick() override
    {
        bool ch = false;
        for (int r = 0; r < kNumRoutes; ++r)
        {
            const float v = proc.engine.telemetry.routeValue[r].load();
            ch |= std::abs (v - activity[(size_t) r]) > 0.005f;
            activity[(size_t) r] = v;
        }
        if (ch) repaint (matrixArea);
    }
private:
    struct Row
    {
        std::unique_ptr<Choice> src, dst;
        std::unique_ptr<juce::Slider> depth, smooth;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> depthAtt, smoothAtt;
        juce::Rectangle<int> activity;
    };
    struct Macro
    {
        std::unique_ptr<Knob> knob;
        std::vector<std::unique_ptr<Choice>> dst;
        std::vector<std::unique_ptr<juce::Slider>> amt;
        std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> att;
    };
    VowelWormProcessor& proc;
    std::vector<Row> rows;
    std::vector<Macro> macros;
    std::unique_ptr<Section> lfo;
    std::array<float, kNumRoutes> activity {};
    juce::Rectangle<int> matrixArea, macroArea;
};

class SettingsPage : public GridPage
{
public:
    explicit SettingsPage (VowelWormProcessor& p)
        : GridPage (p, { { "Performance", { P::latencyMode, P::voices, P::bendRange, P::velSens, P::gate }, 0, 0, 600, 130 } })
    {
        info.setFont (font (13.0f));
        info.setColour (juce::Label::textColourId, col::text);
        info.setJustificationType (juce::Justification::topLeft);
        addAndMakeVisible (info);
    }
    void resized() override { GridPage::resized(); info.setBounds (24, 160, 990, 460); }
    void tick() override
    {
        if (++n % 15 != 0) return;
        const auto& t = proc.engine.telemetry;
        juce::String s;
        s << (VowelWormProcessor::isInstrument ? "Build: VowelWorm Inst (instrument with vocal sidechain)\n" : "Build: VowelWorm (audio effect with MIDI input)\n");
        s << "Sample rate: " << juce::String (proc.getSampleRate(), 0) << " Hz     Host latency reported: 0 samples (lead is not delayed)\n";
        s << "Current harmony delay: " << juce::String (t.harmonyDelayMs.load(), 1) << " ms  (about 3 pitch periods; lower voices = longer).\n";
        s << "Low Latency mode shortens grains for voices below ~165 Hz (about a third less delay, slightly less natural).\n\n";
        if (VowelWormProcessor::isInstrument)
            s << "Ableton routing (Inst): put VowelWorm Inst on the MIDI track. In the device's sidechain section\n"
                 "choose Audio From = your vocal track. Arm / monitor the MIDI track. Mute the vocal track if you only\n"
                 "want to hear VowelWorm's output (the lead can be mixed in with Lead Vocal).\n";
        else
            s << "Ableton routing: 1. Vocal audio track: input = your microphone, Monitor = In, insert VowelWorm.\n"
                 "2. MIDI track: MIDI From = your keyboard, Monitor = In (or arm it).\n"
                 "3. On the MIDI track, set MIDI To = the vocal track, then choose 'VowelWorm' in the second menu.\n"
                 "Place any Ableton effects after VowelWorm on the vocal track. Map LFO / Envelope Follower / Shaper\n"
                 "with their Map button to any VowelWorm parameter (they move the base value; the internal matrix adds on top).\n";
        s << "\nIf AUDIO IN stays dark: check the vocal track's input and Monitor setting.\n"
             "If MIDI stays dark: check the MIDI track's MIDI To routing and that it is monitoring or armed.\n";
        info.setText (s, juce::dontSendNotification);
    }
private:
    juce::Label info;
    int n = 0;
};

} // namespace ui

// =========================================================================== editor
VowelWormEditor::VowelWormEditor (VowelWormProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (content);

    content.addAndMakeVisible (title);

    // soil texture, generated once
    soil = juce::Image (juce::Image::RGB, baseW, baseH, true);
    {
        juce::Graphics sg (soil);
        sg.fillAll (ui::col::bg);
        juce::Random rnd (1234);
        for (int i = 0; i < 2600; ++i)
        {
            const float x = rnd.nextFloat() * baseW, y = rnd.nextFloat() * baseH, r = 0.6f + rnd.nextFloat() * 1.8f;
            sg.setColour ((rnd.nextBool() ? ui::col::panelHi : juce::Colours::black).withAlpha (0.18f + 0.25f * rnd.nextFloat()));
            sg.fillEllipse (x, y, r, r);
        }
        for (int i = 0; i < 6; ++i)   // faint old burrows
        {
            juce::Path burrow;
            float x = rnd.nextFloat() * baseW, y = rnd.nextFloat() * baseH, a = rnd.nextFloat() * 6.28f;
            burrow.startNewSubPath (x, y);
            for (int k = 0; k < 40; ++k) { a += (rnd.nextFloat() - 0.5f) * 0.6f; x += std::cos (a) * 12; y += std::sin (a) * 12; burrow.lineTo (x, y); }
            sg.setColour (juce::Colours::black.withAlpha (0.12f));
            sg.strokePath (burrow, juce::PathStrokeType (7.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
    }

    content.addAndMakeVisible (presetBox);
    presetBox.onChange = [this] { if (presetBox.getSelectedId() > 0) loadPresetFromMenu (presetBox.getSelectedId()); };
    for (auto* b : { &prevPreset, &nextPreset, &savePreset }) content.addAndMakeVisible (b);
    prevPreset.onClick = [this] { const int n = presetBox.getNumItems(); if (n > 0) presetBox.setSelectedItemIndex ((presetBox.getSelectedItemIndex() - 1 + n) % n); };
    nextPreset.onClick = [this] { const int n = presetBox.getNumItems(); if (n > 0) presetBox.setSelectedItemIndex ((presetBox.getSelectedItemIndex() + 1) % n); };
    savePreset.onClick = [this] { savePresetDialog(); };

    const char* names[] = { "Main", "Harmony", "Vocal Analysis", "Synthesis", "Modulation", "Settings" };
    for (int i = 0; i < 6; ++i)
    {
        auto* b = tabs.add (new juce::TextButton (names[i]));
        b->setClickingTogglesState (true);
        b->setRadioGroupId (77);
        b->onClick = [this, i] { showPage (i); };
        content.addAndMakeVisible (b);
    }
    pages.push_back (std::make_unique<ui::MainPage> (p));
    pages.push_back (std::make_unique<ui::HarmonyPage> (p));
    pages.push_back (std::make_unique<ui::AnalysisPage> (p));
    pages.push_back (std::make_unique<ui::SynthPage> (p));
    pages.push_back (std::make_unique<ui::ModulationPage> (p));
    pages.push_back (std::make_unique<ui::SettingsPage> (p));
    for (auto& pg : pages) content.addChildComponent (*pg);
    content.addAndMakeVisible (status);

    refreshPresetList();
    showPage ((int) proc.uiState.getProperty ("page", 0));

    setResizable (true, true);
    setResizeLimits (baseW * 3 / 4, baseH * 3 / 4, baseW * 2, baseH * 2);
    getConstrainer()->setFixedAspectRatio ((double) baseW / (double) baseH);
    const double scale = (double) proc.uiState.getProperty ("scale", 1.0);
    setSize ((int) (baseW * scale), (int) (baseH * scale));
    startTimerHz (30);
}

VowelWormEditor::~VowelWormEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void VowelWormEditor::paint (juce::Graphics& g)
{
    g.drawImage (soil, getLocalBounds().toFloat());
}

void VowelWormEditor::resized()
{
    const float scale = (float) getWidth() / (float) baseW;
    content.setBounds (0, 0, baseW, baseH);
    content.setTransform (juce::AffineTransform::scale (scale));
    proc.uiState.setProperty ("scale", scale, nullptr);

    auto r = juce::Rectangle<int> (0, 0, baseW, baseH);
    auto header = r.removeFromTop (52).reduced (12, 10);
    title.setBounds (header.removeFromLeft (172));
    presetBox.setBounds (header.removeFromLeft (220));
    header.removeFromLeft (4);
    prevPreset.setBounds (header.removeFromLeft (28)); header.removeFromLeft (2);
    nextPreset.setBounds (header.removeFromLeft (28)); header.removeFromLeft (4);
    savePreset.setBounds (header.removeFromLeft (52));
    status.setBounds (header.removeFromRight (290).reduced (0, 2));
    header.removeFromRight (10);
    auto tabRow = r.removeFromTop (34).reduced (12, 2);
    const int tw = 128;
    for (auto* t : tabs) { t->setBounds (tabRow.removeFromLeft (tw)); tabRow.removeFromLeft (4); }
    for (auto& pg : pages) pg->setBounds (r);
}

void VowelWormEditor::showPage (int index)
{
    currentPage = juce::jlimit (0, (int) pages.size() - 1, index);
    for (size_t i = 0; i < pages.size(); ++i) pages[i]->setVisible ((int) i == currentPage);
    tabs[currentPage]->setToggleState (true, juce::dontSendNotification);
    proc.uiState.setProperty ("page", currentPage, nullptr);
}

void VowelWormEditor::timerCallback()
{
    auto& t = proc.engine.telemetry;
    auto& page = *pages[(size_t) currentPage];
    page.inPeak = t.inputPeak.exchange (0.0f);
    page.outPeak = t.outputPeak.exchange (0.0f);
    page.tick();
    int held = 0; for (int k = 0; k < 4; ++k) held += juce::countNumberOfBits (t.heldNotes[k].load());
    status.update (page.inPeak, t.midiActivity.load(), held, t.pitchHz.load() > 0);
    title.update (t.vowelY.load(), t.vowelX.load(), t.pitchHz.load() > 0 ? t.amplitude.load() : 0.0f);
}

juce::File VowelWormEditor::userPresetDir() const
{
    return juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("VowelWorm").getChildFile ("Presets");
}

void VowelWormEditor::refreshPresetList()
{
    presetBox.clear (juce::dontSendNotification);
    presetBox.addSectionHeading ("Factory");
    const auto& f = vw::factoryPresets();
    for (size_t i = 0; i < f.size(); ++i) presetBox.addItem (f[i].name, (int) i + 1);
    userPresets = userPresetDir().findChildFiles (juce::File::findFiles, false, "*.vwpreset");
    userPresets.sort();
    if (! userPresets.isEmpty())
    {
        presetBox.addSectionHeading ("User");
        for (int i = 0; i < userPresets.size(); ++i) presetBox.addItem (userPresets[i].getFileNameWithoutExtension(), 1000 + i);
    }
    const auto cur = proc.getCurrentPresetName();
    for (int i = 0; i < presetBox.getNumItems(); ++i)
        if (presetBox.getItemText (i) == cur) { presetBox.setSelectedItemIndex (i, juce::dontSendNotification); return; }
    presetBox.setText (cur, juce::dontSendNotification);
}

void VowelWormEditor::loadPresetFromMenu (int id)
{
    if (id >= 1000)
    {
        const int i = id - 1000;
        if (! juce::isPositiveAndBelow (i, userPresets.size())) return;
        if (auto xml = juce::XmlDocument::parse (userPresets[i]))
        {
            vw::PresetData d; d.name = userPresets[i].getFileNameWithoutExtension().toStdString();
            for (auto* e : xml->getChildWithTagNameIterator ("Param"))
                d.values[e->getStringAttribute ("id").toStdString()] = (float) e->getDoubleAttribute ("value");
            proc.applyPreset (d);
        }
    }
    else proc.setCurrentProgram (id - 1);
}

void VowelWormEditor::savePresetDialog()
{
    saveWindow = std::make_unique<juce::AlertWindow> ("Save preset", "Name your preset:", juce::MessageBoxIconType::NoIcon, this);
    saveWindow->addTextEditor ("name", proc.getCurrentPresetName());
    saveWindow->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    saveWindow->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    saveWindow->enterModalState (true, juce::ModalCallbackFunction::create ([this] (int result)
    {
        if (result == 1 && saveWindow != nullptr)
        {
            const auto name = juce::File::createLegalFileName (saveWindow->getTextEditorContents ("name").trim());
            if (name.isNotEmpty())
            {
                const auto d = proc.captureCurrent (name);
                juce::XmlElement xml ("VowelWormPreset");
                xml.setAttribute ("name", name);
                xml.setAttribute ("version", 1);
                for (auto& [id, v] : d.values)
                {
                    auto* e = xml.createNewChildElement ("Param");
                    e->setAttribute ("id", juce::String (id)); e->setAttribute ("value", v);
                }
                auto dir = userPresetDir(); dir.createDirectory();
                xml.writeTo (dir.getChildFile (name + ".vwpreset"));
                proc.setCurrentPresetName (name);
                refreshPresetList();
            }
        }
        saveWindow.reset();
    }), false);
}
