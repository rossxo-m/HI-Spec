# HI-Spec — Project Instructions

**What this is:** JUCE 8 spectral delay plugin (VST3 / AU / AAX). Hilbert/analytic filterbank, selectable 8/16/32 bands.

**Read first, in order:**
1. `docs/superpowers/specs/2026-04-18-hi-spec-design.md` — full design spec. Authoritative.
2. `docs/superpowers/HANDOFF.md` — current state + next task.
3. `mockup/hi-spec-mockup.html` — approved UI reference. Launch with `.claude/launch.json` → `python -m http.server 5757 --directory mockup`.

## Key decisions already locked

- **Two inner FX modules only**: `Vocoder` (carrier source Self/Noise + vowel resonator F1/F2/F3 + Texture feedback-comb) and `SpectralFilter` (LP/HP/BP/Notch/Peak/Tilt with LFO + envelope).
- **No separate Flanger module** — flange behavior is the Vocoder's Texture section (flange emerges from feedback through resonators). Don't re-introduce.
- **Position-on-module UI**: every FX module instance owns a Position chip with 5 valid cells — `{Pre, Post, FB} × {Global, Per-Band}` (FB is Per-Band-only). No separate slot rails or insert-rows.
- **UI aesthetic**: chrome goo / Frutiger Aero / Y2K. FabFilter-style single card with frosted-aero panels, Aqua gel buttons, SVG goo filter visualiser. Not a mixer rack.
- **Engine**: Hilbert analytic filterbank (heterodyne → LPF → re-modulate), NOT STFT. Band count hot-swappable via shadow-filterbank + atomic pointer swap.

## Conventions

- Params via `juce::AudioProcessorValueTreeState`. ID format `{moduleType}_{slotPath}_{paramName}`, e.g. `formant_band_fbInsert_texRate`.
- Smoothing: `SmoothedValue<float, Multiplicative>` for gain/freq, `Linear` for pan/mix.
- TDD. Catch2 tests live under `tests/`. `pluginval --strictness 10` in CI.
- Small, focused files. `FxModule` interface under `Source/DSP/`. GUI components under `Source/GUI/`.

## Working directory

Primary path is `C:\Users\Ross\HI-Spec\`. Ignore `C:\Users\Ross\Props Plugin\` — stale, locked as session cwd only.

## What NOT to do

- Don't re-lay-out the mockup unprompted. User accepted "slightly cramped" as a known trade-off.
- Don't reintroduce Flanger as a standalone module.
- Don't add side rails or insert-row UI — use the Position chip on the card.
- Don't propose STFT as the engine — Hilbert is chosen.
