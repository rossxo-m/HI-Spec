#include "BandDetailStrip.h"
#include "Assets/chrome_gradient.h"
#include "Parameters/ParameterIDs.h"

namespace hispec
{
    namespace
    {
        const std::array<juce::String, BandDetailStrip::kNumKnobs> kKnobLabels {
            "Time", "Feedback", "Gain", "Pan", "Pitch"
        };

        juce::String paramIdForKnob (int bandIdx, BandDetailStrip::Knob k)
        {
            switch (k)
            {
                case BandDetailStrip::Time:     return ids::band_time  (bandIdx);
                case BandDetailStrip::Feedback: return ids::band_fb    (bandIdx);
                case BandDetailStrip::Gain:     return ids::band_gain  (bandIdx);
                case BandDetailStrip::Pan:      return ids::band_pan   (bandIdx);
                case BandDetailStrip::Pitch:    return ids::band_pitch (bandIdx);
                default:                        return {};
            }
        }
    }

    BandDetailStrip::BandDetailStrip (APVTS& a)
        : apvts (a)
    {
        for (size_t i = 0; i < sliders.size(); ++i)
        {
            auto& s = sliders[i];
            s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 14);
            s.setColour (juce::Slider::rotarySliderFillColourId, palette::colours::aqua);
            s.setEnabled (false);
            addAndMakeVisible (s);

            auto& lab = labels[i];
            lab.setText (kKnobLabels[i], juce::dontSendNotification);
            lab.setJustificationType (juce::Justification::centred);
            lab.setColour (juce::Label::textColourId, palette::colours::textMid);
            addAndMakeVisible (lab);
        }
    }

    BandDetailStrip::~BandDetailStrip()
    {
        // Detach before sliders destruct.
        for (auto& a : attachments) a.reset();
    }

    void BandDetailStrip::setSelectedBand (int idx)
    {
        if (idx < 0) idx = -1;
        if (idx == selectedBand) return;
        selectedBand = idx;
        rebindAttachments();
        repaint();
    }

    void BandDetailStrip::rebindAttachments()
    {
        // Drop existing attachments first.
        for (auto& a : attachments) a.reset();

        const bool haveBand = (selectedBand >= 0);
        for (size_t i = 0; i < sliders.size(); ++i)
        {
            auto& s = sliders[i];
            s.setEnabled (haveBand);
            if (! haveBand)
            {
                s.setValue (0.0, juce::dontSendNotification);
                continue;
            }
            const auto pid = paramIdForKnob (selectedBand, static_cast<Knob> (i));
            if (apvts.getParameter (pid) == nullptr) continue;
            attachments[i] = std::make_unique<APVTS::SliderAttachment> (apvts, pid, s);
        }
    }

    void BandDetailStrip::paint (juce::Graphics& g)
    {
        auto area = getLocalBounds().toFloat();
        palette::paintFrostedAero (g, area.reduced (1.0f), 8.0f);

        // Heading on the left: "Band N" / "Global".
        const auto heading = (selectedBand < 0) ? juce::String ("Global")
                                                : ("Band " + juce::String (selectedBand + 1));
        g.setColour (palette::colours::text);
        g.setFont (juce::Font (juce::FontOptions (14.0f)).withStyle (juce::Font::bold));
        g.drawText (heading, area.reduced (14.0f, 6.0f).removeFromLeft (90.0f).toNearestInt(),
                    juce::Justification::centredLeft);
    }

    void BandDetailStrip::resized()
    {
        auto area = getLocalBounds().reduced (8);
        area.removeFromLeft (100); // heading gutter

        const int slotW = juce::jmax (50, area.getWidth() / kNumKnobs);
        for (size_t i = 0; i < sliders.size(); ++i)
        {
            auto slot = area.removeFromLeft (slotW);
            auto lab  = slot.removeFromTop (14);
            labels[i].setBounds (lab);
            sliders[i].setBounds (slot.reduced (2));
        }
    }
}
