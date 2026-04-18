# HI-Spec Implementation Plan — Part 1: Engine

> Use `superpowers:executing-plans` to implement task-by-task. Steps use `- [ ]` checkboxes.

**Goal:** Build the DSP foundation: CMake/JUCE scaffold, APVTS parameters, Hilbert filterbank, SpectralDelay engine, Vocoder DSP, SpectralFilter DSP.

**Architecture:** JUCE 8 pulled via CMake FetchContent. DSP headers in `Source/DSP/`. Each DSP unit owns a small C++ class with `prepare` / `reset` / `process…Block` methods. TDD with Catch2 — write failing test, implement, pass, commit.

**Tech Stack:** C++20, JUCE 8, CMake ≥ 3.22, Catch2 v3, chowdsp_utils (git submodule for SVF / delay helpers).

---

## Task 0: Project scaffold

**Files:**
- Create: `CMakeLists.txt`, `.gitignore`, `tests/CMakeLists.txt`, `tests/main.cpp`
- Create: `Source/PluginProcessor.h`, `Source/PluginProcessor.cpp`, `Source/PluginEditor.h`, `Source/PluginEditor.cpp` (minimal stubs)

- [ ] **Step 1: `.gitignore`**

```gitignore
build/
cmake-build-*/
.idea/
.vscode/
*.user
JuceLibraryCode/
Builds/
_deps/
```

- [ ] **Step 2: Root `CMakeLists.txt`**

```cmake
cmake_minimum_required(VERSION 3.22)
project(HISpec VERSION 0.1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(JUCE
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG 8.0.4)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(HISpec
  COMPANY_NAME "HI"
  PLUGIN_MANUFACTURER_CODE HiSp
  PLUGIN_CODE HISP
  FORMATS VST3 Standalone
  PRODUCT_NAME "HI-Spec"
  NEEDS_MIDI_INPUT FALSE)

target_sources(HISpec PRIVATE
  Source/PluginProcessor.cpp
  Source/PluginEditor.cpp)

target_compile_definitions(HISpec PRIVATE
  JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(HISpec PRIVATE
  juce::juce_audio_utils juce::juce_dsp
  PUBLIC juce::juce_recommended_config_flags
         juce::juce_recommended_lto_flags
         juce::juce_recommended_warning_flags)

enable_testing()
add_subdirectory(tests)
```

- [ ] **Step 3: Minimal PluginProcessor + PluginEditor stubs**

Write boilerplate that compiles — `juce::AudioProcessor` subclass with empty `processBlock`, `createEditor` returning a `juce::GenericAudioProcessorEditor` for now.

- [ ] **Step 4: Catch2 harness in `tests/CMakeLists.txt`**

```cmake
FetchContent_Declare(Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG v3.5.4)
FetchContent_MakeAvailable(Catch2)

add_executable(hispec_tests main.cpp)
target_link_libraries(hispec_tests PRIVATE Catch2::Catch2WithMain juce::juce_dsp)
target_compile_features(hispec_tests PRIVATE cxx_std_20)
include(CTest)
include(Catch)
catch_discover_tests(hispec_tests)
```

`tests/main.cpp`:
```cpp
#include <catch2/catch_test_macros.hpp>
TEST_CASE("harness lives") { REQUIRE(1 + 1 == 2); }
```

- [ ] **Step 5: Configure + build smoke**

```bash
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Debug --target HISpec_VST3 hispec_tests
ctest --test-dir build -C Debug --output-on-failure
```
Expected: plugin compiles, test harness passes.

- [ ] **Step 6: git init + first commit**

```bash
git init
git add .gitignore CMakeLists.txt tests Source docs mockup CLAUDE.md
git commit -m "chore: initial JUCE + Catch2 scaffold"
```

---

## Task 1: APVTS parameter layout

**Files:**
- Create: `Source/Parameters/ParameterIDs.h`, `Source/Parameters/ParameterLayout.h`, `Source/Parameters/ParameterLayout.cpp`
- Modify: `Source/PluginProcessor.h/.cpp` to own `juce::AudioProcessorValueTreeState`
- Test: `tests/ParameterLayoutTests.cpp`

- [ ] **Step 1: Failing test — all IDs present**

```cpp
#include <catch2/catch_test_macros.hpp>
#include "Parameters/ParameterLayout.h"
TEST_CASE("parameter layout contains core delay params") {
    auto layout = hispec::createParameterLayout();
    REQUIRE(layout.getParameter("delay_bandCount") != nullptr);
    REQUIRE(layout.getParameter("delay_band_00_time") != nullptr);
    REQUIRE(layout.getParameter("voc_carrier") != nullptr);
    REQUIRE(layout.getParameter("sf_shape") != nullptr);
}
```

- [ ] **Step 2: Run — expect FAIL (no layout yet)**

- [ ] **Step 3: Implement `ParameterLayout.h/.cpp`**

- `hispec::createParameterLayout()` returns `juce::AudioProcessorValueTreeState::ParameterLayout`.
- Allocate 32 bands × {time, fb, gain, pan, pitch} flat params (inactive bands ignored later).
- Vocoder params: `voc_carrier` (choice Self|Noise), `voc_vowelX`, `voc_vowelY`, `voc_q`, `voc_formantShift`, `voc_mix`, `voc_texDrive`, `voc_texRate`, `voc_texDepth`, `voc_texDelay`, `voc_texLfoShape`, `voc_texThruZero`, `voc_position` (6-way).
- Spectral Filter: `sf_shape` (LP/HP/BP/Notch/Peak/Tilt), `sf_freq`, `sf_q`, `sf_gain`, `sf_mix`, `sf_lfoRate`, `sf_lfoDepth`, `sf_lfoShape`, `sf_envAttack`, `sf_envRelease`, `sf_envDepth`, `sf_position`.
- Global: `delay_bandCount` (8|16|32), `macro_time`, `macro_fb`, `macro_spread`, `macro_mix`.

- [ ] **Step 4: Test passes; build plugin with APVTS wired**

- [ ] **Step 5: Commit** — `feat(params): APVTS layout for delay + vocoder + spectral filter`

---

## Task 2: ComplexBuffer + Hilbert filterbank

**Files:**
- Create: `Source/DSP/ComplexBuffer.h`, `Source/DSP/HilbertFilterbank.h`, `Source/DSP/HilbertFilterbank.cpp`
- Test: `tests/HilbertFilterbankTests.cpp`

- [ ] **Step 1: Test — reconstruction to within −40 dB**

```cpp
TEST_CASE("filterbank reconstructs white noise within tolerance") {
    hispec::HilbertFilterbank fb;
    fb.prepare({ 48000.0, 512, 1 });
    fb.setBandCount(16);
    // generate 1s of white noise, split → bands → re-modulate → sum, compare
    // require rms(input - output) < rms(input) * 0.01  (~-40 dB)
}
```

- [ ] **Step 2: Implement `ComplexBuffer`**

Minimal struct holding `juce::AudioBuffer<float>` for real + imag, same shape.

- [ ] **Step 3: Implement `HilbertFilterbank`**

- `prepare(juce::dsp::ProcessSpec)`.
- `setBandCount(int n)` — schedules a shadow rebuild, atomic pointer swap on next block with one-block crossfade.
- Per band: heterodyne by `e^{-j 2π fc t}`, 4-pole TPT lowpass at half-band bandwidth.
- `analyse(const juce::AudioBuffer<float>& in, std::vector<ComplexBuffer>& bandsOut)`.
- `synthesise(const std::vector<ComplexBuffer>& bandsIn, juce::AudioBuffer<float>& out)` — re-modulate back to passband and sum.

- [ ] **Step 4: Test passes; tune LPF cutoff if reconstruction fails**

- [ ] **Step 5: Add test for hot-swap — 8→16→32 without clicks (check output RMS stays smooth across the swap block)**

- [ ] **Step 6: Commit** — `feat(dsp): Hilbert filterbank with hot-swap band count`

---

## Task 3: SpectralDelay engine

**Files:**
- Create: `Source/DSP/SpectralDelay.h`, `Source/DSP/SpectralDelay.cpp`
- Test: `tests/SpectralDelayTests.cpp`

- [ ] **Step 1: Test — single band, 100 ms echo on impulse**

Input impulse at t=0, band-0 time = 100 ms, FB = 0, expect peaks at 0 and 100 ms ± half-band group-delay tolerance.

- [ ] **Step 2: Implement per-band `DelayLine` on complex signal**

Use `juce::dsp::DelayLine<float, Lagrange3rd>` × 2 (real, imag). Range 0–2000 ms.

- [ ] **Step 3: Per-band feedback**

FB multiplier (0–110%) with `tanh` saturator on the recirculated signal. DC-blocker on the feedback path.

- [ ] **Step 4: Per-band gain + pan → stereo mix bus**

Complex → stereo: after re-modulate, `L = cos(pan) * sample`, `R = sin(pan) * sample` normalised.

- [ ] **Step 5: Pitch shift via complex phase rotation (v1)**

Multiply baseband by `e^{j 2π Δf t}` where Δf = fc * (2^(semitones/12) − 1). Accept artifacts.

- [ ] **Step 6: Tests pass for delay, FB, pan**

- [ ] **Step 7: Commit** — `feat(dsp): spectral delay engine with per-band time/fb/gain/pan/pitch`

---

## Task 4: Vocoder module (DSP only — no GUI yet)

**Files:**
- Create: `Source/DSP/FxModule.h` (abstract base), `Source/DSP/Vocoder.h`, `Source/DSP/Vocoder.cpp`
- Test: `tests/VocoderTests.cpp`

- [ ] **Step 1: `FxModule.h`** — abstract interface matching spec §4.1

- [ ] **Step 2: Test — Self carrier passes through vowel resonances correctly**

Inject sine sweep; confirm gain peaks near F1, F2, F3 for "A" vowel within ±2 dB of Hillenbrand table.

- [ ] **Step 3: Implement resonator — 3× `juce::dsp::StateVariableTPTFilter` in BP mode**

Coefficients recomputed every 32 samples. Hillenbrand 1995 vowel table for A/E/I/O (4 corners) embedded as `constexpr`.

- [ ] **Step 4: Test — Noise carrier produces output when input is silent impulse envelope**

Feed gate signal (1 sample impulse, then silence); Noise carrier must produce white-noise-shaped-by-envelope output (release ~50 ms default).

- [ ] **Step 5: Implement carrier-source switch**

`voc_carrier` = Self: pass input through resonator. Noise: generate white noise, gate each band by envelope (one-pole follower, attack 5 ms / release 50 ms default on `|input|`), then through resonator.

- [ ] **Step 6: Implement Texture section**

Short modulated delay (Lagrange3, 0.05–8 ms range) inside resonator's internal FB loop. LFO (sine|tri), `texDrive` as feedback gain into the comb path (0 = bypass), `texThruZero` optional phase inversion. tanh on recirculation.

- [ ] **Step 7: Per-band variant — `processComplexBlock`**

Same coefficients applied to real and imag streams in parallel. `texDelay` clamped 0.05–2 ms.

- [ ] **Step 8: Tests pass**

- [ ] **Step 9: Commit** — `feat(dsp): Vocoder module — carrier/resonator/texture`

---

## Task 5: SpectralFilter module

**Files:**
- Create: `Source/DSP/SpectralFilter.h`, `Source/DSP/SpectralFilter.cpp`
- Test: `tests/SpectralFilterTests.cpp`

- [ ] **Step 1: Test — LP shape attenuates high bands**

Set shape=LP, freq=1 kHz, feed pink noise split into 16 bands; assert bands above 1 kHz have ≥ 6 dB attenuation per octave.

- [ ] **Step 2: Implement per-band gain rendering**

Evaluate filter magnitude response at each band's center freq → per-band linear gain multiplier. Smooth across bands to avoid stairstepping (3-tap gaussian across neighbours).

- [ ] **Step 3: Shape options LP/HP/BP/Notch/Peak/Tilt**

Use classical analog prototypes; for v1 it's magnitude-only, phase unchanged.

- [ ] **Step 4: LFO modulation on `sf_freq`**

LFO (sine|tri|s&h) modulates log-freq by ±2 octaves × `sf_lfoDepth`.

- [ ] **Step 5: Envelope follower on total input level → modulates `sf_freq`**

One-pole; bipolar `sf_envDepth` (−100…+100%).

- [ ] **Step 6: Tests pass**

- [ ] **Step 7: Commit** — `feat(dsp): Spectral Filter module — shape + LFO + envelope`

---

## Self-review checklist

- [ ] Every task has a failing test before implementation.
- [ ] Every task ends with a commit.
- [ ] All parameter IDs from §5 of the spec appear in Task 1.
- [ ] Hilbert engine covers Spec §3.1 (hot-swap, reconstruction).
- [ ] Vocoder covers Spec §4.3 all subsections (carrier, resonator, texture, per-band).
- [ ] Spectral Filter covers Spec §4.4 (shape, LFO, env, per-band).

**Next plan file:** `2026-04-18-hi-spec-02-processor.md` — wire DSP into PluginProcessor, state/preset, pluginval.
