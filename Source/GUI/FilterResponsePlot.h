#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <memory>

namespace hispec
{
    /**
        Draws the magnitude response of a configurable biquad shape (LP/HP/BP/
        Notch/Peak/Tilt) over a log-frequency axis. Samples the response at 128
        log-spaced points between 20 Hz and 20 kHz each repaint.

        Exposes getMagnitudeAt() so tests can assert shape behavior.
    */
    class FilterResponsePlot : public juce::Component,
                               private juce::Timer
    {
    public:
        using APVTS = juce::AudioProcessorValueTreeState;

        static constexpr int kNumPoints = 128;

        explicit FilterResponsePlot (APVTS& apvts);
        ~FilterResponsePlot() override;

        void paint (juce::Graphics&) override;

        /** Returns the current magnitude (linear, not dB) at a frequency. */
        float getMagnitudeAt (float freqHz) const;

        /** Recomputes the 128-sample response from current param values. */
        void recomputeNow();

    private:
        void timerCallback() override;

        APVTS& apvts;

        // Log-spaced frequency axis (Hz) and corresponding linear magnitude.
        std::array<float, kNumPoints> freqs  {};
        std::array<float, kNumPoints> mags   {};

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FilterResponsePlot)
    };
}
