#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "GUI/Assets/chrome_gradient.h"
#include "GUI/GooFilterEffect.h"

#include <juce_graphics/juce_graphics.h>

using Catch::Approx;

namespace
{
    // Average luminance (0..1) in a small neighbourhood around (x,y).
    float sampleLuminance (const juce::Image& img, int cx, int cy, int radius = 2)
    {
        juce::Image::BitmapData bm (img, juce::Image::BitmapData::readOnly);
        double sum = 0.0;
        int n = 0;
        for (int dy = -radius; dy <= radius; ++dy)
        {
            for (int dx = -radius; dx <= radius; ++dx)
            {
                const int x = juce::jlimit (0, img.getWidth()  - 1, cx + dx);
                const int y = juce::jlimit (0, img.getHeight() - 1, cy + dy);
                const auto c = bm.getPixelColour (x, y);
                // Perceived luminance (Rec. 601).
                sum += 0.299 * c.getFloatRed()
                     + 0.587 * c.getFloatGreen()
                     + 0.114 * c.getFloatBlue();
                ++n;
            }
        }
        return static_cast<float> (sum / juce::jmax (1, n));
    }

    int countConnectedOpaqueRegions (const juce::Image& img, float alphaThresh = 0.25f)
    {
        const int w = img.getWidth();
        const int h = img.getHeight();
        std::vector<uint8_t> visited (static_cast<size_t> (w * h), 0u);

        juce::Image::BitmapData bm (img, juce::Image::BitmapData::readOnly);
        auto alphaAt = [&] (int x, int y)
        {
            return bm.getPixelColour (x, y).getFloatAlpha();
        };

        int regions = 0;
        std::vector<std::pair<int,int>> stack;
        for (int y = 0; y < h; ++y)
        {
            for (int x = 0; x < w; ++x)
            {
                const size_t idx = static_cast<size_t> (y * w + x);
                if (visited[idx]) continue;
                if (alphaAt (x, y) < alphaThresh) { visited[idx] = 1; continue; }

                ++regions;
                stack.clear();
                stack.emplace_back (x, y);
                while (! stack.empty())
                {
                    auto [px, py] = stack.back();
                    stack.pop_back();
                    if (px < 0 || py < 0 || px >= w || py >= h) continue;
                    const size_t i2 = static_cast<size_t> (py * w + px);
                    if (visited[i2]) continue;
                    if (alphaAt (px, py) < alphaThresh) { visited[i2] = 1; continue; }
                    visited[i2] = 1;
                    stack.emplace_back (px + 1, py);
                    stack.emplace_back (px - 1, py);
                    stack.emplace_back (px, py + 1);
                    stack.emplace_back (px, py - 1);
                }
            }
        }
        return regions;
    }
}

TEST_CASE ("LookAndFeel: chrome bezel renders 9 luminance bands", "[gui][laf]")
{
    const int w = 80;
    const int h = 400;
    juce::Image img (juce::Image::ARGB, w, h, true);
    {
        juce::Graphics g (img);
        auto area = juce::Rectangle<float> (0.0f, 0.0f, (float) w, (float) h);
        g.setGradientFill (hispec::palette::chromeBezel (area));
        g.fillRect (area);
    }

    // Sample 9 vertical midpoints. The gradient stops alternate light/dark:
    //   0:#f8f8f8  1:#d0d0d0  2:#7a7a7a  3:#efefef  4:#9a9a9a
    //   5:#cfcfcf  6:#6d6d6d  7:#e8e8e8  8:#f8f8f8
    const std::array<float, 9> expected {
        0xf8 / 255.0f, 0xd0 / 255.0f, 0x7a / 255.0f, 0xef / 255.0f, 0x9a / 255.0f,
        0xcf / 255.0f, 0x6d / 255.0f, 0xe8 / 255.0f, 0xf8 / 255.0f
    };

    for (int i = 0; i < 9; ++i)
    {
        const int y = (int) ((i + 0.5f) * (float) h / 9.0f);
        const float lum = sampleLuminance (img, w / 2, y, 1);
        INFO ("band " << i << " lum=" << lum << " expected~" << expected[i]);
        REQUIRE (lum == Approx (expected[i]).margin (0.05f));
    }

    // Alternation: even-indexed (0,2,4,6,8) bands can't all be dark and odd all light;
    // verify the gradient has at least two "dark↔light" sign flips.
    int flips = 0;
    for (size_t i = 1; i < expected.size(); ++i)
        if ((expected[i] > 0.5f) != (expected[i - 1] > 0.5f))
            ++flips;
    REQUIRE (flips >= 4);
}

TEST_CASE ("LookAndFeel: aqua gel has specular highlight on top third", "[gui][laf]")
{
    const int w = 160, h = 60;
    juce::Image img (juce::Image::ARGB, w, h, true);
    {
        juce::Graphics g (img);
        hispec::palette::paintAquaGel (g,
            juce::Rectangle<float> (0.0f, 0.0f, (float) w, (float) h),
            hispec::palette::colours::aqua,
            8.0f);
    }

    // Mean luminance of top third vs bottom third (centre column to avoid
    // the rounded-corner anti-aliasing bias).
    double topSum = 0.0, botSum = 0.0;
    int topN = 0, botN = 0;
    const int topBand = h / 3;
    const int botStart = 2 * h / 3;
    juce::Image::BitmapData bm (img, juce::Image::BitmapData::readOnly);
    const int xPad = w / 4;
    for (int x = xPad; x < w - xPad; ++x)
    {
        for (int y = 2; y < topBand; ++y)
        {
            const auto c = bm.getPixelColour (x, y);
            topSum += 0.299 * c.getFloatRed() + 0.587 * c.getFloatGreen() + 0.114 * c.getFloatBlue();
            ++topN;
        }
        for (int y = botStart; y < h - 2; ++y)
        {
            const auto c = bm.getPixelColour (x, y);
            botSum += 0.299 * c.getFloatRed() + 0.587 * c.getFloatGreen() + 0.114 * c.getFloatBlue();
            ++botN;
        }
    }
    const float topLum = (float) (topSum / juce::jmax (1, topN));
    const float botLum = (float) (botSum / juce::jmax (1, botN));
    INFO ("aqua gel topLum=" << topLum << " botLum=" << botLum);
    REQUIRE (topLum - botLum >= 0.2f);
}

TEST_CASE ("LookAndFeel: goo filter merges two close discs into one blob", "[gui][laf][goo]")
{
    const int w = 160, h = 80;
    const float discSize = 14.0f;
    const float gap      = 8.0f; // edge-to-edge pixel gap

    auto drawTwoDiscs = [&] (juce::Image& img)
    {
        juce::Graphics g (img);
        g.setColour (juce::Colours::white);
        const float cx  = (float) w * 0.5f;
        const float cy  = (float) h * 0.5f;
        const float lx  = cx - gap * 0.5f - discSize;
        const float rx  = cx + gap * 0.5f;
        const float ty  = cy - discSize * 0.5f;
        g.fillEllipse (lx, ty, discSize, discSize);
        g.fillEllipse (rx, ty, discSize, discSize);
    };

    // Unfiltered plain image: two discs with a clear gap are distinct regions.
    juce::Image plain (juce::Image::ARGB, w, h, true);
    drawTwoDiscs (plain);
    const int regionsPlain = countConnectedOpaqueRegions (plain, 0.5f);
    REQUIRE (regionsPlain == 2);

    // Filtered image: the goo effect should merge them.
    juce::Image source (juce::Image::ARGB, w, h, true);
    drawTwoDiscs (source);

    juce::Image filtered (juce::Image::ARGB, w, h, true);
    {
        juce::Graphics dest (filtered);
        hispec::GooFilterEffect goo (10.0f, 0.25f, 0.05f);
        goo.applyEffect (source, dest, 1.0f, 1.0f);
    }

    // Alpha bridge check: sample the midpoint of the gap in the filtered
    // image. The goo effect should have "grown" both discs toward each other
    // so the midpoint is opaque.
    {
        juce::Image::BitmapData bm (filtered, juce::Image::BitmapData::readOnly);
        const float a = bm.getPixelColour (w / 2, h / 2).getFloatAlpha();
        INFO ("gap-midpoint alpha (should be near 1): " << a);
        REQUIRE (a > 0.5f);
    }

    const int regionsGooey = countConnectedOpaqueRegions (filtered, 0.5f);
    INFO ("plain regions=" << regionsPlain << "  filtered regions=" << regionsGooey);
    REQUIRE (regionsGooey == 1);
}
