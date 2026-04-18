#include "FxModuleCard.h"
#include "Assets/chrome_gradient.h"

namespace hispec
{
    FxModuleCard::FxModuleCard (APVTS& a, juce::Component& b, Config c)
        : apvts (a), body (b), cfg (std::move (c))
    {
        titleLabel.setText (cfg.title, juce::dontSendNotification);
        titleLabel.setJustificationType (juce::Justification::centredLeft);
        titleLabel.setColour (juce::Label::textColourId, palette::colours::chromeLo);
        titleLabel.setFont (juce::Font (juce::FontOptions (15.0f)).withStyle (juce::Font::bold));
        addAndMakeVisible (titleLabel);

        powerBtn.setButtonText ({});
        powerBtn.setClickingTogglesState (true);
        addAndMakeVisible (powerBtn);
        if (apvts.getParameter (cfg.powerParamID) != nullptr)
            powerAttach = std::make_unique<APVTS::ButtonAttachment> (apvts, cfg.powerParamID, powerBtn);

        mixKnob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        mixKnob.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 50, 14);
        mixKnob.setColour (juce::Slider::rotarySliderFillColourId, cfg.accent);
        addAndMakeVisible (mixKnob);
        if (apvts.getParameter (cfg.mixParamID) != nullptr)
            mixAttach = std::make_unique<APVTS::SliderAttachment> (apvts, cfg.mixParamID, mixKnob);

        mixLabel.setText ("Mix", juce::dontSendNotification);
        mixLabel.setJustificationType (juce::Justification::centred);
        mixLabel.setColour (juce::Label::textColourId, palette::colours::chromeLo);
        addAndMakeVisible (mixLabel);

        positionChip = std::make_unique<PositionChip> (apvts, cfg.positionParamID);
        addAndMakeVisible (*positionChip);

        addAndMakeVisible (body);
    }

    FxModuleCard::~FxModuleCard()
    {
        powerAttach.reset();
        mixAttach.reset();
    }

    void FxModuleCard::paint (juce::Graphics& g)
    {
        auto area = getLocalBounds().toFloat();

        // Chrome bezel as card body.
        palette::paintChromeBezel (g, area.reduced (1.0f), 12.0f);

        // Accent-tinted header strip.
        auto header = area.reduced (8.0f).removeFromTop (34.0f);
        g.setGradientFill (palette::vertical (header, {
            { 0.00f, cfg.accent.brighter (0.25f) },
            { 0.50f, cfg.accent },
            { 1.00f, cfg.accent.darker (0.3f) }
        }));
        g.fillRoundedRectangle (header, 8.0f);
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.drawRoundedRectangle (header.reduced (0.5f), 8.0f, 1.0f);
    }

    void FxModuleCard::resized()
    {
        auto area = getLocalBounds().reduced (12);

        auto header = area.removeFromTop (34);
        // Power button on the left.
        powerBtn.setBounds (header.removeFromLeft (26).reduced (2));
        header.removeFromLeft (6);

        // Position chip on the right of the header.
        positionChip->setBounds (header.removeFromRight (72).reduced (2));
        header.removeFromRight (6);

        // Mix knob + label right-aligned before the chip.
        const int mixW = 56;
        auto mixArea = header.removeFromRight (mixW);
        mixLabel.setBounds (mixArea.removeFromTop (14));
        mixKnob.setBounds (mixArea.reduced (2));
        header.removeFromRight (6);

        titleLabel.setBounds (header);

        area.removeFromTop (6);
        body.setBounds (area);
    }
}
