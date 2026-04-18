#pragma once

#include <juce_dsp/juce_dsp.h>
#include <array>
#include <random>
#include "FxModule.h"

namespace hispec
{

/**
    Resonator-style vocoder.

    The module behaves as a 3-formant parallel bandpass resonator
    (driven by Hillenbrand 1995 adult-male vowel table, 4 corners:
    /a/, /e/, /i/, /o/) that gets bilinearly morphed by a 2D
    `(vowelX, vowelY)` control. The modulator is always the input
    (plugin sidechain is out of scope for v1). The carrier is either:

        - **Self**: the input itself (classical "resonant EQ" mode).
        - **Noise**: white noise gated by a one-pole envelope on the
          input's magnitude (attack ≈ 5 ms / release ≈ 50 ms).

    A **Texture** section adds a short modulated feedback delay inside
    the resonator summing bus, producing short comb/chorus effects.
*/
class Vocoder : public FxModule
{
public:
    enum class Carrier { Self = 0, Noise = 1 };
    enum class LfoShape { Sine = 0, Tri = 1 };

    struct Params
    {
        bool     power         = false;
        float    mix           = 1.0f;   // 0..1
        Carrier  carrier       = Carrier::Self;
        float    vowelX        = 0.5f;   // 0..1
        float    vowelY        = 0.5f;   // 0..1
        float    q             = 0.5f;   // 0..1
        float    formantShift  = 0.0f;   // ±24 semitones
        float    texDrive      = 0.0f;   // 0..1
        float    texRate       = 1.0f;   // 0..20 Hz
        float    texDepth      = 0.0f;   // 0..1
        float    texDelay      = 2.0f;   // 0.05..8 ms
        LfoShape texLfoShape   = LfoShape::Sine;
        bool     texThruZero   = false;
    };

    void prepare (const ProcessSpec& spec) override;
    void reset () override;

    void setParams (const Params& p);

    void processStereo  (juce::AudioBuffer<float>& audio) override;
    void processComplex (std::vector<ComplexBuffer>& bands) override;

    // Diagnostic accessors (also useful for tests)
    std::array<float, 3> getFormants() const noexcept { return currentFormantsHz; }

    static constexpr int kNumFormants = 2;  // 3 would be ideal; we drop
                                            // the F3 resonator for v1 to
                                            // keep CPU cheap. Formant
                                            // table still carries F3 for
                                            // future use.

    // Hillenbrand 1995 adult-male averages; we keep a 2×2 corner grid.
    // X axis: /o/ → /a/  (low frontness ↔ high frontness of tongue body)
    // Y axis: /i/ → /e/  (high tongue height ↔ mid)
    // Corners chosen so the square loosely covers the IPA vowel chart.
    static constexpr float kVowels[2][2][3] = {
        // y = 0 (top)          // y = 1 (bottom)
        { { 270.f, 2290.f, 3010.f }, { 600.f, 1930.f, 2550.f } },  // x = 0 (/i/, /e/)
        { { 570.f, 840.f,  2410.f }, { 730.f, 1090.f, 2440.f } }   // x = 1 (/o/, /a/)
    };

private:

    void     updateCoefficients (float centerHz, float bwHz,
                                 juce::dsp::StateVariableTPTFilter<float>& filt) const;
    void     recomputeFormants();
    float    tickEnvelope (float absInput, float& state);
    float    tickNoise () noexcept;
    float    tickLfo (double& phase, float rateHz, LfoShape shape, bool thruZero);

    ProcessSpec spec;
    Params      params;
    Params      pendingParams;

    // Current formants (after vowel morph + formant shift)
    std::array<float, 3> currentFormantsHz { 730.f, 1090.f, 2440.f };
    float                currentBwScale    = 1.0f;

    // Per-channel SVF bank for stereo path
    std::array<std::array<juce::dsp::StateVariableTPTFilter<float>, kNumFormants>, 2> stereoSvf;

    // Per-band × channel × re/im SVF bank for complex path
    std::vector<std::array<std::array<std::array<juce::dsp::StateVariableTPTFilter<float>, kNumFormants>, 2>, 2>> bandSvf;

    // Envelope follower state per channel (for Noise-carrier gating)
    std::array<float, 2> envState { 0.0f, 0.0f };

    // Noise source (per-block reseed-less white)
    std::mt19937 noiseRng { 0xC0FFEE };
    std::uniform_real_distribution<float> noiseDist { -1.0f, 1.0f };

    // Texture delay lines (stereo path)
    std::array<juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Lagrange3rd>, 2> texDelayStereo;
    std::array<float, 2> texFbState { 0.0f, 0.0f };

    // Texture LFO phase per channel
    std::array<double, 2> texLfoPhase { 0.0, 0.0 };
};

} // namespace hispec
