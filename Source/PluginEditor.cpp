#include "PluginEditor.h"
#include "GUI/Assets/chrome_gradient.h"
#include "Parameters/ParameterIDs.h"

namespace hispec
{
    namespace
    {
        constexpr int kWidth  = 1100;
        constexpr int kHeight = 700;
        constexpr int kHeaderH     = 48;
        constexpr int kBreadcrumbH = 20;
        constexpr int kBandStripH  = 70;
        constexpr int kFxRowH      = 260;
        constexpr int kGap         = 6;
    }

    HISpecAudioProcessorEditor::HISpecAudioProcessorEditor (HISpecAudioProcessor& p)
        : AudioProcessorEditor (&p), processor (p),
          header (p.getAPVTS(), p.getPresetManager()),
          spectrum(),
          bandStrip (p.getAPVTS()),
          vocoderBody (p.getAPVTS()),
          filterBody  (p.getAPVTS())
    {
        juce::LookAndFeel::setDefaultLookAndFeel (&laf);

        setResizable (false, false);

        addAndMakeVisible (header);
        addAndMakeVisible (breadcrumb);
        addAndMakeVisible (spectrum);
        addAndMakeVisible (bandStrip);

        FxModuleCard::Config vocCfg;
        vocCfg.title           = "Vocoder";
        vocCfg.accent          = palette::colours::violet;
        vocCfg.powerParamID    = ids::voc_power;
        vocCfg.mixParamID      = ids::voc_mix;
        vocCfg.positionParamID = ids::voc_position;
        vocoderCard = std::make_unique<FxModuleCard> (p.getAPVTS(), vocoderBody, vocCfg);
        addAndMakeVisible (*vocoderCard);

        FxModuleCard::Config sfCfg;
        sfCfg.title           = "Spectral Filter";
        sfCfg.accent          = palette::colours::aqua;
        sfCfg.powerParamID    = ids::sf_power;
        sfCfg.mixParamID      = ids::sf_mix;
        sfCfg.positionParamID = ids::sf_position;
        filterCard = std::make_unique<FxModuleCard> (p.getAPVTS(), filterBody, sfCfg);
        addAndMakeVisible (*filterCard);

        // Placeholder magnitudes for the spectrum until live data is wired:
        // 24 decaying lobes so the UI isn't flat.
        std::vector<float> mags (24, 0.0f);
        for (int i = 0; i < (int) mags.size(); ++i)
            mags[(size_t) i] = 0.35f + 0.55f * std::sin ((float) i * 0.35f) * std::exp (-0.04f * i);
        spectrum.setMagnitudes (mags);

        spectrum.onBandClicked = [this] (int idx)
        {
            spectrum.setSelectedBand (idx);
            bandStrip.setSelectedBand (idx);
            breadcrumb.setBand (idx + 1);
        };

        header.onSimpleToggled = [this] (bool s) { setSimpleMode (s); };
        header.onSelectA       = [this] { selectA(); };
        header.onSelectB       = [this] { selectB(); };
        header.onCopyAtoB      = [this] { copyAtoB(); };

        // Seed snapshot slot A with the current state so the first swap has content.
        processor.getStateInformation (slotA);

        setSize (kWidth, kHeight);
        startTimerHz (30);
    }

    HISpecAudioProcessorEditor::~HISpecAudioProcessorEditor()
    {
        stopTimer();
        juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    }

    void HISpecAudioProcessorEditor::paint (juce::Graphics& g)
    {
        palette::paintPluginBackground (g, getLocalBounds().toFloat());
    }

    void HISpecAudioProcessorEditor::resized()
    {
        auto area = getLocalBounds();

        header    .setBounds (area.removeFromTop (kHeaderH));
        breadcrumb.setBounds (area.removeFromTop (kBreadcrumbH));

        // FX row docked to the bottom.
        auto fxRow = area.removeFromBottom (kFxRowH);
        fxRow.removeFromLeft (kGap);
        fxRow.removeFromRight (kGap);
        fxRow.removeFromBottom (kGap);
        const int cardW = (fxRow.getWidth() - kGap) / 2;
        if (vocoderCard != nullptr) vocoderCard->setBounds (fxRow.removeFromLeft (cardW));
        fxRow.removeFromLeft (kGap);
        if (filterCard  != nullptr) filterCard->setBounds (fxRow);

        // In simple mode we hide the fx row + band strip; the spectrum
        // expands into that space.
        if (! simpleMode)
        {
            bandStrip.setBounds (area.removeFromBottom (kBandStripH).reduced (kGap, 2));
            area.removeFromBottom (kGap);
        }
        spectrum.setBounds (area.reduced (kGap, 2));
    }

    void HISpecAudioProcessorEditor::setSimpleMode (bool simple)
    {
        simpleMode = simple;
        bandStrip   .setVisible (! simple);
        if (vocoderCard != nullptr) vocoderCard->setVisible (! simple);
        if (filterCard  != nullptr) filterCard ->setVisible (! simple);
        resized();
    }

    void HISpecAudioProcessorEditor::selectA()
    {
        if (slotAActive) return;
        // Save current state into B, restore A.
        slotB.reset();
        processor.getStateInformation (slotB);
        if (slotA.getSize() > 0)
            processor.setStateInformation (slotA.getData(), (int) slotA.getSize());
        slotAActive = true;
        header.setActiveAB (true);
    }

    void HISpecAudioProcessorEditor::selectB()
    {
        if (! slotAActive) return;
        slotA.reset();
        processor.getStateInformation (slotA);
        if (slotB.getSize() > 0)
            processor.setStateInformation (slotB.getData(), (int) slotB.getSize());
        slotAActive = false;
        header.setActiveAB (false);
    }

    void HISpecAudioProcessorEditor::copyAtoB()
    {
        juce::MemoryBlock current;
        processor.getStateInformation (current);
        if (slotAActive) slotB = current;
        else             slotA = current;
    }

    void HISpecAudioProcessorEditor::timerCallback()
    {
        // Placeholder: would push FFT magnitudes in a real graph. For now just
        // wobble the existing magnitudes so the UI feels alive.
        auto now = juce::Time::getMillisecondCounterHiRes() * 0.001;
        std::vector<float> mags (spectrum.getNumBands(), 0.0f);
        for (int i = 0; i < (int) mags.size(); ++i)
        {
            const float base = 0.35f + 0.55f * std::sin ((float) i * 0.35f) * std::exp (-0.04f * i);
            const float wobble = 0.06f * std::sin ((float) (now + i * 0.2));
            mags[(size_t) i] = juce::jlimit (0.0f, 1.0f, base + wobble);
        }
        spectrum.setMagnitudes (mags);
    }

} // namespace hispec
