#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

namespace hispec
{

/**
    Pair of AudioBuffers representing a complex-valued signal (real + imag).
    Same channel count and length on both halves.
*/
struct ComplexBuffer
{
    juce::AudioBuffer<float> real;
    juce::AudioBuffer<float> imag;

    void setSize (int numChannels, int numSamples, bool clear = true)
    {
        real.setSize (numChannels, numSamples, false, clear, true);
        imag.setSize (numChannels, numSamples, false, clear, true);
    }

    int getNumChannels() const noexcept { return real.getNumChannels(); }
    int getNumSamples()  const noexcept { return real.getNumSamples();  }

    void clear()
    {
        real.clear();
        imag.clear();
    }

    float* getRealWrite (int ch) noexcept  { return real.getWritePointer (ch); }
    float* getImagWrite (int ch) noexcept  { return imag.getWritePointer (ch); }
    const float* getRealRead (int ch) const noexcept { return real.getReadPointer (ch); }
    const float* getImagRead (int ch) const noexcept { return imag.getReadPointer (ch); }
};

} // namespace hispec
