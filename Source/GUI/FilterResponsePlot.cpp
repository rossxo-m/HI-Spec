#include "FilterResponsePlot.h"
#include "Assets/chrome_gradient.h"
#include "Parameters/ParameterIDs.h"

#include <cmath>
#include <complex>

namespace hispec
{
    namespace
    {
        constexpr float kFreqMin = 20.0f;
        constexpr float kFreqMax = 20000.0f;
        constexpr double kFsModel = 48000.0;

        // Evaluate a biquad's H(e^{j omega}) magnitude from b0/b1/b2/a0/a1/a2.
        float biquadMag (double b0, double b1, double b2,
                         double a0, double a1, double a2,
                         double omega)
        {
            using cplx = std::complex<double>;
            const cplx z1 = std::polar (1.0, -omega);
            const cplx z2 = std::polar (1.0, -2.0 * omega);
            const cplx num = b0 + b1 * z1 + b2 * z2;
            const cplx den = a0 + a1 * z1 + a2 * z2;
            return (float) std::abs (num / den);
        }

        // RBJ-style coefficients for a given shape. 0=LP,1=HP,2=BP,3=Notch,4=Peak,5=Tilt.
        void buildCoeffs (int shape, double f0, double Q, double gainDb,
                          double fs,
                          double& b0, double& b1, double& b2,
                          double& a0, double& a1, double& a2)
        {
            const double w0    = 2.0 * juce::MathConstants<double>::pi * juce::jlimit (10.0, fs * 0.49, f0) / fs;
            const double cosw0 = std::cos (w0);
            const double sinw0 = std::sin (w0);
            const double alpha = sinw0 / (2.0 * juce::jmax (0.1, Q));
            const double A     = std::pow (10.0, gainDb / 40.0);

            switch (shape)
            {
                case 0: // LP
                    b0 = (1.0 - cosw0) * 0.5;
                    b1 =  1.0 - cosw0;
                    b2 = (1.0 - cosw0) * 0.5;
                    a0 =  1.0 + alpha;
                    a1 = -2.0 * cosw0;
                    a2 =  1.0 - alpha;
                    break;
                case 1: // HP
                    b0 = (1.0 + cosw0) * 0.5;
                    b1 = -(1.0 + cosw0);
                    b2 = (1.0 + cosw0) * 0.5;
                    a0 =  1.0 + alpha;
                    a1 = -2.0 * cosw0;
                    a2 =  1.0 - alpha;
                    break;
                case 2: // BP (constant-0 peak gain)
                    b0 =  alpha;
                    b1 =  0.0;
                    b2 = -alpha;
                    a0 =  1.0 + alpha;
                    a1 = -2.0 * cosw0;
                    a2 =  1.0 - alpha;
                    break;
                case 3: // Notch
                    b0 =  1.0;
                    b1 = -2.0 * cosw0;
                    b2 =  1.0;
                    a0 =  1.0 + alpha;
                    a1 = -2.0 * cosw0;
                    a2 =  1.0 - alpha;
                    break;
                case 4: // Peak EQ
                    b0 =  1.0 + alpha * A;
                    b1 = -2.0 * cosw0;
                    b2 =  1.0 - alpha * A;
                    a0 =  1.0 + alpha / A;
                    a1 = -2.0 * cosw0;
                    a2 =  1.0 - alpha / A;
                    break;
                case 5: // Tilt (low shelf form as placeholder)
                {
                    const double beta = std::sqrt (A) / juce::jmax (0.1, Q);
                    b0 =    A * ((A + 1.0) - (A - 1.0) * cosw0 + beta * sinw0);
                    b1 = 2* A * ((A - 1.0) - (A + 1.0) * cosw0);
                    b2 =    A * ((A + 1.0) - (A - 1.0) * cosw0 - beta * sinw0);
                    a0 =        (A + 1.0) + (A - 1.0) * cosw0 + beta * sinw0;
                    a1 =  -2.0 * ((A - 1.0) + (A + 1.0) * cosw0);
                    a2 =        (A + 1.0) + (A - 1.0) * cosw0 - beta * sinw0;
                    break;
                }
                default:
                    b0 = 1.0; b1 = 0.0; b2 = 0.0;
                    a0 = 1.0; a1 = 0.0; a2 = 0.0;
                    break;
            }
        }
    }

    FilterResponsePlot::FilterResponsePlot (APVTS& a) : apvts (a)
    {
        setInterceptsMouseClicks (false, false);

        for (int i = 0; i < kNumPoints; ++i)
        {
            const float t = (float) i / (float) (kNumPoints - 1);
            freqs[(size_t) i] = kFreqMin * std::pow (kFreqMax / kFreqMin, t);
        }
        recomputeNow();
        startTimerHz (30);
    }

    FilterResponsePlot::~FilterResponsePlot() { stopTimer(); }

    void FilterResponsePlot::timerCallback()
    {
        recomputeNow();
        repaint();
    }

    void FilterResponsePlot::recomputeNow()
    {
        const auto shape = (int) juce::roundToInt (apvts.getRawParameterValue (ids::sf_shape)  ->load());
        const auto f0    = (double) apvts.getRawParameterValue (ids::sf_freq) ->load();
        const auto Q     = (double) apvts.getRawParameterValue (ids::sf_q)    ->load();
        const auto gain  = (double) apvts.getRawParameterValue (ids::sf_gain) ->load();

        double b0=0, b1=0, b2=0, a0=1, a1=0, a2=0;
        buildCoeffs (shape, f0, Q, gain, kFsModel, b0, b1, b2, a0, a1, a2);

        for (int i = 0; i < kNumPoints; ++i)
        {
            const double w = 2.0 * juce::MathConstants<double>::pi * freqs[(size_t) i] / kFsModel;
            mags[(size_t) i] = biquadMag (b0, b1, b2, a0, a1, a2, w);
        }
    }

    float FilterResponsePlot::getMagnitudeAt (float freqHz) const
    {
        // Linear interpolate over the log-spaced array.
        const float t = std::log (freqHz / kFreqMin) / std::log (kFreqMax / kFreqMin);
        const float idxF = juce::jlimit (0.0f, (float) (kNumPoints - 1), t * (float) (kNumPoints - 1));
        const int lo = (int) std::floor (idxF);
        const int hi = juce::jmin (kNumPoints - 1, lo + 1);
        const float f = idxF - (float) lo;
        return juce::jmap (f, mags[(size_t) lo], mags[(size_t) hi]);
    }

    void FilterResponsePlot::paint (juce::Graphics& g)
    {
        auto r = getLocalBounds().toFloat().reduced (4.0f);

        // Background: subtle vertical chrome gradient.
        g.setGradientFill (juce::ColourGradient (palette::colours::chrome6.withAlpha (0.6f), r.getX(), r.getY(),
                                                 palette::colours::chromeLo.withAlpha (0.9f), r.getX(), r.getBottom(), false));
        g.fillRoundedRectangle (r, 6.0f);

        // Grid lines (octave marks).
        g.setColour (palette::colours::chromeLo.withAlpha (0.25f));
        for (float f = 100.0f; f < kFreqMax; f *= 10.0f)
        {
            const float tx = std::log (f / kFreqMin) / std::log (kFreqMax / kFreqMin);
            const float x  = r.getX() + tx * r.getWidth();
            g.drawLine (x, r.getY(), x, r.getBottom(), 0.5f);
        }

        // Build polyline of the response, mapped to height with dB scaling.
        juce::Path path;
        const float dbMin = -36.0f, dbMax = 12.0f;
        for (int i = 0; i < kNumPoints; ++i)
        {
            const float x = r.getX() + (float) i / (float) (kNumPoints - 1) * r.getWidth();
            const float db = juce::jlimit (dbMin, dbMax,
                                           juce::Decibels::gainToDecibels (mags[(size_t) i], dbMin));
            const float ty = 1.0f - (db - dbMin) / (dbMax - dbMin);
            const float y  = r.getY() + ty * r.getHeight();
            if (i == 0) path.startNewSubPath (x, y);
            else        path.lineTo (x, y);
        }

        // Glowing shadow underneath.
        g.setColour (palette::colours::violet.withAlpha (0.35f));
        g.strokePath (path, juce::PathStrokeType (3.5f));

        // Chrome polyline on top.
        g.setColour (palette::colours::text);
        g.strokePath (path, juce::PathStrokeType (1.5f));
    }
}
