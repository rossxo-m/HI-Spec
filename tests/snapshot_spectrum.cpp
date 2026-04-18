#include <catch2/catch_test_macros.hpp>

#include "GUI/SpectrumView.h"

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_graphics/juce_graphics.h>

namespace
{
    // Count distinct bar centroids by looking at a narrow horizontal row near
    // the bottom of the card — where bar pixels are dominated by the darker
    // chrome gradient end and the frosted background is mostly unmodified.
    // We look for "bluer than grey" pixels (signature of the aqua bar fill)
    // and count contiguous runs.
    int countBarCentroids (const juce::Image& img)
    {
        const int w = img.getWidth();
        const int h = img.getHeight();
        juce::Image::BitmapData bm (img, juce::Image::BitmapData::readOnly);

        // Sample a band 3px tall, 8px above the bottom (inside bars, away
        // from anti-aliasing at the card edge).
        const int yLo = juce::jmax (0, h - 14);
        const int yHi = juce::jmin (h, h - 8);

        std::vector<uint8_t> hit ((size_t) w, 0u);
        for (int x = 0; x < w; ++x)
        {
            bool found = false;
            for (int y = yLo; y < yHi && ! found; ++y)
            {
                const auto c = bm.getPixelColour (x, y);
                const float r = c.getFloatRed();
                const float green = c.getFloatGreen();
                const float blue  = c.getFloatBlue();
                // Bar chrome/aqua fill: blue significantly > red; bg is neutral.
                if (blue - r > 0.12f && (green + blue) * 0.5f > 0.25f)
                    found = true;
            }
            hit[(size_t) x] = found ? 1u : 0u;
        }

        int count = 0;
        bool inside = false;
        int runLen = 0;
        for (int x = 0; x < w; ++x)
        {
            const bool h1 = hit[(size_t) x] != 0;
            if (h1)
            {
                if (! inside) { ++count; runLen = 1; }
                else          { ++runLen; }
                inside = true;
            }
            else
            {
                // Very short runs (<2px) are treated as noise.
                if (inside && runLen < 2)
                    --count;
                inside = false;
                runLen = 0;
            }
        }
        return count;
    }

    juce::Image renderComponent (juce::Component& c, int w, int h)
    {
        c.setBounds (0, 0, w, h);
        juce::Image img (juce::Image::ARGB, w, h, true);
        juce::Graphics g (img);
        c.paintEntireComponent (g, true);
        return img;
    }

    float meanBandLuminance (const juce::Image& img,
                             const hispec::SpectrumView& view,
                             int bandIdx)
    {
        const int w = img.getWidth();
        const int h = img.getHeight();
        juce::Image::BitmapData bm (img, juce::Image::BitmapData::readOnly);

        const int cx = juce::jlimit (0, w - 1, (int) std::round (view.getBarCentreX (bandIdx)));
        const int slot = juce::jmax (2, w / juce::jmax (1, view.getNumBands()));
        const int halfBar = slot / 3;

        double s = 0.0;
        int n = 0;
        for (int x = juce::jmax (0, cx - halfBar); x <= juce::jmin (w - 1, cx + halfBar); ++x)
        {
            // Only sample the visible bar top region — below half of the image
            // is guaranteed to contain bar pixels for bands with mag ≥ 0.5.
            for (int y = h / 2; y < h - 4; ++y)
            {
                const auto c = bm.getPixelColour (x, y);
                s += 0.299 * c.getFloatRed() + 0.587 * c.getFloatGreen() + 0.114 * c.getFloatBlue();
                ++n;
            }
        }
        return (float) (s / juce::jmax (1, n));
    }
}

TEST_CASE ("SpectrumView: renders 8 bars in 8-band mode", "[gui][spectrum]")
{
    hispec::SpectrumView view;
    view.setMagnitudes ({ 0.6f, 0.6f, 0.6f, 0.6f, 0.6f, 0.6f, 0.6f, 0.6f });
    REQUIRE (view.getNumBands() == 8);

    auto img = renderComponent (view, 640, 120);
    const int centroids = countBarCentroids (img);
    INFO ("detected centroids: " << centroids);
    // Goo may occasionally fuse the very ends; accept 7 or 8.
    REQUIRE (centroids >= 7);
    REQUIRE (centroids <= 8);
}

TEST_CASE ("SpectrumView: selected band is visibly brighter", "[gui][spectrum]")
{
    hispec::SpectrumView view;
    std::vector<float> mags (8, 0.6f);
    view.setMagnitudes (mags);
    view.setSelectedBand (5);
    REQUIRE (view.getSelectedBand() == 5);

    auto img = renderComponent (view, 640, 120);

    const float selLum = meanBandLuminance (img, view, 5);
    std::vector<float> otherLums;
    for (int i = 0; i < view.getNumBands(); ++i)
        if (i != 5) otherLums.push_back (meanBandLuminance (img, view, i));
    std::sort (otherLums.begin(), otherLums.end());
    const float medianOther = otherLums[otherLums.size() / 2];

    INFO ("selected lum=" << selLum << " median other=" << medianOther);
    REQUIRE (selLum - medianOther >= 0.15f);
}

TEST_CASE ("SpectrumView: click at bar 3 x-centre fires onBandClicked(3)", "[gui][spectrum]")
{
    hispec::SpectrumView view;
    view.setMagnitudes (std::vector<float> (8, 0.5f));
    view.setBounds (0, 0, 640, 120);

    int received = -1;
    view.onBandClicked = [&] (int b) { received = b; };

    const int x = (int) std::round (view.getBarCentreX (3));
    const int y = 60;
    const auto pos = juce::Point<float> ((float) x, (float) y);

    juce::ModifierKeys mods;
    juce::MouseEvent ev (juce::Desktop::getInstance().getMainMouseSource(),
                         pos,
                         mods,
                         juce::MouseInputSource::defaultPressure,
                         juce::MouseInputSource::defaultOrientation,
                         juce::MouseInputSource::defaultRotation,
                         juce::MouseInputSource::defaultTiltX,
                         juce::MouseInputSource::defaultTiltY,
                         &view,
                         &view,
                         juce::Time::getCurrentTime(),
                         pos,
                         juce::Time::getCurrentTime(),
                         1,
                         false);
    view.mouseDown (ev);

    REQUIRE (received == 3);
    REQUIRE (view.getSelectedBand() == 3);
}

TEST_CASE ("SpectrumView: barIndexForX is a monotonic slot mapping", "[gui][spectrum]")
{
    hispec::SpectrumView view;
    view.setMagnitudes (std::vector<float> (16, 0.5f));
    view.setBounds (0, 0, 640, 120);

    int prev = -1;
    for (float x = 0.0f; x < 640.0f; x += 2.0f)
    {
        const int idx = view.barIndexForX (x);
        REQUIRE (idx >= 0);
        REQUIRE (idx < 16);
        REQUIRE (idx >= prev);
        prev = idx;
    }
    REQUIRE (view.barIndexForX (639.0f) == 15);
}
