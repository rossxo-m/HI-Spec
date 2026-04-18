#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "PluginProcessor.h"
#include "GUI/LookAndFeel_HISpec.h"
#include "GUI/SpectrumView.h"
#include "GUI/BandDetailStrip.h"
#include "GUI/FxModuleCard.h"
#include "GUI/VocoderBody.h"
#include "GUI/SpectralFilterBody.h"
#include "GUI/HeaderBar.h"
#include "GUI/Breadcrumb.h"

#include <memory>

namespace hispec
{

class HISpecAudioProcessorEditor : public juce::AudioProcessorEditor,
                                   private juce::Timer
{
public:
    explicit HISpecAudioProcessorEditor (HISpecAudioProcessor&);
    ~HISpecAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    // Accessors exposed to tests.
    HeaderBar&          getHeader()     noexcept { return header; }
    Breadcrumb&         getBreadcrumb() noexcept { return breadcrumb; }
    SpectrumView&       getSpectrum()   noexcept { return spectrum; }
    BandDetailStrip&    getBandStrip()  noexcept { return bandStrip; }
    FxModuleCard&       getVocoderCard()     noexcept { return *vocoderCard; }
    FxModuleCard&       getFilterCard()      noexcept { return *filterCard; }

    void setSimpleMode (bool simple);
    bool isSimpleMode() const noexcept { return simpleMode; }

    // A/B state snapshots. On click we stash the current state into the current
    // slot, then restore the other. getStateInformation / setStateInformation.
    void selectA();
    void selectB();
    void copyAtoB();

private:
    void timerCallback() override;

    HISpecAudioProcessor& processor;
    LookAndFeel_HISpec laf;

    HeaderBar   header;
    Breadcrumb  breadcrumb;
    SpectrumView     spectrum;
    BandDetailStrip  bandStrip;

    VocoderBody        vocoderBody;
    SpectralFilterBody filterBody;

    std::unique_ptr<FxModuleCard> vocoderCard, filterCard;

    bool simpleMode { false };
    bool slotAActive { true };
    juce::MemoryBlock slotA, slotB;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HISpecAudioProcessorEditor)
};

} // namespace hispec
