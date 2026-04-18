#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "PositionChip.h"

#include <memory>

namespace hispec
{
    /**
        Reusable chrome card that hosts a Vocoder or SpectralFilter body.
        Owns a power toggle, title label, Position chip, and a Mix knob. The
        body Component is passed in and gets the remaining area.
    */
    class FxModuleCard : public juce::Component
    {
    public:
        using APVTS = juce::AudioProcessorValueTreeState;

        struct Config
        {
            juce::String title;
            juce::Colour accent { 0xffb48cffu };
            juce::String powerParamID;
            juce::String mixParamID;
            juce::String positionParamID;
        };

        FxModuleCard (APVTS& apvts, juce::Component& body, Config cfg);
        ~FxModuleCard() override;

        juce::Component& getBody() noexcept                  { return body; }
        juce::ToggleButton& getPowerButton() noexcept        { return powerBtn; }
        juce::Slider&       getMixKnob() noexcept            { return mixKnob; }
        PositionChip&       getPositionChip() noexcept       { return *positionChip; }

        const juce::Colour& getAccent() const noexcept       { return cfg.accent; }

        void paint (juce::Graphics&) override;
        void resized() override;

    private:
        APVTS& apvts;
        juce::Component& body;
        Config cfg;

        juce::Label titleLabel;
        juce::ToggleButton powerBtn;
        juce::Slider mixKnob;
        juce::Label mixLabel;
        std::unique_ptr<PositionChip> positionChip;

        std::unique_ptr<APVTS::ButtonAttachment> powerAttach;
        std::unique_ptr<APVTS::SliderAttachment> mixAttach;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FxModuleCard)
    };
}
