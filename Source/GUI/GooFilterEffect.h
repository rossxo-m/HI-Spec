#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace hispec
{
    /**
        Approximation of an SVG `<feGaussianBlur>` + `<feColorMatrix>` "gooey" filter.

        The effect:
            1. Gaussian-blurs the alpha channel of the source image.
            2. Thresholds the blurred alpha through a steep contrast curve
               (alpha < 0.5 → 0, ≥ 0.5 → 1), which makes nearby shapes merge
               into a single blob with a rubbery edge.
            3. Composites the thresholded result (using the source's colour)
               into the destination graphics.

        Use by assigning as a component's @c setComponentEffect() and drawing
        shapes normally.
    */
    class GooFilterEffect : public juce::ImageEffectFilter
    {
    public:
        explicit GooFilterEffect (float gaussianRadius = 6.0f,
                                  float alphaThreshold = 0.5f,
                                  float thresholdSoftness = 0.08f) noexcept;

        /** Blur radius in pixels (at scaleFactor == 1). */
        void setRadius (float r) noexcept                { radius = r; }
        /** Alpha value at which the output transitions 0 → 1. 0..1. */
        void setThreshold (float t) noexcept             { threshold = t; }
        /** Half-width of the soft transition around the threshold. 0..0.5. */
        void setThresholdSoftness (float s) noexcept     { softness = s; }

        /** @see juce::ImageEffectFilter */
        void applyEffect (juce::Image& sourceImage,
                          juce::Graphics& destContext,
                          float scaleFactor,
                          float alpha) override;

    private:
        float radius;
        float threshold;
        float softness;
    };
}
