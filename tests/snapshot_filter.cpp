#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "GUI/SpectralFilterBody.h"
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

    // Set an APVTS parameter directly via the APVTS helper.
    void setParam (juce::AudioProcessorValueTreeState& apvts, const juce::String& id, float v)
    {
        if (auto* p = apvts.getParameter (id))
            p->setValueNotifyingHost (p->convertTo0to1 (v));
    }
}

TEST_CASE ("FilterResponsePlot: LP shape has falling high end", "[gui][filter]")
{
    HostProc host;
    // LP, 1kHz, Q=0.707
    setParam (host.apvts, hispec::ids::sf_shape, 0.0f);
    setParam (host.apvts, hispec::ids::sf_freq,  1000.0f);
    setParam (host.apvts, hispec::ids::sf_q,     0.707f);

    hispec::FilterResponsePlot plot (host.apvts);
    plot.recomputeNow();

    const float m100   = plot.getMagnitudeAt (100.0f);
    const float m10k   = plot.getMagnitudeAt (10000.0f);

    const float db100 = juce::Decibels::gainToDecibels (m100);
    const float db10k = juce::Decibels::gainToDecibels (m10k);

    INFO ("db100=" << db100 << " db10k=" << db10k);
    REQUIRE ((db100 - db10k) >= 20.0f);
}

TEST_CASE ("FilterResponsePlot: BP shape peaks near center freq", "[gui][filter]")
{
    HostProc host;
    const float centerHz = 1000.0f;
    setParam (host.apvts, hispec::ids::sf_shape, 2.0f);     // BP
    setParam (host.apvts, hispec::ids::sf_freq,  centerHz);
    setParam (host.apvts, hispec::ids::sf_q,     5.0f);

    hispec::FilterResponsePlot plot (host.apvts);
    plot.recomputeNow();

    // Check response at center exceeds response at extremes.
    const float mCenter = plot.getMagnitudeAt (centerHz);
    const float mLow    = plot.getMagnitudeAt (100.0f);
    const float mHigh   = plot.getMagnitudeAt (10000.0f);

    INFO ("mCenter=" << mCenter << " mLow=" << mLow << " mHigh=" << mHigh);
    REQUIRE (mCenter > mLow * 3.0f);
    REQUIRE (mCenter > mHigh * 3.0f);

    // Sweep and confirm argmax sits within ±1/3 octave of centerHz.
    int    bestIdx = 0;
    float  bestVal = 0.0f;
    constexpr int N = hispec::FilterResponsePlot::kNumPoints;
    constexpr float fMin = 20.0f, fMax = 20000.0f;

    for (int i = 0; i < N; ++i)
    {
        const float t = (float) i / (float) (N - 1);
        const float f = fMin * std::pow (fMax / fMin, t);
        const float m = plot.getMagnitudeAt (f);
        if (m > bestVal) { bestVal = m; bestIdx = i; }
    }
    const float tMax  = (float) bestIdx / (float) (N - 1);
    const float fPeak = fMin * std::pow (fMax / fMin, tMax);
    INFO ("fPeak=" << fPeak);
    REQUIRE (fPeak > centerHz * 0.79f);   // -1/3 oct
    REQUIRE (fPeak < centerHz * 1.26f);   // +1/3 oct
}

TEST_CASE ("SpectralFilterBody: Mod sub-section rendered below curve", "[gui][filter]")
{
    HostProc host;
    hispec::SpectralFilterBody body (host.apvts);
    body.setBounds (0, 0, 560, 260);

    const auto main = body.getMainRowBounds();
    const auto mod  = body.getModRowBounds();

    REQUIRE (main.getHeight() > 0);
    REQUIRE (mod.getHeight()  > 0);
    REQUIRE (mod.getY() >= main.getBottom() - 1);
}
