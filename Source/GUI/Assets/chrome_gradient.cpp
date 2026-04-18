#include "chrome_gradient.h"

namespace hispec::palette
{
    juce::ColourGradient vertical (juce::Rectangle<float> area,
                                   std::initializer_list<std::pair<float, juce::Colour>> stops)
    {
        auto it = stops.begin();
        juce::ColourGradient g (it->second,
                                area.getCentreX(), area.getY(),
                                it->second,
                                area.getCentreX(), area.getBottom(),
                                false);
        // The ctor already sets position-0 and position-1; reset and re-add.
        g.clearColours();
        for (auto& [t, c] : stops)
            g.addColour (juce::jlimit (0.0f, 1.0f, t), c);
        return g;
    }

    juce::ColourGradient chromeBezel (juce::Rectangle<float> area)
    {
        // 9-stop alternating light/dark ramp approximating a conic bezel.
        // Stops sit at the centres of 9 equal vertical bands so sampling at
        // ((i + 0.5) / 9) of the height hits the intended colour.
        constexpr float one9 = 1.0f / 9.0f;
        return vertical (area, {
            { 0.5f * one9, juce::Colour (0xfff8f8f8u) },
            { 1.5f * one9, juce::Colour (0xffd0d0d0u) },
            { 2.5f * one9, juce::Colour (0xff7a7a7au) },
            { 3.5f * one9, juce::Colour (0xffefefefu) },
            { 4.5f * one9, juce::Colour (0xff9a9a9au) },
            { 5.5f * one9, juce::Colour (0xffcfcfcfu) },
            { 6.5f * one9, juce::Colour (0xff6d6d6du) },
            { 7.5f * one9, juce::Colour (0xffe8e8e8u) },
            { 8.5f * one9, juce::Colour (0xfff8f8f8u) }
        });
    }

    void paintChromeBezel (juce::Graphics& g, juce::Rectangle<float> area, float cornerRadius)
    {
        g.setGradientFill (chromeBezel (area));
        g.fillRoundedRectangle (area, cornerRadius);

        g.setColour (juce::Colours::black.withAlpha (0.8f));
        g.drawRoundedRectangle (area.reduced (0.5f), cornerRadius, 1.0f);
    }

    void paintAquaGel (juce::Graphics& g, juce::Rectangle<float> area,
                       juce::Colour accent, float cornerRadius)
    {
        const auto bright = accent.brighter (0.85f);
        const auto deep   = accent.darker   (0.55f);

        // Base vertical ramp.
        g.setGradientFill (vertical (area, {
            { 0.00f, juce::Colours::white },
            { 0.25f, bright },
            { 0.50f, accent },
            { 1.00f, deep }
        }));
        g.fillRoundedRectangle (area, cornerRadius);

        // Specular highlight — bright white-to-transparent ellipse in the top third.
        {
            const float pad = area.getHeight() * 0.08f;
            auto spec = area.reduced (pad, 0.0f)
                           .withHeight (area.getHeight() * 0.45f)
                           .translated (0.0f, pad * 0.5f);

            juce::ColourGradient specGrad (juce::Colours::white.withAlpha (0.9f),
                                            spec.getCentreX(), spec.getY(),
                                            juce::Colours::white.withAlpha (0.0f),
                                            spec.getCentreX(), spec.getBottom(),
                                            false);
            g.setGradientFill (specGrad);
            g.fillRoundedRectangle (spec, cornerRadius * 0.8f);
        }

        // Thin dark outline.
        g.setColour (juce::Colours::black.withAlpha (0.75f));
        g.drawRoundedRectangle (area.reduced (0.5f), cornerRadius, 1.0f);
    }

    void paintFrostedAero (juce::Graphics& g, juce::Rectangle<float> area, float cornerRadius)
    {
        // Dither layer — a few low-alpha stripes.
        g.setColour (juce::Colour (0x30000000u));
        g.fillRoundedRectangle (area, cornerRadius);

        g.setColour (juce::Colours::white.withAlpha (0.55f));
        g.fillRoundedRectangle (area, cornerRadius);

        // Subtle top highlight.
        juce::ColourGradient topGloss (juce::Colours::white.withAlpha (0.35f),
                                        area.getCentreX(), area.getY(),
                                        juce::Colours::transparentWhite,
                                        area.getCentreX(), area.getY() + area.getHeight() * 0.45f,
                                        false);
        g.setGradientFill (topGloss);
        g.fillRoundedRectangle (area, cornerRadius);

        g.setColour (juce::Colours::white.withAlpha (0.3f));
        g.drawRoundedRectangle (area.reduced (0.5f), cornerRadius, 1.0f);
    }

    void paintPluginBackground (juce::Graphics& g, juce::Rectangle<float> area)
    {
        // Base vertical ramp (dark blue → black).
        g.setGradientFill (vertical (area, {
            { 0.00f, colours::bgTop },
            { 0.70f, colours::bgMid },
            { 1.00f, colours::bgBot }
        }));
        g.fillRect (area);

        // Top-left aqua glow.
        {
            juce::ColourGradient glow (juce::Colour::fromFloatRGBA (0.43f, 0.78f, 1.0f, 0.18f),
                                       area.getX() + area.getWidth()  * 0.30f,
                                       area.getY() + area.getHeight() * 0.20f,
                                       juce::Colours::transparentBlack,
                                       area.getX() + area.getWidth()  * 0.30f + 400.0f,
                                       area.getY() + area.getHeight() * 0.20f + 400.0f,
                                       true);
            g.setGradientFill (glow);
            g.fillRect (area);
        }

        // Bottom-right violet glow.
        {
            juce::ColourGradient glow (juce::Colour::fromFloatRGBA (0.70f, 0.55f, 1.0f, 0.12f),
                                       area.getX() + area.getWidth()  * 0.80f,
                                       area.getBottom() + 50.0f,
                                       juce::Colours::transparentBlack,
                                       area.getX() + area.getWidth()  * 0.80f + 350.0f,
                                       area.getBottom() + 50.0f + 350.0f,
                                       true);
            g.setGradientFill (glow);
            g.fillRect (area);
        }
    }
}
