#include "PluginEditor.h"

namespace hispec
{

HISpecAudioProcessorEditor::HISpecAudioProcessorEditor (HISpecAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setSize (1100, 700);
}

void HISpecAudioProcessorEditor::paint (juce::Graphics& g)
{
    // Placeholder chrome background; real LookAndFeel in Plan 03 Task 8.
    g.fillAll (juce::Colour (0xff1a1a1a));
    g.setColour (juce::Colours::white);
    g.setFont (18.0f);
    g.drawFittedText ("HI-Spec (scaffold)", getLocalBounds(), juce::Justification::centred, 1);
}

void HISpecAudioProcessorEditor::resized()
{
}

} // namespace hispec
