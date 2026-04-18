#include <catch2/catch_test_macros.hpp>

#include "GUI/FxModuleCard.h"
#include "GUI/Assets/chrome_gradient.h"
#include "Parameters/ParameterLayout.h"
#include "Parameters/ParameterIDs.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_graphics/juce_graphics.h>

namespace
{
    class HostProc : public juce::AudioProcessor
    {
    public:
        HostProc()
            : AudioProcessor (BusesProperties().withOutput ("Out", juce::AudioChannelSet::stereo(), true)),
              apvts (*this, nullptr, "P", hispec::createParameterLayout())
        {}
        const juce::String getName() const override { return "HostProc"; }
        void prepareToPlay (double, int) override {}
        void releaseResources() override {}
        void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}
        double getTailLengthSeconds() const override { return 0.0; }
        bool acceptsMidi() const override { return false; }
        bool producesMidi() const override { return false; }
        juce::AudioProcessorEditor* createEditor() override { return nullptr; }
        bool hasEditor() const override { return false; }
        int getNumPrograms() override { return 1; }
        int getCurrentProgram() override { return 0; }
        void setCurrentProgram (int) override {}
        const juce::String getProgramName (int) override { return {}; }
        void changeProgramName (int, const juce::String&) override {}
        void getStateInformation (juce::MemoryBlock&) override {}
        void setStateInformation (const void*, int) override {}

        juce::AudioProcessorValueTreeState apvts;
    };

    juce::Image renderCard (juce::Component& c, int w, int h)
    {
        c.setBounds (0, 0, w, h);
        juce::Image img (juce::Image::ARGB, w, h, true);
        juce::Graphics g (img);
        c.paintEntireComponent (g, true);
        return img;
    }

    // Mean colour over the horizontal strip occupied by the header gradient.
    juce::Colour meanHeaderColour (const juce::Image& img)
    {
        const int w = img.getWidth();
        juce::Image::BitmapData bm (img, juce::Image::BitmapData::readOnly);
        // Header lives between y=12 (card pad) + top bezel and header height ≈ 34.
        const int yLo = 18;
        const int yHi = 38;
        double r = 0.0, g = 0.0, b = 0.0;
        int n = 0;
        for (int y = yLo; y < yHi; ++y)
            for (int x = w / 4; x < (3 * w) / 4; ++x)
            {
                const auto c = bm.getPixelColour (x, y);
                r += c.getFloatRed();
                g += c.getFloatGreen();
                b += c.getFloatBlue();
                ++n;
            }
        return juce::Colour::fromFloatRGBA ((float) (r / n), (float) (g / n), (float) (b / n), 1.0f);
    }
}

TEST_CASE ("FxModuleCard: violet accent vs amber accent differ in hue", "[gui][fx-card]")
{
    HostProc host;
    juce::Component dummyBody;

    hispec::FxModuleCard violet (host.apvts, dummyBody, {
        "Vocoder",
        hispec::palette::colours::violet,
        hispec::ids::voc_power,
        hispec::ids::voc_mix,
        hispec::ids::voc_position
    });

    hispec::FxModuleCard amber (host.apvts, dummyBody, {
        "Filter",
        hispec::palette::colours::amber,
        hispec::ids::sf_power,
        hispec::ids::sf_mix,
        hispec::ids::sf_position
    });

    auto imgV = renderCard (violet, 380, 240);
    auto imgA = renderCard (amber,  380, 240);

    const auto cV = meanHeaderColour (imgV);
    const auto cA = meanHeaderColour (imgA);

    INFO ("violet header RGB=" << cV.toDisplayString (false)
          << "  amber header RGB=" << cA.toDisplayString (false));

    // Violet header: blue > red.  Amber header: red > blue.
    REQUIRE (cV.getFloatBlue() > cV.getFloatRed());
    REQUIRE (cA.getFloatRed() > cA.getFloatBlue());
}

TEST_CASE ("FxModuleCard: body gets the remaining area", "[gui][fx-card]")
{
    HostProc host;
    juce::Component body;

    hispec::FxModuleCard card (host.apvts, body, {
        "Vocoder",
        hispec::palette::colours::violet,
        hispec::ids::voc_power,
        hispec::ids::voc_mix,
        hispec::ids::voc_position
    });
    card.setBounds (0, 0, 400, 260);

    const auto bodyB = body.getBoundsInParent();
    REQUIRE (bodyB.getWidth()  > 300);
    REQUIRE (bodyB.getHeight() > 140);
    REQUIRE (bodyB.getY()     >  34);
}
