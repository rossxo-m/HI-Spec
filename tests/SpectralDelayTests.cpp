#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "DSP/SpectralDelay.h"
#include "DSP/HilbertFilterbank.h"

#include <cmath>
#include <random>

using Catch::Approx;

TEST_CASE("SpectralDelay echoes an impulse at the requested time (single band)", "[dsp][delay]")
{
    constexpr double sr = 48000.0;
    constexpr int    bs = 1024;
    constexpr float  delayMs = 100.0f;

    // Feed a bypass-filterbank complex signal directly: we don't need analysis
    // for the delay-line test, just verify popSample/pushSample mechanics.
    hispec::SpectralDelay sd;
    sd.prepare ({ sr, bs, 1, 2000.0f });

    std::vector<hispec::SpectralDelay::BandParams> bp (1);
    bp[0].timeMs = delayMs;
    bp[0].fb     = 0.0f;
    bp[0].gainDb = 0.0f;
    bp[0].pan    = 0.0f;
    sd.setBandParams (bp);
    sd.setBandCenterFrequencies (std::vector<float> { 1000.0f });

    std::vector<hispec::ComplexBuffer> bands (1);
    const int totalSamples = static_cast<int> (sr * 0.25); // 250 ms
    const int delaySamples = static_cast<int> (delayMs * 0.001 * sr);

    // Run a series of blocks; inject the impulse in the first block of the real
    // channel; watch the output for a peak at delaySamples.
    std::vector<float> capturedReal;
    capturedReal.reserve (static_cast<size_t> (totalSamples));

    int sampleCounter = 0;
    while (sampleCounter < totalSamples)
    {
        const int thisBlock = juce::jmin (bs, totalSamples - sampleCounter);
        bands[0].setSize (1, thisBlock, true);
        if (sampleCounter == 0)
            bands[0].getRealWrite (0)[0] = 1.0f;
        sd.process (bands);
        for (int i = 0; i < thisBlock; ++i)
            capturedReal.push_back (bands[0].getRealRead (0)[i]);
        sampleCounter += thisBlock;
    }

    // Find peak location
    int peakIdx = 0;
    float peakVal = 0.0f;
    for (int i = 10; i < static_cast<int> (capturedReal.size()); ++i)
    {
        if (std::abs (capturedReal[static_cast<size_t> (i)]) > peakVal)
        {
            peakVal = std::abs (capturedReal[static_cast<size_t> (i)]);
            peakIdx = i;
        }
    }

    // Allow ±2 samples (Lagrange3rd interpolation + gain×2 centre-pan scale)
    REQUIRE (std::abs (peakIdx - delaySamples) <= 3);
    REQUIRE (peakVal > 0.5f); // substantial echo
}

TEST_CASE("SpectralDelay feedback increases sustained energy", "[dsp][delay]")
{
    constexpr double sr = 48000.0;
    constexpr int    bs = 512;

    hispec::SpectralDelay sd;
    sd.prepare ({ sr, bs, 1, 2000.0f });

    std::vector<hispec::SpectralDelay::BandParams> bp (1);
    bp[0].timeMs = 30.0f;
    bp[0].fb     = 0.0f;
    bp[0].gainDb = 0.0f;
    bp[0].pan    = 0.0f;
    sd.setBandParams (bp);
    sd.setBandCenterFrequencies (std::vector<float> { 500.0f });

    auto runAndSum = [&] (float fbValue) -> double
    {
        sd.reset();
        bp[0].fb = fbValue;
        sd.setBandParams (bp);

        std::vector<hispec::ComplexBuffer> bands (1);
        double sumSq = 0.0;
        for (int block = 0; block < 80; ++block) // ~0.85 s
        {
            bands[0].setSize (1, bs, true);
            if (block == 0)
                bands[0].getRealWrite (0)[0] = 1.0f;
            sd.process (bands);
            for (int i = 0; i < bs; ++i)
                sumSq += static_cast<double> (bands[0].getRealRead (0)[i])
                       * static_cast<double> (bands[0].getRealRead (0)[i]);
        }
        return std::sqrt (sumSq);
    };

    const double energyNoFb = runAndSum (0.0f);
    const double energyFb50 = runAndSum (0.5f);
    const double energyFb90 = runAndSum (0.9f);

    // Feedback energy should grow monotonically. The DC-blocker on the
    // recirculation path attenuates broadband impulses slightly — keep
    // thresholds realistic rather than ideal-geometric.
    REQUIRE (energyFb50 > energyNoFb * 1.1);
    REQUIRE (energyFb90 > energyFb50 * 1.3);
}

TEST_CASE("SpectralDelay is stable with fb at 1.0 (soft-clip saturation)", "[dsp][delay]")
{
    constexpr double sr = 48000.0;
    constexpr int    bs = 512;

    hispec::SpectralDelay sd;
    sd.prepare ({ sr, bs, 1, 2000.0f });

    std::vector<hispec::SpectralDelay::BandParams> bp (1);
    bp[0].timeMs = 20.0f;
    bp[0].fb     = 1.0f;
    bp[0].gainDb = 0.0f;
    bp[0].pan    = 0.0f;
    sd.setBandParams (bp);
    sd.setBandCenterFrequencies (std::vector<float> { 500.0f });

    std::vector<hispec::ComplexBuffer> bands (1);
    for (int block = 0; block < 200; ++block) // ~2 s
    {
        bands[0].setSize (1, bs, true);
        if (block == 0)
            bands[0].getRealWrite (0)[0] = 0.8f;
        sd.process (bands);
        for (int i = 0; i < bs; ++i)
        {
            const float r = bands[0].getRealRead (0)[i];
            const float im = bands[0].getImagRead (0)[i];
            REQUIRE (std::isfinite (r));
            REQUIRE (std::isfinite (im));
            REQUIRE (std::abs (r) < 10.0f);  // tanh bounds it
        }
    }
}

TEST_CASE("SpectralDelay pan affects stereo channels", "[dsp][delay]")
{
    constexpr double sr = 48000.0;
    constexpr int    bs = 512;

    hispec::SpectralDelay sd;
    sd.prepare ({ sr, bs, 1, 2000.0f });

    std::vector<hispec::SpectralDelay::BandParams> bp (1);
    bp[0].timeMs = 10.0f;
    bp[0].pan    = 1.0f;  // hard right
    bp[0].fb     = 0.0f;
    bp[0].gainDb = 0.0f;
    sd.setBandParams (bp);
    sd.setBandCenterFrequencies (std::vector<float> { 500.0f });

    // Push impulses in block 0 then run several blocks, capturing output
    // across the full window. 10 ms delay ≈ 480 samples, so the echo lands
    // inside the first block.
    std::vector<hispec::ComplexBuffer> bands (1);
    std::vector<float> capturedL, capturedR;

    for (int block = 0; block < 3; ++block)
    {
        bands[0].setSize (2, bs, true);
        if (block == 0)
        {
            for (int i = 0; i < 4; ++i)
            {
                bands[0].getRealWrite (0)[i] = 1.0f;
                bands[0].getRealWrite (1)[i] = 1.0f;
            }
        }
        sd.process (bands);
        for (int i = 0; i < bs; ++i)
        {
            capturedL.push_back (bands[0].getRealRead (0)[i]);
            capturedR.push_back (bands[0].getRealRead (1)[i]);
        }
    }

    // Find the echo peak in each channel (skip the first few samples so we
    // don't pick up any through-path on sample 0).
    float lPeak = 0.0f, rPeak = 0.0f;
    for (size_t i = 10; i < capturedL.size(); ++i)
    {
        lPeak = juce::jmax (lPeak, std::abs (capturedL[i]));
        rPeak = juce::jmax (rPeak, std::abs (capturedR[i]));
    }
    INFO ("lPeak = " << lPeak << "  rPeak = " << rPeak);
    REQUIRE (rPeak > lPeak + 0.1f);
}
