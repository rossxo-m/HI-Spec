#include "SpectralFilter.h"
#include <cmath>

namespace hispec
{

namespace
{
    constexpr double kTwoPi = 6.283185307179586476925286766559;
}

void SpectralFilter::prepare (const ProcessSpec& s)
{
    spec = s;
    for (int ch = 0; ch < 2; ++ch)
    {
        auto& filt = stereoSvf[static_cast<size_t> (ch)];
        filt.reset();
        filt.prepare ({ spec.sampleRate,
                        static_cast<juce::uint32> (spec.maxBlockSize),
                        1u });
        filt.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
    }
    reset();
}

void SpectralFilter::reset ()
{
    for (auto& filt : stereoSvf)
        filt.reset();
    envState = 0.0f;
    lfoPhase = 0.0;
    sampleHoldValue = 0.0f;
    sampleHoldPhaseLast = 0.0;
}

void SpectralFilter::setParams (const Params& p)
{
    params = p;
}

float SpectralFilter::evalMagnitude (Shape shape, float freqHz, float cutoffHz,
                                     float q, float gainDb) noexcept
{
    // Normalised frequency relative to cutoff
    const float r = juce::jmax (1.0e-4f, freqHz / juce::jmax (1.0f, cutoffHz));
    const float r2 = r * r;
    const float qClamped = juce::jlimit (0.3f, 20.0f, q);

    // 2nd-order Butterworth LP/HP prototype + standard analog BP/Notch/Peak/Tilt
    const float lp = 1.0f / std::sqrt (1.0f + r2 * r2);
    const float hp = r2    / std::sqrt (1.0f + r2 * r2);

    // Analog BP: |H| = (ω/Q) / sqrt((1-ω²)² + (ω/Q)²)  with ω = r
    const float bpNum = r / qClamped;
    const float bpDen = std::sqrt ((1.0f - r2) * (1.0f - r2) + bpNum * bpNum);
    const float bp = bpNum / juce::jmax (1.0e-8f, bpDen);

    switch (shape)
    {
        case Shape::LP:     return lp;
        case Shape::HP:     return hp;
        case Shape::BP:     return bp;
        case Shape::Notch:  return std::sqrt (juce::jmax (0.0f, 1.0f - bp * bp));
        case Shape::Peak:
        {
            const float g = juce::Decibels::decibelsToGain (gainDb, -60.0f);
            return 1.0f + (g - 1.0f) * bp;
        }
        case Shape::Tilt:
        {
            const float g = juce::Decibels::decibelsToGain (gainDb * 0.5f, -60.0f);
            // Boost highs by g, cut lows by 1/g (or vice versa for negative)
            return g * hp + (1.0f / juce::jmax (1.0e-4f, g)) * lp + bp * 0.25f;
        }
    }
    return 1.0f;
}

float SpectralFilter::currentCutoff () const
{
    return params.freq;
}

float SpectralFilter::tickLfo ()
{
    const float rate = juce::jlimit (0.0f, 20.0f, params.lfoRate);
    const double inc = kTwoPi * static_cast<double> (rate) / spec.sampleRate;
    const double prevPhase = lfoPhase;
    lfoPhase += inc;
    if (lfoPhase > kTwoPi) lfoPhase -= kTwoPi;

    switch (params.lfoShape)
    {
        case LfoShape::Sine:
            return static_cast<float> (std::sin (lfoPhase));
        case LfoShape::Tri:
        {
            const float norm = static_cast<float> (lfoPhase / kTwoPi);
            return (norm < 0.5f) ? (4.0f * norm - 1.0f) : (3.0f - 4.0f * norm);
        }
        case LfoShape::SampleAndHold:
        {
            // Wrap-around indicates a new period → draw a new sample
            if (lfoPhase < prevPhase)
                sampleHoldValue = distSH (rngSH);
            return sampleHoldValue;
        }
    }
    return 0.0f;
}

float SpectralFilter::tickEnv (float absInput)
{
    const double sr = spec.sampleRate;
    const float att = static_cast<float> (std::exp (-1.0 / (juce::jmax (0.001f, params.envAttack)  * sr)));
    const float rel = static_cast<float> (std::exp (-1.0 / (juce::jmax (0.001f, params.envRelease) * sr)));
    const float coef = (absInput > envState) ? att : rel;
    envState = absInput + coef * (envState - absInput);
    return envState;
}

void SpectralFilter::processStereo (juce::AudioBuffer<float>& audio)
{
    if (! params.power || params.mix <= 0.0001f)
        return;

    const int chs = juce::jmin (2, audio.getNumChannels());
    const int n   = audio.getNumSamples();
    const float mix = juce::jlimit (0.0f, 1.0f, params.mix);
    const float dry = 1.0f - mix;

    // Pick the SVF mode that best matches the shape for the stereo path.
    juce::dsp::StateVariableTPTFilterType mode =
        juce::dsp::StateVariableTPTFilterType::lowpass;
    switch (params.shape)
    {
        case Shape::LP:    mode = juce::dsp::StateVariableTPTFilterType::lowpass;  break;
        case Shape::HP:    mode = juce::dsp::StateVariableTPTFilterType::highpass; break;
        case Shape::BP:
        case Shape::Peak:
        case Shape::Tilt:  mode = juce::dsp::StateVariableTPTFilterType::bandpass; break;
        case Shape::Notch: mode = juce::dsp::StateVariableTPTFilterType::bandpass; break;
    }

    for (auto& filt : stereoSvf)
    {
        filt.setType (mode);
        filt.setCutoffFrequency (juce::jlimit (20.0f,
                                               static_cast<float> (spec.sampleRate * 0.49),
                                               currentCutoff()));
        filt.setResonance (juce::jlimit (0.3f, 20.0f, params.q));
    }

    for (int ch = 0; ch < chs; ++ch)
    {
        float* data = audio.getWritePointer (ch);
        auto& filt  = stereoSvf[static_cast<size_t> (ch)];
        for (int i = 0; i < n; ++i)
        {
            const float x = data[i];
            const float filtered = filt.processSample (0, x);
            float y = filtered;
            switch (params.shape)
            {
                case Shape::Notch: y = x - filtered; break;
                case Shape::Peak:
                {
                    const float g = juce::Decibels::decibelsToGain (params.gainDb, -60.0f);
                    y = x + (g - 1.0f) * filtered;
                    break;
                }
                case Shape::Tilt:
                {
                    const float g = juce::Decibels::decibelsToGain (params.gainDb * 0.5f, -60.0f);
                    y = g * x + (1.0f - g) * filtered;
                    break;
                }
                default: break;
            }
            data[i] = dry * x + mix * y;
        }
    }
}

void SpectralFilter::processComplex (std::vector<ComplexBuffer>& bands)
{
    if (! params.power || params.mix <= 0.0001f)
        return;

    const int numBands = juce::jmin (static_cast<int> (bands.size()),
                                     static_cast<int> (centersHz.size()));
    if (numBands <= 0)
        return;

    const float mix = juce::jlimit (0.0f, 1.0f, params.mix);
    const float dry = 1.0f - mix;

    // Per-sample modulation is overkill; a single block-rate evaluation is
    // usually inaudible for LFO/env rates we care about. Compute the
    // modulated cutoff using the last envelope state and the LFO evaluated
    // at the first sample of the block.
    float avgAbs = 0.0f;
    int   sampleCount = 0;
    for (int k = 0; k < numBands; ++k)
    {
        auto& buf = bands[static_cast<size_t> (k)];
        const int chs = juce::jmin (2, buf.getNumChannels());
        const int n   = buf.getNumSamples();
        for (int ch = 0; ch < chs; ++ch)
        {
            const float* re = buf.getRealRead (ch);
            const float* im = buf.getImagRead (ch);
            for (int i = 0; i < n; ++i)
            {
                avgAbs += std::sqrt (re[i] * re[i] + im[i] * im[i]);
                ++sampleCount;
            }
        }
    }
    if (sampleCount > 0) avgAbs /= static_cast<float> (sampleCount);

    const float envVal = tickEnv (avgAbs);
    const float lfoVal = tickLfo();
    const float lfoMod = juce::jlimit (0.0f, 1.0f, params.lfoDepth) * lfoVal * 2.0f; // ±2 oct
    const float envMod = juce::jlimit (-1.0f, 1.0f, params.envDepth) * envVal * 2.0f;
    const float baseLog = std::log2 (juce::jmax (20.0f, params.freq));
    const float modulatedCutoff = juce::jlimit (20.0f,
                                                static_cast<float> (spec.sampleRate * 0.49),
                                                std::exp2 (baseLog + lfoMod + envMod));

    // Render per-band magnitude response
    std::vector<float> gains (static_cast<size_t> (numBands), 1.0f);
    for (int k = 0; k < numBands; ++k)
        gains[static_cast<size_t> (k)] =
            evalMagnitude (params.shape,
                           centersHz[static_cast<size_t> (k)],
                           modulatedCutoff,
                           params.q,
                           params.gainDb);

    // 3-tap gaussian smoothing across neighbours (0.25, 0.5, 0.25)
    if (numBands >= 3)
    {
        std::vector<float> smoothed (static_cast<size_t> (numBands));
        smoothed[0] = gains[0];
        smoothed[static_cast<size_t> (numBands - 1)] = gains[static_cast<size_t> (numBands - 1)];
        for (int k = 1; k < numBands - 1; ++k)
            smoothed[static_cast<size_t> (k)] = 0.25f * gains[static_cast<size_t> (k - 1)]
                                              + 0.5f  * gains[static_cast<size_t> (k)]
                                              + 0.25f * gains[static_cast<size_t> (k + 1)];
        gains.swap (smoothed);
    }

    // Apply per-band
    for (int k = 0; k < numBands; ++k)
    {
        auto& buf = bands[static_cast<size_t> (k)];
        const int chs = juce::jmin (2, buf.getNumChannels());
        const int n   = buf.getNumSamples();
        const float g = gains[static_cast<size_t> (k)];
        const float blendedG = dry + mix * g;
        for (int ch = 0; ch < chs; ++ch)
        {
            float* re = buf.getRealWrite (ch);
            float* im = buf.getImagWrite (ch);
            for (int i = 0; i < n; ++i)
            {
                re[i] *= blendedG;
                im[i] *= blendedG;
            }
        }
    }
}

} // namespace hispec
