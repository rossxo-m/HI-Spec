#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "VowelPad.h"
#include <memory>
#include <vector>

namespace hispec
{
    /**
        Inside contents of the Vocoder FxModuleCard. Layout:

            ┌──────────────────────────────────────────────┐
            │  Carrier  │       VowelPad           │ Q  Shift│
            │  [Self]   │   A ─── E                │         │
            │  [Noise]  │   │     │                │         │
            │           │   I ─── O                │         │
            ├──────────────────────────────────────────────┤
            │  Texture:  Drive Rate Depth Delay   [Sine|Tri]│
            └──────────────────────────────────────────────┘
    */
    class VocoderBody : public juce::Component
    {
    public:
        using APVTS = juce::AudioProcessorValueTreeState;

        explicit VocoderBody (APVTS& apvts);
        ~VocoderBody() override;

        void paint (juce::Graphics&) override;
        void resized() override;

        VowelPad& getVowelPad() noexcept { return vowelPad; }

        juce::Rectangle<int> getMainRowBounds()    const { return mainRowBounds; }
        juce::Rectangle<int> getTextureRowBounds() const { return textureRowBounds; }

    private:
        void addKnob (juce::Slider& s, juce::Label& l,
                      const juce::String& caption, const juce::String& paramId,
                      std::unique_ptr<APVTS::SliderAttachment>& attach);

        APVTS& apvts;

        // Carrier segmented buttons (Self / Noise).
        juce::TextButton selfBtn { "Self" }, noiseBtn { "Noise" };
        std::unique_ptr<juce::ParameterAttachment> carrierAttach;

        // Vowel XY pad.
        VowelPad vowelPad;

        // Q + Formant shift.
        juce::Slider qKnob, shiftKnob;
        juce::Label  qLabel, shiftLabel;
        std::unique_ptr<APVTS::SliderAttachment> qAttach, shiftAttach;

        // Texture row.
        juce::Label textureHeading;
        juce::Slider driveKnob, rateKnob, depthKnob, delayKnob;
        juce::Label  driveLabel, rateLabel, depthLabel, delayLabel;
        std::unique_ptr<APVTS::SliderAttachment> driveAttach, rateAttach, depthAttach, delayAttach;

        juce::TextButton sineBtn { "Sine" }, triBtn { "Tri" };
        std::unique_ptr<juce::ParameterAttachment> lfoShapeAttach;
        juce::TextButton thruZeroBtn { "Thru-Zero" };
        std::unique_ptr<APVTS::ButtonAttachment>    thruZeroAttach;

        juce::Rectangle<int> mainRowBounds, textureRowBounds;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VocoderBody)
    };
}
