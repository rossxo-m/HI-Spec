#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace hispec
{

/// Builds the APVTS parameter layout for the entire plugin.
juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

} // namespace hispec
