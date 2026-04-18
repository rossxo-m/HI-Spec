#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "GUI/VowelPad.h"
#include "Parameters/ParameterLayout.h"
#include "Parameters/ParameterIDs.h"

#include <juce_audio_processors/juce_audio_processors.h>

using Catch::Approx;

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

    // Fake mouse event at (x, y) pixel coordinates.
    juce::MouseEvent fakeMouseEvent (juce::Component& c, float x, float y)
    {
        const auto pos = juce::Point<float> (x, y);
        return juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(),
                                 pos,
                                 juce::ModifierKeys(),
                                 juce::MouseInputSource::defaultPressure,
                                 juce::MouseInputSource::defaultOrientation,
                                 juce::MouseInputSource::defaultRotation,
                                 juce::MouseInputSource::defaultTiltX,
                                 juce::MouseInputSource::defaultTiltY,
                                 &c, &c,
                                 juce::Time::getCurrentTime(),
                                 pos,
                                 juce::Time::getCurrentTime(),
                                 1, false);
    }
}

TEST_CASE ("VowelPad: corner A (top-left) maps to (0,0) params", "[gui][vowel]")
{
    HostProc host;
    hispec::VowelPad pad (host.apvts, hispec::ids::voc_vowelX, hispec::ids::voc_vowelY);
    pad.setBounds (0, 0, 200, 200);

    // Click the top-left corner of the pad — far enough in to dodge the 2px
    // inset used for drawing.
    pad.mouseDown (fakeMouseEvent (pad, 2.0f, 2.0f));

    REQUIRE (pad.getX() == Approx (0.0f).margin (0.02f));
    REQUIRE (pad.getY() == Approx (0.0f).margin (0.02f));

    auto* px = host.apvts.getParameter (hispec::ids::voc_vowelX);
    auto* py = host.apvts.getParameter (hispec::ids::voc_vowelY);
    REQUIRE (px != nullptr);
    REQUIRE (py != nullptr);
    REQUIRE (px->getValue() == Approx (0.0f).margin (0.02f));
    REQUIRE (py->getValue() == Approx (0.0f).margin (0.02f));
}

TEST_CASE ("VowelPad: drag moves dot continuously and monotonically", "[gui][vowel]")
{
    HostProc host;
    hispec::VowelPad pad (host.apvts, hispec::ids::voc_vowelX, hispec::ids::voc_vowelY);
    pad.setBounds (0, 0, 200, 200);

    float prevX = -1.0f;
    for (int i = 0; i <= 10; ++i)
    {
        const float x = 10.0f + (float) i * 18.0f;
        const float y = 100.0f;
        pad.mouseDrag (fakeMouseEvent (pad, x, y));
        const float nx = pad.getX();
        REQUIRE (nx >= prevX - 0.01f); // monotonic non-decreasing
        prevX = nx;
    }

    REQUIRE (pad.getX() > 0.5f);
    REQUIRE (pad.getY() == Approx (0.5f).margin (0.05f));
}

TEST_CASE ("VowelPad: setXY with notification writes both params", "[gui][vowel]")
{
    HostProc host;
    hispec::VowelPad pad (host.apvts, hispec::ids::voc_vowelX, hispec::ids::voc_vowelY);
    pad.setBounds (0, 0, 200, 200);

    pad.setXY (0.7f, 0.3f, juce::sendNotificationSync);

    auto* px = host.apvts.getParameter (hispec::ids::voc_vowelX);
    auto* py = host.apvts.getParameter (hispec::ids::voc_vowelY);
    REQUIRE (px->getValue() == Approx (0.7f).margin (1.0e-3f));
    REQUIRE (py->getValue() == Approx (0.3f).margin (1.0e-3f));
}
