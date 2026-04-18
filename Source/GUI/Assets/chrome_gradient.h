#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <initializer_list>
#include <utility>

namespace hispec::palette
{
    namespace colours
    {
        // Chrome ramp (black → white)
        inline const juce::Colour chromeLo { 0xff050608u };
        inline const juce::Colour chrome6  { 0xff14171cu };
        inline const juce::Colour chrome5  { 0xff2a2f38u };
        inline const juce::Colour chrome4  { 0xff4a525eu };
        inline const juce::Colour chrome3  { 0xff8a93a3u };
        inline const juce::Colour chrome2  { 0xffc8d0dcu };
        inline const juce::Colour chrome1  { 0xfff0f4fau };
        inline const juce::Colour chromeHi { 0xffffffffu };

        // Accents
        inline const juce::Colour aqua     { 0xff66c8ffu };
        inline const juce::Colour aquaDeep { 0xff0078d4u };
        inline const juce::Colour aero     { 0xff88e0c8u };
        inline const juce::Colour sky      { 0xffb8e8ffu };
        inline const juce::Colour plasma   { 0xff6effe8u };
        inline const juce::Colour violet   { 0xffb48cffu };
        inline const juce::Colour hot      { 0xffff6ec7u };
        inline const juce::Colour acid     { 0xffc0ff5au };
        inline const juce::Colour amber    { 0xffffae3bu };
        inline const juce::Colour magenta  { 0xffd769e0u };

        // Text
        inline const juce::Colour text     { 0xfff0f4fau };
        inline const juce::Colour textMid  { 0xffb0b8c4u };
        inline const juce::Colour textDim  { 0xff6a7180u };

        // Background
        inline const juce::Colour bgTop    { 0xff1a2030u };
        inline const juce::Colour bgMid    { 0xff0a0d14u };
        inline const juce::Colour bgBot    { 0xff050608u };
    }

    // Linear top→bottom gradient helper. Stops are pairs of (0..1 position, colour).
    juce::ColourGradient vertical (juce::Rectangle<float> area,
                                   std::initializer_list<std::pair<float, juce::Colour>> stops);

    // 9-stop linear chrome bezel, rendered vertically. The 9 stops alternate
    // light/dark to approximate a conic chrome look when painted on a bezel ring.
    juce::ColourGradient chromeBezel (juce::Rectangle<float> area);

    // Paint the chrome bezel into a rectangle with a subtle 1px dark border.
    void paintChromeBezel (juce::Graphics& g, juce::Rectangle<float> area, float cornerRadius = 6.0f);

    // Aqua-gel button/pill gradient. Radial: brighter-accent in top-third,
    // fading to accent then slightly darker accent at bottom.
    // Paints a specular highlight ellipse in the top third for the Aqua look.
    void paintAquaGel (juce::Graphics& g, juce::Rectangle<float> area,
                       juce::Colour accent = colours::aqua, float cornerRadius = 8.0f);

    // Frosted aero panel — translucent white over a dithered mid-tone.
    void paintFrostedAero (juce::Graphics& g, juce::Rectangle<float> area, float cornerRadius = 10.0f);

    // Ambient plugin background (radial blue + violet glow over dark).
    void paintPluginBackground (juce::Graphics& g, juce::Rectangle<float> area);
}
