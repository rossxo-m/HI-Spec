#include "SpectralFilterBody.h"
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

    void SpectralFilterBody::addKnob (juce::Slider& s, juce::Label& l,
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

    SpectralFilterBody::SpectralFilterBody (APVTS& a)
        : apvts (a), plot (a)
    {
        auto* shape = apvts.getParameter (ids::sf_shape);

        auto shapeSetup = [&] (juce::TextButton& b, int index)
        {
            b.setClickingTogglesState (true);
            b.setRadioGroupId (11);
            b.onClick = [this, shape, index, &b]
            {
                if (shape == nullptr || ! b.getToggleState()) return;
                shape->beginChangeGesture();
                shape->setValueNotifyingHost (shape->convertTo0to1 ((float) index));
                shape->endChangeGesture();
            };
            addAndMakeVisible (b);
        };
        shapeSetup (lpBtn,    0);
        shapeSetup (hpBtn,    1);
        shapeSetup (bpBtn,    2);
        shapeSetup (notchBtn, 3);

        if (shape != nullptr)
        {
            shapeAttach = std::make_unique<juce::ParameterAttachment> (*shape,
                [this] (float newValue)
                {
                    const int idx = juce::jlimit (0, 3, (int) std::round (newValue));
                    lpBtn   .setToggleState (idx == 0, juce::dontSendNotification);
                    hpBtn   .setToggleState (idx == 1, juce::dontSendNotification);
                    bpBtn   .setToggleState (idx == 2, juce::dontSendNotification);
                    notchBtn.setToggleState (idx == 3, juce::dontSendNotification);
                });
            shapeAttach->sendInitialUpdate();
        }

        addAndMakeVisible (plot);

        addKnob (freqKnob, freqLabel, "Freq", ids::sf_freq, freqAttach);
        addKnob (qKnob,    qLabel,    "Q",    ids::sf_q,    qAttach);

        styleCaption (modHeading, "Mod");
        modHeading.setFont (juce::Font (juce::FontOptions (11.0f)).withStyle (juce::Font::bold));
        modHeading.setJustificationType (juce::Justification::centredLeft);
        modHeading.setColour (juce::Label::textColourId, palette::colours::text);
        addAndMakeVisible (modHeading);

        addKnob (rateKnob,   rateLabel,   "Rate",    ids::sf_lfoRate,    rateAttach);
        addKnob (lfoAmtKnob, lfoAmtLabel, "LFO Amt", ids::sf_lfoDepth,   lfoAmtAttach);
        addKnob (envAmtKnob, envAmtLabel, "Env Amt", ids::sf_envDepth,  envAmtAttach);
        addKnob (atkKnob,    atkLabel,    "Atk",     ids::sf_envAttack,  atkAttach);
        addKnob (relKnob,    relLabel,    "Rel",     ids::sf_envRelease, relAttach);

        auto* lfoShape = apvts.getParameter (ids::sf_lfoShape);
        auto lfoSetup = [&] (juce::TextButton& b, int index)
        {
            b.setClickingTogglesState (true);
            b.setRadioGroupId (12);
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
        lfoSetup (shBtn,   2);

        if (lfoShape != nullptr)
        {
            lfoShapeAttach = std::make_unique<juce::ParameterAttachment> (*lfoShape,
                [this] (float newValue)
                {
                    const int idx = juce::jlimit (0, 2, (int) std::round (newValue));
                    sineBtn.setToggleState (idx == 0, juce::dontSendNotification);
                    triBtn .setToggleState (idx == 1, juce::dontSendNotification);
                    shBtn  .setToggleState (idx == 2, juce::dontSendNotification);
                });
            lfoShapeAttach->sendInitialUpdate();
        }
    }

    SpectralFilterBody::~SpectralFilterBody() = default;

    void SpectralFilterBody::paint (juce::Graphics& g)
    {
        auto divider = juce::Rectangle<float> ((float) getLocalBounds().getX() + 8.0f,
                                               (float) modRowBounds.getY() - 6.0f,
                                               (float) getLocalBounds().getWidth() - 16.0f,
                                               1.0f);
        g.setColour (palette::colours::chromeLo.withAlpha (0.5f));
        g.fillRect (divider);
    }

    void SpectralFilterBody::resized()
    {
        auto area = getLocalBounds().reduced (6);
        const int modH = 80;
        modRowBounds  = area.removeFromBottom (modH);
        mainRowBounds = area;

        // ------- Main row -------
        {
            auto row = mainRowBounds;

            // Top strip: shape segmented (4 buttons).
            auto shapeStrip = row.removeFromTop (24);
            const int segW = juce::jmax (48, shapeStrip.getWidth() / 5);
            lpBtn   .setBounds (shapeStrip.removeFromLeft (segW).reduced (2));
            hpBtn   .setBounds (shapeStrip.removeFromLeft (segW).reduced (2));
            bpBtn   .setBounds (shapeStrip.removeFromLeft (segW).reduced (2));
            notchBtn.setBounds (shapeStrip.removeFromLeft (segW).reduced (2));

            row.removeFromTop (4);

            // Right column: Freq + Q knobs.
            const int knobCol = 56;
            auto right = row.removeFromRight (knobCol * 2 + 8);
            auto fArea = right.removeFromLeft (knobCol);
            freqLabel.setBounds (fArea.removeFromTop (14));
            freqKnob .setBounds (fArea.reduced (2));
            right.removeFromLeft (8);
            auto qArea = right;
            qLabel.setBounds (qArea.removeFromTop (14));
            qKnob .setBounds (qArea.reduced (2));

            // Remainder: response plot.
            plot.setBounds (row.reduced (4));
        }

        // ------- Mod row -------
        {
            auto row = modRowBounds.reduced (2);
            modHeading.setBounds (row.removeFromLeft (40));
            row.removeFromLeft (4);

            // 5 knobs + shape button strip on right.
            const int rightW  = 120;
            auto rightArea = row.removeFromRight (rightW);
            const int knobCol = juce::jmax (40, row.getWidth() / 5);

            auto placeKnob = [&] (juce::Slider& s, juce::Label& l)
            {
                auto slot = row.removeFromLeft (knobCol);
                l.setBounds (slot.removeFromTop (14));
                s.setBounds (slot.reduced (2));
            };
            placeKnob (rateKnob,   rateLabel);
            placeKnob (lfoAmtKnob, lfoAmtLabel);
            placeKnob (envAmtKnob, envAmtLabel);
            placeKnob (atkKnob,    atkLabel);
            placeKnob (relKnob,    relLabel);

            // LFO shape: 3 segmented buttons.
            auto seg = rightArea.removeFromTop (22);
            const int bw = seg.getWidth() / 3;
            sineBtn.setBounds (seg.removeFromLeft (bw).reduced (2));
            triBtn .setBounds (seg.removeFromLeft (bw).reduced (2));
            shBtn  .setBounds (seg.reduced (2));
        }
    }
}
