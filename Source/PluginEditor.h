#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

namespace hispec
{

class HISpecAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit HISpecAudioProcessorEditor (HISpecAudioProcessor&);
    ~HISpecAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    HISpecAudioProcessor& processor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HISpecAudioProcessorEditor)
};

} // namespace hispec
