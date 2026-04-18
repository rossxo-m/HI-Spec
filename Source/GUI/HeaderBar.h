#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <memory>

namespace hispec
{
    class PresetManager;

    /**
        Top strip: HI-Spec mercury logo + title | preset menu | Simple/Advanced
        toggle | A/B state buttons with copy-A→B.

        The editor owns state snapshots and the AB-swap callback; this widget
        is pure presentation + callbacks.
    */
    class HeaderBar : public juce::Component
    {
    public:
        using APVTS = juce::AudioProcessorValueTreeState;

        HeaderBar (APVTS& apvts, PresetManager& pm);
        ~HeaderBar() override;

        // Simple/Advanced.
        juce::ToggleButton& getSimpleToggle() noexcept { return simpleBtn; }
        bool isSimpleMode() const noexcept             { return simpleBtn.getToggleState(); }
        std::function<void (bool simple)> onSimpleToggled;

        // A/B snapshot buttons. State is held in the editor; these emit callbacks.
        std::function<void()> onSelectA;
        std::function<void()> onSelectB;
        std::function<void()> onCopyAtoB;

        /** Sets the visual "selected" indication on the A/B buttons. */
        void setActiveAB (bool aActive);

        juce::TextButton& getAButton() noexcept { return abA; }
        juce::TextButton& getBButton() noexcept { return abB; }
        juce::TextButton& getCopyButton() noexcept { return abCopy; }

        std::function<void (const juce::String& presetName)> onPresetSelected;

        void paint (juce::Graphics&) override;
        void resized() override;

    private:
        void showPresetMenu();

        APVTS& apvts;
        PresetManager& presets;

        juce::Label title;
        juce::TextButton presetBtn { "Default Preset ▾" };
        juce::ToggleButton simpleBtn { "Simple" };
        juce::TextButton abA { "A" }, abB { "B" }, abCopy { "A→B" };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HeaderBar)
    };
}
