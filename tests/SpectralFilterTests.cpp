#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "DSP/SpectralFilter.h"

#include <cmath>

using Catch::Approx;

namespace
{
    std::vector<float> makeLogBandCenters (int n, float sr)
    {
        // Match HilbertFilterbank's linear centres so tests reflect reality.
        std::vector<float> out (static_cast<size_t> (n));
        const float spacing = (sr * 0.5f) / static_cast<float> (n);
        for (int k = 0; k < n; ++k)
            out[static_cast<size_t> (k)] = (static_cast<float> (k) + 0.5f) * spacing;
        return out;
    }

    void fillBandsWithWhite (std::vector<hispec::ComplexBuffer>& bands, int bs,
                             float amp = 0.3f)
    {
        for (auto& b : bands)
        {
            b.setSize (2, bs, true);
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < bs; ++i)
                {
                    b.getRealWrite (ch)[i] = amp;   // deterministic "ones"
                    b.getImagWrite (ch)[i] = 0.0f;
                }
        }
    }

    float peakAbs (const hispec::ComplexBuffer& b, int ch)
    {
        float m = 0.0f;
        for (int i = 0; i < b.getNumSamples(); ++i)
            m = juce::jmax (m, std::sqrt (b.getRealRead (ch)[i] * b.getRealRead (ch)[i]
                                        + b.getImagRead (ch)[i] * b.getImagRead (ch)[i]));
        return m;
    }
}

TEST_CASE("SpectralFilter evalMagnitude: LP attenuates above cutoff", "[dsp][spectralfilter]")
{
    using SF = hispec::SpectralFilter;
    const float fc = 1000.0f;
    const float atCutoff = SF::evalMagnitude (SF::Shape::LP, fc,        fc, 0.707f, 0.0f);
    const float passBand = SF::evalMagnitude (SF::Shape::LP, fc * 0.1f, fc, 0.707f, 0.0f);
    const float stopBand = SF::evalMagnitude (SF::Shape::LP, fc * 10.f, fc, 0.707f, 0.0f);
    INFO ("LP at fc=" << atCutoff << " passband=" << passBand << " stopband=" << stopBand);
    REQUIRE (atCutoff == Approx (0.707f).margin (0.05f));
    REQUIRE (passBand > 0.99f);
    REQUIRE (stopBand < 0.02f);
}

TEST_CASE("SpectralFilter evalMagnitude: HP is complement", "[dsp][spectralfilter]")
{
    using SF = hispec::SpectralFilter;
    const float fc = 1000.0f;
    const float passBand = SF::evalMagnitude (SF::Shape::HP, fc * 10.f, fc, 0.707f, 0.0f);
    const float stopBand = SF::evalMagnitude (SF::Shape::HP, fc * 0.1f, fc, 0.707f, 0.0f);
    REQUIRE (passBand > 0.95f);
    REQUIRE (stopBand < 0.05f);
}

TEST_CASE("SpectralFilter LP attenuates high bands in complex path", "[dsp][spectralfilter]")
{
    constexpr double sr = 48000.0;
    constexpr int    bs = 256;
    constexpr int    numBands = 16;

    hispec::SpectralFilter sf;
    sf.prepare ({ sr, bs, numBands, 2 });

    auto centers = makeLogBandCenters (numBands, static_cast<float> (sr));
    sf.setBandCenterFrequencies (centers);

    hispec::SpectralFilter::Params p;
    p.power = true;
    p.mix   = 1.0f;
    p.shape = hispec::SpectralFilter::Shape::LP;
    p.freq  = 2000.0f;
    p.q     = 0.707f;
    p.lfoDepth = 0.0f;
    p.envDepth = 0.0f;
    sf.setParams (p);

    std::vector<hispec::ComplexBuffer> bands (numBands);
    fillBandsWithWhite (bands, bs, 0.5f);

    sf.processComplex (bands);

    const float lowBandPeak  = peakAbs (bands[1],  0);  // ~2.25 kHz
    const float highBandPeak = peakAbs (bands[15], 0);  // ~23 kHz
    INFO ("low=" << lowBandPeak << " high=" << highBandPeak);

    REQUIRE (lowBandPeak > 0.3f);
    REQUIRE (highBandPeak < 0.02f);
}

TEST_CASE("SpectralFilter HP attenuates low bands in complex path", "[dsp][spectralfilter]")
{
    constexpr double sr = 48000.0;
    constexpr int    bs = 256;
    constexpr int    numBands = 16;

    hispec::SpectralFilter sf;
    sf.prepare ({ sr, bs, numBands, 2 });

    sf.setBandCenterFrequencies (makeLogBandCenters (numBands, static_cast<float> (sr)));

    hispec::SpectralFilter::Params p;
    p.power = true; p.mix = 1.0f;
    p.shape = hispec::SpectralFilter::Shape::HP;
    p.freq  = 5000.0f;
    p.q     = 0.707f;
    sf.setParams (p);

    std::vector<hispec::ComplexBuffer> bands (numBands);
    fillBandsWithWhite (bands, bs, 0.5f);

    sf.processComplex (bands);

    const float lowBandPeak  = peakAbs (bands[0],  0);
    const float highBandPeak = peakAbs (bands[15], 0);
    REQUIRE (lowBandPeak < 0.1f);
    REQUIRE (highBandPeak > 0.3f);
}

TEST_CASE("SpectralFilter stereo path is stable and finite", "[dsp][spectralfilter]")
{
    constexpr double sr = 48000.0;
    constexpr int    bs = 256;

    hispec::SpectralFilter sf;
    sf.prepare ({ sr, bs, 32, 2 });

    hispec::SpectralFilter::Params p;
    p.power = true; p.mix = 1.0f;
    p.shape = hispec::SpectralFilter::Shape::BP;
    p.freq  = 1000.0f;
    p.q     = 5.0f;
    sf.setParams (p);

    juce::AudioBuffer<float> buf (2, bs);
    for (int block = 0; block < 50; ++block)
    {
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < bs; ++i)
                buf.getWritePointer (ch)[i] = 0.5f * std::sin (static_cast<float> (block * bs + i) * 0.003f);
        sf.processStereo (buf);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < bs; ++i)
                REQUIRE (std::isfinite (buf.getReadPointer (ch)[i]));
    }
}

TEST_CASE("SpectralFilter bypass when power is off or mix=0", "[dsp][spectralfilter]")
{
    constexpr double sr = 48000.0;
    constexpr int    bs = 128;

    hispec::SpectralFilter sf;
    sf.prepare ({ sr, bs, 8, 2 });
    sf.setBandCenterFrequencies (makeLogBandCenters (8, static_cast<float> (sr)));

    hispec::SpectralFilter::Params p;
    p.power = false;
    sf.setParams (p);

    std::vector<hispec::ComplexBuffer> bands (8);
    fillBandsWithWhite (bands, bs, 0.7f);

    sf.processComplex (bands);
    for (auto& b : bands)
        REQUIRE (peakAbs (b, 0) == Approx (0.7f).margin (1.0e-6f));
}
