#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>
#include "ComplexBuffer.h"

namespace hispec
{

/**
    Abstract base class for the two FX modules that can be slotted into
    the HI-Spec signal graph:
        Slot 0 — Vocoder
        Slot 1 — SpectralFilter

    A module can run in one of two domains, depending on where the user
    places it on the position chip:

        - **Stereo** — module sees the full-band audio before or after
          the Hilbert filterbank. Used for Global Pre/Post positions.

        - **Complex per-band** — module sees each band's (re, im) streams
          individually. Used for Per-Band Pre/Post/FB positions.

    The same parameters apply in both domains; a module is free to
    behave differently in each.
*/
class FxModule
{
public:
    struct ProcessSpec
    {
        double sampleRate    = 48000.0;
        int    maxBlockSize  = 512;
        int    maxBands      = 32;
        int    numChannels   = 2;
    };

    virtual ~FxModule() = default;

    virtual void prepare (const ProcessSpec& spec) = 0;
    virtual void reset () = 0;

    /// Stereo-domain processing: in-place.
    virtual void processStereo (juce::AudioBuffer<float>& audio) = 0;

    /// Complex per-band processing: in-place on every band. The caller
    /// must set centre frequencies first via `setBandCenterFrequencies`.
    virtual void processComplex (std::vector<ComplexBuffer>& bands) = 0;

    void setBandCenterFrequencies (const std::vector<float>& c) { centersHz = c; }

protected:
    std::vector<float> centersHz;
};

} // namespace hispec
