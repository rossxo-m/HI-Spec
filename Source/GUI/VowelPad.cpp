#include "VowelPad.h"
#include "Assets/chrome_gradient.h"

namespace hispec
{
    // Listens to parameter changes so GUI reflects external edits (host
    // automation, preset loads, etc.).
    class VowelPad::XYListener : public juce::AudioProcessorParameter::Listener
    {
    public:
        explicit XYListener (VowelPad& p) : pad (p) {}
        void parameterValueChanged (int, float) override
        {
            juce::MessageManager::callAsync ([w = juce::Component::SafePointer<VowelPad> (&pad)]
            {
                if (w) w->readParamsToXY();
            });
        }
        void parameterGestureChanged (int, bool) override {}
    private:
        VowelPad& pad;
    };

    VowelPad::VowelPad (APVTS& a, juce::String ix, juce::String iy)
        : apvts (a), idX (std::move (ix)), idY (std::move (iy))
    {
        pX = apvts.getParameter (idX);
        pY = apvts.getParameter (idY);
        jassert (pX != nullptr && pY != nullptr);

        listener = std::make_unique<XYListener> (*this);
        if (pX) pX->addListener (listener.get());
        if (pY) pY->addListener (listener.get());

        setInterceptsMouseClicks (true, false);
        readParamsToXY();
    }

    VowelPad::~VowelPad()
    {
        if (pX && listener) pX->removeListener (listener.get());
        if (pY && listener) pY->removeListener (listener.get());
    }

    void VowelPad::setXY (float x, float y, juce::NotificationType n)
    {
        normX = juce::jlimit (0.0f, 1.0f, x);
        normY = juce::jlimit (0.0f, 1.0f, y);
        if (n != juce::dontSendNotification)
            writeParamsFromXY();
        repaint();
    }

    void VowelPad::writeParamsFromXY()
    {
        auto write = [] (juce::RangedAudioParameter* p, float norm)
        {
            if (p == nullptr) return;
            p->beginChangeGesture();
            p->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, norm));
            p->endChangeGesture();
        };
        write (pX, normX);
        write (pY, normY);
    }

    void VowelPad::readParamsToXY()
    {
        if (pX) normX = pX->getValue();
        if (pY) normY = pY->getValue();
        repaint();
    }

    void VowelPad::paint (juce::Graphics& g)
    {
        auto bounds = getLocalBounds().toFloat().reduced (2.0f);

        // Frosted aero card with a subtle grid overlay.
        palette::paintFrostedAero (g, bounds, 8.0f);

        g.setColour (juce::Colours::white.withAlpha (0.18f));
        for (int i = 1; i < 4; ++i)
        {
            const float tx = bounds.getX() + bounds.getWidth()  * (float) i / 4.0f;
            const float ty = bounds.getY() + bounds.getHeight() * (float) i / 4.0f;
            g.drawLine (tx, bounds.getY() + 4.0f, tx, bounds.getBottom() - 4.0f, 0.75f);
            g.drawLine (bounds.getX() + 4.0f, ty, bounds.getRight() - 4.0f, ty, 0.75f);
        }

        // Corner labels.
        auto drawCorner = [&] (const char* s, juce::Justification j, juce::Rectangle<float> r)
        {
            g.setColour (palette::colours::chromeLo);
            g.setFont (juce::Font (juce::FontOptions (14.0f)).withStyle (juce::Font::bold));
            g.drawText (s, r.reduced (6.0f).toNearestInt(), j);
        };
        drawCorner ("A", juce::Justification::topLeft,     bounds);
        drawCorner ("E", juce::Justification::topRight,    bounds);
        drawCorner ("I", juce::Justification::bottomLeft,  bounds);
        drawCorner ("O", juce::Justification::bottomRight, bounds);

        // Dot at current XY.
        const float px = bounds.getX() + bounds.getWidth()  * normX;
        const float py = bounds.getY() + bounds.getHeight() * normY;
        const float r  = 9.0f;

        g.setColour (juce::Colours::white.withAlpha (0.85f));
        g.fillEllipse (px - r, py - r, r * 2.0f, r * 2.0f);
        g.setColour (palette::colours::aquaDeep);
        g.drawEllipse (px - r, py - r, r * 2.0f, r * 2.0f, 2.0f);
        g.setColour (palette::colours::aqua);
        g.fillEllipse (px - 3.0f, py - 3.0f, 6.0f, 6.0f);
    }

    void VowelPad::mouseDown (const juce::MouseEvent& e) { mouseDrag (e); }

    void VowelPad::mouseDrag (const juce::MouseEvent& e)
    {
        auto bounds = getLocalBounds().toFloat().reduced (2.0f);
        const float x = (float (e.x) - bounds.getX()) / juce::jmax (1.0f, bounds.getWidth());
        const float y = (float (e.y) - bounds.getY()) / juce::jmax (1.0f, bounds.getHeight());
        setXY (x, y, juce::sendNotificationSync);
    }
}
