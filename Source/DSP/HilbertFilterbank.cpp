#include "HilbertFilterbank.h"
#include <cmath>

namespace hispec
{

namespace
{
    constexpr double kTwoPi = 6.283185307179586476925286766559;
}

void HilbertFilterbank::prepare (const ProcessSpec& spec)
{
    sampleRate   = spec.sampleRate;
    maxBlockSize = spec.maxBlockSize;
    numChannels  = juce::jlimit (1, 2, spec.numChannels);

    activeBandCount  = juce::jlimit (1, 64, pendingBandCount.load());
    rebuildBandsForCount (activeBandCount);
}

void HilbertFilterbank::setBandCount (int n)
{
    jassert (n == 8 || n == 16 || n == 32);
    pendingBandCount.store (n);
}

float HilbertFilterbank::getBandCenterHz (int k) const noexcept
{
    const double spacing = sampleRate * 0.5 / static_cast<double> (activeBandCount);
    return static_cast<float> ((static_cast<double> (k) + 0.5) * spacing);
}

void HilbertFilterbank::rebuildBandsForCount (int n)
{
    activeBandCount = n;
    bands.clear();
    bands.resize (static_cast<size_t> (n));

    const double spacing = sampleRate * 0.5 / static_cast<double> (n);
    const double lpfCutoff = spacing * 0.5; // half-band bandwidth

    juce::dsp::ProcessSpec lpfSpec
    {
        sampleRate,
        static_cast<juce::uint32> (maxBlockSize),
        1u   // each LPF instance processes a single scalar stream
    };

    for (int k = 0; k < n; ++k)
    {
        auto& band = bands[static_cast<size_t> (k)];
        band.phase    = 0.0;
        band.phaseInc = kTwoPi * static_cast<double> (k + 0.5) * spacing / sampleRate;

        for (int ch = 0; ch < 2; ++ch)
            for (int ri = 0; ri < 2; ++ri)
                for (int stage = 0; stage < kLpfStages; ++stage)
                {
                    auto& f = band.lpf[ch][ri][stage];
                    f.prepare (lpfSpec);
                    f.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
                    f.setCutoffFrequency (static_cast<float> (lpfCutoff));
                    f.setResonance (0.5f);  // Butterworth-ish
                }
    }
}

void HilbertFilterbank::analyse (const juce::AudioBuffer<float>& in,
                                 std::vector<ComplexBuffer>& bandsOut)
{
    // Apply any pending band-count change between blocks.
    const int requested = pendingBandCount.load();
    if (requested != activeBandCount)
        rebuildBandsForCount (requested);

    const int numSamples = in.getNumSamples();
    const int chs        = juce::jmin (numChannels, in.getNumChannels());

    if (static_cast<int> (bandsOut.size()) != activeBandCount)
        bandsOut.resize (static_cast<size_t> (activeBandCount));

    for (auto& band : bandsOut)
        band.setSize (chs, numSamples, true);

    for (int k = 0; k < activeBandCount; ++k)
    {
        auto& state  = bands[static_cast<size_t> (k)];
        auto& cBand  = bandsOut[static_cast<size_t> (k)];
        double phase = state.phase;
        const double inc = state.phaseInc;

        for (int ch = 0; ch < chs; ++ch)
        {
            const float* src = in.getReadPointer (ch);
            float* dstRe = cBand.getRealWrite (ch);
            float* dstIm = cBand.getImagWrite (ch);

            double p = phase;

            // e^{-j wc t}: real = cos(wc t), imag = -sin(wc t)
            for (int s = 0; s < numSamples; ++s)
            {
                const float c = static_cast<float> (std::cos (p));
                const float si = static_cast<float> (-std::sin (p));
                float re = src[s] * c;
                float im = src[s] * si;

                // Cascade of 2-pole TPT LPFs → 4-pole response
                for (int stage = 0; stage < kLpfStages; ++stage)
                {
                    re = state.lpf[ch][0][stage].processSample (0, re);
                    im = state.lpf[ch][1][stage].processSample (0, im);
                }

                dstRe[s] = re;
                dstIm[s] = im;

                p += inc;
                if (p >= kTwoPi) p -= kTwoPi;
            }

            // Only advance the shared phase once (use the last channel's final value).
            if (ch == chs - 1)
                phase = p;
        }

        state.phase = phase;
    }
}

void HilbertFilterbank::synthesise (const std::vector<ComplexBuffer>& bandsIn,
                                    juce::AudioBuffer<float>& out)
{
    const int numSamples = out.getNumSamples();
    const int chs        = juce::jmin (numChannels, out.getNumChannels());
    const int nBands     = juce::jmin (static_cast<int> (bandsIn.size()), activeBandCount);

    out.clear();

    // Synthesis re-uses the analysis phases directly. Each band state has been
    // advanced by `analyse`; we need the phase that was ACTIVE during the block.
    // Simplest correct approach: run synthesis using a parallel phase accumulator
    // starting from (state.phase - numSamples * inc).
    for (int k = 0; k < nBands; ++k)
    {
        const auto& state = bands[static_cast<size_t> (k)];
        const auto& cBand = bandsIn[static_cast<size_t> (k)];
        const double inc  = state.phaseInc;

        // Reconstruct starting phase used during analyse:
        double p = state.phase - inc * static_cast<double> (numSamples);
        // Normalise to positive range
        p = std::fmod (p, kTwoPi);
        if (p < 0.0) p += kTwoPi;

        for (int ch = 0; ch < chs && ch < cBand.getNumChannels(); ++ch)
        {
            float* dst = out.getWritePointer (ch);
            const float* re = cBand.getRealRead (ch);
            const float* im = cBand.getImagRead (ch);

            double pp = p;
            for (int s = 0; s < numSamples; ++s)
            {
                const float c = static_cast<float> (std::cos (pp));
                const float si = static_cast<float> (std::sin (pp));
                // Re[(re + j*im) * (cos + j*sin)] = re*cos - im*sin
                dst[s] += 2.0f * (re[s] * c - im[s] * si);
                pp += inc;
                if (pp >= kTwoPi) pp -= kTwoPi;
            }
        }
    }
}

} // namespace hispec
