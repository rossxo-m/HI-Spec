#include "Breadcrumb.h"
#include "Assets/chrome_gradient.h"

namespace hispec
{
    Breadcrumb::Breadcrumb()
    {
        setInterceptsMouseClicks (false, false);
        rebuildText();
    }

    void Breadcrumb::setBand (int oneBasedIndex)
    {
        band = oneBasedIndex;
        rebuildText();
        repaint();
    }

    void Breadcrumb::setModule (const juce::String& s)
    {
        module = s;
        rebuildText();
        repaint();
    }

    void Breadcrumb::rebuildText()
    {
        juce::StringArray parts;
        parts.add ("Spectrum");
        if (band > 0) parts.add ("Band " + juce::String (band));
        if (module.isNotEmpty()) parts.add (module);
        fullText = parts.joinIntoString (" / ");
    }

    void Breadcrumb::paint (juce::Graphics& g)
    {
        g.setColour (palette::colours::textMid);
        g.setFont (juce::Font (juce::FontOptions (12.0f)));
        g.drawFittedText (fullText, getLocalBounds().reduced (6, 0),
                          juce::Justification::centredLeft, 1);
    }
}
