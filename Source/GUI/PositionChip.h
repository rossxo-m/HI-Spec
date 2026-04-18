#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "Parameters/ParameterIDs.h"

namespace hispec
{
    /**
        Five-cell position selector. Logical model is a 3×2 matrix
        rows × cols =  { Pre, Post, FB } × { Global, Per-Band }, with the
        FB × Global cell disabled (FB is per-band only).

        Bound to one of the {module}_position APVTS choice params. Left click
        cycles through the 5 enabled cells. Right click opens a popup menu.
    */
    class PositionChip : public juce::Component
    {
    public:
        using APVTS = juce::AudioProcessorValueTreeState;

        enum class Row { Pre = 0, Post, FB };
        enum class Col { Global = 0, PerBand };

        PositionChip (APVTS& apvts, juce::String paramID);
        ~PositionChip() override = default;

        // Programmatic access — the UI funnels through these.
        ids::Position getCurrent() const;
        bool          setRowCol (Row r, Col c);   // returns false if disabled
        void          cycleNext();                // skips disabled cells

        // Helpers for tests / callers that want to introspect the logical grid.
        static bool isCellEnabled (Row r, Col c) noexcept;
        static ids::Position positionForCell (Row r, Col c) noexcept;
        static juce::String shortLabel (ids::Position p);

        // Component
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;

    private:
        void writePosition (ids::Position p);
        void refresh();

        APVTS& apvts;
        juce::String paramID;
        juce::RangedAudioParameter* param { nullptr };

        juce::String currentLabel;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PositionChip)
    };
}
