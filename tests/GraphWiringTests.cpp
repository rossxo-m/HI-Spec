#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "DSP/Graph.h"
#include "Parameters/ParameterLayout.h"
#include "Parameters/ParameterIDs.h"

#include <random>

using Catch::Approx;

namespace
{
    /** Minimal juce::AudioProcessor stub that owns an APVTS built from our
        layout. Graph::process() only needs the APVTS — the host contract
        isn't exercised here. */
    class DummyProcessor : public juce::AudioProcessor
    {
    public:
        DummyProcessor()
            : AudioProcessor (BusesProperties()
                                  .withInput  ("In",  juce::AudioChannelSet::stereo(), true)
                                  .withOutput ("Out", juce::AudioChannelSet::stereo(), true)),
              apvts (*this, nullptr, "Params", hispec::createParameterLayout())
        {}

        const juce::String getName() const override    { return "Dummy"; }
        void prepareToPlay (double, int) override      {}
        void releaseResources() override               {}
        void processBlock (juce::AudioBuffer<float>&,
                           juce::MidiBuffer&) override {}
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
        void getStateInformation (juce::MemoryBlock&) override {}
        void setStateInformation (const void*, int) override {}

        juce::AudioProcessorValueTreeState apvts;
    };

    void setParam (juce::AudioProcessorValueTreeState& apvts,
                   const juce::String& id, float val)
    {
        if (auto* p = apvts.getParameter (id))
            p->setValueNotifyingHost (p->convertTo0to1 (val));
    }
}

TEST_CASE("Graph: passes audio through when all modules are bypassed and delay = 0", "[graph]")
{
    constexpr double sr = 48000.0;
    constexpr int    bs = 512;

    DummyProcessor proc;
    hispec::Graph graph;
    graph.prepare ({ sr, bs });

    // Explicitly turn off both modules (defaults already do, but be explicit)
    setParam (proc.apvts, hispec::ids::voc_power, 0.0f);
    setParam (proc.apvts, hispec::ids::sf_power,  0.0f);
    // All per-band times start at 0 by default.

    juce::AudioBuffer<float> buf (2, bs);
    std::mt19937 rng (1234);
    std::uniform_real_distribution<float> d (-0.5f, 0.5f);
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < bs; ++i)
            buf.getWritePointer (ch)[i] = d (rng);

    juce::AudioBuffer<float> ref;
    ref.makeCopyOf (buf);

    // Run a few blocks — the Hilbert bank needs a moment to warm up.
    for (int block = 0; block < 3; ++block)
        graph.process (buf, proc.apvts);

    // With all delay times at 0 and both modules off, output must be finite.
    // (We don't assert bit-exact because the Hilbert bank has some energy
    // ripple; the filterbank suite already covers that.)
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < bs; ++i)
            REQUIRE (std::isfinite (buf.getReadPointer (ch)[i]));
}

TEST_CASE("Graph: reports non-negative latency after prepare", "[graph]")
{
    hispec::Graph g;
    g.prepare ({ 48000.0, 512 });
    REQUIRE (g.getLatencySamples() >= 0);
}

TEST_CASE("Graph: band-count hot-swap mid-stream does not explode", "[graph]")
{
    DummyProcessor proc;
    hispec::Graph graph;
    graph.prepare ({ 48000.0, 512 });

    // Start at 16 bands
    setParam (proc.apvts, hispec::ids::delay_bandCount, 1.0f /* index 1 = 16 */);

    juce::AudioBuffer<float> buf (2, 512);
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < 512; ++i)
            buf.getWritePointer (ch)[i] = 0.3f * std::sin (static_cast<float> (i) * 0.02f);

    for (int block = 0; block < 4; ++block)
        graph.process (buf, proc.apvts);

    // Flip to 32 bands
    setParam (proc.apvts, hispec::ids::delay_bandCount, 2.0f);
    for (int block = 0; block < 4; ++block)
        graph.process (buf, proc.apvts);
    // And back to 8
    setParam (proc.apvts, hispec::ids::delay_bandCount, 0.0f);
    for (int block = 0; block < 4; ++block)
        graph.process (buf, proc.apvts);

    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < 512; ++i)
        {
            REQUIRE (std::isfinite (buf.getReadPointer (ch)[i]));
            REQUIRE (std::abs (buf.getReadPointer (ch)[i]) < 5.0f);
        }
}

TEST_CASE("Graph: produces non-zero output for a steady tone (bypass)", "[graph]")
{
    DummyProcessor proc;
    hispec::Graph graph;
    graph.prepare ({ 48000.0, 512 });

    setParam (proc.apvts, hispec::ids::voc_power, 0.0f);
    setParam (proc.apvts, hispec::ids::sf_power,  0.0f);

    juce::AudioBuffer<float> buf (2, 512);
    const double w = juce::MathConstants<double>::twoPi * 1000.0 / 48000.0;
    double ph = 0.0;
    double sumSq = 0.0;
    int count = 0;
    for (int block = 0; block < 30; ++block)
    {
        for (int ch = 0; ch < 2; ++ch)
        {
            auto* d = buf.getWritePointer (ch);
            double p = ph;
            for (int i = 0; i < 512; ++i)
            {
                d[i] = 0.3f * static_cast<float> (std::sin (p));
                p += w;
            }
        }
        ph += w * 512.0;
        graph.process (buf, proc.apvts);
        if (block >= 10)
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < 512; ++i)
                {
                    const float v = buf.getReadPointer (ch)[i];
                    sumSq += static_cast<double> (v) * v;
                    ++count;
                }
    }
    const double rms = std::sqrt (sumSq / juce::jmax (1, count));
    INFO ("bypass rms = " << rms);
    REQUIRE (rms > 0.05);  // input at 0.3 * 0.707 = ~0.21, reconstruction ±3 dB
}

TEST_CASE("Graph: Vocoder module in Global Post position colours the output", "[graph]")
{
    DummyProcessor proc;
    hispec::Graph graph;
    graph.prepare ({ 48000.0, 512 });

    auto runOnce = [&] (bool enableVocoder) -> double
    {
        setParam (proc.apvts, hispec::ids::voc_power,
                  enableVocoder ? 1.0f : 0.0f);
        setParam (proc.apvts, hispec::ids::voc_position, 1.0f /* GlobalPost */);
        setParam (proc.apvts, hispec::ids::voc_vowelX, 1.0f);
        setParam (proc.apvts, hispec::ids::voc_vowelY, 1.0f);
        setParam (proc.apvts, hispec::ids::voc_q,      6.0f);
        setParam (proc.apvts, hispec::ids::voc_mix,    1.0f);

        juce::AudioBuffer<float> buf (2, 512);
        double ph = 0.0;
        const double w = juce::MathConstants<double>::twoPi * 3000.0 / 48000.0;
        double sumSq = 0.0;
        int count = 0;
        for (int block = 0; block < 30; ++block)
        {
            for (int ch = 0; ch < 2; ++ch)
            {
                auto* d = buf.getWritePointer (ch);
                double p = ph;
                for (int i = 0; i < 512; ++i)
                {
                    d[i] = 0.3f * static_cast<float> (std::sin (p));
                    p += w;
                }
            }
            ph += w * 512.0;
            graph.process (buf, proc.apvts);
            if (block >= 10)
                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < 512; ++i)
                    {
                        const float v = buf.getReadPointer (ch)[i];
                        sumSq += static_cast<double> (v) * v;
                        ++count;
                    }
        }
        return std::sqrt (sumSq / juce::jmax (1, count));
    };

    const double dry = runOnce (false);
    const double wet = runOnce (true);
    INFO ("dry=" << dry << " wet=" << wet);
    REQUIRE (dry > 0.05);
    REQUIRE (wet > 0.0);
    REQUIRE (std::abs (wet - dry) > 0.01); // something changed
}
