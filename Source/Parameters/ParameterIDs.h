#pragma once

#include <juce_core/juce_core.h>

namespace hispec::ids
{

// Max number of bands we allocate params for (actual count is hot-swappable 8/16/32)
inline constexpr int kMaxBands = 32;

// ─── Global delay / engine ──────────────────────────────────────────────
inline constexpr auto delay_bandCount = "delay_bandCount";

// Macros (exposed on header for quick host automation)
inline constexpr auto macro_time   = "macro_time";
inline constexpr auto macro_fb     = "macro_fb";
inline constexpr auto macro_spread = "macro_spread";
inline constexpr auto macro_mix    = "macro_mix";

// ─── Vocoder ────────────────────────────────────────────────────────────
inline constexpr auto voc_power         = "voc_power";
inline constexpr auto voc_position      = "voc_position";
inline constexpr auto voc_mix           = "voc_mix";
inline constexpr auto voc_carrier       = "voc_carrier";      // Self | Noise
inline constexpr auto voc_vowelX        = "voc_vowelX";
inline constexpr auto voc_vowelY        = "voc_vowelY";
inline constexpr auto voc_q             = "voc_q";
inline constexpr auto voc_formantShift  = "voc_formantShift";
inline constexpr auto voc_texDrive      = "voc_texDrive";
inline constexpr auto voc_texRate       = "voc_texRate";
inline constexpr auto voc_texDepth      = "voc_texDepth";
inline constexpr auto voc_texDelay      = "voc_texDelay";
inline constexpr auto voc_texLfoShape   = "voc_texLfoShape";  // Sine | Tri
inline constexpr auto voc_texThruZero   = "voc_texThruZero";

// ─── Spectral Filter ────────────────────────────────────────────────────
inline constexpr auto sf_power       = "sf_power";
inline constexpr auto sf_position    = "sf_position";
inline constexpr auto sf_mix         = "sf_mix";
inline constexpr auto sf_shape       = "sf_shape";   // LP|HP|BP|Notch|Peak|Tilt
inline constexpr auto sf_freq        = "sf_freq";
inline constexpr auto sf_q           = "sf_q";
inline constexpr auto sf_gain        = "sf_gain";
inline constexpr auto sf_lfoRate     = "sf_lfoRate";
inline constexpr auto sf_lfoDepth    = "sf_lfoDepth";
inline constexpr auto sf_lfoShape    = "sf_lfoShape"; // Sine|Tri|S&H
inline constexpr auto sf_envAttack   = "sf_envAttack";
inline constexpr auto sf_envRelease  = "sf_envRelease";
inline constexpr auto sf_envDepth    = "sf_envDepth";

// ─── Per-band (32-entry flat layout; inactive bands ignored) ────────────
inline juce::String band (const char* suffix, int index)
{
    return juce::String ("delay_band_") + juce::String::formatted ("%02d_", index) + suffix;
}

inline juce::String band_time  (int i) { return band ("time",  i); }
inline juce::String band_fb    (int i) { return band ("fb",    i); }
inline juce::String band_gain  (int i) { return band ("gain",  i); }
inline juce::String band_pan   (int i) { return band ("pan",   i); }
inline juce::String band_pitch (int i) { return band ("pitch", i); }

// Enum helpers (match choice order in layout)
enum class Position
{
    GlobalPre = 0,
    GlobalPost,
    PerBandPre,
    PerBandPost,
    PerBandFB,
    Off,
};

enum class VocoderCarrier { Self = 0, Noise };

enum class FilterShape { LP = 0, HP, BP, Notch, Peak, Tilt };

enum class LfoShape { Sine = 0, Tri, SampleAndHold };

} // namespace hispec::ids
