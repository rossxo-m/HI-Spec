#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <atomic>
#include "ComplexBuffer.h"

namespace hispec
{

/**
    Hilbert/analytic complex filterbank.

    Splits a real input into `bandCount` complex (analytic) baseband streams by
    heterodyning each band down to DC with e^{-j 2π fc_k t} and lowpass
    filtering. Synthesis re-modulates and sums — reconstruction is approximate.

    Band centers are linearly spaced:  fc_k = (k + 0.5) * (sr / (2N))
    Band bandwidth ≈ sr / (2N)  (cutoff of the LPF is half the spacing).

    `setBandCount` is safe to call from the audio thread; it schedules a shadow
    rebuild that swaps in on the next `analyse` call with a one-block crossfade.
*/
class HilbertFilterbank
{
public:
    HilbertFilterbank() = default;

    struct ProcessSpec
    {
        double sampleRate    = 48000.0;
        int    maxBlockSize  = 512;
        int    numChannels   = 2;
    };

    void prepare (const ProcessSpec& spec);

    /// Schedule a band-count change. Valid: 8, 16, 32.
    void setBandCount (int n);

    int getBandCount() const noexcept { return activeBandCount; }

    /// Fixed group delay introduced by the filterbank (0 for this TPT design;
    /// the TPT lowpass has only phase delay, no fixed group delay to report).
    int getLatencySamples() const noexcept { return 0; }

    /// Center frequency of band k in Hz, given the currently active band count.
    float getBandCenterHz (int k) const noexcept;

    /// Analyse real input → N complex bands.
    ///  bandsOut is resized to activeBandCount; each buffer is
    ///  (numChannels × in.getNumSamples()).
    void analyse (const juce::AudioBuffer<float>& in,
                  std::vector<ComplexBuffer>& bandsOut);

    /// Synthesise N complex bands → real output (sum of re-modulated bands).
    /// `out` is assumed to already have the right size and channel count.
    void synthesise (const std::vector<ComplexBuffer>& bands,
                     juce::AudioBuffer<float>& out);

private:
    static constexpr int kLpfStages = 2; // cascade of 2-pole TPT → 4-pole response

    struct Band
    {
        double phase    = 0.0;  // current heterodyne phase (rad), wrapped to [0, 2π)
        double phaseInc = 0.0;  // 2π·fc/sr

        // LPFs — one per channel, per real/imag, per cascade stage.
        // Indexed [channel][0=real,1=imag][stage]
        std::array<std::array<std::array<juce::dsp::StateVariableTPTFilter<float>, kLpfStages>, 2>, 2> lpf;
    };

    void rebuildBandsForCount (int n);

    double sampleRate   = 48000.0;
    int    maxBlockSize = 512;
    int    numChannels  = 2;

    int activeBandCount = 16;
    std::atomic<int> pendingBandCount { 16 };

    std::vector<Band> bands;
};

} // namespace hispec
