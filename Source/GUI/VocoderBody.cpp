#include "VocoderBody.h"
#include "Assets/chrome_gradient.h"
#include "Parameters/ParameterIDs.h"

namespace hispec
{
    namespace
    {
        void styleCaption (juce::Label& l, const juce::String& s)
        {
            l.setText (s, juce::dontSendNotification);
            l.setJustificationType (juce::Justification::centred);
            l.setColour (juce::Label::textColourId, palette::colours::textMid);
            l.setFont (juce::Font (juce::FontOptions (11.0f)));
        }
    }

    void VocoderBody::addKnob (juce::Slider& s, juce::Label& l,
                               const juce::String& caption, const juce::String& paramId,
                               std::unique_ptr<APVTS::SliderAttachment>& attach)
    {
        s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 48, 14);
        s.setColour (juce::Slider::rotarySliderFillColourId, palette::colours::violet);
        addAndMakeVisible (s);
        styleCaption (l, caption);
        addAndMakeVisible (l);

        if (apvts.getParameter (paramId) != nullptr)
            attach = std::make_unique<APVTS::SliderAttachment> (apvts, paramId, s);
    }

    VocoderBody::VocoderBody (APVTS& a)
        : apvts (a),
          vowelPad (a, ids::voc_vowelX, ids::voc_vowelY)
    {
        auto* carrier = apvts.getParameter (ids::voc_carrier);

        auto carrierSetup = [&] (juce::TextButton& b, int index)
        {
            b.setClickingTogglesState (true);
            b.setRadioGroupId (1);
            b.onClick = [this, carrier, index, &b]
            {
                if (carrier == nullptr || ! b.getToggleState()) return;
                carrier->beginChangeGesture();
                carrier->setValueNotifyingHost (carrier->convertTo0to1 ((float) index));
                carrier->endChangeGesture();
            };
            addAndMakeVisible (b);
        };
        carrierSetup (selfBtn, 0);
        carrierSetup (noiseBtn, 1);

        if (carrier != nullptr)
        {
            carrierAttach = std::make_unique<juce::ParameterAttachment> (*carrier,
                [this] (float newValue)
                {
                    const int idx = juce::jlimit (0, 1, (int) std::round (newValue));
                    selfBtn .setToggleState (idx == 0, juce::dontSendNotification);
                    noiseBtn.setToggleState (idx == 1, juce::dontSendNotification);
                });
            carrierAttach->sendInitialUpdate();
        }

        addAndMakeVisible (vowelPad);

        addKnob (qKnob,     qLabel,     "Q",     ids::voc_q,            qAttach);
        addKnob (shiftKnob, shiftLabel, "Shift", ids::voc_formantShift, shiftAttach);

        styleCaption (textureHeading, "Texture");
        textureHeading.setFont (juce::Font (juce::FontOptions (11.0f)).withStyle (juce::Font::bold));
        textureHeading.setJustificationType (juce::Justification::centredLeft);
        textureHeading.setColour (juce::Label::textColourId, palette::colours::text);
        addAndMakeVisible (textureHeading);

        addKnob (driveKnob, driveLabel, "Drive", ids::voc_texDrive, driveAttach);
        addKnob (rateKnob,  rateLabel,  "Rate",  ids::voc_texRate,  rateAttach);
        addKnob (depthKnob, depthLabel, "Depth", ids::voc_texDepth, depthAttach);
        addKnob (delayKnob, delayLabel, "Delay", ids::voc_texDelay, delayAttach);

        auto* lfoShape = apvts.getParameter (ids::voc_texLfoShape);
        auto lfoSetup = [&] (juce::TextButton& b, int index)
        {
            b.setClickingTogglesState (true);
            b.setRadioGroupId (2);
            b.onClick = [this, lfoShape, index, &b]
            {
                if (lfoShape == nullptr || ! b.getToggleState()) return;
                lfoShape->beginChangeGesture();
                lfoShape->setValueNotifyingHost (lfoShape->convertTo0to1 ((float) index));
                lfoShape->endChangeGesture();
            };
            addAndMakeVisible (b);
        };
        lfoSetup (sineBtn, 0);
        lfoSetup (triBtn,  1);

        if (lfoShape != nullptr)
        {
            lfoShapeAttach = std::make_unique<juce::ParameterAttachment> (*lfoShape,
                [this] (float newValue)
                {
                    const int idx = juce::jlimit (0, 1, (int) std::round (newValue));
                    sineBtn.setToggleState (idx == 0, juce::dontSendNotification);
                    triBtn .setToggleState (idx == 1, juce::dontSendNotification);
                });
            lfoShapeAttach->sendInitialUpdate();
        }

        thruZeroBtn.setClickingTogglesState (true);
        addAndMakeVisible (thruZeroBtn);
        if (apvts.getParameter (ids::voc_texThruZero) != nullptr)
            thruZeroAttach = std::make_unique<APVTS::ButtonAttachment>
                (apvts, ids::voc_texThruZero, thruZeroBtn);
    }

    VocoderBody::~VocoderBody()
    {
        // Smart ptrs handle the rest.
    }

    void VocoderBody::paint (juce::Graphics& g)
    {
        // Subtle divider between main row and texture row.
        auto divider = juce::Rectangle<float> ((float) getLocalBounds().getX() + 8.0f,
                                               (float) textureRowBounds.getY() - 6.0f,
                                               (float) getLocalBounds().getWidth() - 16.0f,
                                               1.0f);
        g.setColour (palette::colours::chromeLo.withAlpha (0.5f));
        g.fillRect (divider);
    }

    void VocoderBody::resized()
    {
        auto area = getLocalBounds().reduced (6);
        const int textureH = 80;
        textureRowBounds = area.removeFromBottom (textureH);
        mainRowBounds = area;

        // ------- Main row -------
        {
            auto row = mainRowBounds;
            // Left: carrier buttons.
            auto left = row.removeFromLeft (80);
            selfBtn .setBounds (left.removeFromTop (24).reduced (2));
            left.removeFromTop (4);
            noiseBtn.setBounds (left.removeFromTop (24).reduced (2));

            // Right: Q and Shift knobs.
            const int knobCol = 56;
            auto right = row.removeFromRight (knobCol * 2 + 8);
            auto qArea = right.removeFromLeft (knobCol);
            qLabel.setBounds (qArea.removeFromTop (14));
            qKnob .setBounds (qArea.reduced (2));
            right.removeFromLeft (8);
            auto sArea = right;
            shiftLabel.setBounds (sArea.removeFromTop (14));
            shiftKnob .setBounds (sArea.reduced (2));

            // Centre: vowel pad.
            vowelPad.setBounds (row.reduced (6));
        }

        // ------- Texture row -------
        {
            auto row = textureRowBounds.reduced (2);
            textureHeading.setBounds (row.removeFromLeft (70));
            row.removeFromLeft (4);

            const int knobCol = juce::jmax (48, (row.getWidth() - 160) / 4);
            auto placeKnob = [&] (juce::Slider& s, juce::Label& l)
            {
                auto slot = row.removeFromLeft (knobCol);
                l.setBounds (slot.removeFromTop (14));
                s.setBounds (slot.reduced (2));
                row.removeFromLeft (2);
            };
            placeKnob (driveKnob, driveLabel);
            placeKnob (rateKnob,  rateLabel);
            placeKnob (depthKnob, depthLabel);
            placeKnob (delayKnob, delayLabel);

            // Remaining right portion: Sine/Tri buttons + Thru-zero toggle.
            row.removeFromLeft (4);
            auto btnCol = row;
            auto seg = btnCol.removeFromTop (22);
            sineBtn.setBounds (seg.removeFromLeft (seg.getWidth() / 2).reduced (2));
            triBtn .setBounds (seg.reduced (2));
            btnCol.removeFromTop (4);
            thruZeroBtn.setBounds (btnCol.removeFromTop (22).reduced (2));
        }
    }
}
