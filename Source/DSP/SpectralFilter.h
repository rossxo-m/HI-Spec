#pragma once

#include <juce_dsp/juce_dsp.h>
#include <array>
#include <random>
#include "FxModule.h"

namespace hispec
{

/**
    Per-band spectral filter.

    In the complex-per-band path this module evaluates a classical 2nd-order
    analog-prototype magnitude response at each band's centre frequency,
    3-tap-gaussian-smooths the resulting gain vector across neighbouring
    bands (to avoid stairstepping), then multiplies the band's (re, im)
    stream by that gain. Phase is passed through unchanged — this is
    deliberately magnitude-only for v1.

    In the stereo path the same shape is applied via a real 2nd-order
    SVF filter (LP, HP, BP, Notch) or an SVF-derived peaking/tilt curve.

    `sf_freq` is modulated by:
        - an LFO (sine/tri/S&H) whose depth spans ±2 octaves × `sf_lfoDepth`
        - a one-pole envelope follower on the input |sum| whose depth is
          bipolar (−100..+100% of ±2 octaves)
*/
class SpectralFilter : public FxModule
{
public:
    enum class Shape   { LP = 0, HP = 1, BP = 2, Notch = 3, Peak = 4, Tilt = 5 };
    enum class LfoShape { Sine = 0, Tri = 1, SampleAndHold = 2 };

    struct Params
    {
        bool     power      = false;
        float    mix        = 1.0f;   // 0..1
        Shape    shape      = Shape::LP;
        float    freq       = 1000.0f; // Hz (pre-modulation)
        float    q          = 0.707f;  // 0.3..20
        float    gainDb     = 0.0f;    // -24..+24  (Peak / Tilt only)
        float    lfoRate    = 0.5f;    // 0..20 Hz
        float    lfoDepth   = 0.0f;    // 0..1    (0 = off)
        LfoShape lfoShape   = LfoShape::Sine;
        float    envAttack  = 0.010f;  // s
        float    envRelease = 0.150f;  // s
        float    envDepth   = 0.0f;    // -1..+1
    };

    void prepare (const ProcessSpec& spec) override;
    void reset () override;

    void setParams (const Params& p);

    void processStereo  (juce::AudioBuffer<float>& audio) override;
    void processComplex (std::vector<ComplexBuffer>& bands) override;

    // Diagnostic: evaluate the shape's magnitude (linear) at freqHz given
    // a current cutoff. Exposed so tests can crosscheck.
    static float evalMagnitude (Shape shape, float freqHz, float cutoffHz,
                                float q, float gainDb) noexcept;

private:
    float  currentCutoff () const;
    float  tickLfo ();
    float  tickEnv (float absInput);

    ProcessSpec spec;
    Params      params;

    // Stereo path SVF (mode set from shape each block)
    std::array<juce::dsp::StateVariableTPTFilter<float>, 2> stereoSvf;

    // Envelope follower state (single mono follower on input sum)
    float envState = 0.0f;

    // LFO state
    double lfoPhase = 0.0;
    float  sampleHoldValue = 0.0f;
    double sampleHoldPhaseLast = 0.0;

    std::mt19937 rngSH { 0xB00B5 };
    std::uniform_real_distribution<float> distSH { -1.0f, 1.0f };
};

} // namespace hispec
