#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "DSP/Vocoder.h"

#include <cmath>

using Catch::Approx;

namespace
{
    double rms (const juce::AudioBuffer<float>& buf, int ch = 0, int skip = 0)
    {
        double sum = 0.0;
        int n = 0;
        auto* p = buf.getReadPointer (ch);
        for (int i = skip; i < buf.getNumSamples(); ++i)
        {
            sum += static_cast<double> (p[i]) * p[i];
            ++n;
        }
        return std::sqrt (sum / juce::jmax (1, n));
    }

    /** Feed a steady sine at `freqHz` through the Vocoder (Self carrier) and
        return the output RMS (trimmed). */
    double runSineAtFreq (hispec::Vocoder& voc, double sr, int bs, float freqHz,
                          int numBlocks = 40)
    {
        juce::AudioBuffer<float> buf (2, bs);

        const double w = juce::MathConstants<double>::twoPi * freqHz / sr;
        double ph = 0.0;
        double sumSq = 0.0;
        int    count = 0;

        for (int block = 0; block < numBlocks; ++block)
        {
            for (int i = 0; i < bs; ++i)
            {
                const float s = 0.3f * static_cast<float> (std::sin (ph));
                ph += w;
                buf.getWritePointer (0)[i] = s;
                buf.getWritePointer (1)[i] = s;
            }
            voc.processStereo (buf);

            if (block >= 10) // skip transient
            {
                for (int i = 0; i < bs; ++i)
                {
                    const float y = buf.getReadPointer (0)[i];
                    sumSq += static_cast<double> (y) * y;
                    ++count;
                }
            }
        }
        return std::sqrt (sumSq / juce::jmax (1, count));
    }
}

TEST_CASE("Vocoder bypass when power is off", "[dsp][vocoder]")
{
    constexpr double sr = 48000.0;
    constexpr int    bs = 512;

    hispec::Vocoder voc;
    voc.prepare ({ sr, bs, 16, 2 });

    hispec::Vocoder::Params p;
    p.power = false;
    voc.setParams (p);

    juce::AudioBuffer<float> buf (2, bs);
    for (int i = 0; i < bs; ++i)
    {
        buf.getWritePointer (0)[i] = 0.3f * std::sin (static_cast<float> (i) * 0.01f);
        buf.getWritePointer (1)[i] = 0.3f * std::sin (static_cast<float> (i) * 0.01f);
    }

    juce::AudioBuffer<float> ref;
    ref.makeCopyOf (buf);

    voc.processStereo (buf);

    // Bit-exact passthrough when bypassed
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < bs; ++i)
            REQUIRE (buf.getReadPointer (ch)[i] == ref.getReadPointer (ch)[i]);
}

TEST_CASE("Vocoder Self carrier emphasises energy near the selected vowel's F1", "[dsp][vocoder]")
{
    constexpr double sr = 48000.0;
    constexpr int    bs = 512;

    hispec::Vocoder voc;
    voc.prepare ({ sr, bs, 16, 2 });

    // Pick "/a/" corner (vowelX = 1, vowelY = 1): F1 ≈ 730 Hz
    hispec::Vocoder::Params p;
    p.power = true;
    p.mix   = 1.0f;
    p.carrier = hispec::Vocoder::Carrier::Self;
    p.vowelX = 1.0f;
    p.vowelY = 1.0f;
    p.q      = 0.7f;  // fairly narrow
    p.texDrive = 0.0f;
    p.texDepth = 0.0f;
    voc.setParams (p);

    const auto formants = voc.getFormants();
    INFO ("F1 = " << formants[0] << " Hz");
    REQUIRE (formants[0] == Approx (730.0f).margin (5.0f));

    const double onF1  = runSineAtFreq (voc, sr, bs, formants[0], 40);
    voc.reset();
    voc.setParams (p);
    const double offF1 = runSineAtFreq (voc, sr, bs, 4000.0f, 40);

    INFO ("rms onF1 = " << onF1 << "  offF1 = " << offF1);
    // On-formant response should be substantially louder than far-from-formant
    REQUIRE (onF1 > offF1 * 3.0);
}

TEST_CASE("Vocoder Noise carrier produces output that tracks input envelope", "[dsp][vocoder]")
{
    constexpr double sr = 48000.0;
    constexpr int    bs = 256;

    hispec::Vocoder voc;
    voc.prepare ({ sr, bs, 16, 2 });

    hispec::Vocoder::Params p;
    p.power = true;
    p.mix   = 1.0f;
    p.carrier = hispec::Vocoder::Carrier::Noise;
    p.vowelX = 0.5f;
    p.vowelY = 0.5f;
    p.q      = 0.5f;
    voc.setParams (p);

    juce::AudioBuffer<float> silent (2, bs);
    silent.clear();

    // First: feed silence → output should be near-zero (noise gated off)
    voc.processStereo (silent);
    REQUIRE (rms (silent, 0) < 0.02);

    // Second: feed a gated noise-ish input for 10 blocks, then silence.
    juce::AudioBuffer<float> active (2, bs);
    for (int block = 0; block < 10; ++block)
    {
        for (int i = 0; i < bs; ++i)
        {
            const float x = 0.4f * std::sin (static_cast<float> (block * bs + i) * 0.002f);
            active.getWritePointer (0)[i] = x;
            active.getWritePointer (1)[i] = x;
        }
        voc.processStereo (active);
    }
    const double activeRms = rms (active, 0);
    INFO ("noise-carrier active rms = " << activeRms);
    REQUIRE (activeRms > 0.005);

    REQUIRE (std::isfinite (activeRms));
}

TEST_CASE("Vocoder is stable with aggressive texture feedback", "[dsp][vocoder]")
{
    constexpr double sr = 48000.0;
    constexpr int    bs = 256;

    hispec::Vocoder voc;
    voc.prepare ({ sr, bs, 16, 2 });

    hispec::Vocoder::Params p;
    p.power = true;
    p.mix   = 1.0f;
    p.carrier = hispec::Vocoder::Carrier::Self;
    p.vowelX = 0.5f; p.vowelY = 0.5f; p.q = 0.8f;
    p.texDrive = 0.9f;
    p.texDepth = 0.9f;
    p.texRate  = 3.0f;
    p.texDelay = 2.0f;
    voc.setParams (p);

    juce::AudioBuffer<float> buf (2, bs);
    for (int block = 0; block < 200; ++block)
    {
        for (int i = 0; i < bs; ++i)
        {
            const float s = 0.5f * std::sin (static_cast<float> (block * bs + i) * 0.005f);
            buf.getWritePointer (0)[i] = s;
            buf.getWritePointer (1)[i] = s;
        }
        voc.processStereo (buf);

        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < bs; ++i)
            {
                const float y = buf.getReadPointer (ch)[i];
                REQUIRE (std::isfinite (y));
                REQUIRE (std::abs (y) < 20.0f);
            }
    }
}

TEST_CASE("Vocoder complex per-band path is finite and mix blends correctly", "[dsp][vocoder]")
{
    constexpr double sr = 48000.0;
    constexpr int    bs = 256;
    constexpr int    numBands = 8;

    hispec::Vocoder voc;
    voc.prepare ({ sr, bs, numBands, 2 });

    std::vector<float> centers (numBands);
    for (int k = 0; k < numBands; ++k)
        centers[static_cast<size_t> (k)] = (static_cast<float> (k) + 0.5f) * 1500.0f;
    voc.setBandCenterFrequencies (centers);

    hispec::Vocoder::Params p;
    p.power = true;
    p.mix   = 1.0f;
    p.carrier = hispec::Vocoder::Carrier::Self;
    p.vowelX = 1.0f; p.vowelY = 1.0f;
    p.q      = 0.5f;
    voc.setParams (p);

    std::vector<hispec::ComplexBuffer> bands (numBands);
    for (auto& b : bands)
    {
        b.setSize (2, bs, true);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < bs; ++i)
            {
                b.getRealWrite (ch)[i] = 0.2f * std::sin (static_cast<float> (i) * 0.01f);
                b.getImagWrite (ch)[i] = 0.2f * std::cos (static_cast<float> (i) * 0.01f);
            }
    }

    voc.processComplex (bands);

    for (auto& b : bands)
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < bs; ++i)
            {
                REQUIRE (std::isfinite (b.getRealRead (ch)[i]));
                REQUIRE (std::isfinite (b.getImagRead (ch)[i]));
            }

    // mix=0 should be a clean passthrough
    p.mix = 0.0f;
    voc.setParams (p);
    hispec::ComplexBuffer ref;
    ref.setSize (2, bs, true);
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < bs; ++i)
        {
            ref.getRealWrite (ch)[i] = 0.2f * std::sin (static_cast<float> (i) * 0.01f);
            ref.getImagWrite (ch)[i] = 0.2f * std::cos (static_cast<float> (i) * 0.01f);
        }
    std::vector<hispec::ComplexBuffer> bands2 (1);
    bands2[0].setSize (2, bs, true);
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < bs; ++i)
        {
            bands2[0].getRealWrite (ch)[i] = ref.getRealRead (ch)[i];
            bands2[0].getImagWrite (ch)[i] = ref.getImagRead (ch)[i];
        }
    voc.processComplex (bands2);
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < bs; ++i)
        {
            REQUIRE (bands2[0].getRealRead (ch)[i] == Approx (ref.getRealRead (ch)[i]).margin (1.0e-6f));
            REQUIRE (bands2[0].getImagRead (ch)[i] == Approx (ref.getImagRead (ch)[i]).margin (1.0e-6f));
        }
}
