# HI-Spec — Design Spec

**Status:** Draft v1 (2026-04-18)
**Owner:** Ross
**Type:** JUCE 8 audio effect plugin (VST3 / AU / AAX)

---

## 1. One-liner

A Hilbert/analytic spectral delay (selectable 8/16/32 bands) with two nested FX modules — a **Vocoder** (vowel resonator + Ableton-style carrier source Self/Noise + Texture flange-in-feedback) and a **Spectral Filter** (modulated per-band shape: LP/HP/BP/Notch/Peak/Tilt, with LFO + envelope modulation) — both insertable globally (pre/post) and per-band (pre-insert, post-insert, feedback-insert).

## 2. Problem & motivation

Traditional spectral delays use either STFT (smeary, latent, awkward to mix with time-domain effects) or simple LR-tree filterbanks (no per-band frequency/phase manipulation). HI-Spec uses an **analytic-signal complex filterbank** so each band carries instantaneous magnitude *and* phase — enabling per-band pitch shift, frequency shift, and ring mod alongside conventional delay/feedback, while keeping latency low (~2–3 ms) and per-band time-domain effect insertion musically clean.

**The unique sound:** a Formant module placed inside the per-band feedback insert produces vowel-morphing tap-decays — each delay echo speaks a different vowel as it fades. Engaging Texture inside the formant adds animated comb/flange coloration as a feedback property of the resonators themselves, rather than as a separate effect — keeping the architecture to one inner module type.

## 3. Engine architecture

### 3.1 Filterbank
- **User-selectable band count: {8, 16, 32}** (default 16), log-spaced 80 Hz – 16 kHz. Changing band count triggers a message-thread rebuild of a shadow filterbank, then a lock-free atomic pointer swap into the audio thread (one-block fade between old and new to avoid clicks).
- **Hilbert/analytic per band**: heterodyne the input down to baseband by multiplying with `e^{-j 2π fc t}`, lowpass with a TPT/SVF lowpass at the half-band bandwidth, optionally re-modulate back with `e^{+j 2π fc t}` for output.
- Internally each band carries a **complex (real, imag) signal stream** at sample rate.
- **Reconstruction**: sum of all bands' real parts after re-modulation reproduces the input signal (within filter overlap tolerance). Verify against pluginval.

### 3.2 Per-band processing chain
For each band:
```
input → heterodyne → LPF → [Pre-Insert] → DelayLine(complex) → [Post-Insert]
                                                  ↑                  │
                                                  └── [FB-Insert] ←──┘
                                                       (× feedback gain
                                                        × tanh saturate)
                                                                     │
                                                              re-modulate
                                                                     ↓
                                                                pan/gain → mix bus
```
Inserts are optional — empty by default. Each insert is a `FxModule*` (Formant | Flanger | nullptr).

### 3.3 Per-band parameters
- **Time** — 0–2000 ms free, or note division when synced (toggle per band)
- **Feedback** — 0–110% (soft-clipped above 100%)
- **Gain** — −∞ to +12 dB
- **Pan** — −1 to +1
- **Pitch shift** — ±12 semitones (v1: continuous via complex phase rotation rate; expect artifacts at extreme settings — acceptable for creative use)
- **Solo / Mute**

### 3.4 Cross-band feedback (v2, not in v1)
Sparse N×N matrix with row-sum cap < 1.0. Out of v1 scope.

## 4. Module system

### 4.1 FxModule interface
```cpp
struct FxModule {
    virtual ~FxModule() = default;
    virtual void prepare(const juce::dsp::ProcessSpec&) = 0;
    virtual void reset() = 0;
    virtual void processBlock(juce::AudioBuffer<float>&) = 0;       // for global slots
    virtual void processComplexBlock(ComplexBuffer&) {}             // for per-band slots; default no-op
    virtual int getLatencySamples() const { return 0; }
};
```
- Global slots use `processBlock` (real audio).
- Per-band slots use `processComplexBlock` (operates on the complex baseband signal of that band).

**v1 ships two FxModule types — Vocoder and Spectral Filter** (see §4.3 and §4.4). Both are valid in every slot. Additional types (Granular Smear, Spectral Freeze) can land in v2 without changing the slot system.

### 4.2 Slots — module-owned placement

Rather than scattering insert positions across the UI (rails on the side, insert rows under the spectrum, etc.), **each FX module instance owns its own Position chip**. The placement options are a single 2-D matrix of three positions × two scopes:

|             | **Global** (operates on the summed signal) | **Per-Band** (operates on each band's complex baseband) |
|---|---|---|
| **Pre**     | before the spectral delay                  | before each band's delay tap                            |
| **Post**    | after the spectral delay                   | after each band's delay tap                             |
| **FB**      | (n/a — global has no per-tap feedback)     | inside each band's feedback loop                        |

That is **5 valid (position, scope) cells**, exposed as a single dropdown chip on each module ("Position: Per-Band FB ▾"). Default placements:
- New Vocoder → **Per-Band FB** (the signature sound of the plugin).
- New Spectral Filter → **Global Post** (the most familiar use).

Per-band scope still uses one shared parameter set per module instance in v1 (broadcast to all bands, with a per-band on/off mask). v2 unlocks per-band-individual params.

**This change replaces the previous design's "global slot rails + per-band insert rows" UI** with a single FX modules row that holds all module instances, each declaring its own placement. Result: one place to see what FX exist; the spectrum panel is no longer cluttered with insert-row UI. At v1 there is **one shared parameter set per insert position** that broadcasts to all bands (per-band-toggle + global params for that insert). v2 unlocks per-band-individual params.
- The musically interesting position is **FB-Insert** with Texture engaged: each delay tap re-passes through the resonators and through the short-modulated comb, so vowel identity drifts and a flange-like motion emerges only on the recirculating signal.

### 4.3 Vocoder module

The single inner FX type. Three sections: **Carrier source** (what gets vocoded), **Resonator** (the vowel filter that shapes the carrier), and **Texture** (modulated short-delay feedback inside the resonator). Topology mirrors Ableton's Vocoder but specialised to HI-Spec's filterbank.

#### 4.3.1 Carrier source

The carrier is the signal that the modulator's spectral envelope is imposed onto. v1 ships two carriers; v2 adds the remaining two for full Ableton parity.

| Mode | v1 | Behaviour |
|---|---|---|
| **Modulator (Self)** | ✅ | Carrier = the input itself. Equivalent to Ableton's "Modulator" carrier. The input passes through the resonator and Texture; vowel coloration without envelope re-shaping. Useful for vowel-tinting drums, pads, vocals. |
| **Noise** | ✅ | Carrier = internal white noise. Per-band envelope (`|input_band[i]|` from the Hilbert bank, smoothed with a one-pole follower; default attack 5 ms / release 50 ms, exposed as `vocAttack` / `vocRelease`) gates noise within each band before re-modulation. Classical channel-vocoder behaviour with noise carrier — talking-noise, breath, whispered vowels. |
| External (sidechain) | ⏳ v2 | Carrier = sidechain audio bus. Requires JUCE sidechain layout plumbing. |
| Internal Pitched | ⏳ v2 | Carrier = monophonic saw/pulse synth driven by MIDI input. Requires `wantsMidiInput`. |

Because each band already maintains its instantaneous complex magnitude, **no separate analysis filterbank is needed** — the band magnitude *is* the envelope follower for that band, sample-accurate and free.

- **Params**: `vocCarrier` (enum: Self | Noise — choices Sidechain/Pitched added in v2), `vocAttack` (0.1–200 ms), `vocRelease` (1–2000 ms), `vocCarrierGain` (−24 to +24 dB).

#### 4.3.2 Resonator
- **Topology**: 3 parallel TPT/SVF bandpass filters tuned to F1/F2/F3 with per-formant gain. Always applied to the (post-carrier-mix) signal.
- **Vowel data**: Hillenbrand 1995 (male; female/child via formant-shift scalar).
- **Vowel morph**: 2-D X/Y pad with vowels A/E/I/O at the four corners. Bilinear interpolation of `(F1, F2, F3, gain1, gain2, gain3)`; coefficients recomputed every 32 samples.
- **Params**: `vowelX`, `vowelY`, `Q` (5–20), `formantShift` (0.5–2.0 ×), `mix` (dry/wet).

#### 4.3.3 Texture (flange/comb in the resonator's feedback path)
A short modulated delay sits inside the resonator's own internal feedback loop. With `texDrive = 0` the module behaves as a pure (carrier-sourced) vocoder/formant; as `texDrive` increases, recirculation through the comb introduces flange/chorus-like motion that inherits the resonator's vowel coloration.

```
            ┌───────────────────────────────┐
            ▼                               │
in ─► [carrier mix: Self|Noise·env] ─► [resonator: F1∥F2∥F3] ─┬─► out
                                                              │
                                                       [tanh × texDrive]
                                                              │
                                                    [LFO short delay
                                                       0.05–8 ms]
                                                              │
                                                              └── (sums back
                                                                   pre-resonator)
```

- **Params**:
  - `texDrive` (0–100%) — feedback gain into the comb path; 0 = bypass.
  - `texRate` (0.05–10 Hz) — LFO rate.
  - `texDepth` (0–100%) — modulation depth.
  - `texDelay` (0.05–8 ms) — center delay.
  - `texLfoShape` (sine | triangle).
  - `texThroughZero` (bool).

#### 4.3.4 Why this collapse
- One module to learn (Carrier · Vowel · Texture) rather than two competing inserts.
- The flange character lives *inside* the vowel resonances, so its comb notches inherit vowel coloration rather than fighting them.
- Same module covers: gentle vowel tint (Self carrier, low texDrive), classical vocoder (Noise carrier), animated formant comb (Self + texDrive up), through-zero whispered vowels (Noise + texThroughZero + texDrive).

#### 4.3.5 Per-band variant
Same module operating on the complex baseband. Per-band scope clamps `texDelay` to 0.05–2 ms. With Noise carrier on a per-band insert, each band's noise carrier is shaped by *that band's* magnitude — the result is a per-frequency-bin whispered echo when placed in FB-Insert.

### 4.4 Spectral Filter module

A modulated per-band magnitude shaper. Operates as gain multipliers on each band's complex signal (real and imag scaled identically — phase-preserving). Because the filterbank already owns the band grid, this is essentially a re-shape of the band magnitudes per block, with smoothing across bands to avoid stairstepping.

#### 4.4.1 Shape
- `sfShape`: enum {LP, HP, BP, Notch, Peak, Tilt}.
  - **LP / HP**: 12 dB/oct virtual slope rendered as per-band attenuation.
  - **BP / Notch**: bell with `sfQ` width, peak/dip = ±24 dB.
  - **Peak**: parametric bell, ±24 dB gain.
  - **Tilt**: linear seesaw across the spectrum, slope ±12 dB/decade.
- `sfFreq` (80–16 000 Hz, log) — center / cutoff.
- `sfQ` (0.3–20) — bandwidth where applicable.
- `sfGain` (−24 to +24 dB) — Peak / Tilt only.
- `sfMix` (dry/wet on the per-band gain change, 0–100%).

#### 4.4.2 Modulation
- **LFO**: `sfLfoRate` (0.05–10 Hz), `sfLfoDepth` (0–100% — modulates `sfFreq` over ±2 octaves at 100%), `sfLfoShape` (sine | tri | s&h).
- **Envelope follower** (driven by total input level): `sfEnvAttack` (0.1–200 ms), `sfEnvRelease` (1–2000 ms), `sfEnvDepth` (−100…+100% — bipolar).

#### 4.4.3 Per-band variant
At per-band scope, the shape is rendered against the **single band's** position in the spectrum (i.e. a band's gain is computed by evaluating the filter response at that band's center frequency). With LFO modulation this gives a moving comb pattern *across* the bands when the same param is broadcast — the same module, used per-band, produces a vocoder-friendly sweeping resonant pattern when paired with Self carrier in another insert.

#### 4.4.4 Why it earns its place
- Vocoder shapes *vowel formants* (resonance peaks tied to speech). Spectral Filter shapes *arbitrary magnitude curves* (sweeps, notches, tilts).
- Together they cover the two main "do something to the band magnitudes over time" use-cases. Without Spectral Filter you'd need to automate per-band gains by hand to get a filter sweep — that's tedious and not modulatable.

## 5. Parameter management

- `juce::AudioProcessorValueTreeState` (APVTS) holds all params.
- Parameter IDs follow `{moduleType}_{slotPath}_{paramName}` convention, e.g. `formant_pre_vowelX`, `formant_band_fbInsert_texRate`, `delay_band_03_time`.
- Per-band params are flattened (one parameter per band per attribute) — necessary for host automation. We always allocate parameters for the maximum band count (32 × ~6 attrs ≈ 192 band params); inactive bands are simply ignored when the user picks a smaller count.
- Smoothing: `juce::SmoothedValue<float, ValueSmoothingTypes::Multiplicative>` for gain/freq, `Linear` for pan/mix.
- State save/load via APVTS XML, with a `pluginVersion` attribute on the root for migration.

## 6. UI architecture

### 6.1 Layout (1100 × 700 default)
```
+----------------------------------------------------------------------------+
| HI-Spec  [Preset ▾] [A/B] [Undo] [Redo]    [Simple | Advanced]  [Help]    |
+----------------------------------------------------------------------------+
|  SPECTRAL DELAY  [HILBERT]                          Bands: [ 8 | 16 | 32 ] |
|  ┌──────────────────────────────────────────────────────────────────────┐  |
|  │ spectrum (full width — no rails)                                     │  |
|  │ │ │ │ │ │ │ █ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │  │  |
|  └──────────────────────────────────────────────────────────────────────┘  |
|  Selected: Band 6 (1.4 kHz)   [Time][FB][Gain][Pan][Pitch][Spread][Mix]    |
|                                                                            |
|  ┌─── ⏻  VOCODER         [▾ Per-Band FB] ─┬─ ⏻  SPECTRAL FILTER  [▾ Post] ┐|
|  │  Carrier: [Self|Noise]                 │  Shape: [LP|HP|BP|Notch]      │|
|  │  vowel pad + Q/Shift/Mix               │  curve + Freq/Q/Mix           │|
|  │  ▒ Texture (Drive/Rate/Depth)          │  ↻ Mod (Rate/LFO/Env)         │|
|  └────────────────────────────────────────┴───────────────────────────────┘|
|  [+ Add Module]                                                            |
+----------------------------------------------------------------------------+
| HI-Spec › Vocoder · FB · Self · tex 38%             Latency  CPU  In  Out  |
+----------------------------------------------------------------------------+
```

### 6.2 Principles
1. **One module = one card**. Position lives on the card. Two FX modules → two cards.
2. **Position chip on every module** (`Pre / Post / FB · Global / Per-Band`) replaces all the old slot rails and insert-row UI.
3. **One spectrum, selection-driven params** (FabFilter pattern). Click a band → band-strip below updates.
4. **Inline expansion, no modals**. Texture/Mod sub-sections live inside their parent module card; drawer-style power toggle.
5. **Simple / Advanced toggle**: Simple hides FX module cards + band-strip detail, exposing only the spectrum + 4 macros (Time, FB, Spread, Mix).
6. **Persistent breadcrumb** at the bottom for context, including each module's current position.
7. **Color**: violet=Vocoder, amber=Spectral Filter and Texture, plasma=spectral delay. Color is semantic, not decorative.
8. **Drag-drop modules** with right-click typed-menu fallback (v2; v1 ships an Add Module button).

### 6.3 Sub-components
- `HeaderBar` — preset, A/B, undo/redo, mode toggle, help
- `SpectralCard` — full-width spectrum view + selected-band detail strip
  - `SpectrumView` — band columns, magnitude bars, click-to-select, drag for time
  - `BandDetailStrip` — Time/FB/Gain/Pan/Pitch/Spread/Mix knobs for selected band
- `FxModulesRow` — horizontally scrollable list of FX module cards; each card owns:
  - power, name, **position chip** (`▾ Per-Band FB` etc.), per-module mix
  - module-specific body (Vocoder body / SpectralFilter body)
  - sub-sections collapsed by default (Texture, Mod)
- `AddModuleButton` — bottom of FxModulesRow, picker for new Vocoder / Spectral Filter
- `Breadcrumb` — bottom

## 7. Latency

Sum module latencies + filterbank group delay. Target reported PDC < 5 ms. Tested via `pluginval --validate` and an internal latency probe.

## 8. State / preset / automation

- All params via APVTS — automation works automatically.
- Preset format: APVTS XML wrapped with name + tags.
- Initial factory presets: `Init`, `Vowel Echoes`, `Through-Zero Comb`, `Diffuse Hilbert`, `Granular Whisper`. Authored after engine works.

## 9. Testing strategy

- **Catch2** unit tests for: filterbank reconstruction (sum of bands ≈ input within −60 dB), formant biquad coefficient correctness, flanger LFO bounds, parameter smoothing.
- **`pluginval` strictness 10** in CI.
- **Manual perceptual tests** — listen to factory presets on drum loops, vocals, synth pads.

## 10. Project layout

```
HI-Spec/
├── CMakeLists.txt
├── cmake/
├── modules/                  # JUCE submodule, chowdsp_utils submodule
├── Source/
│   ├── PluginProcessor.{h,cpp}
│   ├── PluginEditor.{h,cpp}
│   ├── Parameters/
│   │   ├── ParameterIDs.h
│   │   └── ParameterLayout.cpp
│   ├── DSP/
│   │   ├── FxModule.h
│   │   ├── ComplexBuffer.h
│   │   ├── HilbertFilterbank.{h,cpp}
│   │   ├── SpectralDelay.{h,cpp}
│   │   ├── Vocoder.{h,cpp}        # carrier source + resonator + texture
│   │   └── SpectralFilter.{h,cpp} # shape + LFO + envelope
│   ├── GUI/
│   │   ├── HeaderBar.{h,cpp}
│   │   ├── SpectralCard.{h,cpp}
│   │   ├── SpectrumView.{h,cpp}
│   │   ├── BandDetailStrip.{h,cpp}
│   │   ├── FxModulesRow.{h,cpp}
│   │   ├── FxModuleCard.{h,cpp}        # base card w/ power + position chip
│   │   ├── PositionChip.{h,cpp}        # Pre/Post/FB · Global/Per-Band picker
│   │   ├── VocoderBody.{h,cpp}
│   │   ├── SpectralFilterBody.{h,cpp}
│   │   ├── Breadcrumb.{h,cpp}
│   │   └── LookAndFeel_HISpec.{h,cpp}
│   └── Utils/
├── docs/superpowers/specs/
├── mockup/                   # single-file HTML mockup
└── tests/
```

## 11. Out of scope for v1

- Cross-band feedback matrix (v2)
- Per-band-individual insert params (v1 broadcasts; v2 unlocks per-band)
- **Vocoder carriers**: External (sidechain audio) and Internal Pitched (MIDI-driven synth) — v2
- Spectral freeze / smear module (v2)
- MIDI control / modulation routing matrix (v2+)
- Per-band spectral pitch shift via formant-preserving cepstral lift (v2+)
- M/S processing (v2)

## 12. Open questions

- Best LPF order for the heterodyned baseband (TPT 4-pole vs cascade). Decide during prototyping.
- Whether per-band pitch shift via complex phase rotation is musical enough at ±12 semitones, or needs a phase-vocoder fallback.
- Preset format: bare APVTS XML vs chowdsp_presets module.

## 13. References (from research agents)

- Spectral delay: Surge XT, airwindows, paulxstretch, ChowMatrix, Faust filterbank libs
- Formant: Hillenbrand 1995 vowel data, Surge XT VocalFilter, MI Plaits
- Flanger: airwindows, Surge XT FlangerEffect, ToobAmp ToobFlanger
- JUCE: ChowDSP repos, Surge XT slot architecture, Vital modulation UI
- UI: Phase Plant (Snapins), FabFilter Pro-Q (selection-driven), Valhalla Supermassive (single-screen), u-he Bazille (cables), Bitwig Grid (color-coded flow)
