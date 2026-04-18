#include "Vocoder.h"
#include <cmath>

namespace hispec
{

namespace
{
    constexpr double kTwoPi = 6.283185307179586476925286766559;

    inline float softClipTanh (float x) noexcept
    {
        const float x2 = x * x;
        return x * (27.0f + x2) / (27.0f + 9.0f * x2);
    }

    // Bilinear interpolation of a 3-formant triplet across the 2×2 vowel square.
    inline std::array<float, 3> morphVowel (float x, float y)
    {
        const float x0 = 1.0f - x, x1 = x;
        const float y0 = 1.0f - y, y1 = y;
        std::array<float, 3> out { 0.0f, 0.0f, 0.0f };
        for (int i = 0; i < 3; ++i)
            out[static_cast<size_t> (i)] =
                 Vocoder::kVowels[0][0][i] * x0 * y0
               + Vocoder::kVowels[0][1][i] * x0 * y1
               + Vocoder::kVowels[1][0][i] * x1 * y0
               + Vocoder::kVowels[1][1][i] * x1 * y1;
        return out;
    }
}

// Static constexpr definition (needed for ODR-use in the morphVowel helper).
constexpr float Vocoder::kVowels[2][2][3];

void Vocoder::prepare (const ProcessSpec& s)
{
    spec = s;

    for (int ch = 0; ch < 2; ++ch)
    {
        for (int f = 0; f < kNumFormants; ++f)
        {
            auto& filt = stereoSvf[static_cast<size_t> (ch)][static_cast<size_t> (f)];
            filt.reset();
            filt.prepare ({ spec.sampleRate,
                            static_cast<juce::uint32> (spec.maxBlockSize),
                            1u });
            filt.setType (juce::dsp::StateVariableTPTFilterType::bandpass);
        }

        auto& dl = texDelayStereo[static_cast<size_t> (ch)];
        const int maxTexSamples = static_cast<int> (std::ceil (8.0 * 0.001 * spec.sampleRate)) + 8;
        dl.setMaximumDelayInSamples (maxTexSamples);
        dl.prepare ({ spec.sampleRate,
                      static_cast<juce::uint32> (spec.maxBlockSize),
                      1u });
        dl.reset();
    }

    bandSvf.resize (static_cast<size_t> (spec.maxBands));
    for (auto& bank : bandSvf)
        for (int ch = 0; ch < 2; ++ch)
            for (int ri = 0; ri < 2; ++ri)
                for (int f = 0; f < kNumFormants; ++f)
                {
                    auto& filt = bank[static_cast<size_t> (ch)][static_cast<size_t> (ri)][static_cast<size_t> (f)];
                    filt.reset();
                    filt.prepare ({ spec.sampleRate,
                                    static_cast<juce::uint32> (spec.maxBlockSize),
                                    1u });
                    filt.setType (juce::dsp::StateVariableTPTFilterType::bandpass);
                }

    recomputeFormants();
}

void Vocoder::reset ()
{
    for (int ch = 0; ch < 2; ++ch)
    {
        for (int f = 0; f < kNumFormants; ++f)
            stereoSvf[static_cast<size_t> (ch)][static_cast<size_t> (f)].reset();
        texDelayStereo[static_cast<size_t> (ch)].reset();
        texLfoPhase[static_cast<size_t> (ch)] = 0.0;
        texFbState[static_cast<size_t> (ch)]  = 0.0f;
        envState[static_cast<size_t> (ch)]    = 0.0f;
    }
    for (auto& bank : bandSvf)
        for (auto& perCh : bank)
            for (auto& perRe : perCh)
                for (auto& filt : perRe)
                    filt.reset();
}

void Vocoder::setParams (const Params& p)
{
    pendingParams = p;
    params = p;
    recomputeFormants();
}

void Vocoder::recomputeFormants ()
{
    const float vx = juce::jlimit (0.0f, 1.0f, params.vowelX);
    const float vy = juce::jlimit (0.0f, 1.0f, params.vowelY);
    auto base = morphVowel (vx, vy);

    const float shiftMult = std::pow (2.0f, params.formantShift / 12.0f);
    for (int i = 0; i < 3; ++i)
        currentFormantsHz[static_cast<size_t> (i)] = base[static_cast<size_t> (i)] * shiftMult;

    // Q in [0..1] maps to resonator bandwidth scale. Narrower → more
    // resonant. Use a shaped curve so the "open" end isn't too bland.
    currentBwScale = juce::jmap (juce::jlimit (0.0f, 1.0f, params.q),
                                 0.6f, 0.08f);  // proportion of centre freq
}

void Vocoder::updateCoefficients (float centerHz, float bwHz,
                                  juce::dsp::StateVariableTPTFilter<float>& filt) const
{
    const float nyq = static_cast<float> (spec.sampleRate * 0.49);
    const float cf  = juce::jlimit (20.0f, nyq, centerHz);
    const float q   = juce::jlimit (0.3f, 25.0f, cf / juce::jmax (1.0f, bwHz));
    filt.setCutoffFrequency (cf);
    filt.setResonance (q);
}

float Vocoder::tickEnvelope (float absInput, float& state)
{
    // One-pole attack ≈ 5 ms, release ≈ 50 ms
    const double sr = spec.sampleRate;
    const float att = static_cast<float> (std::exp (-1.0 / (0.005 * sr)));
    const float rel = static_cast<float> (std::exp (-1.0 / (0.050 * sr)));
    const float coef = (absInput > state) ? att : rel;
    state = absInput + coef * (state - absInput);
    return state;
}

float Vocoder::tickNoise () noexcept
{
    return noiseDist (noiseRng);
}

float Vocoder::tickLfo (double& phase, float rateHz, LfoShape shape, bool thruZero)
{
    phase += kTwoPi * static_cast<double> (rateHz) / spec.sampleRate;
    if (phase > kTwoPi)  phase -= kTwoPi;
    if (phase < 0.0)     phase += kTwoPi;

    float v = 0.0f;
    switch (shape)
    {
        case LfoShape::Sine: v = static_cast<float> (std::sin (phase)); break;
        case LfoShape::Tri:
        {
            const float norm = static_cast<float> (phase / kTwoPi);  // 0..1
            v = (norm < 0.5f) ? (4.0f * norm - 1.0f) : (3.0f - 4.0f * norm);
            break;
        }
    }
    if (thruZero && v < 0.0f) v = -v;
    return v;
}

void Vocoder::processStereo (juce::AudioBuffer<float>& audio)
{
    if (! params.power || params.mix <= 0.0001f)
        return;

    const int chs = juce::jmin (2, audio.getNumChannels());
    const int n   = audio.getNumSamples();
    const float mix = juce::jlimit (0.0f, 1.0f, params.mix);
    const float dry = 1.0f - mix;

    // Update formant filter coefficients (block-rate is enough)
    for (int ch = 0; ch < chs; ++ch)
        for (int f = 0; f < kNumFormants; ++f)
        {
            const float cf = currentFormantsHz[static_cast<size_t> (f)];
            const float bw = juce::jmax (30.0f, cf * currentBwScale);
            updateCoefficients (cf, bw,
                                stereoSvf[static_cast<size_t> (ch)][static_cast<size_t> (f)]);
        }

    const float texDriveClamped = juce::jlimit (0.0f, 0.95f, params.texDrive);
    const float texDepthClamped = juce::jlimit (0.0f, 1.0f,  params.texDepth);
    const float texBaseMs       = juce::jlimit (0.05f, 8.0f, params.texDelay);

    for (int ch = 0; ch < chs; ++ch)
    {
        float* data = audio.getWritePointer (ch);
        auto& dl    = texDelayStereo[static_cast<size_t> (ch)];

        for (int i = 0; i < n; ++i)
        {
            const float x = data[i];
            const float absX = std::abs (x);
            const float env  = tickEnvelope (absX, envState[static_cast<size_t> (ch)]);

            // Choose carrier signal that gets run through the resonator
            float carrier = x;
            if (params.carrier == Carrier::Noise)
                carrier = tickNoise() * env;

            // Sum the parallel formant resonators
            float resSum = 0.0f;
            for (int f = 0; f < kNumFormants; ++f)
                resSum += stereoSvf[static_cast<size_t> (ch)][static_cast<size_t> (f)]
                              .processSample (0, carrier);

            // Texture: short LFO-modulated delay around the resonator bus
            float texOut = 0.0f;
            if (texDriveClamped > 0.0001f || texDepthClamped > 0.0001f)
            {
                const float lfo   = tickLfo (texLfoPhase[static_cast<size_t> (ch)],
                                             params.texRate, params.texLfoShape,
                                             params.texThruZero);
                const float texMs = juce::jlimit (0.05f, 8.0f,
                                                  texBaseMs * (1.0f + 0.8f * texDepthClamped * lfo));
                const float delaySamples = texMs * 0.001f * static_cast<float> (spec.sampleRate);

                texOut = dl.popSample (0, delaySamples, true);
                const float pushSig = softClipTanh (resSum + texDriveClamped * texFbState[static_cast<size_t> (ch)]);
                dl.pushSample (0, pushSig);
                texFbState[static_cast<size_t> (ch)] = texOut;
            }

            const float wet = resSum + texDriveClamped * texOut;
            data[i] = dry * x + mix * wet;
        }
    }
}

void Vocoder::processComplex (std::vector<ComplexBuffer>& bands)
{
    if (! params.power || params.mix <= 0.0001f)
        return;

    const int numBands = juce::jmin (static_cast<int> (bands.size()),
                                     static_cast<int> (bandSvf.size()));
    const float mix = juce::jlimit (0.0f, 1.0f, params.mix);
    const float dry = 1.0f - mix;

    for (int k = 0; k < numBands; ++k)
    {
        auto& bank = bandSvf[static_cast<size_t> (k)];
        auto& buf  = bands[static_cast<size_t> (k)];
        const int chs = juce::jmin (2, buf.getNumChannels());
        const int n   = buf.getNumSamples();

        // Coefficient update per band (block-rate)
        for (int ch = 0; ch < chs; ++ch)
            for (int ri = 0; ri < 2; ++ri)
                for (int f = 0; f < kNumFormants; ++f)
                {
                    const float cf = currentFormantsHz[static_cast<size_t> (f)];
                    const float bw = juce::jmax (30.0f, cf * currentBwScale);
                    updateCoefficients (cf, bw,
                                        bank[static_cast<size_t> (ch)][static_cast<size_t> (ri)][static_cast<size_t> (f)]);
                }

        for (int ch = 0; ch < chs; ++ch)
        {
            float* re = buf.getRealWrite (ch);
            float* im = buf.getImagWrite (ch);
            for (int i = 0; i < n; ++i)
            {
                float reSum = 0.0f, imSum = 0.0f;
                for (int f = 0; f < kNumFormants; ++f)
                {
                    reSum += bank[static_cast<size_t> (ch)][0][static_cast<size_t> (f)].processSample (0, re[i]);
                    imSum += bank[static_cast<size_t> (ch)][1][static_cast<size_t> (f)].processSample (0, im[i]);
                }
                re[i] = dry * re[i] + mix * reSum;
                im[i] = dry * im[i] + mix * imSum;
            }
        }
    }
}

} // namespace hispec
