#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Parameters/ParameterLayout.h"

namespace hispec
{

HISpecAudioProcessor::HISpecAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Params", createParameterLayout())
{
}

void HISpecAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    Graph::ProcessSpec spec;
    spec.sampleRate   = sampleRate;
    spec.maxBlockSize = samplesPerBlock;
    graph.prepare (spec);
    setLatencySamples (graph.getLatencySamples());
}

void HISpecAudioProcessor::releaseResources()
{
    graph.reset();
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

    graph.process (buffer, apvts);
}

juce::AudioProcessorEditor* HISpecAudioProcessor::createEditor()
{
    return new HISpecAudioProcessorEditor (*this);
}

void HISpecAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void HISpecAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

} // namespace hispec

// JUCE plugin factory
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new hispec::HISpecAudioProcessor();
}
