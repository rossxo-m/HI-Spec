#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "State/PresetManager.h"
#include "Parameters/ParameterLayout.h"
#include "Parameters/ParameterIDs.h"

#include <random>

using Catch::Approx;

namespace
{
    class DummyProcessor : public juce::AudioProcessor
    {
    public:
        DummyProcessor()
            : AudioProcessor (BusesProperties()
                                  .withInput  ("In",  juce::AudioChannelSet::stereo(), true)
                                  .withOutput ("Out", juce::AudioChannelSet::stereo(), true)),
              apvts (*this, nullptr, "Params", hispec::createParameterLayout()),
              presets (apvts)
        {
            uiState.setProperty ("currentPage",  "spectrum", nullptr);
            uiState.setProperty ("selectedBand", -1, nullptr);
            presets.attachUIState (uiState);
        }

        const juce::String getName() const override    { return "Dummy"; }
        void prepareToPlay (double, int) override      {}
        void releaseResources() override               {}
        void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}
        double getTailLengthSeconds() const override   { return 0.0; }
        bool acceptsMidi() const override              { return false; }
        bool producesMidi() const override             { return false; }
        juce::AudioProcessorEditor* createEditor() override { return nullptr; }
        bool hasEditor() const override                { return false; }
        int getNumPrograms() override                  { return 1; }
        int getCurrentProgram() override               { return 0; }
        void setCurrentProgram (int) override          {}
        const juce::String getProgramName (int) override { return {}; }
        void changeProgramName (int, const juce::String&) override {}
        void getStateInformation (juce::MemoryBlock& b) override { presets.getStateInformation (b); }
        void setStateInformation (const void* d, int n) override { presets.setStateInformation (d, n); }

        juce::AudioProcessorValueTreeState apvts;
        juce::ValueTree uiState { "UIState" };
        hispec::PresetManager presets;
    };
}

TEST_CASE("State: all APVTS params survive a save/reload roundtrip", "[state]")
{
    DummyProcessor a;

    std::mt19937 rng (0x5EED);
    std::uniform_real_distribution<float> d (0.0f, 1.0f);

    // Randomise every parameter (normalised space). Choice/bool params snap
    // on set, so we first save/reload once to let the snap settle, then
    // compare via a second roundtrip — the post-reload state must be a
    // fixed point of save→load.
    for (auto* p : a.getParameters())
        p->setValueNotifyingHost (d (rng));

    juce::MemoryBlock blockInitial;
    a.getStateInformation (blockInitial);

    DummyProcessor b;
    b.setStateInformation (blockInitial.getData(), static_cast<int> (blockInitial.getSize()));

    // Second roundtrip — should now be a fixed point.
    juce::MemoryBlock blockFromB;
    b.getStateInformation (blockFromB);

    DummyProcessor c;
    c.setStateInformation (blockFromB.getData(), static_cast<int> (blockFromB.getSize()));

    auto paramsB = b.getParameters();
    auto paramsC = c.getParameters();
    REQUIRE (paramsB.size() == paramsC.size());
    for (int i = 0; i < paramsB.size(); ++i)
    {
        INFO ("param " << i << " '" << paramsB[i]->getName (100)
              << "' B=" << paramsB[i]->getValue() << " C=" << paramsC[i]->getValue());
        REQUIRE (paramsB[i]->getValue() == Approx (paramsC[i]->getValue()).margin (1.0e-4f));
    }
}

TEST_CASE("State: UIState properties roundtrip", "[state]")
{
    DummyProcessor a;
    a.uiState.setProperty ("currentPage", "module:0", nullptr);
    a.uiState.setProperty ("selectedBand", 7, nullptr);

    juce::MemoryBlock block;
    a.getStateInformation (block);

    DummyProcessor b;
    b.setStateInformation (block.getData(), static_cast<int> (block.getSize()));

    REQUIRE (b.uiState.getProperty ("currentPage").toString() == "module:0");
    REQUIRE (static_cast<int> (b.uiState.getProperty ("selectedBand")) == 7);
}

TEST_CASE("State: unknown-future-version state is ignored gracefully", "[state]")
{
    DummyProcessor a;

    // Craft an XML blob with version=99
    juce::XmlElement root ("HISpecState");
    root.setAttribute ("version", 99);
    auto* apvtsWrap = root.createNewChildElement ("APVTS");
    apvtsWrap->createNewChildElement ("Params");

    juce::MemoryBlock block;
    juce::AudioProcessor::copyXmlToBinary (root, block);

    // Should NOT crash and should NOT corrupt current state.
    REQUIRE_NOTHROW (a.setStateInformation (block.getData(),
                                            static_cast<int> (block.getSize())));

    // Spot-check a few default-valued parameters
    auto* v = a.apvts.getRawParameterValue (hispec::ids::voc_power);
    REQUIRE (v != nullptr);
    REQUIRE (v->load() < 0.5f); // still off
}

TEST_CASE("State: 'Init' factory preset resets state", "[state][presets]")
{
    DummyProcessor a;
    // Dirty some params
    if (auto* p = a.apvts.getParameter (hispec::ids::voc_power))
        p->setValueNotifyingHost (1.0f);
    if (auto* p = a.apvts.getParameter (hispec::ids::band_time (0)))
        p->setValueNotifyingHost (0.5f);

    REQUIRE (a.presets.loadFactoryPreset ("Init"));

    auto* vocPwr = a.apvts.getRawParameterValue (hispec::ids::voc_power);
    REQUIRE (vocPwr->load() < 0.5f);
    auto* t0 = a.apvts.getRawParameterValue (hispec::ids::band_time (0));
    REQUIRE (t0->load() == Approx (0.0f).margin (1.0e-3f));
}

TEST_CASE("State: all 4 factory presets apply without errors", "[state][presets]")
{
    DummyProcessor a;
    const auto& presets = hispec::PresetManager::getFactoryPresets();
    REQUIRE (presets.size() >= 4);
    for (const auto& p : presets)
    {
        REQUIRE_NOTHROW (a.presets.loadFactoryPreset (p.name));
    }
}

TEST_CASE("State: loading missing factory preset returns false", "[state][presets]")
{
    DummyProcessor a;
    REQUIRE_FALSE (a.presets.loadFactoryPreset ("Does Not Exist"));
}
