#pragma once
#include "PluginProcessor.h"

class MuffLookAndFeel : public juce::LookAndFeel_V4
{
public:
    MuffLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
};

class BigMuffEditor : public juce::AudioProcessorEditor
{
public:
    explicit BigMuffEditor (BigMuffAudioProcessor&);
    ~BigMuffEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboAtt  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<SliderAtt> att;
    };

    void addKnob (Knob&, const juce::String& id, const juce::String& text);

    BigMuffAudioProcessor& proc;
    MuffLookAndFeel lnf;

    Knob volume, tone, sustain;
    juce::ToggleButton onSwitch { "ON / OFF" };
    juce::ComboBox diodeBox;
    juce::Label diodeLabel, outLabel;
    juce::Slider outSlider;

    std::unique_ptr<ButtonAtt> onAtt;
    std::unique_ptr<ComboAtt> diodeAtt;
    std::unique_ptr<SliderAtt> outAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BigMuffEditor)
};
