#include "PluginProcessor.h"
#include "PluginEditor.h"

using APVTS = juce::AudioProcessorValueTreeState;

APVTS::ParameterLayout BigMuffAudioProcessor::createLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    auto knob = [&p] (const char* id, const char* name, float def)
    {
        p.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 }, name,
                                                                  juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), def));
    };
    knob ("volume",  "Volume",  0.50f);
    knob ("tone",    "Tone",    0.50f);
    knob ("sustain", "Sustain", 0.60f);

    p.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "output", 1 }, "Output",
                                                              juce::NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f));
    p.push_back (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "on", 1 }, "Effect On", true));
    p.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "diode", 1 }, "Clipping Diodes",
                                                               juce::StringArray { "Silicon", "Germanium", "LED" }, 0));
    return { p.begin(), p.end() };
}

BigMuffAudioProcessor::BigMuffAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    pVol = apvts.getRawParameterValue ("volume");
    pTone = apvts.getRawParameterValue ("tone");
    pSus = apvts.getRawParameterValue ("sustain");
    pOut = apvts.getRawParameterValue ("output");
    pOn = apvts.getRawParameterValue ("on");
    pDiode = apvts.getRawParameterValue ("diode");
}

void BigMuffAudioProcessor::prepareToPlay (double sampleRate, int)
{
    engine.prepare (sampleRate);
    setLatencySamples (engine.getLatencySamples());
}

bool BigMuffAudioProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto out = l.getMainOutputChannelSet();
    return (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo())
           && out == l.getMainInputChannelSet();
}

void BigMuffAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    bigmuff::Params p;
    p.volume   = pVol->load();
    p.tone     = pTone->load();
    p.sustain  = pSus->load();
    p.outputDb = pOut->load();
    p.on       = pOn->load() > 0.5f;
    p.diode    = (int) std::lround (pDiode->load());

    engine.process (buffer, p);
}

juce::AudioProcessorEditor* BigMuffAudioProcessor::createEditor() { return new BigMuffEditor (*this); }

void BigMuffAudioProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, dest);
}

void BigMuffAudioProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new BigMuffAudioProcessor(); }
