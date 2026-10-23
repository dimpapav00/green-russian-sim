#include "PluginEditor.h"

namespace
{
const juce::Colour kOlive  { 0xff4b5d3a };
const juce::Colour kDark   { 0xff2b3623 };
const juce::Colour kCream  { 0xffe8dcc0 };
const juce::Colour kRed    { 0xffd0382a };
}

MuffLookAndFeel::MuffLookAndFeel()
{
    setColour (juce::Label::textColourId, kCream);
    setColour (juce::ComboBox::backgroundColourId, kDark);
    setColour (juce::ComboBox::textColourId, kCream);
    setColour (juce::ComboBox::outlineColourId, kCream.withAlpha (0.6f));
    setColour (juce::ComboBox::arrowColourId, kCream);
    setColour (juce::PopupMenu::backgroundColourId, kDark);
    setColour (juce::PopupMenu::textColourId, kCream);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, kCream.withAlpha (0.25f));
    setColour (juce::Slider::trackColourId, kCream);
    setColour (juce::Slider::thumbColourId, kCream);
    setColour (juce::Slider::backgroundColourId, kDark);
    setColour (juce::TextEditor::textColourId, kCream);
    setColour (juce::TextEditor::backgroundColourId, kDark);
}

void MuffLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                        float startAngle, float endAngle, juce::Slider&)
{
    const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (6.0f);
    const float radius = std::min (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);

    juce::Path track;
    track.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, startAngle, endAngle, true);
    g.setColour (kCream.withAlpha (0.25f));
    g.strokePath (track, juce::PathStrokeType (2.5f));

    // tick marks
    for (int i = 0; i <= 10; ++i)
    {
        const float a = startAngle + (float) i / 10.0f * (endAngle - startAngle);
        const float r1 = radius, r2 = radius - (i % 5 == 0 ? 7.0f : 4.0f);
        g.drawLine (centre.x + std::sin (a) * r1, centre.y - std::cos (a) * r1,
                    centre.x + std::sin (a) * r2, centre.y - std::cos (a) * r2, 1.2f);
    }

    // black knob with fluted edge
    const float kr = radius - 12.0f;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a3a3a), centre.x - kr, centre.y - kr,
                                             juce::Colour (0xff070707), centre.x + kr, centre.y + kr, false));
    g.fillEllipse (centre.x - kr, centre.y - kr, kr * 2.0f, kr * 2.0f);
    g.setColour (juce::Colour (0xff555555));
    g.drawEllipse (centre.x - kr, centre.y - kr, kr * 2.0f, kr * 2.0f, 1.5f);

    juce::Path ptr;
    ptr.addRoundedRectangle (-2.0f, -kr + 2.0f, 4.0f, kr * 0.5f, 1.5f);
    g.setColour (kCream);
    g.fillPath (ptr, juce::AffineTransform::rotation (angle).translated (centre.x, centre.y));
}

void MuffLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    const auto area = b.getLocalBounds().toFloat();
    const float d = std::min (area.getWidth() * 0.6f, area.getHeight() - 30.0f);
    const auto sw = juce::Rectangle<float> (d, d).withCentre ({ area.getCentreX(), area.getY() + 22.0f + d * 0.5f });

    const bool on = b.getToggleState();
    const juce::Rectangle<float> led (area.getCentreX() - 6.0f, area.getY() + 2.0f, 12.0f, 12.0f);
    if (on)
    {
        g.setColour (kRed.withAlpha (0.35f));
        g.fillEllipse (led.expanded (5.0f));
    }
    g.setColour (on ? kRed : juce::Colour (0xff4a1b16));
    g.fillEllipse (led);

    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffd0d0d0), sw.getX(), sw.getY(),
                                             juce::Colour (0xff707070), sw.getRight(), sw.getBottom(), false));
    g.fillEllipse (sw);
    g.setColour (highlighted ? kCream : juce::Colour (0xff1d1d1d));
    g.drawEllipse (sw, 2.0f);

    g.setColour (kCream);
    g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    g.drawText (b.getButtonText(), b.getLocalBounds().removeFromBottom (18), juce::Justification::centred);
}

BigMuffEditor::BigMuffEditor (BigMuffAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);
    setSize (560, 470);

    addKnob (volume,  "volume",  "VOLUME");
    addKnob (tone,    "tone",    "TONE");
    addKnob (sustain, "sustain", "SUSTAIN");

    addAndMakeVisible (onSwitch);
    onAtt = std::make_unique<ButtonAtt> (proc.apvts, "on", onSwitch);

    diodeBox.addItemList ({ "Silicon", "Germanium", "LED" }, 1);
    addAndMakeVisible (diodeBox);
    diodeAtt = std::make_unique<ComboAtt> (proc.apvts, "diode", diodeBox);

    for (auto* l : { &diodeLabel, &outLabel })
    {
        l->setJustificationType (juce::Justification::centredLeft);
        l->setFont (juce::FontOptions (12.0f, juce::Font::bold));
        addAndMakeVisible (l);
    }
    diodeLabel.setText ("DIODES", juce::dontSendNotification);
    outLabel.setText ("OUTPUT", juce::dontSendNotification);

    outSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    outSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 60, 20);
    outSlider.setTextValueSuffix (" dB");
    addAndMakeVisible (outSlider);
    outAtt = std::make_unique<SliderAtt> (proc.apvts, "output", outSlider);
}

BigMuffEditor::~BigMuffEditor() { setLookAndFeel (nullptr); }

void BigMuffEditor::addKnob (Knob& k, const juce::String& id, const juce::String& text)
{
    k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    k.slider.setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);
    k.slider.setDoubleClickReturnValue (true, proc.apvts.getParameter (id)->convertFrom0to1 (
                                                  proc.apvts.getParameter (id)->getDefaultValue()));
    k.label.setText (text, juce::dontSendNotification);
    k.label.setJustificationType (juce::Justification::centred);
    k.label.setFont (juce::FontOptions (14.0f, juce::Font::bold));
    addAndMakeVisible (k.slider);
    addAndMakeVisible (k.label);
    k.att = std::make_unique<SliderAtt> (proc.apvts, id, k.slider);
}

void BigMuffEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff10150d));
    const auto body = getLocalBounds().toFloat().reduced (8.0f);
    g.setGradientFill (juce::ColourGradient (kOlive.brighter (0.1f), 0, 0, kDark, 0, (float) getHeight(), false));
    g.fillRoundedRectangle (body, 12.0f);
    g.setColour (juce::Colour (0xff1a2114));
    g.drawRoundedRectangle (body, 12.0f, 3.0f);

    g.setColour (kCream);
    g.setFont (juce::FontOptions (28.0f, juce::Font::bold));
    g.drawText ("GREEN RUSSIAN SIM", getLocalBounds().removeFromTop (60).withTrimmedTop (14), juce::Justification::centred);
    g.setColour (kRed);
    g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    g.drawText ("big muff pi  -  distortion / sustainer", getLocalBounds().removeFromTop (82).withTrimmedTop (60),
                juce::Justification::centred);
    g.setColour (kCream.withAlpha (0.55f));
    g.setFont (juce::FontOptions (11.0f));
    g.drawText ("unofficial emulation, not affiliated with any manufacturer",
                getLocalBounds().removeFromBottom (24), juce::Justification::centred);
}

void BigMuffEditor::resized()
{
    auto r = getLocalBounds().reduced (24);
    r.removeFromTop (78);

    auto row = r.removeFromTop (150);
    const int w = row.getWidth() / 3;
    for (auto* k : { &volume, &tone, &sustain })
    {
        auto cell = row.removeFromLeft (w);
        k->label.setBounds (cell.removeFromBottom (22));
        k->slider.setBounds (cell);
    }

    r.removeFromTop (8);
    onSwitch.setBounds (r.removeFromTop (110).withSizeKeepingCentre (130, 110));

    r.removeFromTop (8);
    auto bottom = r.removeFromTop (30);
    diodeLabel.setBounds (bottom.removeFromLeft (60));
    diodeBox.setBounds (bottom.removeFromLeft (120));
    bottom.removeFromLeft (24);
    outLabel.setBounds (bottom.removeFromLeft (56));
    outSlider.setBounds (bottom);
}
