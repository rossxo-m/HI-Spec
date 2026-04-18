#include <catch2/catch_test_macros.hpp>
#include "Parameters/ParameterLayout.h"
#include "Parameters/ParameterIDs.h"

namespace
{
    // Build an APVTS off a dummy processor so we can query parameters by ID.
    struct DummyProcessor : juce::AudioProcessor
    {
        DummyProcessor()
            : apvts (*this, nullptr, "Params", hispec::createParameterLayout()) {}

        const juce::String getName() const override { return "Dummy"; }
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

TEST_CASE("parameter layout contains core delay params", "[params]")
{
    DummyProcessor p;

    REQUIRE (p.apvts.getParameter (hispec::ids::delay_bandCount) != nullptr);
    REQUIRE (p.apvts.getParameter (hispec::ids::band_time  (0).toRawUTF8()) != nullptr);
    REQUIRE (p.apvts.getParameter (hispec::ids::band_time  (31).toRawUTF8()) != nullptr);
    REQUIRE (p.apvts.getParameter (hispec::ids::band_pitch (15).toRawUTF8()) != nullptr);

    // Vocoder
    REQUIRE (p.apvts.getParameter (hispec::ids::voc_carrier)     != nullptr);
    REQUIRE (p.apvts.getParameter (hispec::ids::voc_texThruZero) != nullptr);
    REQUIRE (p.apvts.getParameter (hispec::ids::voc_position)    != nullptr);

    // Spectral filter
    REQUIRE (p.apvts.getParameter (hispec::ids::sf_shape)    != nullptr);
    REQUIRE (p.apvts.getParameter (hispec::ids::sf_lfoShape) != nullptr);
    REQUIRE (p.apvts.getParameter (hispec::ids::sf_envDepth) != nullptr);

    // Macros
    REQUIRE (p.apvts.getParameter (hispec::ids::macro_time) != nullptr);
    REQUIRE (p.apvts.getParameter (hispec::ids::macro_mix)  != nullptr);
}

TEST_CASE("per-band params exist for all 32 slots", "[params]")
{
    DummyProcessor p;
    for (int i = 0; i < hispec::ids::kMaxBands; ++i)
    {
        REQUIRE (p.apvts.getParameter (hispec::ids::band_time  (i).toRawUTF8()) != nullptr);
        REQUIRE (p.apvts.getParameter (hispec::ids::band_fb    (i).toRawUTF8()) != nullptr);
        REQUIRE (p.apvts.getParameter (hispec::ids::band_gain  (i).toRawUTF8()) != nullptr);
        REQUIRE (p.apvts.getParameter (hispec::ids::band_pan   (i).toRawUTF8()) != nullptr);
        REQUIRE (p.apvts.getParameter (hispec::ids::band_pitch (i).toRawUTF8()) != nullptr);
    }
}

TEST_CASE("position choice parameters default to sensible values", "[params]")
{
    DummyProcessor p;
    auto* vocPos = dynamic_cast<juce::AudioParameterChoice*> (p.apvts.getParameter (hispec::ids::voc_position));
    auto* sfPos  = dynamic_cast<juce::AudioParameterChoice*> (p.apvts.getParameter (hispec::ids::sf_position));
    REQUIRE (vocPos != nullptr);
    REQUIRE (sfPos  != nullptr);
    REQUIRE (vocPos->choices.size() == 6);
    REQUIRE (sfPos ->choices.size() == 6);
}
