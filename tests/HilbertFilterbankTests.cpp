#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "DSP/HilbertFilterbank.h"

#include <random>

namespace
{
    double rms (const juce::AudioBuffer<float>& buf)
    {
        double sum = 0.0;
        int n = 0;
        for (int ch = 0; ch < buf.getNumChannels(); ++ch)
        {
            auto* p = buf.getReadPointer (ch);
            for (int i = 0; i < buf.getNumSamples(); ++i)
            {
                sum += static_cast<double> (p[i]) * p[i];
                ++n;
            }
        }
        return std::sqrt (sum / juce::jmax (1, n));
    }

    juce::AudioBuffer<float> makeDifference (const juce::AudioBuffer<float>& a,
                                              const juce::AudioBuffer<float>& b,
                                              int skipFirst = 0)
    {
        const int n = juce::jmin (a.getNumSamples(), b.getNumSamples());
        const int chs = juce::jmin (a.getNumChannels(), b.getNumChannels());
        juce::AudioBuffer<float> out (chs, juce::jmax (0, n - skipFirst));
        out.clear();
        for (int ch = 0; ch < chs; ++ch)
        {
            auto* dst = out.getWritePointer (ch);
            auto* pa  = a.getReadPointer (ch);
            auto* pb  = b.getReadPointer (ch);
            for (int i = skipFirst; i < n; ++i)
                dst[i - skipFirst] = pa[i] - pb[i];
        }
        return out;
    }
}

TEST_CASE("HilbertFilterbank approximates unity energy gain on white noise", "[dsp][hilbert]")
{
    constexpr double sr = 48000.0;
    constexpr int    bs = 512;
    constexpr int    seconds = 1;
    constexpr int    totalSamples = static_cast<int> (sr) * seconds;

    hispec::HilbertFilterbank fb;
    fb.prepare ({ sr, bs, 1 });
    fb.setBandCount (16);

    juce::AudioBuffer<float> input  (1, totalSamples);
    juce::AudioBuffer<float> output (1, totalSamples);
    output.clear();
    std::mt19937 rng (0x51EC7A1u);
    std::uniform_real_distribution<float> d (-1.0f, 1.0f);
    for (int i = 0; i < totalSamples; ++i)
        input.getWritePointer (0)[i] = d (rng) * 0.3f;

    std::vector<hispec::ComplexBuffer> bands;

    for (int offset = 0; offset + bs <= totalSamples; offset += bs)
    {
        juce::AudioBuffer<float> inBlock  (input .getArrayOfWritePointers(), 1, offset, bs);
        juce::AudioBuffer<float> outBlock (output.getArrayOfWritePointers(), 1, offset, bs);

        fb.analyse    (inBlock, bands);
        fb.synthesise (bands, outBlock);
    }

    // Skip initial transient (~10ms) so LPFs have settled.
    const int skip = static_cast<int> (sr * 0.01);
    juce::AudioBuffer<float> inputTrimmed  (input .getArrayOfWritePointers(), 1, skip, totalSamples - skip);
    juce::AudioBuffer<float> outputTrimmed (output.getArrayOfWritePointers(), 1, skip, totalSamples - skip);

    const double rIn  = rms (inputTrimmed);
    const double rOut = rms (outputTrimmed);

    // Energy-domain reconstruction: total RMS within ±3 dB of input.
    // (Sample-accurate PR is a −40 dB polish-task — the TPT bank has a few dB
    //  of magnitude ripple + group-delay phase offset.)
    REQUIRE (rIn > 0.0);
    const double ratioDb = 20.0 * std::log10 (rOut / rIn);
    INFO ("RMS ratio out/in in dB = " << ratioDb);
    REQUIRE (ratioDb > -3.0);
    REQUIRE (ratioDb <  3.0);

    // And the output is bounded (no runaway).
    REQUIRE (rOut < 2.0);
}

TEST_CASE("HilbertFilterbank: output is finite for broadband input", "[dsp][hilbert]")
{
    constexpr double sr = 48000.0;
    constexpr int    bs = 512;

    hispec::HilbertFilterbank fb;
    fb.prepare ({ sr, bs, 1 });
    fb.setBandCount (32);

    juce::AudioBuffer<float> in (1, bs), out (1, bs);
    std::vector<hispec::ComplexBuffer> bands;
    std::mt19937 rng (42);
    std::uniform_real_distribution<float> d (-1.0f, 1.0f);

    for (int block = 0; block < 20; ++block)
    {
        for (int i = 0; i < bs; ++i) in.getWritePointer (0)[i] = d (rng) * 0.3f;
        fb.analyse (in, bands);
        fb.synthesise (bands, out);

        for (int i = 0; i < bs; ++i)
            REQUIRE (std::isfinite (out.getReadPointer (0)[i]));
    }
}

TEST_CASE("HilbertFilterbank band-count hot-swap does not blow up", "[dsp][hilbert]")
{
    constexpr double sr = 48000.0;
    constexpr int    bs = 512;

    hispec::HilbertFilterbank fb;
    fb.prepare ({ sr, bs, 1 });
    fb.setBandCount (8);

    juce::AudioBuffer<float> in (1, bs), out (1, bs);
    std::vector<hispec::ComplexBuffer> bands;

    // Fill with a sine at 500Hz
    const double w = juce::MathConstants<double>::twoPi * 500.0 / sr;
    double ph = 0.0;
    auto fillSine = [&](juce::AudioBuffer<float>& b)
    {
        auto* p = b.getWritePointer (0);
        for (int i = 0; i < b.getNumSamples(); ++i)
        {
            p[i] = 0.3f * static_cast<float> (std::sin (ph));
            ph += w;
        }
    };

    for (int block = 0; block < 10; ++block)
    {
        fillSine (in);
        fb.analyse (in, bands);
        fb.synthesise (bands, out);

        // No NaNs, no infs
        for (int i = 0; i < bs; ++i)
            REQUIRE (std::isfinite (out.getReadPointer (0)[i]));

        // Swap band counts mid-stream
        if (block == 2) fb.setBandCount (16);
        if (block == 5) fb.setBandCount (32);
        if (block == 7) fb.setBandCount (8);
    }
}

TEST_CASE("HilbertFilterbank reports sensible band centers", "[dsp][hilbert]")
{
    hispec::HilbertFilterbank fb;
    fb.prepare ({ 48000.0, 512, 1 });
    fb.setBandCount (16);

    // At 16 bands over Nyquist (24kHz), band spacing is 1500 Hz.
    // Band 0 center = 0.5 * 1500 = 750 Hz. Band 15 center = 15.5 * 1500 = 23250 Hz.
    // Hot-swap takes effect on next analyse; force the swap first.
    juce::AudioBuffer<float> dummy (1, 8);
    std::vector<hispec::ComplexBuffer> bands;
    fb.analyse (dummy, bands);

    REQUIRE (fb.getBandCount() == 16);
    REQUIRE (fb.getBandCenterHz (0)  == Catch::Approx (750.0f).margin (0.5f));
    REQUIRE (fb.getBandCenterHz (15) == Catch::Approx (23250.0f).margin (0.5f));
}
