#include "ParameterLayout.h"
#include "ParameterIDs.h"

namespace hispec
{

namespace
{
    using APVTS  = juce::AudioProcessorValueTreeState;
    using FRange = juce::NormalisableRange<float>;

    std::unique_ptr<juce::AudioParameterFloat>
    makeFloat (const juce::String& id, const juce::String& name,
               FRange range, float defaultValue, const juce::String& suffix = {})
    {
        juce::AudioParameterFloatAttributes attrs;
        if (suffix.isNotEmpty())
            attrs = attrs.withLabel (suffix);
        return std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { id, 1 }, name, range, defaultValue, attrs);
    }

    std::unique_ptr<juce::AudioParameterChoice>
    makeChoice (const juce::String& id, const juce::String& name,
                juce::StringArray choices, int defaultIndex = 0)
    {
        return std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { id, 1 }, name, std::move (choices), defaultIndex);
    }

    std::unique_ptr<juce::AudioParameterBool>
    makeBool (const juce::String& id, const juce::String& name, bool defaultValue)
    {
        return std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID { id, 1 }, name, defaultValue);
    }

    // Position choice list (matches ids::Position enum order)
    const juce::StringArray kPositionChoices { "Global Pre", "Global Post",
                                               "Per-Band Pre", "Per-Band Post",
                                               "Per-Band FB", "Off" };

    // Sensible ranges
    FRange timeRange()   { return FRange { 0.0f, 2000.0f, 0.0f, 0.3f }; }   // ms, skewed
    FRange fbRange()     { return FRange { 0.0f, 1.1f,    0.0f, 0.5f }; }   // 0..110%
    FRange gainRange()   { return FRange { -60.0f, 12.0f, 0.0f, 1.0f }; }   // dB
    FRange panRange()    { return FRange { -1.0f, 1.0f,   0.0f, 1.0f }; }
    FRange pitchRange()  { return FRange { -24.0f, 24.0f, 0.0f, 1.0f }; }   // semitones
    FRange unitRange()   { return FRange { 0.0f, 1.0f, 0.0f, 1.0f }; }
    FRange bipolarUnit() { return FRange { -1.0f, 1.0f, 0.0f, 1.0f }; }
    FRange hzRangeLog(float lo, float hi, float def)
    {
        FRange r { lo, hi, 0.0f, 0.3f };  // log-ish skew
        juce::ignoreUnused (def);
        return r;
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    APVTS::ParameterLayout layout;

    // ─── Global ──────────────────────────────────────────────────────
    layout.add (makeChoice (ids::delay_bandCount, "Band Count",
                            juce::StringArray { "8", "16", "32" }, 1)); // default 16

    layout.add (makeFloat (ids::macro_time,   "Macro Time",   unitRange(),   0.25f));
    layout.add (makeFloat (ids::macro_fb,     "Macro FB",     unitRange(),   0.30f));
    layout.add (makeFloat (ids::macro_spread, "Macro Spread", bipolarUnit(), 0.0f));
    layout.add (makeFloat (ids::macro_mix,    "Macro Mix",    unitRange(),   0.5f));

    // ─── Per-band (flat 32 entries) ──────────────────────────────────
    for (int i = 0; i < ids::kMaxBands; ++i)
    {
        auto suffix = juce::String::formatted (" %02d", i + 1);
        layout.add (makeFloat (ids::band_time  (i), "Band Time"  + suffix, timeRange(),  0.0f, "ms"));
        layout.add (makeFloat (ids::band_fb    (i), "Band FB"    + suffix, fbRange(),    0.0f));
        layout.add (makeFloat (ids::band_gain  (i), "Band Gain"  + suffix, gainRange(),  0.0f, "dB"));
        layout.add (makeFloat (ids::band_pan   (i), "Band Pan"   + suffix, panRange(),   0.0f));
        layout.add (makeFloat (ids::band_pitch (i), "Band Pitch" + suffix, pitchRange(), 0.0f, "st"));
    }

    // ─── Vocoder ─────────────────────────────────────────────────────
    layout.add (makeBool   (ids::voc_power,    "Vocoder On",       false));
    layout.add (makeChoice (ids::voc_position, "Vocoder Position", kPositionChoices,
                            static_cast<int> (ids::Position::PerBandPost)));
    layout.add (makeFloat  (ids::voc_mix,      "Vocoder Mix",      unitRange(), 1.0f));

    layout.add (makeChoice (ids::voc_carrier, "Vocoder Carrier",
                            juce::StringArray { "Self", "Noise" }, 0));
    layout.add (makeFloat  (ids::voc_vowelX,       "Vowel X",         unitRange(),   0.5f));
    layout.add (makeFloat  (ids::voc_vowelY,       "Vowel Y",         unitRange(),   0.5f));
    layout.add (makeFloat  (ids::voc_q,            "Vocoder Q",       FRange { 0.5f, 30.0f, 0.0f, 0.35f }, 6.0f));
    layout.add (makeFloat  (ids::voc_formantShift, "Formant Shift",   FRange { -12.0f, 12.0f, 0.0f, 1.0f }, 0.0f, "st"));

    layout.add (makeFloat  (ids::voc_texDrive,    "Texture Drive",  unitRange(),   0.0f));
    layout.add (makeFloat  (ids::voc_texRate,     "Texture Rate",   FRange { 0.05f, 8.0f, 0.0f, 0.4f }, 0.3f, "Hz"));
    layout.add (makeFloat  (ids::voc_texDepth,    "Texture Depth",  unitRange(),   0.3f));
    layout.add (makeFloat  (ids::voc_texDelay,    "Texture Delay",  FRange { 0.05f, 8.0f, 0.0f, 0.35f }, 1.5f, "ms"));
    layout.add (makeChoice (ids::voc_texLfoShape, "Texture LFO",    juce::StringArray { "Sine", "Tri" }, 0));
    layout.add (makeBool   (ids::voc_texThruZero, "Texture ThruZero", false));

    // ─── Spectral Filter ─────────────────────────────────────────────
    layout.add (makeBool   (ids::sf_power,    "Filter On",       false));
    layout.add (makeChoice (ids::sf_position, "Filter Position", kPositionChoices,
                            static_cast<int> (ids::Position::GlobalPost)));
    layout.add (makeFloat  (ids::sf_mix,      "Filter Mix",      unitRange(), 1.0f));

    layout.add (makeChoice (ids::sf_shape, "Filter Shape",
                            juce::StringArray { "LP", "HP", "BP", "Notch", "Peak", "Tilt" }, 0));
    layout.add (makeFloat  (ids::sf_freq, "Filter Freq", hzRangeLog (20.0f, 20000.0f, 1000.0f), 1000.0f, "Hz"));
    layout.add (makeFloat  (ids::sf_q,    "Filter Q",    FRange { 0.3f, 20.0f, 0.0f, 0.35f }, 0.707f));
    layout.add (makeFloat  (ids::sf_gain, "Filter Gain", FRange { -24.0f, 24.0f, 0.0f, 1.0f }, 0.0f, "dB"));

    layout.add (makeFloat  (ids::sf_lfoRate,  "Filter LFO Rate",  FRange { 0.02f, 20.0f, 0.0f, 0.4f }, 0.5f, "Hz"));
    layout.add (makeFloat  (ids::sf_lfoDepth, "Filter LFO Depth", unitRange(), 0.0f));
    layout.add (makeChoice (ids::sf_lfoShape, "Filter LFO Shape",
                            juce::StringArray { "Sine", "Tri", "S&H" }, 0));

    layout.add (makeFloat  (ids::sf_envAttack,  "Filter Env Attack",  FRange { 0.1f, 500.0f, 0.0f, 0.3f }, 5.0f,  "ms"));
    layout.add (makeFloat  (ids::sf_envRelease, "Filter Env Release", FRange { 1.0f, 2000.0f, 0.0f, 0.3f }, 50.0f, "ms"));
    layout.add (makeFloat  (ids::sf_envDepth,   "Filter Env Depth",   bipolarUnit(), 0.0f));

    return layout;
}

} // namespace hispec
