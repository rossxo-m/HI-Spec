#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Assets/chrome_gradient.h"

namespace hispec
{
    /**
        Single LookAndFeel for the entire HI-Spec plugin.

        Provides chrome knobs, aqua-gel buttons, and mercury text defaults.
        All component-specific chrome is painted through helpers in
        @ref hispec::palette, so components that want non-standard chrome
        (bezels, cards, etc.) can paint themselves without fighting the LNF.
    */
    class LookAndFeel_HISpec : public juce::LookAndFeel_V4
    {
    public:
        LookAndFeel_HISpec();
        ~LookAndFeel_HISpec() override = default;

        // Rotary slider: chrome ring, inky pointer, dot cap, faint value arc.
        void drawRotarySlider (juce::Graphics& g,
                               int x, int y, int width, int height,
                               float sliderPos,
                               float rotaryStartAngle,
                               float rotaryEndAngle,
                               juce::Slider&) override;

        // Toggle / text buttons render as aqua-gel pills.
        void drawButtonBackground (juce::Graphics& g,
                                   juce::Button& b,
                                   const juce::Colour& backgroundColour,
                                   bool shouldDrawButtonAsHighlighted,
                                   bool shouldDrawButtonAsDown) override;

        juce::Font getLabelFont (juce::Label&) override;
        juce::Font getPopupMenuFont() override;

    private:
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LookAndFeel_HISpec)
    };
}
