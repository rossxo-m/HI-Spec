#include <catch2/catch_test_macros.hpp>

#include "GUI/VocoderBody.h"
#include "Parameters/ParameterLayout.h"
#include "Parameters/ParameterIDs.h"

#include <juce_audio_processors/juce_audio_processors.h>

namespace
{
    class HostProc : public juce::AudioProcessor
    {
    public:
        HostProc()
            : AudioProcessor (BusesProperties().withOutput ("Out", juce::AudioChannelSet::stereo(), true)),
              apvts (*this, nullptr, "P", hispec::createParameterLayout())
        {}
        const juce::String getName() const override { return "HostProc"; }
        void prepareToPlay (double, int) override {}
        void releaseResources() override {}
        void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}
        double getTailLengthSeconds() const override { return 0.0; }
        bool acceptsMidi() const override { return false; }
        bool producesMidi() const override { return false; }
        juce::AudioProcessorEditor* createEditor() override { return nullptr; }
        bool hasEditor() const override { return false; }
        int getNumPrograms() override { return 1; }
        int getCurrentProgram() override { return 0; }
        void setCurrentProgram (int) override {}
        const juce::String getProgramName (int) override { return {}; }
        void changeProgramName (int, const juce::String&) override {}
        void getStateInformation (juce::MemoryBlock&) override {}
        void setStateInformation (const void*, int) override {}

        juce::AudioProcessorValueTreeState apvts;
    };
}

TEST_CASE ("VocoderBody: Texture sub-section sits below main row", "[gui][vocoder]")
{
    HostProc host;
    hispec::VocoderBody body (host.apvts);
    body.setBounds (0, 0, 560, 260);

    const auto main    = body.getMainRowBounds();
    const auto texture = body.getTextureRowBounds();

    REQUIRE (main.getHeight() > 0);
    REQUIRE (texture.getHeight() > 0);
    // Texture row's top is ≥ main row's bottom.
    REQUIRE (texture.getY() >= main.getBottom() - 1);
}

TEST_CASE ("VocoderBody: vowel pad is bound to the voc_vowelX/Y params", "[gui][vocoder]")
{
    HostProc host;
    hispec::VocoderBody body (host.apvts);
    body.setBounds (0, 0, 560, 260);

    body.getVowelPad().setXY (0.25f, 0.75f, juce::sendNotificationSync);

    auto* px = host.apvts.getParameter (hispec::ids::voc_vowelX);
    auto* py = host.apvts.getParameter (hispec::ids::voc_vowelY);
    REQUIRE (px->getValue() > 0.2f);
    REQUIRE (px->getValue() < 0.3f);
    REQUIRE (py->getValue() > 0.7f);
    REQUIRE (py->getValue() < 0.8f);
}
