#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <memory>
#include <array>
#include <vector>

#include "HilbertFilterbank.h"
#include "SpectralDelay.h"
#include "Vocoder.h"
#include "SpectralFilter.h"
#include "FxModule.h"
#include "Parameters/ParameterIDs.h"

namespace hispec
{

/**
    Owns the fixed engine pipeline and wires it together every block.

    The graph topology, evaluated per-block based on the current position
    parameters:

        input (stereo)
            │
            ▼
        [Pre Global FX]    ← any module whose pos == GlobalPre
            │
            ▼
        HilbertFilterbank.analyse()  →  std::vector<ComplexBuffer> bands
            │
            ▼
        [Pre PerBand FX]   ← any module whose pos == PerBandPre
            │
            ▼
        SpectralDelay.process(bands)
            │
            ▼
        [Post PerBand FX]  ← any module whose pos == PerBandPost
            │
            ▼
        HilbertFilterbank.synthesise()
            │
            ▼
        [Post Global FX]   ← any module whose pos == GlobalPost
            │
            ▼
        output (stereo)

    The FB position is recognised by the APVTS layout but not implemented
    in v1 (it would need a hook inside SpectralDelay's recirculation loop,
    which is deferred).
*/
class Graph
{
public:
    static constexpr int kNumModules = 2;  // 0 = Vocoder, 1 = SpectralFilter

    struct ProcessSpec
    {
        double sampleRate   = 48000.0;
        int    maxBlockSize = 512;
    };

    Graph();

    void prepare (const ProcessSpec& spec);
    void reset ();

    /// Process a full stereo block in-place. Pulls every relevant parameter
    /// from `apvts` (atomic reads) before wiring up per-band state.
    void process (juce::AudioBuffer<float>& buffer,
                  juce::AudioProcessorValueTreeState& apvts);

    /// Report PDC latency in samples. This is the fixed part of the graph
    /// (Hilbert group delay only). The musical delay is NOT reported.
    int getLatencySamples () const noexcept;

    // Accessors for tests / UI
    int                  getActiveBandCount () const noexcept { return activeBandCount; }
    const std::vector<float>& getBandCenterFrequencies () const noexcept { return centerFrequencies; }

private:
    void renderBandParamsFromApvts (juce::AudioProcessorValueTreeState& apvts);
    void renderModuleParamsFromApvts (juce::AudioProcessorValueTreeState& apvts);

    ids::Position positionFor (int moduleSlot) const noexcept;

    ProcessSpec spec;

    HilbertFilterbank  filterbank;
    SpectralDelay      delay;
    std::array<std::unique_ptr<FxModule>, kNumModules> fx;

    std::vector<ComplexBuffer>              bands;
    std::vector<SpectralDelay::BandParams>  bandParams;
    std::vector<float>                      centerFrequencies;

    std::array<ids::Position, kNumModules>  modulePositions { ids::Position::Off, ids::Position::Off };

    int activeBandCount = 16;
    int fixedLatencySamples = 0;
};

} // namespace hispec
