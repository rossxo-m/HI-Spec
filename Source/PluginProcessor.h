#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <memory>
#include "DSP/Graph.h"

namespace hispec
{

class HISpecAudioProcessor : public juce::AudioProcessor
{
public:
    HISpecAudioProcessor();
    ~HISpecAudioProcessor() override = default;

    juce::AudioProcessorValueTreeState& getAPVTS() noexcept { return apvts; }

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "HI-Spec"; }

    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override    { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    Graph& getGraph() noexcept { return graph; }

private:
    juce::AudioProcessorValueTreeState apvts;
    Graph graph;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HISpecAudioProcessor)
};

} // namespace hispec
