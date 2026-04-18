#include "HeaderBar.h"
#include "Assets/chrome_gradient.h"
#include "State/PresetManager.h"

namespace hispec
{
    HeaderBar::HeaderBar (APVTS& a, PresetManager& pm)
        : apvts (a), presets (pm)
    {
        title.setText ("HI-Spec", juce::dontSendNotification);
        title.setFont (juce::Font (juce::FontOptions (20.0f)).withStyle (juce::Font::bold));
        title.setColour (juce::Label::textColourId, palette::colours::chromeHi);
        addAndMakeVisible (title);

        presetBtn.onClick = [this] { showPresetMenu(); };
        addAndMakeVisible (presetBtn);

        simpleBtn.setClickingTogglesState (true);
        simpleBtn.onClick = [this]
        {
            if (onSimpleToggled) onSimpleToggled (simpleBtn.getToggleState());
        };
        addAndMakeVisible (simpleBtn);

        auto setupAB = [this] (juce::TextButton& b, std::function<void()>& cb)
        {
            b.setClickingTogglesState (true);
            b.setRadioGroupId (33);
            b.onClick = [&cb, &b] { if (b.getToggleState() && cb) cb(); };
            addAndMakeVisible (b);
        };
        setupAB (abA, onSelectA);
        setupAB (abB, onSelectB);
        abA.setToggleState (true, juce::dontSendNotification);

        abCopy.onClick = [this] { if (onCopyAtoB) onCopyAtoB(); };
        addAndMakeVisible (abCopy);
    }

    HeaderBar::~HeaderBar() = default;

    void HeaderBar::setActiveAB (bool aActive)
    {
        abA.setToggleState (aActive,  juce::dontSendNotification);
        abB.setToggleState (! aActive, juce::dontSendNotification);
    }

    void HeaderBar::showPresetMenu()
    {
        juce::PopupMenu menu;
        menu.addSectionHeader ("Factory");
        const auto& factory = PresetManager::getFactoryPresets();
        for (int i = 0; i < (int) factory.size(); ++i)
            menu.addItem (1000 + i, factory[(size_t) i].name);

        menu.addSeparator();
        menu.addSectionHeader ("User");
        const auto user = presets.listUserPresets();
        for (int i = 0; i < user.size(); ++i)
            menu.addItem (2000 + i, user[i]);

        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetBtn),
            [this, factory, user] (int result)
            {
                if (result == 0) return;
                if (result >= 1000 && result < 2000)
                {
                    const auto& f = factory[(size_t) (result - 1000)];
                    presets.loadFactoryPreset (f.name);
                    presetBtn.setButtonText (f.name);
                    if (onPresetSelected) onPresetSelected (f.name);
                }
                else if (result >= 2000)
                {
                    const auto name = user[result - 2000];
                    presets.loadUserPreset (name);
                    presetBtn.setButtonText (name);
                    if (onPresetSelected) onPresetSelected (name);
                }
            });
    }

    void HeaderBar::paint (juce::Graphics& g)
    {
        auto r = getLocalBounds().toFloat();
        palette::paintChromeBezel (g, r, 0.0f);
        // Thin aqua underline.
        g.setColour (palette::colours::aqua.withAlpha (0.6f));
        g.fillRect (juce::Rectangle<float> (0.0f, r.getBottom() - 1.5f, r.getWidth(), 1.5f));
    }

    void HeaderBar::resized()
    {
        auto r = getLocalBounds().reduced (8, 6);
        title.setBounds (r.removeFromLeft (160));

        // Right side: AB swap + copy + Simple.
        auto right = r.removeFromRight (280);
        right.removeFromLeft (4);
        abCopy.setBounds (right.removeFromRight (46).reduced (3));
        abB   .setBounds (right.removeFromRight (36).reduced (3));
        abA   .setBounds (right.removeFromRight (36).reduced (3));
        right.removeFromRight (12);
        simpleBtn.setBounds (right.removeFromRight (80).reduced (3));

        // Remainder: preset menu button (centre-ish).
        presetBtn.setBounds (r.reduced (4));
    }
}
