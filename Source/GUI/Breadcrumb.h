#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace hispec
{
    /**
        Tiny chrome breadcrumb label: "Spectrum / Band 12 / Vocoder".
        The spectrum/band/module fields are set via setters; the component
        redraws whenever any of them changes.
    */
    class Breadcrumb : public juce::Component
    {
    public:
        Breadcrumb();
        ~Breadcrumb() override = default;

        /** -1 clears the band segment. */
        void setBand (int oneBasedIndex);
        /** Empty string clears the module segment. */
        void setModule (const juce::String& s);

        juce::String getFullText() const noexcept { return fullText; }

        void paint (juce::Graphics&) override;

    private:
        void rebuildText();

        int band { -1 };
        juce::String module;
        juce::String fullText { "Spectrum" };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Breadcrumb)
    };
}
