#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <vector>
#include "ComplexBuffer.h"

namespace hispec
{

/**
    Per-band complex spectral delay.

    For each band the engine maintains a pair of `juce::dsp::DelayLine`
    instances (real + imag) with Lagrange3rd interpolation. Feedback is
    applied around the complex delay-line with a tanh saturator and a
    single-pole DC blocker on the recirculated signal.

    Baseband phase rotation is used for per-band pitch shift (v1): multiply
    the baseband complex by e^{+j 2π Δf t} where Δf = fc · (2^(semi/12) − 1).
    This introduces artefacts for wide intervals — acceptable for v1.

    Panning converts the band back to stereo at output time via
    equal-power cos/sin on the band's real-valued output (after re-mod,
    performed by `HilbertFilterbank::synthesise`). This class instead
    applies a stereo pan on the complex baseband by scaling re/im per
    channel; downstream synthesis just sums channels.
*/
class SpectralDelay
{
public:
    struct ProcessSpec
    {
        double sampleRate   = 48000.0;
        int    maxBlockSize = 512;
        int    maxBands     = 32;
        float  maxDelayMs   = 2000.0f;
    };

    struct BandParams
    {
        float timeMs  = 0.0f;   // 0..maxDelayMs
        float fb      = 0.0f;   // 0..1.1
        float gainDb  = 0.0f;   // -60..+12
        float pan     = 0.0f;   // -1..+1
        float pitchSt = 0.0f;   // -24..+24 semitones
    };

    void prepare (const ProcessSpec& spec);
    void reset();

    /// Update per-band params; size of `params` is expected to be activeBandCount.
    /// Caller must ensure this vector matches the filterbank's band count.
    void setBandParams (const std::vector<BandParams>& params);

    /// Center frequencies needed to compute the pitch-shift phase rate.
    void setBandCenterFrequencies (const std::vector<float>& centersHz);

    /// Process one block in place. `bands[k]` is (numChannels × numSamples).
    /// Called AFTER filterbank::analyse, BEFORE filterbank::synthesise.
    void process (std::vector<ComplexBuffer>& bands);

private:
    struct BandState
    {
        // One pair of delay lines per channel.
        // Index [channel][0=real,1=imag]
        std::array<std::array<juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Lagrange3rd>, 2>, 2> dl;

        // DC-blocker state on feedback (per channel, complex)
        std::array<float, 2> dcBlockPrevInRe  { 0.0f, 0.0f };
        std::array<float, 2> dcBlockPrevOutRe { 0.0f, 0.0f };
        std::array<float, 2> dcBlockPrevInIm  { 0.0f, 0.0f };
        std::array<float, 2> dcBlockPrevOutIm { 0.0f, 0.0f };

        // Pitch-shift phase accumulator (per channel)
        std::array<double, 2> pitchPhase { 0.0, 0.0 };
    };

    double sampleRate  = 48000.0;
    int    maxBlockSz  = 512;
    int    maxBands    = 32;
    float  maxDelayMs  = 2000.0f;

    std::vector<BandState>  states;
    std::vector<BandParams> params;
    std::vector<float>      centersHz;

    static constexpr float kDcBlockR = 0.995f;
};

} // namespace hispec
