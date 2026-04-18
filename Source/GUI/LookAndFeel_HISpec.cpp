#include "LookAndFeel_HISpec.h"

namespace hispec
{
    LookAndFeel_HISpec::LookAndFeel_HISpec()
    {
        using namespace palette::colours;

        // Baseline colour assignments — individual components may override.
        setColour (juce::ResizableWindow::backgroundColourId, bgMid);
        setColour (juce::Label::textColourId,                 text);
        setColour (juce::Label::textWhenEditingColourId,      text);
        setColour (juce::Slider::textBoxTextColourId,         text);
        setColour (juce::Slider::textBoxOutlineColourId,      juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxBackgroundColourId,   juce::Colours::transparentBlack);
        setColour (juce::TextButton::buttonColourId,          chrome4);
        setColour (juce::TextButton::buttonOnColourId,        aqua);
        setColour (juce::TextButton::textColourOnId,          chromeLo);
        setColour (juce::TextButton::textColourOffId,         text);
        setColour (juce::ComboBox::backgroundColourId,        chrome5);
        setColour (juce::ComboBox::textColourId,              text);
        setColour (juce::PopupMenu::backgroundColourId,       chrome6);
        setColour (juce::PopupMenu::textColourId,             text);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, aquaDeep);
        setColour (juce::PopupMenu::highlightedTextColourId,  chromeHi);
    }

    void LookAndFeel_HISpec::drawRotarySlider (juce::Graphics& g,
                                               int x, int y, int width, int height,
                                               float sliderPos,
                                               float rotaryStartAngle,
                                               float rotaryEndAngle,
                                               juce::Slider& slider)
    {
        using namespace palette;

        auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height).reduced (2.0f);
        const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto centre  = bounds.getCentre();
        const auto angle   = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

        // Outer chrome bezel ring.
        const float ringThick = juce::jmax (2.0f, radius * 0.18f);
        auto outer = juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (centre);
        paintChromeBezel (g, outer, radius);

        // Inner dark inlay.
        auto inner = outer.reduced (ringThick);
        g.setColour (colours::chrome6);
        g.fillEllipse (inner);
        g.setColour (colours::chromeLo);
        g.drawEllipse (inner.reduced (0.5f), 1.0f);

        // Faint value arc around the inlay.
        const float arcRadius = inner.getWidth() * 0.5f - 1.5f;
        {
            juce::Path track;
            track.addCentredArc (centre.x, centre.y, arcRadius, arcRadius,
                                 0.0f, rotaryStartAngle, rotaryEndAngle, true);
            g.setColour (colours::chrome4);
            g.strokePath (track, juce::PathStrokeType (1.5f));

            juce::Path value;
            value.addCentredArc (centre.x, centre.y, arcRadius, arcRadius,
                                 0.0f, rotaryStartAngle, angle, true);
            auto arcColour = slider.findColour (juce::Slider::rotarySliderFillColourId,
                                                /*inheritFromParent*/ true);
            if (arcColour.isTransparent())
                arcColour = colours::aqua;
            g.setColour (arcColour);
            g.strokePath (value, juce::PathStrokeType (2.0f));
        }

        // Inky pointer.
        {
            const float pointerLength = arcRadius * 0.72f;
            const float pointerThick  = juce::jmax (2.0f, radius * 0.08f);
            juce::Path pointer;
            pointer.addRoundedRectangle (-pointerThick * 0.5f,
                                         -pointerLength,
                                         pointerThick,
                                         pointerLength,
                                         pointerThick * 0.5f);
            pointer.applyTransform (juce::AffineTransform::rotation (angle).translated (centre));
            g.setColour (colours::chromeLo);
            g.fillPath (pointer);
        }

        // Centre cap dot.
        {
            const float capR = radius * 0.12f;
            auto cap = juce::Rectangle<float> (capR * 2.0f, capR * 2.0f).withCentre (centre);
            g.setColour (colours::chrome2);
            g.fillEllipse (cap);
            g.setColour (colours::chrome5);
            g.drawEllipse (cap, 1.0f);
        }
    }

    void LookAndFeel_HISpec::drawButtonBackground (juce::Graphics& g,
                                                   juce::Button& b,
                                                   const juce::Colour& backgroundColour,
                                                   bool shouldDrawButtonAsHighlighted,
                                                   bool shouldDrawButtonAsDown)
    {
        const auto area = b.getLocalBounds().toFloat().reduced (1.0f);
        const bool on = b.getToggleState();

        auto accent = on ? (backgroundColour.isOpaque() ? backgroundColour : palette::colours::aqua)
                         : palette::colours::chrome4;

        if (shouldDrawButtonAsDown)
            accent = accent.darker (0.25f);
        else if (shouldDrawButtonAsHighlighted)
            accent = accent.brighter (0.12f);

        palette::paintAquaGel (g, area, accent, juce::jmin (area.getHeight() * 0.5f, 10.0f));
    }

    juce::Font LookAndFeel_HISpec::getLabelFont (juce::Label& l)
    {
        return juce::Font (juce::FontOptions (l.getFont().getHeight()))
                  .withStyle (juce::Font::plain);
    }

    juce::Font LookAndFeel_HISpec::getPopupMenuFont()
    {
        return juce::Font (juce::FontOptions (14.0f));
    }
}
