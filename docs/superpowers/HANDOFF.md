# HI-Spec — Handoff (2026-04-18)

## Status

**Spec:** ✅ Approved. `docs/superpowers/specs/2026-04-18-hi-spec-design.md`.
**Mockup:** ✅ Approved. `mockup/hi-spec-mockup.html`. User said "way better, ui a little cramped but lets proceed with building it out".
**Plan:** ✅ Written as three files under `docs/superpowers/plans/` (01-engine, 02-processor, 03-gui).
**Code:** ❌ Not started. `Source/` does not exist yet. Next step = Task 0 from plan 01 (CMake scaffold + first commit).

## Environment

- Windows 11 Pro, bash shell available, CMake 4.3.1, git 2.51.
- JUCE is NOT installed locally — plan to pull via CMake `FetchContent`.
- Preview server launches at `http://localhost:5757` via `.claude/launch.json`.

## Immediate next step

Write the implementation plan as **three smaller files** (not one) to avoid the Write-tool hang seen last session:

1. `docs/superpowers/plans/2026-04-18-hi-spec-01-engine.md` — Tasks 0–5: CMake scaffold, APVTS params, Hilbert filterbank, SpectralDelay engine, Vocoder DSP, SpectralFilter DSP.
2. `docs/superpowers/plans/2026-04-18-hi-spec-02-processor.md` — Tasks 6–7: PluginProcessor wiring, preset/state, latency reporting, pluginval smoke.
3. `docs/superpowers/plans/2026-04-18-hi-spec-03-gui.md` — Tasks 8–14: LookAndFeel_HISpec, SpectrumView, BandDetailStrip, FxModuleCard + PositionChip, VocoderBody, SpectralFilterBody, HeaderBar + Breadcrumb, PluginEditor assembly.

Each plan file should follow `superpowers:writing-plans` structure (goal / tech stack / bite-sized TDD tasks / commit per task), but stay under ~300 lines each. If any file hangs on Write, split it further.

## Task 0 (belongs in engine plan) — project scaffolding

- `CMakeLists.txt` at repo root using `FetchContent` for JUCE 8.
- `juce_add_plugin` with formats `VST3 AU Standalone`, company `HI`, plugin code `HISP`, BusesProperties stereo in/out.
- `.gitignore` for CMake build dirs, user files.
- `git init`, first commit on `main`.
- Initial headless Catch2 harness at `tests/CMakeLists.txt`.

## Key locked decisions (don't re-litigate)

- Hilbert/analytic filterbank, not STFT.
- Two FX module types: `Vocoder` (with built-in Texture), `SpectralFilter`. No standalone Flanger.
- Position-on-module UI (5-cell matrix), no slot rails.
- Band count {8, 16, 32} hot-swappable.
- v1 ships Self and Noise carriers; External + Pitched deferred to v2.

## Memory loaded automatically

Three memory files live at the user's auto-memory path and will load in every session:
- `feedback_hispec_aesthetic.md` — chrome goo Y2K mandatory
- `feedback_hispec_density.md` — cramped but accepted, don't re-lay-out
- `project_hispec.md` — architecture + working directory

## If the Write tool hangs again

- Split the target file further.
- Use `Edit` with an empty-old-string pattern is not possible — instead write a small stub, then `Edit` to append sections.
- Confirm Write works with a trivial 5-line file first before large writes.
