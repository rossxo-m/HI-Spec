#include <catch2/catch_test_macros.hpp>

#include "GUI/BandDetailStrip.h"
#include "Parameters/ParameterLayout.h"
#include "Parameters/ParameterIDs.h"

#include <juce_audio_processors/juce_audio_processors.h>

namespace
{
    // Tiny host processor so APVTS has a parent.
    class HostProc : public juce::AudioProcessor
    {
    public:
        HostProc()
            : AudioProcessor (BusesProperties()
                                  .withOutput ("Out", juce::AudioChannelSet::stereo(), true)),
              apvts (*this, nullptr, "P", hispec::createParameterLayout())
        {}
        const juce::String getName() const override { return "HostProc"; }
        void prepareToPlay (double, int) override   {}
        void releaseResources() override            {}
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

TEST_CASE ("BandDetailStrip: attaching to band 2 binds five params", "[gui][band-strip]")
{
    HostProc host;
    hispec::BandDetailStrip strip (host.apvts);
    strip.setBounds (0, 0, 600, 80);

    strip.setSelectedBand (2);
    REQUIRE (strip.getSelectedBand() == 2);

    using Knob = hispec::BandDetailStrip;
    const std::array<std::pair<Knob::Knob, juce::String>, Knob::kNumKnobs> pairs {{
        { Knob::Time,     hispec::ids::band_time  (2) },
        { Knob::Feedback, hispec::ids::band_fb    (2) },
        { Knob::Gain,     hispec::ids::band_gain  (2) },
        { Knob::Pan,      hispec::ids::band_pan   (2) },
        { Knob::Pitch,    hispec::ids::band_pitch (2) }
    }};

    for (const auto& [k, pid] : pairs)
    {
        auto* param = host.apvts.getParameter (pid);
        REQUIRE (param != nullptr);

        auto& slider = strip.getKnob (k);
        // Drive the knob to its mid-range value; expect the param to follow.
        const auto range = slider.getRange();
        const double midVal = range.getStart() + range.getLength() * 0.5;
        slider.setValue (midVal, juce::sendNotificationSync);

        const float normalised = param->getValue();
        INFO ("param '" << pid << "' norm=" << normalised);
        REQUIRE (normalised > 0.0f);
        REQUIRE (normalised < 1.0f);
    }
}

TEST_CASE ("BandDetailStrip: deselecting detaches cleanly", "[gui][band-strip]")
{
    HostProc host;
    hispec::BandDetailStrip strip (host.apvts);
    strip.setBounds (0, 0, 600, 80);

    // Attach, set a value, deselect.
    strip.setSelectedBand (3);
    auto& timeKnob = strip.getKnob (hispec::BandDetailStrip::Time);
    const auto range = timeKnob.getRange();
    timeKnob.setValue (range.getStart() + range.getLength() * 0.6, juce::sendNotificationSync);

    auto* param = host.apvts.getParameter (hispec::ids::band_time (3));
    REQUIRE (param != nullptr);
    const float before = param->getValue();

    strip.setSelectedBand (-1);
    REQUIRE (strip.getSelectedBand() == -1);

    // Knob should now be disabled and further changes should NOT touch the param.
    REQUIRE (timeKnob.isEnabled() == false);

    const float paramBefore = param->getValue();
    timeKnob.setValue (range.getStart() + range.getLength() * 0.9, juce::sendNotificationSync);
    const float paramAfter = param->getValue();
    INFO ("before=" << before << " paramBefore=" << paramBefore << " paramAfter=" << paramAfter);
    REQUIRE (paramBefore == paramAfter);
}

TEST_CASE ("BandDetailStrip: heading reflects selection state", "[gui][band-strip]")
{
    HostProc host;
    hispec::BandDetailStrip strip (host.apvts);
    strip.setBounds (0, 0, 600, 80);

    REQUIRE (strip.getSelectedBand() == -1);
    strip.setSelectedBand (5);
    REQUIRE (strip.getSelectedBand() == 5);
    strip.setSelectedBand (-7); // clamp
    REQUIRE (strip.getSelectedBand() == -1);
}
