#include "SpectralDelay.h"
#include <cmath>

namespace hispec
{

namespace
{
    constexpr double kTwoPi = 6.283185307179586476925286766559;

    inline float softClipTanh (float x) noexcept
    {
        // Fast tanh approximation sufficient for musical saturation on a
        // feedback path — see "cheap tanh" lit. Bounded in (-1, 1).
        const float x2 = x * x;
        return x * (27.0f + x2) / (27.0f + 9.0f * x2);
    }
}

void SpectralDelay::prepare (const ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    maxBlockSz = spec.maxBlockSize;
    maxBands   = spec.maxBands;
    maxDelayMs = spec.maxDelayMs;

    const int maxDelaySamples = static_cast<int> (std::ceil (maxDelayMs * 0.001 * sampleRate)) + 4;

    states.clear();
    states.resize (static_cast<size_t> (maxBands));
    for (auto& s : states)
    {
        for (int ch = 0; ch < 2; ++ch)
            for (int ri = 0; ri < 2; ++ri)
            {
                s.dl[ch][ri].setMaximumDelayInSamples (maxDelaySamples);
                s.dl[ch][ri].prepare ({ sampleRate,
                                        static_cast<juce::uint32> (maxBlockSz),
                                        1u });
                s.dl[ch][ri].reset();
            }
        s.dcBlockPrevInRe  = { 0.0f, 0.0f };
        s.dcBlockPrevOutRe = { 0.0f, 0.0f };
        s.dcBlockPrevInIm  = { 0.0f, 0.0f };
        s.dcBlockPrevOutIm = { 0.0f, 0.0f };
        s.pitchPhase       = { 0.0, 0.0 };
    }

    params.assign (static_cast<size_t> (maxBands), BandParams {});
    centersHz.assign (static_cast<size_t> (maxBands), 0.0f);
}

void SpectralDelay::reset()
{
    for (auto& s : states)
    {
        for (int ch = 0; ch < 2; ++ch)
            for (int ri = 0; ri < 2; ++ri)
                s.dl[ch][ri].reset();
        s.dcBlockPrevInRe  = { 0.0f, 0.0f };
        s.dcBlockPrevOutRe = { 0.0f, 0.0f };
        s.dcBlockPrevInIm  = { 0.0f, 0.0f };
        s.dcBlockPrevOutIm = { 0.0f, 0.0f };
        s.pitchPhase       = { 0.0, 0.0 };
    }
}

void SpectralDelay::setBandParams (const std::vector<BandParams>& p)
{
    const auto n = juce::jmin (static_cast<int> (p.size()),
                               static_cast<int> (params.size()));
    for (int i = 0; i < n; ++i)
        params[static_cast<size_t> (i)] = p[static_cast<size_t> (i)];
}

void SpectralDelay::setBandCenterFrequencies (const std::vector<float>& c)
{
    const auto n = juce::jmin (static_cast<int> (c.size()),
                               static_cast<int> (centersHz.size()));
    for (int i = 0; i < n; ++i)
        centersHz[static_cast<size_t> (i)] = c[static_cast<size_t> (i)];
}

void SpectralDelay::process (std::vector<ComplexBuffer>& bands)
{
    const int numBands = juce::jmin (static_cast<int> (bands.size()),
                                     static_cast<int> (states.size()));

    for (int k = 0; k < numBands; ++k)
    {
        auto&       state = states[static_cast<size_t> (k)];
        const auto& par   = params[static_cast<size_t> (k)];
        auto&       buf   = bands[static_cast<size_t> (k)];

        const int numSamples = buf.getNumSamples();
        const int chs        = juce::jmin (2, buf.getNumChannels());

        // Lagrange3rd needs at least 2 samples of delay to produce valid output.
        // Smaller values quietly return 0 from the JUCE delay line — clamp so
        // "delay = 0" still produces a through-path (imperceptible 2-sample delay).
        const float delaySamples = juce::jlimit (
            2.0f,
            static_cast<float> (maxDelayMs * 0.001 * sampleRate),
            par.timeMs * 0.001f * static_cast<float> (sampleRate));

        const float fb     = juce::jlimit (0.0f, 1.1f, par.fb);
        const float gainLin = juce::Decibels::decibelsToGain (par.gainDb, -72.0f);

        // Equal-power pan per channel (0 = centre). Left ch gets more at pan<0.
        const float panNorm = juce::jlimit (-1.0f, 1.0f, par.pan);
        const float panAngle = (panNorm + 1.0f) * 0.25f * juce::MathConstants<float>::pi; // 0..π/2
        const float leftGain  = std::cos (panAngle);
        const float rightGain = std::sin (panAngle);

        // Pitch-shift rate: Δf = fc · (2^(semi/12) − 1); multiply baseband
        // by e^{+j 2π Δf t} per sample.
        const float fc     = (k < static_cast<int> (centersHz.size())) ? centersHz[static_cast<size_t> (k)] : 0.0f;
        const float ratio  = std::pow (2.0f, par.pitchSt / 12.0f) - 1.0f;
        const double dwell = kTwoPi * static_cast<double> (fc) * static_cast<double> (ratio) / sampleRate;

        for (int ch = 0; ch < chs; ++ch)
        {
            float* re = buf.getRealWrite (ch);
            float* im = buf.getImagWrite (ch);

            auto& dlR = state.dl[ch][0];
            auto& dlI = state.dl[ch][1];

            const float chGain = (ch == 0 ? leftGain : rightGain) * gainLin * 2.0f;
            // ×2 to preserve centre-pan total energy (cos + sin at π/4 = √2, squared = 1)

            double pitchPh = state.pitchPhase[ch];

            // DC-blocker coefficients shorthand
            float dcInR  = state.dcBlockPrevInRe[ch];
            float dcOutR = state.dcBlockPrevOutRe[ch];
            float dcInI  = state.dcBlockPrevInIm[ch];
            float dcOutI = state.dcBlockPrevOutIm[ch];

            for (int s = 0; s < numSamples; ++s)
            {
                // Read delayed complex sample
                const float dR = dlR.popSample (0, delaySamples, true);
                const float dI = dlI.popSample (0, delaySamples, true);

                // Feedback path: tanh-saturated + DC-blocker (per real/imag)
                float fbR = softClipTanh (dR * fb);
                float fbI = softClipTanh (dI * fb);

                const float dcNewR = fbR - dcInR + kDcBlockR * dcOutR;
                dcInR  = fbR;
                dcOutR = dcNewR;
                const float dcNewI = fbI - dcInI + kDcBlockR * dcOutI;
                dcInI  = fbI;
                dcOutI = dcNewI;

                // Write to delay line: input sample + feedback from tap
                dlR.pushSample (0, re[s] + dcNewR);
                dlI.pushSample (0, im[s] + dcNewI);

                // Output: the delayed sample (pre-pitch, pre-gain)
                float outR = dR;
                float outI = dI;

                // Pitch shift via complex phase rotation
                if (std::abs (par.pitchSt) > 1.0e-3f)
                {
                    const float cph = static_cast<float> (std::cos (pitchPh));
                    const float sph = static_cast<float> (std::sin (pitchPh));
                    const float nR = outR * cph - outI * sph;
                    const float nI = outR * sph + outI * cph;
                    outR = nR;
                    outI = nI;
                    pitchPh += dwell;
                    if (pitchPh >  kTwoPi) pitchPh -= kTwoPi;
                    if (pitchPh < -kTwoPi) pitchPh += kTwoPi;
                }

                re[s] = outR * chGain;
                im[s] = outI * chGain;
            }

            state.pitchPhase[ch]        = pitchPh;
            state.dcBlockPrevInRe[ch]   = dcInR;
            state.dcBlockPrevOutRe[ch]  = dcOutR;
            state.dcBlockPrevInIm[ch]   = dcInI;
            state.dcBlockPrevOutIm[ch]  = dcOutI;
        }
    }
}

} // namespace hispec
