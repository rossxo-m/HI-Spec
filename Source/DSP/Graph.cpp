#include "Graph.h"
#include <cmath>

namespace hispec
{

namespace
{
    float getFloat (juce::AudioProcessorValueTreeState& apvts, juce::StringRef id, float defaultVal = 0.0f)
    {
        if (auto* p = apvts.getRawParameterValue (id))
            return p->load();
        return defaultVal;
    }

    int getChoiceIndex (juce::AudioProcessorValueTreeState& apvts, juce::StringRef id)
    {
        if (auto* p = apvts.getRawParameterValue (id))
            return static_cast<int> (p->load());
        return 0;
    }

    bool getBool (juce::AudioProcessorValueTreeState& apvts, juce::StringRef id)
    {
        if (auto* p = apvts.getRawParameterValue (id))
            return p->load() > 0.5f;
        return false;
    }

    int bandCountFromParam (int choice)
    {
        switch (choice) { case 0: return 8; case 1: return 16; case 2: return 32; }
        return 16;
    }
}

Graph::Graph()
{
    fx[0] = std::make_unique<Vocoder>();
    fx[1] = std::make_unique<SpectralFilter>();
}

void Graph::prepare (const ProcessSpec& s)
{
    spec = s;

    HilbertFilterbank::ProcessSpec hbSpec { spec.sampleRate, spec.maxBlockSize, 2 };
    filterbank.prepare (hbSpec);
    filterbank.setBandCount (activeBandCount);

    SpectralDelay::ProcessSpec dlSpec {};
    dlSpec.sampleRate   = spec.sampleRate;
    dlSpec.maxBlockSize = spec.maxBlockSize;
    dlSpec.maxBands     = ids::kMaxBands;
    dlSpec.maxDelayMs   = 2000.0f;
    delay.prepare (dlSpec);

    FxModule::ProcessSpec fxSpec {};
    fxSpec.sampleRate   = spec.sampleRate;
    fxSpec.maxBlockSize = spec.maxBlockSize;
    fxSpec.maxBands     = ids::kMaxBands;
    fxSpec.numChannels  = 2;
    for (auto& m : fx) m->prepare (fxSpec);

    bandParams.assign (ids::kMaxBands, SpectralDelay::BandParams {});
    centerFrequencies.assign (ids::kMaxBands, 0.0f);

    // The TPT filterbank reports zero fixed group delay. Keep the hook
    // wired so we can revise if we swap in an FIR design later.
    fixedLatencySamples = filterbank.getLatencySamples();
}

void Graph::reset ()
{
    delay.reset();
    for (auto& m : fx) m->reset();
    // Hilbert bank has no explicit reset() — rebuild via setBandCount
    filterbank.setBandCount (activeBandCount);
}

int Graph::getLatencySamples () const noexcept
{
    return fixedLatencySamples;
}

ids::Position Graph::positionFor (int slot) const noexcept
{
    return modulePositions[static_cast<size_t> (slot)];
}

void Graph::renderBandParamsFromApvts (juce::AudioProcessorValueTreeState& apvts)
{
    for (int i = 0; i < ids::kMaxBands; ++i)
    {
        SpectralDelay::BandParams bp;
        bp.timeMs  = getFloat (apvts, ids::band_time  (i), 0.0f);
        bp.fb      = getFloat (apvts, ids::band_fb    (i), 0.0f);
        bp.gainDb  = getFloat (apvts, ids::band_gain  (i), 0.0f);
        bp.pan     = getFloat (apvts, ids::band_pan   (i), 0.0f);
        bp.pitchSt = getFloat (apvts, ids::band_pitch (i), 0.0f);
        bandParams[static_cast<size_t> (i)] = bp;
    }
    delay.setBandParams (bandParams);
}

void Graph::renderModuleParamsFromApvts (juce::AudioProcessorValueTreeState& apvts)
{
    // ── Vocoder ─────────────────────────────────────────────────────────
    {
        auto* v = static_cast<Vocoder*> (fx[0].get());
        Vocoder::Params p;
        p.power        = getBool (apvts, ids::voc_power);
        p.mix          = getFloat (apvts, ids::voc_mix, 1.0f);
        p.carrier      = static_cast<Vocoder::Carrier> (getChoiceIndex (apvts, ids::voc_carrier));
        p.vowelX       = getFloat (apvts, ids::voc_vowelX, 0.5f);
        p.vowelY       = getFloat (apvts, ids::voc_vowelY, 0.5f);
        p.q            = getFloat (apvts, ids::voc_q,      0.5f);
        p.formantShift = getFloat (apvts, ids::voc_formantShift, 0.0f);
        p.texDrive     = getFloat (apvts, ids::voc_texDrive, 0.0f);
        p.texRate      = getFloat (apvts, ids::voc_texRate, 1.0f);
        p.texDepth     = getFloat (apvts, ids::voc_texDepth, 0.0f);
        p.texDelay     = getFloat (apvts, ids::voc_texDelay, 2.0f);
        p.texLfoShape  = static_cast<Vocoder::LfoShape> (getChoiceIndex (apvts, ids::voc_texLfoShape));
        p.texThruZero  = getBool (apvts, ids::voc_texThruZero);
        v->setParams (p);

        modulePositions[0] = static_cast<ids::Position> (getChoiceIndex (apvts, ids::voc_position));
    }

    // ── Spectral Filter ─────────────────────────────────────────────────
    {
        auto* s = static_cast<SpectralFilter*> (fx[1].get());
        SpectralFilter::Params p;
        p.power      = getBool (apvts, ids::sf_power);
        p.mix        = getFloat (apvts, ids::sf_mix, 1.0f);
        p.shape      = static_cast<SpectralFilter::Shape> (getChoiceIndex (apvts, ids::sf_shape));
        p.freq       = getFloat (apvts, ids::sf_freq, 1000.0f);
        p.q          = getFloat (apvts, ids::sf_q, 0.707f);
        p.gainDb     = getFloat (apvts, ids::sf_gain, 0.0f);
        p.lfoRate    = getFloat (apvts, ids::sf_lfoRate, 0.5f);
        p.lfoDepth   = getFloat (apvts, ids::sf_lfoDepth, 0.0f);
        p.lfoShape   = static_cast<SpectralFilter::LfoShape> (getChoiceIndex (apvts, ids::sf_lfoShape));
        p.envAttack  = getFloat (apvts, ids::sf_envAttack,  0.010f);
        p.envRelease = getFloat (apvts, ids::sf_envRelease, 0.150f);
        p.envDepth   = getFloat (apvts, ids::sf_envDepth, 0.0f);
        s->setParams (p);

        modulePositions[1] = static_cast<ids::Position> (getChoiceIndex (apvts, ids::sf_position));
    }
}

void Graph::process (juce::AudioBuffer<float>& buffer,
                     juce::AudioProcessorValueTreeState& apvts)
{
    // 1. Apply pending band count (hot-swap) — read once per block.
    const int requested = bandCountFromParam (getChoiceIndex (apvts, ids::delay_bandCount));
    if (requested != activeBandCount)
    {
        activeBandCount = requested;
        filterbank.setBandCount (activeBandCount);
    }

    renderBandParamsFromApvts (apvts);
    renderModuleParamsFromApvts (apvts);

    // Refresh centre-frequency list (cheap; nBands ≤ 32)
    centerFrequencies.resize (static_cast<size_t> (ids::kMaxBands), 0.0f);
    for (int k = 0; k < ids::kMaxBands; ++k)
        centerFrequencies[static_cast<size_t> (k)] =
            k < filterbank.getBandCount() ? filterbank.getBandCenterHz (k) : 0.0f;
    delay.setBandCenterFrequencies (centerFrequencies);
    for (auto& m : fx) m->setBandCenterFrequencies (centerFrequencies);

    // 2. Pre Global FX
    for (int i = 0; i < kNumModules; ++i)
        if (positionFor (i) == ids::Position::GlobalPre)
            fx[static_cast<size_t> (i)]->processStereo (buffer);

    // 3. Analysis → bands
    filterbank.analyse (buffer, bands);

    // 4. Pre PerBand FX
    for (int i = 0; i < kNumModules; ++i)
        if (positionFor (i) == ids::Position::PerBandPre)
            fx[static_cast<size_t> (i)]->processComplex (bands);

    // 5. Spectral delay
    delay.process (bands);

    // 6. Post PerBand FX
    for (int i = 0; i < kNumModules; ++i)
        if (positionFor (i) == ids::Position::PerBandPost)
            fx[static_cast<size_t> (i)]->processComplex (bands);

    // 7. Synthesis → stereo
    filterbank.synthesise (bands, buffer);

    // 8. Post Global FX
    for (int i = 0; i < kNumModules; ++i)
        if (positionFor (i) == ids::Position::GlobalPost)
            fx[static_cast<size_t> (i)]->processStereo (buffer);
}

} // namespace hispec
