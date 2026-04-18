#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <memory>

namespace hispec
{
    /**
        Thin strip under the spectrum. When a band is selected, five small
        chrome knobs bind to that band's @c delay_band<N>_{time,fb,gain,pan,pitch}
        parameters. On deselect, knobs detach and the strip shows "Global".

        The layout is fixed — we only repurpose labels/attachments on selection
        change, never re-lay-out.
    */
    class BandDetailStrip : public juce::Component
    {
    public:
        using APVTS = juce::AudioProcessorValueTreeState;

        explicit BandDetailStrip (APVTS& apvts);
        ~BandDetailStrip() override;

        /** -1 (or out-of-range) clears the binding. */
        void setSelectedBand (int idx);
        int  getSelectedBand() const noexcept { return selectedBand; }

        // Slider accessors — exposed for tests to poke knob values.
        enum Knob { Time = 0, Feedback, Gain, Pan, Pitch, kNumKnobs };
        juce::Slider& getKnob (Knob k) noexcept { return sliders[(size_t) k]; }

        void paint (juce::Graphics&) override;
        void resized() override;

    private:
        void rebindAttachments();

        APVTS& apvts;
        int selectedBand { -1 };

        std::array<juce::Slider, kNumKnobs> sliders;
        std::array<juce::Label,  kNumKnobs> labels;
        std::array<std::unique_ptr<APVTS::SliderAttachment>, kNumKnobs> attachments;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BandDetailStrip)
    };
}
