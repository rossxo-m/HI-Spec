#include "PresetManager.h"
#include "Parameters/ParameterIDs.h"

namespace hispec
{

namespace
{
    constexpr const char* kRootTag    = "HISpecState";
    constexpr const char* kVersionAtt = "version";
    constexpr const char* kApvtsTag   = "APVTS";
    constexpr const char* kUiStateTag = "UIState";

    void setFloat (juce::AudioProcessorValueTreeState& apvts, juce::StringRef id, float value)
    {
        if (auto* p = apvts.getParameter (id))
            p->setValueNotifyingHost (p->convertTo0to1 (value));
    }

    void setChoice (juce::AudioProcessorValueTreeState& apvts, juce::StringRef id, int index)
    {
        if (auto* p = apvts.getParameter (id))
            p->setValueNotifyingHost (p->convertTo0to1 (static_cast<float> (index)));
    }

    void setBool (juce::AudioProcessorValueTreeState& apvts, juce::StringRef id, bool on)
    {
        if (auto* p = apvts.getParameter (id))
            p->setValueNotifyingHost (on ? 1.0f : 0.0f);
    }
}

PresetManager::PresetManager (juce::AudioProcessorValueTreeState& s)
    : apvts (s)
{
}

void PresetManager::attachUIState (juce::ValueTree& v) noexcept
{
    uiStateRef = v;
}

void PresetManager::getStateInformation (juce::MemoryBlock& destData) const
{
    juce::XmlElement root (kRootTag);
    root.setAttribute (kVersionAtt, kStateVersion);

    if (auto xml = apvts.copyState().createXml())
    {
        // Rename the <Params> root into <APVTS> so our wrapper is clear.
        auto* wrapper = root.createNewChildElement (kApvtsTag);
        wrapper->addChildElement (xml.release());
    }

    if (uiStateRef.isValid())
    {
        if (auto uiXml = uiStateRef.createXml())
        {
            auto* wrapper = root.createNewChildElement (kUiStateTag);
            wrapper->addChildElement (uiXml.release());
        }
    }

    juce::AudioProcessor::copyXmlToBinary (root, destData);
}

void PresetManager::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = juce::AudioProcessor::getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr)
        return;

    // Back-compat: legacy saves were the raw APVTS root, not the new wrapper.
    if (xml->hasTagName (apvts.state.getType()))
    {
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
        return;
    }

    if (! xml->hasTagName (kRootTag))
        return;

    const int version = xml->getIntAttribute (kVersionAtt, 0);
    if (version > kStateVersion)
    {
        // Future version — ignore rather than crash.
        return;
    }

    if (auto* apvtsWrapper = xml->getChildByName (kApvtsTag))
        if (auto* inner = apvtsWrapper->getFirstChildElement())
            if (inner->hasTagName (apvts.state.getType()))
                apvts.replaceState (juce::ValueTree::fromXml (*inner));

    if (uiStateRef.isValid())
    {
        if (auto* uiWrapper = xml->getChildByName (kUiStateTag))
            if (auto* inner = uiWrapper->getFirstChildElement())
                uiStateRef.copyPropertiesAndChildrenFrom (juce::ValueTree::fromXml (*inner), nullptr);
    }
}

// ── Factory presets ─────────────────────────────────────────────────────

const std::vector<PresetManager::FactoryPreset>& PresetManager::getFactoryPresets()
{
    static const std::vector<FactoryPreset> presets = {
        { "Init", [] (juce::AudioProcessorValueTreeState& apvts)
        {
            setBool   (apvts, ids::voc_power, false);
            setBool   (apvts, ids::sf_power,  false);
            setChoice (apvts, ids::delay_bandCount, 1); // 16
            setFloat  (apvts, ids::macro_time, 0.0f);
            setFloat  (apvts, ids::macro_fb,   0.0f);
            setFloat  (apvts, ids::macro_mix,  0.5f);
            for (int i = 0; i < ids::kMaxBands; ++i)
            {
                setFloat (apvts, ids::band_time  (i), 0.0f);
                setFloat (apvts, ids::band_fb    (i), 0.0f);
                setFloat (apvts, ids::band_gain  (i), 0.0f);
                setFloat (apvts, ids::band_pan   (i), 0.0f);
                setFloat (apvts, ids::band_pitch (i), 0.0f);
            }
        }},
        { "Cloud Vocoder", [] (juce::AudioProcessorValueTreeState& apvts)
        {
            setChoice (apvts, ids::delay_bandCount, 1); // 16
            setBool   (apvts, ids::voc_power, true);
            setChoice (apvts, ids::voc_position, static_cast<int> (ids::Position::PerBandPost));
            setChoice (apvts, ids::voc_carrier, 0); // Self
            setFloat  (apvts, ids::voc_vowelX, 1.0f); // /a/
            setFloat  (apvts, ids::voc_vowelY, 1.0f);
            setFloat  (apvts, ids::voc_q, 6.0f);
            setFloat  (apvts, ids::voc_texDrive, 0.3f);
            setFloat  (apvts, ids::voc_texRate,  0.4f);
            setFloat  (apvts, ids::voc_texDepth, 0.35f);
            setFloat  (apvts, ids::voc_mix, 0.5f);
            for (int i = 0; i < ids::kMaxBands; ++i)
                setFloat (apvts, ids::band_time (i), 80.0f + 10.0f * static_cast<float> (i % 4));
        }},
        { "Granular Filter Sweep", [] (juce::AudioProcessorValueTreeState& apvts)
        {
            setChoice (apvts, ids::delay_bandCount, 2); // 32
            setBool   (apvts, ids::sf_power, true);
            setChoice (apvts, ids::sf_position, static_cast<int> (ids::Position::PerBandPost));
            setChoice (apvts, ids::sf_shape, 2); // BP
            setFloat  (apvts, ids::sf_freq, 600.0f);
            setFloat  (apvts, ids::sf_q, 4.0f);
            setFloat  (apvts, ids::sf_lfoRate, 0.2f);
            setFloat  (apvts, ids::sf_lfoDepth, 0.8f);
            setFloat  (apvts, ids::sf_mix, 1.0f);
            for (int i = 0; i < ids::kMaxBands; ++i)
                setFloat (apvts, ids::band_time (i), 40.0f + 6.0f * static_cast<float> (i));
        }},
        { "Formant Shift", [] (juce::AudioProcessorValueTreeState& apvts)
        {
            setChoice (apvts, ids::delay_bandCount, 1); // 16
            setBool   (apvts, ids::voc_power, true);
            setChoice (apvts, ids::voc_position, static_cast<int> (ids::Position::GlobalPost));
            setChoice (apvts, ids::voc_carrier, 1); // Noise
            setFloat  (apvts, ids::voc_vowelX, 0.3f);
            setFloat  (apvts, ids::voc_vowelY, 0.3f);
            setFloat  (apvts, ids::voc_q, 8.0f);
            setFloat  (apvts, ids::voc_formantShift, 3.0f);
            setFloat  (apvts, ids::voc_mix, 1.0f);
        }},
    };
    return presets;
}

bool PresetManager::loadFactoryPreset (const juce::String& name)
{
    for (const auto& p : getFactoryPresets())
        if (p.name == name)
        {
            p.apply (apvts);
            return true;
        }
    return false;
}

// ── User presets ────────────────────────────────────────────────────────

juce::File PresetManager::getUserPresetsDirectory() const
{
    const auto base = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory);
    auto dir = base.getChildFile ("HI-Spec").getChildFile ("Presets");
    if (! dir.exists()) dir.createDirectory();
    return dir;
}

juce::StringArray PresetManager::listUserPresets() const
{
    juce::StringArray names;
    auto dir = getUserPresetsDirectory();
    for (const auto& entry : juce::RangedDirectoryIterator (dir, false, "*.hispecpreset"))
        names.add (entry.getFile().getFileNameWithoutExtension());
    names.sortNatural();
    return names;
}

bool PresetManager::saveUserPreset (const juce::String& name)
{
    juce::MemoryBlock block;
    getStateInformation (block);

    auto file = getUserPresetsDirectory().getChildFile (name + ".hispecpreset");
    return file.replaceWithData (block.getData(), block.getSize());
}

bool PresetManager::loadUserPreset (const juce::String& name)
{
    auto file = getUserPresetsDirectory().getChildFile (name + ".hispecpreset");
    if (! file.existsAsFile())
        return false;
    juce::MemoryBlock block;
    if (! file.loadFileAsData (block))
        return false;
    setStateInformation (block.getData(), static_cast<int> (block.getSize()));
    return true;
}

} // namespace hispec
