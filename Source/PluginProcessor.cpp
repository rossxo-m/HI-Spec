#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace hispec
{

HISpecAudioProcessor::HISpecAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
}

void HISpecAudioProcessor::prepareToPlay (double /*sampleRate*/, int /*samplesPerBlock*/)
{
}

void HISpecAudioProcessor::releaseResources()
{
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool HISpecAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& mainIn  = layouts.getMainInputChannelSet();
    const auto& mainOut = layouts.getMainOutputChannelSet();

    if (mainOut != juce::AudioChannelSet::mono()
        && mainOut != juce::AudioChannelSet::stereo())
        return false;

    return mainIn == mainOut;
}
#endif

void HISpecAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const auto totalNumInputChannels  = getTotalNumInputChannels();
    const auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    // Pass-through for now; DSP wired in Task 6.
}

juce::AudioProcessorEditor* HISpecAudioProcessor::createEditor()
{
    return new HISpecAudioProcessorEditor (*this);
}

void HISpecAudioProcessor::getStateInformation (juce::MemoryBlock&)
{
}

void HISpecAudioProcessor::setStateInformation (const void*, int)
{
}

} // namespace hispec

// JUCE plugin factory
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new hispec::HISpecAudioProcessor();
}
