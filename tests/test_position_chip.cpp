#include <catch2/catch_test_macros.hpp>

#include "GUI/PositionChip.h"
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

TEST_CASE ("PositionChip: FB_Global cell is disabled", "[gui][position]")
{
    using PC = hispec::PositionChip;
    REQUIRE_FALSE (PC::isCellEnabled (PC::Row::FB, PC::Col::Global));

    // The other 5 cells are enabled.
    REQUIRE (PC::isCellEnabled (PC::Row::Pre,  PC::Col::Global));
    REQUIRE (PC::isCellEnabled (PC::Row::Pre,  PC::Col::PerBand));
    REQUIRE (PC::isCellEnabled (PC::Row::Post, PC::Col::Global));
    REQUIRE (PC::isCellEnabled (PC::Row::Post, PC::Col::PerBand));
    REQUIRE (PC::isCellEnabled (PC::Row::FB,   PC::Col::PerBand));
}

TEST_CASE ("PositionChip: setRowCol rejects the disabled cell", "[gui][position]")
{
    HostProc host;
    hispec::PositionChip chip (host.apvts, hispec::ids::voc_position);

    const auto before = chip.getCurrent();
    const bool ok = chip.setRowCol (hispec::PositionChip::Row::FB,
                                    hispec::PositionChip::Col::Global);
    REQUIRE_FALSE (ok);
    REQUIRE (chip.getCurrent() == before);
}

TEST_CASE ("PositionChip: cycleNext visits all 5 valid cells and wraps", "[gui][position]")
{
    HostProc host;
    hispec::PositionChip chip (host.apvts, hispec::ids::voc_position);

    // Seed to a known starting point.
    REQUIRE (chip.setRowCol (hispec::PositionChip::Row::Pre,
                             hispec::PositionChip::Col::Global));
    REQUIRE (chip.getCurrent() == hispec::ids::Position::GlobalPre);

    std::vector<hispec::ids::Position> visited;
    visited.push_back (chip.getCurrent());
    for (int i = 0; i < 4; ++i)
    {
        chip.cycleNext();
        visited.push_back (chip.getCurrent());
    }

    // 6th cycle should wrap back to the starting value.
    chip.cycleNext();
    const auto afterWrap = chip.getCurrent();

    // Assert all 5 enabled positions were visited exactly once.
    std::sort (visited.begin(), visited.end(),
               [] (auto a, auto b) { return static_cast<int>(a) < static_cast<int>(b); });
    visited.erase (std::unique (visited.begin(), visited.end()), visited.end());
    REQUIRE (visited.size() == 5);

    // None of them is Off.
    for (auto p : visited) REQUIRE (p != hispec::ids::Position::Off);

    // Wrap landed back on GlobalPre.
    REQUIRE (afterWrap == hispec::ids::Position::GlobalPre);
}
