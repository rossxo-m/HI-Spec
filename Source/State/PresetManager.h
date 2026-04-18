#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>

namespace hispec
{

/**
    Thin wrapper around APVTS + UIState for:
        - Versioned XML serialisation / deserialisation (save/load session
          state from the DAW).
        - Factory preset enumeration + apply.
        - User preset enumeration on disk (presets directory under
          `userApplicationDataDirectory/HI-Spec/Presets`).

    The owning processor passes in its APVTS; the editor owns its own
    `uiState` ValueTree and registers it via `attachUIState()`.
*/
class PresetManager
{
public:
    static constexpr int kStateVersion = 1;

    PresetManager (juce::AudioProcessorValueTreeState& apvts);

    /// Attach (or replace) the UI-state tree. Optional — if never called,
    /// a default empty tree is serialised.
    void attachUIState (juce::ValueTree& uiState) noexcept;

    // ── Serialisation ───────────────────────────────────────────────────
    void getStateInformation (juce::MemoryBlock& destData) const;
    void setStateInformation (const void* data, int sizeInBytes);

    // ── Factory presets ─────────────────────────────────────────────────
    struct FactoryPreset
    {
        juce::String name;
        std::function<void (juce::AudioProcessorValueTreeState&)> apply;
    };

    static const std::vector<FactoryPreset>& getFactoryPresets();

    /// Apply a factory preset by name. Returns false if not found.
    bool loadFactoryPreset (const juce::String& name);

    // ── User presets ────────────────────────────────────────────────────
    juce::File getUserPresetsDirectory() const;
    juce::StringArray listUserPresets() const;

    /// Save the current state to `<userPresets>/<name>.hispecpreset`.
    bool saveUserPreset (const juce::String& name);

    /// Load a user preset from disk.
    bool loadUserPreset (const juce::String& name);

private:
    juce::AudioProcessorValueTreeState& apvts;
    juce::ValueTree uiStateRef;   // non-owning reference snapshot
};

} // namespace hispec
