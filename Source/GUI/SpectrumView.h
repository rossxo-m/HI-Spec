#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Assets/chrome_gradient.h"
#include "GooFilterEffect.h"

#include <functional>
#include <vector>

namespace hispec
{
    /**
        Full-width spectrum bar view. Paints one bar per band using a chrome
        gradient, with a goo filter applied component-wide so adjacent bars
        melt into each other.

        Data flow: the owning editor pushes per-band magnitudes in on a timer
        via @ref setMagnitudes. Click input is forwarded to @ref onBandClicked.
    */
    class SpectrumView : public juce::Component
    {
    public:
        SpectrumView();
        ~SpectrumView() override = default;

        /** 0..1 normalised magnitude per band. Length sets the band count. */
        void setMagnitudes (const std::vector<float>& m);

        /** -1 clears the selection. Out-of-range values also clear. */
        void setSelectedBand (int idx);

        int  getSelectedBand() const noexcept { return selectedBand; }
        int  getNumBands()     const noexcept { return (int) magnitudes.size(); }

        /** Invoked when the user clicks on a bar. Index in [0, numBands). */
        std::function<void (int bandIndex)> onBandClicked;

        // Component
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;

        // x-centre of a bar in component coordinates. Exposed for tests.
        float getBarCentreX (int bandIndex) const noexcept;

        // Convert a mouse x to the nearest band index, or -1 if no bands.
        int   barIndexForX (float x) const noexcept;

    private:
        std::vector<float> magnitudes;
        int selectedBand { -1 };

        GooFilterEffect goo { 4.0f, 0.42f, 0.06f };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpectrumView)
    };
}
