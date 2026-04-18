#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "FilterResponsePlot.h"
#include <memory>
#include <vector>

namespace hispec
{
    /**
        Inside of the Spectral Filter FxModuleCard. Layout:

            ┌────────────────────────────────────────────┐
            │  [LP HP BP Notch]                           │
            │                                             │
            │   FilterResponsePlot (live curve)           │
            │                                             │
            │                          Freq  Q            │
            ├────────────────────────────────────────────┤
            │  Mod:  Rate LFOAmt EnvAmt Atk Rel  [Sine|Tri|S&H] │
            └────────────────────────────────────────────┘
    */
    class SpectralFilterBody : public juce::Component
    {
    public:
        using APVTS = juce::AudioProcessorValueTreeState;

        explicit SpectralFilterBody (APVTS& apvts);
        ~SpectralFilterBody() override;

        void paint (juce::Graphics&) override;
        void resized() override;

        FilterResponsePlot& getPlot() noexcept { return plot; }

        juce::Rectangle<int> getMainRowBounds() const { return mainRowBounds; }
        juce::Rectangle<int> getModRowBounds()  const { return modRowBounds; }

    private:
        void addKnob (juce::Slider& s, juce::Label& l,
                      const juce::String& caption, const juce::String& paramId,
                      std::unique_ptr<APVTS::SliderAttachment>& attach);

        APVTS& apvts;

        // Shape segmented buttons (LP/HP/BP/Notch).
        juce::TextButton lpBtn { "LP" }, hpBtn { "HP" }, bpBtn { "BP" }, notchBtn { "Notch" };
        std::unique_ptr<juce::ParameterAttachment> shapeAttach;

        // Response curve plot.
        FilterResponsePlot plot;

        // Freq + Q.
        juce::Slider freqKnob, qKnob;
        juce::Label  freqLabel, qLabel;
        std::unique_ptr<APVTS::SliderAttachment> freqAttach, qAttach;

        // Mod row.
        juce::Label  modHeading;
        juce::Slider rateKnob, lfoAmtKnob, envAmtKnob, atkKnob, relKnob;
        juce::Label  rateLabel, lfoAmtLabel, envAmtLabel, atkLabel, relLabel;
        std::unique_ptr<APVTS::SliderAttachment> rateAttach, lfoAmtAttach, envAmtAttach, atkAttach, relAttach;

        juce::TextButton sineBtn { "Sine" }, triBtn { "Tri" }, shBtn { "S&H" };
        std::unique_ptr<juce::ParameterAttachment> lfoShapeAttach;

        juce::Rectangle<int> mainRowBounds, modRowBounds;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpectralFilterBody)
    };
}
