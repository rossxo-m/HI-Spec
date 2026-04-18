#include "SpectrumView.h"

namespace hispec
{
    SpectrumView::SpectrumView()
    {
        setOpaque (false);
        setInterceptsMouseClicks (true, true);
        // Goo is applied to the bars layer only (see paint). Applying it as a
        // component effect here would fuse the frosted background into a
        // single blob — not what we want.
    }

    void SpectrumView::setMagnitudes (const std::vector<float>& m)
    {
        const bool countChanged = (m.size() != magnitudes.size());
        magnitudes = m;
        for (auto& v : magnitudes)
            v = juce::jlimit (0.0f, 1.0f, v);
        if (countChanged && selectedBand >= (int) magnitudes.size())
            selectedBand = -1;
        repaint();
    }

    void SpectrumView::setSelectedBand (int idx)
    {
        const int n = (int) magnitudes.size();
        if (idx < 0 || idx >= n) idx = -1;
        if (idx == selectedBand) return;
        selectedBand = idx;
        repaint();
    }

    float SpectrumView::getBarCentreX (int i) const noexcept
    {
        const int n = (int) magnitudes.size();
        if (n <= 0 || i < 0 || i >= n) return 0.0f;
        const float w = (float) getWidth();
        // Evenly spaced bar slots; ×-centre is the slot centre.
        const float slot = w / (float) n;
        return slot * ((float) i + 0.5f);
    }

    int SpectrumView::barIndexForX (float x) const noexcept
    {
        const int n = (int) magnitudes.size();
        if (n <= 0) return -1;
        const float w = (float) getWidth();
        if (w <= 0.0f) return -1;
        const float slot = w / (float) n;
        const int idx = juce::jlimit (0, n - 1, (int) std::floor (x / slot));
        return idx;
    }

    void SpectrumView::paint (juce::Graphics& g)
    {
        using namespace palette;
        auto bounds = getLocalBounds().toFloat();

        // Frosted background card.
        paintFrostedAero (g, bounds.reduced (1.0f), 10.0f);

        const int n = (int) magnitudes.size();
        if (n <= 0) return;

        // Bar geometry.
        const float areaLeft   = bounds.getX()      + 10.0f;
        const float areaRight  = bounds.getRight()  - 10.0f;
        const float areaTop    = bounds.getY()      + 8.0f;
        const float areaBottom = bounds.getBottom() - 8.0f;
        const float areaW      = juce::jmax (1.0f, areaRight - areaLeft);
        const float areaH      = juce::jmax (1.0f, areaBottom - areaTop);
        (void) areaTop;
        const float slot       = areaW / (float) n;
        const float barW       = juce::jmax (2.0f, slot * 0.72f);

        // Paint bars to a temp image so the goo effect only sees bar alpha.
        const int imgW = getWidth();
        const int imgH = getHeight();
        juce::Image barsLayer (juce::Image::ARGB, juce::jmax (1, imgW), juce::jmax (1, imgH), true);
        {
            juce::Graphics bg (barsLayer);
            for (int i = 0; i < n; ++i)
            {
                const float mag = magnitudes[(size_t) i];
                const float barH = juce::jmax (2.0f, mag * areaH);
                const float cx   = areaLeft + slot * ((float) i + 0.5f);
                const auto bar = juce::Rectangle<float> (cx - barW * 0.5f,
                                                         areaBottom - barH,
                                                         barW,
                                                         barH);
                if (i == selectedBand)
                {
                    bg.setGradientFill (vertical (bar, {
                        { 0.00f, juce::Colours::white },
                        { 0.50f, colours::sky },
                        { 1.00f, colours::aquaDeep }
                    }));
                }
                else
                {
                    bg.setGradientFill (vertical (bar, {
                        { 0.00f, colours::chrome1 },
                        { 0.50f, colours::aqua.withMultipliedBrightness (0.9f) },
                        { 1.00f, colours::chrome5 }
                    }));
                }
                bg.fillRoundedRectangle (bar, barW * 0.4f);
            }
        }

        // Apply goo effect to the bars layer — fuses narrow gaps, keeps wider
        // gaps distinct. Composites over the frosted background.
        goo.applyEffect (barsLayer, g, 1.0f, 1.0f);

        // Selected bar: draw a bright mercury rim on top (post-goo so it stays crisp).
        if (selectedBand >= 0 && selectedBand < n)
        {
            const float mag = magnitudes[(size_t) selectedBand];
            const float barH = juce::jmax (2.0f, mag * areaH);
            const float cx   = areaLeft + slot * ((float) selectedBand + 0.5f);
            const auto bar = juce::Rectangle<float> (cx - barW * 0.5f,
                                                     areaBottom - barH,
                                                     barW,
                                                     barH);
            g.setColour (juce::Colours::white.withAlpha (0.9f));
            g.drawRoundedRectangle (bar.expanded (1.5f), barW * 0.5f, 1.5f);
        }
    }

    void SpectrumView::mouseDown (const juce::MouseEvent& e)
    {
        const int idx = barIndexForX ((float) e.x);
        if (idx < 0) return;
        if (onBandClicked)
            onBandClicked (idx);
        setSelectedBand (idx);
    }
}
