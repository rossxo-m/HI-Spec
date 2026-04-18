# HI-Spec — Implementation Plan 02: Processor & State

**Scope:** Tasks 6–7. Wire the DSP graph into `PluginProcessor`, persist state via APVTS, report latency, and pass a `pluginval --strictness 10` smoke test.

**Prereqs:** Plan 01 complete (engine + Vocoder + SpectralFilter + APVTS layout all green).

**Style:** TDD. One commit per task. Keep files small and focused.

---

## Task 6 — PluginProcessor wiring

### Goal
A working audio graph: input → (optional Pre FX) → Hilbert analysis → per-band SpectralDelay → per-band FX (if any module is Per-Band) → synthesis → (optional Post FX) → output. Reads every APVTS param it owns. Reports PDC latency.

### Files touched
- `Source/PluginProcessor.h`
- `Source/PluginProcessor.cpp`
- `Source/DSP/Graph.h` — small header that owns the fixed graph elements
- `Source/DSP/Graph.cpp`
- `tests/test_graph_wiring.cpp`

### Design notes

`Graph` owns:
- `HilbertFilterbank analysis;`
- `HilbertFilterbank synthesis;` (shared table — reused for re-mod)
- `SpectralDelay delay;`
- `std::array<std::unique_ptr<FxModule>, 2> fx;` — slot 0 = Vocoder, slot 1 = SpectralFilter (stable indexing so UI can address them)
- `std::atomic<int> bandCount { 16 };` — shadow swap handled inside filterbanks

Processing order per block:
```
pre-mix buffer  (float stereo)
 → for each module m with pos.stage==Pre && pos.scope==Global:   m.processGlobal(buffer)
 → analysis.process(buffer, complexBands)                         // produces 32 complex bands
 → delay.process(complexBands, modParams)
 → for each module m with pos.scope==PerBand: apply via m.processPerBand(complexBands[b], b)
      (respects pos.stage — Pre runs before delay's taps mix; Post runs after; FB is inside SpectralDelay's FB loop — see below)
 → synthesis.process(complexBands, buffer)
 → for each module m with pos.stage==Post && pos.scope==Global:  m.processGlobal(buffer)
 → write to output
```

FB scope (`stage==FB`) cannot run here — it is *inside* `SpectralDelay::process`. Graph exposes a per-band FB callback hook to `SpectralDelay` that forwards to the matching module. This keeps the tangled order out of the module itself.

### Tests (write first)
- `test_graph_wiring.cpp::"passes through with all modules bypassed"` — noise in, bit-identical noise out (±1 sample alignment allowed) when both `voc_power=0` and `sf_power=0` and `delay_time=0`.
- `"reports non-zero latency when prepared"` — `getLatencySamples()` > 0 after `prepareToPlay(48000, 512)`.
- `"reacts to bandCount switch mid-block"` — push 1s of tone, flip `delay_bandCount` from 16 to 32, expect no NaNs, no discontinuity > −40 dB.
- `"PerBand Vocoder at band 3 affects only that band's output energy"` — set `voc_power=1`, `voc_position=PerBand_Post`, inject narrow tone in band 3, assert energy in band 3's center bin changes and neighbors don't.

### Acceptance
- All 4 tests green.
- `processBlock` denormal-safe (use `juce::ScopedNoDenormals`).
- No allocations on the audio thread (verify by adding a `AudioThreadGuard` in debug).

### Commit
`feat(processor): wire Hilbert bank + SpectralDelay + FX modules into graph`

---

## Task 7 — State, presets, latency reporting, pluginval

### Goal
Plugin saves and restores fully from a DAW session. Latency PDC is accurate. `pluginval --strictness 10` passes with no errors.

### Files touched
- `Source/PluginProcessor.cpp` — `getStateInformation` / `setStateInformation` / `getLatencySamples`
- `Source/State/PresetManager.h`
- `Source/State/PresetManager.cpp`
- `Source/State/factoryPresets.xml` — embedded as BinaryData
- `tests/test_state_roundtrip.cpp`

### Design notes

**State format.** APVTS is authoritative. `getStateInformation` writes:
```xml
<HISpecState version="1">
  <APVTS>…juce::ValueTree…</APVTS>
  <UIState currentPage="spectrum" selectedBand="-1"/>
</HISpecState>
```
UI state is non-automated, lives in a sibling `ValueTree` that `PluginEditor` owns but `PluginProcessor` serializes.

**PresetManager.** Thin wrapper: `loadPreset(name)`, `savePreset(name)`, `listFactoryPresets()`, `listUserPresets()`. Factory presets baked in via `BinaryData`. User presets live in `File::getSpecialLocation(userApplicationDataDirectory) / "HI-Spec/Presets"`.

**Latency.** The Hilbert filterbank has a fixed group delay (half the impulse length of the analysis FIR, ~64 samples at fs=48k for our tap count). `SpectralDelay` adds whatever the max delay-line read is *up to* — but PDC should report the *fixed* portion only (Hilbert group delay + synthesis delay). Musical delay time is intentional and does NOT get reported to the host.

### Tests (write first)
- `test_state_roundtrip.cpp::"saves and reloads every APVTS param"` — randomize all params, save to MemoryBlock, new processor, load, compare every param bit-equal.
- `"factory preset 'Init' loads cleanly"` — zero warnings from the ValueTree parser.
- `"latency is stable across prepareToPlay calls"` — prepare(44100,256), record latency; prepare(96000,1024); latency must match the documented formula within 1 sample.
- `"unknown future version string does not crash"` — feed a `<HISpecState version="99">` blob, expect a graceful reset-to-default.

### Factory presets (minimum)
1. **Init** — all neutral, delay_time=0, both modules off.
2. **Cloud Vocoder** — 16 bands, Vocoder Per-Band Post, Self carrier, vowel=A, Texture Drive=0.3 Rate=0.4Hz Depth=0.35, wet=0.5.
3. **Granular Filter Sweep** — 32 bands, SpectralFilter Per-Band Post, BP shape, LFO Rate=0.2Hz to freq, ms feedback.
4. **Formant Shift** — 16 bands, Vocoder Global Post, Noise carrier, pitch=+3st.

### pluginval smoke

Add a CI-friendly script `tests/run_pluginval.sh` (and `.bat` for Windows) that:
1. Finds the built VST3.
2. Runs `pluginval --strictness 10 --validate <path>`.
3. Exits non-zero on any failure.

Not wired into CTest yet — runs manually. Goal for this task is only that it *passes* when run, not that it's automated.

### Acceptance
- All 4 state tests green.
- Manual pluginval run reports `ALL TESTS PASSED`.
- Saving a session in Reaper/Logic/Live and reloading restores params, selected band, and UI page.

### Commit
`feat(state): preset manager, roundtrip serialization, pluginval green`

---

## Exit criteria for Plan 02

- `cmake --build build --target HISpec_VST3` produces a valid VST3 that loads in Reaper without warnings.
- `ctest` runs all engine + processor tests green.
- `pluginval --strictness 10` passes.
- Session save/restore verified manually in one DAW.

Next: Plan 03 — GUI (LookAndFeel, SpectrumView, FX cards, editor assembly).
