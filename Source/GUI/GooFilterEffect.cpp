#include "GooFilterEffect.h"

namespace hispec
{
    GooFilterEffect::GooFilterEffect (float gaussianRadius,
                                      float alphaThreshold,
                                      float thresholdSoftness) noexcept
        : radius (gaussianRadius),
          threshold (alphaThreshold),
          softness (thresholdSoftness)
    {
    }

    namespace
    {
        // Separable gaussian blur of the alpha channel, in-place in an ARGB image.
        // Keeps the RGB channels untouched — only the alpha gets smeared.
        void blurAlphaChannel (juce::Image& img, float pxRadius)
        {
            const int w = img.getWidth();
            const int h = img.getHeight();
            if (w <= 0 || h <= 0 || pxRadius < 0.5f)
                return;

            // Build a 1-D gaussian kernel.
            const int k = juce::jmax (1, (int) std::ceil (pxRadius * 3.0f));
            const int kw = 2 * k + 1;
            juce::HeapBlock<float> kernel (kw);
            const float sigma = juce::jmax (0.5f, pxRadius);
            const float denom = 2.0f * sigma * sigma;
            float sum = 0.0f;
            for (int i = -k; i <= k; ++i)
            {
                const float v = std::exp (- (float) (i * i) / denom);
                kernel[i + k] = v;
                sum += v;
            }
            for (int i = 0; i < kw; ++i) kernel[i] /= sum;

            juce::HeapBlock<float> tmp (w * h);

            juce::Image::BitmapData bm (img, juce::Image::BitmapData::readWrite);

            auto getAlpha = [&] (int x, int y) -> float
            {
                x = juce::jlimit (0, w - 1, x);
                y = juce::jlimit (0, h - 1, y);
                return bm.getPixelPointer (x, y)[3] / 255.0f; // ARGB → A is index 3 in BGRA? JUCE uses ARGB32 with order B,G,R,A in memory on little-endian. Index 3 == A.
            };

            // Horizontal pass → tmp
            for (int y = 0; y < h; ++y)
            {
                for (int x = 0; x < w; ++x)
                {
                    float a = 0.0f;
                    for (int i = -k; i <= k; ++i)
                        a += kernel[i + k] * getAlpha (x + i, y);
                    tmp[y * w + x] = a;
                }
            }

            // Vertical pass → back to image alpha
            for (int y = 0; y < h; ++y)
            {
                auto* row = bm.getLinePointer (y);
                for (int x = 0; x < w; ++x)
                {
                    float a = 0.0f;
                    for (int i = -k; i <= k; ++i)
                    {
                        const int yy = juce::jlimit (0, h - 1, y + i);
                        a += kernel[i + k] * tmp[yy * w + x];
                    }
                    auto* px = row + x * (int) bm.pixelStride;
                    px[3] = (juce::uint8) juce::jlimit (0, 255, (int) std::round (a * 255.0f));
                }
            }
        }

        // Smooth-step the alpha channel of an image around a threshold.
        void thresholdAlpha (juce::Image& img, float threshold, float softness)
        {
            const int w = img.getWidth();
            const int h = img.getHeight();
            if (w <= 0 || h <= 0) return;

            juce::Image::BitmapData bm (img, juce::Image::BitmapData::readWrite);
            const float lo = juce::jlimit (0.0f, 1.0f, threshold - softness);
            const float hi = juce::jlimit (0.0f, 1.0f, threshold + softness);

            for (int y = 0; y < h; ++y)
            {
                auto* row = bm.getLinePointer (y);
                for (int x = 0; x < w; ++x)
                {
                    auto* px = row + x * (int) bm.pixelStride;
                    const float a = px[3] / 255.0f;
                    float out;
                    if (a <= lo)      out = 0.0f;
                    else if (a >= hi) out = 1.0f;
                    else
                    {
                        const float t = (a - lo) / juce::jmax (1.0e-6f, hi - lo);
                        out = t * t * (3.0f - 2.0f * t);
                    }
                    px[3] = (juce::uint8) juce::jlimit (0, 255, (int) std::round (out * 255.0f));
                }
            }
        }
    }

    void GooFilterEffect::applyEffect (juce::Image& sourceImage,
                                       juce::Graphics& destContext,
                                       float scaleFactor,
                                       float alpha)
    {
        auto working = sourceImage.createCopy();

        const float pxRadius = radius * scaleFactor;
        blurAlphaChannel (working, pxRadius);
        thresholdAlpha   (working, threshold, softness);

        destContext.setOpacity (alpha);
        destContext.drawImageAt (working, 0, 0);
    }
}
