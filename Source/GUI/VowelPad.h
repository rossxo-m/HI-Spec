#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <memory>

namespace hispec
{
    /**
        2D XY pad for vowel selection. Corners labelled A (top-left), E
        (top-right), I (bottom-left), O (bottom-right). Writes the two
        normalised coordinates to @c voc_vowelX / @c voc_vowelY params.

        Y axis runs top=0 → bottom=1 so (0,0) = A (top-left corner).
    */
    class VowelPad : public juce::Component
    {
    public:
        using APVTS = juce::AudioProcessorValueTreeState;

        VowelPad (APVTS& apvts,
                  juce::String paramIdX,
                  juce::String paramIdY);
        ~VowelPad() override;

        float getX() const noexcept { return normX; }
        float getY() const noexcept { return normY; }

        // Normalised coords (0..1). Clamped on entry.
        void  setXY (float x, float y, juce::NotificationType = juce::sendNotificationSync);

        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;

    private:
        void  writeParamsFromXY();
        void  readParamsToXY();

        APVTS& apvts;
        juce::String idX, idY;
        juce::RangedAudioParameter* pX { nullptr };
        juce::RangedAudioParameter* pY { nullptr };

        float normX { 0.5f };
        float normY { 0.5f };

        class XYListener;
        std::unique_ptr<XYListener> listener;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VowelPad)
    };
}
