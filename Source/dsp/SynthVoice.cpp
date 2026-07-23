#include "SynthVoice.h"
#include "SynthSound.h"
#include "Parameters.h"
#include "../PluginProcessor.h"

#include <algorithm>
#include <numbers>
#include <cmath>

namespace as1
{

SynthVoice::SynthVoice(AS1AudioProcessor& ownerProcessor) : processor(ownerProcessor)
{
}

bool SynthVoice::canPlaySound(juce::SynthesiserSound* sound)
{
    return dynamic_cast<SynthSound*>(sound) != nullptr;
}

double SynthVoice::getMidiFreq(int noteNumber)
{
    return 440.0 * std::pow(2.0, (noteNumber - 69) / 12.0);
}

// PolyBLEP residual: the correction added at a discontinuity to band-limit a
// naive saw/pulse edge. `t` is the phase (0..1) and `dt` the per-sample phase
// increment. Without this, the raw ramp/step oscillators alias badly at higher
// notes — a gritty, hissy "digital" edge the analog original never had. This
// removes that grit so saw ensembles and drum tones stay clean.
static inline double polyBlep(double t, double dt)
{
    if (dt <= 0.0)
        return 0.0;
    if (t < dt)
    {
        t /= dt;
        return t + t - t * t - 1.0;
    }
    if (t > 1.0 - dt)
    {
        t = (t - 1.0) / dt;
        return t * t + t + t + 1.0;
    }
    return 0.0;
}

// Band-limited oscillator sample. `phi` = phase 0..1, `dt` = phase increment,
// `sym` = pulse duty (0..1, only used for waveform 2). Waveform codes match
// waveformIndex() (0 Saw, 1 Triangle, 2 Pulse, 3 Sine, 4 Noise).
static double bandlimitedOsc(int waveform, double phi, double dt, double sym,
                             juce::Random& random)
{
    switch (waveform)
    {
        case 3: // Sine
            return std::sin(2.0 * std::numbers::pi * phi);
        case 1: // Triangle (naive; low aliasing, left band-unlimited)
            return 2.0 * std::abs(2.0 * phi - 1.0) - 1.0;
        case 2: // Pulse: BLEP both edges, then remove the duty-cycle DC term.
        {
            double v = (phi < sym) ? 1.0 : -1.0;
            v += polyBlep(phi, dt);
            v -= polyBlep(std::fmod(phi - sym + 1.0, 1.0), dt);
            return v - (2.0 * sym - 1.0);
        }
        case 4: // Noise
            return random.nextDouble() * 2.0 - 1.0;
        default: // Saw: naive ramp minus the rising-edge BLEP residual.
            return (2.0 * phi - 1.0) - polyBlep(phi, dt);
    }
}

static int waveformIndex(Waveform w)
{
    switch (w)
    {
        case Waveform::Saw: return 0;
        case Waveform::Triangle: return 1;
        case Waveform::Square: return 2;
        case Waveform::Sine: return 3;
        case Waveform::Noise: return 4;
        default: return 0;
    }
}

void SynthVoice::startNote(int midiNoteNumber, float velocity, juce::SynthesiserSound*, int)
{
    noteBaseFreq = baseFreq = getMidiFreq(midiNoteNumber);
    velocityGain = static_cast<double>(velocity);
    midiNote = midiNoteNumber;

    preset = processor.getPresetSnapshot();
    usePreset = preset != nullptr && preset->valid && !preset->oscillators.empty();

    filterDsp.setSampleRate(getSampleRate());
    filterDsp.reset();
    for (auto& f : filters)
    {
        f.dsp.setSampleRate(getSampleRate());
        f.dsp.reset();
    }

    if (usePreset)
        startNotePreset();
    else
        startNoteApvts();
}

// ---------------------------------------------------------------------------
// General preset-matrix path
// ---------------------------------------------------------------------------

void SynthVoice::startNotePreset()
{
    // Oscillators (up to three).
    oscCount = 0;
    for (int i = 0; i < kMaxOscillators; ++i)
    {
        oscs[static_cast<size_t>(i)] = OscState{};
        if (i < static_cast<int>(preset->oscillators.size()))
        {
            const auto& o = preset->oscillators[static_cast<size_t>(i)];
            auto& os = oscs[static_cast<size_t>(i)];
            os.enabled = o.enabled;
            os.waveform = waveformIndex(o.waveform);
            os.coarse = o.coarseTune;
            os.fine = o.fineTune;
            os.symmetry = o.symmetry;
            os.volume = o.volume;
            os.phase = 0.0;
            os.fmSource = o.fmSource;
            os.fmAmount = o.fmAmount;
            os.lastOut = 0.0;
            if (o.enabled)
                ++oscCount;
        }
    }
    if (oscCount == 0) { oscs[0] = OscState{}; oscs[0].enabled = true; oscCount = 1; }

    // Filters. The preset may enable one or two; reproduce each filter's type,
    // cutoff/resonance/spread/overdrive and oscillator-input routing. Filter 0's
    // cutoff/resonance come from the APVTS (PresetManager mirrors the preset's
    // values there, so the Main-tab and macro Cutoff/Res knobs stay live);
    // filter 1 is fully snapshot-driven.
    filterCutoffPct = static_cast<double>(processor.apvts.getRawParameterValue(ParamIDs::filterCutoff)->load());
    filterResonance = static_cast<double>(processor.apvts.getRawParameterValue(ParamIDs::filterResonance)->load());

    filterCount = 0;
    bool anyOscRouted = false;
    for (int fi = 0; fi < 2; ++fi)
    {
        auto& fs = filters[static_cast<size_t>(fi)];
        fs = FilterState{};
        if (fi >= static_cast<int>(preset->filters.size()))
            continue;
        const auto& src = preset->filters[static_cast<size_t>(fi)];
        fs.enabled = src.enabled;
        fs.dsp.setType(src.type);
        fs.dsp.setSampleRate(getSampleRate());
        fs.dsp.reset();
        fs.cutoff01 = fi == 0 ? filterCutoffPct / 100.0 : src.cutoff;
        fs.resonance01 = fi == 0 ? filterResonance / 100.0 : src.resonance;
        fs.spread01 = src.spread;
        fs.overdrive01 = src.overdrive;
        for (int oi = 0; oi < kMaxOscillators; ++oi)
        {
            fs.inputOsc[oi] = src.inputOsc[oi];
            anyOscRouted = anyOscRouted || (src.inputOsc[oi] && src.enabled);
        }
        fs.inputOtherFilter = src.inputOtherFilter;
        if (src.enabled)
            filterCount = fi + 1;
    }
    // Fallback: if no enabled filter names any oscillator input (some presets
    // leave the input bits clear), feed all oscillators into filter 0 so the
    // voice isn't silent.
    if (!anyOscRouted && filterCount > 0)
        for (int oi = 0; oi < kMaxOscillators; ++oi)
            filters[0].inputOsc[oi] = true;

    // Modulation sources: an Adsr for each envelope source, an Lfo for each LFO
    // source, parallel to preset->mod.sources.
    const auto& sources = preset->mod.sources;
    size_t n = sources.size();

    // The preset's "amp envelope" = every envelope source routed to master
    // Volume. PresetManager mirrors its times into the flat amp* APVTS params
    // on load, so configuring those sources from the APVTS is identity until
    // the user moves an amp knob — which makes the AMP ENVELOPE section and
    // the macro Attack/Release knobs live for presets too. (Per-oscillator
    // level envelopes — e.g. drum presets — keep their own snapshot times.)
    std::vector<char> isVolumeEnv(n, 0);
    for (const auto& r : preset->mod.routings)
        if (r.dest == ModDest::Volume && r.sourceIndex >= 0 && static_cast<size_t>(r.sourceIndex) < n
            && !sources[static_cast<size_t>(r.sourceIndex)].isLfo)
            isVolumeEnv[static_cast<size_t>(r.sourceIndex)] = 1;

    auto readParam = [this](const char* id)
    { return static_cast<double>(processor.apvts.getRawParameterValue(id)->load()); };

    srcEnvs.assign(n, Adsr{});
    srcLfos.assign(n, Lfo{});
    srcIsLfo.assign(n, 0);
    srcVal.assign(n, 0.0);
    for (size_t i = 0; i < n; ++i)
    {
        if (sources[i].isLfo)
        {
            srcIsLfo[i] = 1;
            srcLfos[i].configure(sources[i].lfo, getSampleRate());
        }
        else
        {
            if (isVolumeEnv[i])
                srcEnvs[i].configure(readParam(ParamIDs::ampAttack), readParam(ParamIDs::ampDecay),
                                     readParam(ParamIDs::ampSustain), readParam(ParamIDs::ampRelease),
                                     getSampleRate(), readParam(ParamIDs::ampSustainDecay));
            else
                srcEnvs[i].configure(sources[i].envAttack, sources[i].envDecay, sources[i].envSustain,
                                     sources[i].envRelease, getSampleRate(), sources[i].envSustainDecay);
            srcEnvs[i].triggerOn();
        }
    }

    routings = preset->mod.routings;

    // Envelope sources that gate the voice's lifetime: those routed to master
    // Volume or to a per-oscillator level.
    ampCriticalEnvs.clear();
    for (const auto& r : routings)
    {
        if ((r.dest == ModDest::Volume || r.dest == ModDest::OscLevel)
            && r.sourceIndex >= 0 && static_cast<size_t>(r.sourceIndex) < n
            && !srcIsLfo[static_cast<size_t>(r.sourceIndex)])
        {
            if (std::find(ampCriticalEnvs.begin(), ampCriticalEnvs.end(), r.sourceIndex) == ampCriticalEnvs.end())
                ampCriticalEnvs.push_back(r.sourceIndex);
        }
    }
    // Fallback: if nothing routes to volume/level, any envelope keeps it alive.
    if (ampCriticalEnvs.empty())
        for (size_t i = 0; i < n; ++i)
            if (!srcIsLfo[i]) ampCriticalEnvs.push_back(static_cast<int>(i));

    silentSamples = 0;
    hasSounded = false;
}

// Resting cutoff + modulation -> Hz. The filter envelope / LFOs add to the cutoff
// KNOB (0..1), and the combined knob is then mapped through the measured resting
// curve (CAnalogConvert::FilterCutoffToHertz) Hz = 22050 * knob^1.5 — the way the
// hardware's cutoff knob and its modulation depth combine. This is what lets a
// preset closed at rest (cutoff knob 0, e.g. a 303 acid line or the many
// env-swept "cutoff=0" pads) actually open: an env amount of ~0.5 sweeps the knob
// to 0.5 -> ~7.8 kHz, then closes back toward the resting cutoff as the env
// decays. (The old octave-above-20Hz model could only reach ~600 Hz from a 0
// base, leaving every one of those presets muted.)
static double cutoffToHz(double knob01, double modKnob, double maxHz)
{
    double knob = std::clamp(knob01 + modKnob, 0.0, 1.0);
    double hz = 22050.0 * std::pow(knob, 1.5);
    return std::min(std::max(hz, 20.0), maxHz);
}

void SynthVoice::evalBuiltins()
{
    // 0 Note (octaves from middle C, for keyboard tracking of cutoff/etc.),
    // 1 Velocity, 2 Mono Aftertouch, 3 Poly AT (unavailable -> mono), 4 Controller
    // A = mod wheel, 5..7 Controller B..D (unmapped -> 0).
    builtinVal[0] = (midiNote - 60) / 12.0;
    builtinVal[1] = velocityGain;
    builtinVal[2] = processor.getAftertouch();
    builtinVal[3] = processor.getAftertouch();
    builtinVal[4] = processor.getModWheel();
    builtinVal[5] = 0.0;
    builtinVal[6] = 0.0;
    builtinVal[7] = 0.0;
}

double SynthVoice::renderPresetSample(double& outPan)
{
    // 1. Evaluate every envelope/LFO source and the built-in sources once.
    size_t n = srcVal.size();
    for (size_t i = 0; i < n; ++i)
        srcVal[i] = srcIsLfo[i] ? srcLfos[i].tick() : srcEnvs[i].tick();
    evalBuiltins();

    auto routingValue = [&](const ModRouting& r) -> double
    {
        double base = 0.0;
        if (r.sourceIndex >= 0 && static_cast<size_t>(r.sourceIndex) < n)
            base = srcVal[static_cast<size_t>(r.sourceIndex)];
        else if (r.builtinSource >= 0 && r.builtinSource < 8)
            base = builtinVal[static_cast<size_t>(r.builtinSource)];
        return base * r.amount;
    };

    // 1b. ModScale pre-pass. A modulator targeted by a "depth" (Amount) routing
    // has its per-sample value scaled by that control (0..1). The routing's SIGN
    // sets the resting behaviour: a POSITIVE amount opens depth up FROM ZERO
    // (base 0 — e.g. a mod-wheel vibrato that is silent until the wheel rises),
    // while a NEGATIVE amount cuts depth DOWN FROM FULL (base 1 — e.g. the
    // "MdWhl Volume Knob" patch, full at rest, the wheel pulling it down). In the
    // factory bank the controls are the mod wheel / aftertouch (0 at rest),
    // sometimes velocity / note. Untargeted modulators are left untouched.
    std::array<double, 64> scaleAcc;
    std::array<bool, 64> scaled;
    std::array<bool, 64> scaleFromFull;
    scaleAcc.fill(0.0);
    scaled.fill(false);
    scaleFromFull.fill(false);
    for (const auto& r : routings)
    {
        if (r.dest != ModDest::ModScale)
            continue;
        size_t mi = static_cast<size_t>(r.modIndex);
        if (mi < n && mi < scaleAcc.size())
        {
            scaleAcc[mi] += routingValue(r);
            scaled[mi] = true;
            if (r.amount < 0.0)
                scaleFromFull[mi] = true;
        }
    }
    for (size_t i = 0; i < n && i < scaleAcc.size(); ++i)
        if (scaled[i])
            srcVal[i] *= juce::jlimit(0.0, 1.0, (scaleFromFull[i] ? 1.0 : 0.0) + scaleAcc[i]);

    // 2. Accumulate routings by destination.
    double pitchOct = 0.0, pan = 0.0, masterVol = 0.0;
    bool hasMasterVol = false;
    std::array<double, kMaxOscillators> oscPitch { 0.0, 0.0, 0.0 };
    std::array<double, kMaxOscillators> oscPwm { 0.0, 0.0, 0.0 };
    std::array<double, kMaxOscillators> oscFM { 0.0, 0.0, 0.0 };
    std::array<double, kMaxOscillators> oscLevel { 0.0, 0.0, 0.0 };
    std::array<bool, kMaxOscillators> oscHasLevel { false, false, false };
    std::array<double, 2> filtCutOct { 0.0, 0.0 };
    std::array<double, 2> filtResAdd { 0.0, 0.0 };
    std::array<double, 2> filtSpreadAdd { 0.0, 0.0 };
    std::array<double, 2> filtODAdd { 0.0, 0.0 };

    for (const auto& r : routings)
    {
        if (r.dest == ModDest::ModScale)
            continue;
        double v = routingValue(r);
        int o = juce::jlimit(0, kMaxOscillators - 1, r.oscIndex);
        int fi = juce::jlimit(0, 1, r.filterIndex);
        switch (r.dest)
        {
            case ModDest::Pitch:           pitchOct += v * (pitchModSemitones / 12.0); break;
            case ModDest::Volume:          masterVol += v; hasMasterVol = true; break;
            case ModDest::Pan:             pan += v; break;
            case ModDest::OscFreq:         oscPitch[static_cast<size_t>(o)] += v * (pitchModSemitones / 12.0); break;
            case ModDest::OscPulse:        oscPwm[static_cast<size_t>(o)] += v * pwmModRange; break;
            case ModDest::OscFMAmount:     oscFM[static_cast<size_t>(o)] += v * fmModRange; break;
            case ModDest::OscLevel:        oscLevel[static_cast<size_t>(o)] += v; oscHasLevel[static_cast<size_t>(o)] = true; break;
            case ModDest::FilterCutoff:    filtCutOct[static_cast<size_t>(fi)] += v * cutoffModDepth; break;
            case ModDest::FilterResonance: filtResAdd[static_cast<size_t>(fi)] += v * resonanceModRange; break;
            case ModDest::FilterSpread:    filtSpreadAdd[static_cast<size_t>(fi)] += v; break;
            case ModDest::FilterOverdrive: filtODAdd[static_cast<size_t>(fi)] += v; break;
            case ModDest::FilterCM:        filtCutOct[static_cast<size_t>(fi)] += v * cutoffModDepth; break; // audio-rate CM approximated as cutoff mod
            case ModDest::ModScale: case ModDest::Send: case ModDest::None: default: break;
        }
    }

    // 3. Oscillators -> per-oscillator output (pre-filter), keeping each one
    // separate so the filters can pull their own input set.
    double sampleRate = getSampleRate();
    std::array<double, kMaxOscillators> oscOut { 0.0, 0.0, 0.0 };
    for (int i = 0; i < kMaxOscillators; ++i)
    {
        auto& osc = oscs[static_cast<size_t>(i)];
        if (!osc.enabled)
            continue;

        double detuneSemitones = osc.coarse + (osc.fine / 100.0);
        double octaves = pitchOct + oscPitch[static_cast<size_t>(i)];
        double freq = baseFreq * std::pow(2.0, detuneSemitones / 12.0) * std::pow(2.0, octaves);
        double phaseInc = freq / sampleRate;
        osc.phase = std::fmod(osc.phase + phaseInc, 1.0);

        // Linear (phase) FM from another oscillator, using its previous sample.
        double phi = osc.phase;
        double fmAmt = osc.fmAmount + oscFM[static_cast<size_t>(i)];
        if (fmAmt > 1.0e-4 && osc.fmSource >= 1 && osc.fmSource <= kMaxOscillators)
        {
            double mod = oscs[static_cast<size_t>(osc.fmSource - 1)].lastOut;
            phi = std::fmod(phi + fmAmt * mod * fmDepthCycles + 1.0, 1.0);
        }
        double sym = juce::jlimit(0.02, 0.98, (osc.symmetry + oscPwm[static_cast<size_t>(i)]) / 100.0);

        double val = bandlimitedOsc(osc.waveform, phi, phaseInc, sym, random);
        osc.lastOut = val;

        double gain = oscHasLevel[static_cast<size_t>(i)]
                        ? std::max(0.0, oscLevel[static_cast<size_t>(i)])
                        : osc.volume;
        oscOut[static_cast<size_t>(i)] = val * gain;
    }

    // 4. Filters. Each pulls its routed oscillator set (plus optionally the other
    // filter's previous output for serial chaining); the voice output is the sum
    // of the filters not consumed by another filter. With no enabled filter the
    // dry oscillator mix passes through.
    double maxHz = sampleRate / 2.2;
    double dryMix = 0.0;
    for (int i = 0; i < kMaxOscillators; ++i)
        dryMix += oscOut[static_cast<size_t>(i)];
    dryMix /= static_cast<double>(oscCount);

    if (filterCount == 0)
    {
        outPan = pan;
        double ampNoFilt = hasMasterVol ? std::max(0.0, masterVol) : 1.0;
        return dryMix * ampNoFilt;
    }

    std::array<double, 2> filtOut { 0.0, 0.0 };
    std::array<double, 2> prevOut { filters[0].lastOutput, filters[1].lastOutput };
    for (int fi = 0; fi < 2; ++fi)
    {
        auto& fs = filters[static_cast<size_t>(fi)];
        if (!fs.enabled)
            continue;
        double in = 0.0;
        for (int oi = 0; oi < kMaxOscillators; ++oi)
            if (fs.inputOsc[oi])
                in += oscOut[static_cast<size_t>(oi)];
        if (fs.inputOtherFilter)
            in += prevOut[static_cast<size_t>(fi == 0 ? 1 : 0)];
        in /= static_cast<double>(oscCount);

        double hz = cutoffToHz(fs.cutoff01, filtCutOct[static_cast<size_t>(fi)], maxHz);
        double res = juce::jlimit(0.0, 1.0, fs.resonance01 + filtResAdd[static_cast<size_t>(fi)]);
        double spread = juce::jlimit(0.0, 2.0, fs.spread01 + filtSpreadAdd[static_cast<size_t>(fi)]);
        double od = juce::jlimit(0.0, 1.0, fs.overdrive01 + filtODAdd[static_cast<size_t>(fi)]);
        filtOut[static_cast<size_t>(fi)] = fs.dsp.process(in, hz, res, spread, od);
        fs.lastOutput = filtOut[static_cast<size_t>(fi)];
    }

    // Output = filters not feeding another filter (the chain's tail); parallel
    // filters sum.
    bool consumed[2] = { false, false };
    for (int fi = 0; fi < 2; ++fi)
        if (filters[static_cast<size_t>(fi)].enabled && filters[static_cast<size_t>(fi)].inputOtherFilter)
            consumed[fi == 0 ? 1 : 0] = true;

    double filtered = 0.0;
    for (int fi = 0; fi < 2; ++fi)
        if (filters[static_cast<size_t>(fi)].enabled && !consumed[fi])
            filtered += filtOut[static_cast<size_t>(fi)];

    // Oscillators not assigned to any enabled filter pass DRY to the output (the
    // AS-1 routes each oscillator into a filter or straight to the amp; an
    // unassigned one is not muted). Without this a patch like "Don't Kill The
    // Whale", whose two main oscillators feed no filter, would be near-silent.
    for (int oi = 0; oi < kMaxOscillators; ++oi)
    {
        if (!oscs[static_cast<size_t>(oi)].enabled)
            continue;
        bool routed = false;
        for (int fi = 0; fi < 2; ++fi)
            if (filters[static_cast<size_t>(fi)].enabled && filters[static_cast<size_t>(fi)].inputOsc[oi])
                routed = true;
        if (!routed)
            filtered += oscOut[static_cast<size_t>(oi)] / static_cast<double>(oscCount);
    }

    // 5. Master amplitude. A routing to Volume is the voice VCA; otherwise the
    // per-oscillator level envelopes (or static levels) already shaped the output.
    double amp = hasMasterVol ? std::max(0.0, masterVol) : 1.0;

    outPan = pan;
    return filtered * amp;
}

bool SynthVoice::presetVoiceAlive() const
{
    for (int idx : ampCriticalEnvs)
        if (srcEnvs[static_cast<size_t>(idx)].getState() != Adsr::State::Idle)
            return true;
    return ampCriticalEnvs.empty(); // no gating envelopes -> rely on the energy cutoff
}

// ---------------------------------------------------------------------------
// APVTS (manual / no-preset) path
// ---------------------------------------------------------------------------

void SynthVoice::startNoteApvts()
{
    auto& apvts = processor.apvts;
    auto readParam = [&apvts](const char* id) { return static_cast<double>(apvts.getRawParameterValue(id)->load()); };

    double ampAttack  = std::max(0.002, readParam(ParamIDs::ampAttack));
    double ampDecay   = readParam(ParamIDs::ampDecay);
    double ampSustain = readParam(ParamIDs::ampSustain);
    double ampRelease = std::max(0.002, readParam(ParamIDs::ampRelease));

    double filtAttack  = readParam(ParamIDs::filtAttack);
    double filtDecay   = readParam(ParamIDs::filtDecay);
    double filtSustain = readParam(ParamIDs::filtSustain);
    double filtRelease = readParam(ParamIDs::filtRelease);
    filterEnvAmount = readParam(ParamIDs::filtEnvAmount);

    double pitchAttack  = readParam(ParamIDs::pitchAttack);
    double pitchDecay   = readParam(ParamIDs::pitchDecay);
    double pitchSustain = readParam(ParamIDs::pitchSustain);
    double pitchRelease = readParam(ParamIDs::pitchRelease);
    pitchEnvAmount = readParam(ParamIDs::pitchEnvAmount);

    ampEnv.configure(ampAttack, ampDecay, ampSustain, ampRelease, getSampleRate(), readParam(ParamIDs::ampSustainDecay));
    filtEnv.configure(filtAttack, filtDecay, filtSustain, filtRelease, getSampleRate(), readParam(ParamIDs::filtSustainDecay));
    pitchEnv.configure(pitchAttack, pitchDecay, pitchSustain, pitchRelease, getSampleRate(), readParam(ParamIDs::pitchSustainDecay));

    filterCutoffPct = readParam(ParamIDs::filterCutoff);
    filterResonance = readParam(ParamIDs::filterResonance);
    filterDsp.setType(static_cast<FilterType>(static_cast<int>(readParam(ParamIDs::filterType))));

    oscs[0] = OscState{};
    oscs[0].enabled = apvts.getRawParameterValue(ParamIDs::oscAEnabled)->load() > 0.5f;
    oscs[0].waveform = static_cast<int>(readParam(ParamIDs::oscAWaveform));
    oscs[0].coarse = readParam(ParamIDs::oscACoarse);
    oscs[0].fine = readParam(ParamIDs::oscAFine);
    oscs[0].symmetry = readParam(ParamIDs::oscASymmetry);
    oscs[0].volume = readParam(ParamIDs::oscAVolume);

    oscs[1] = OscState{};
    oscs[1].enabled = apvts.getRawParameterValue(ParamIDs::oscBEnabled)->load() > 0.5f;
    oscs[1].waveform = static_cast<int>(readParam(ParamIDs::oscBWaveform));
    oscs[1].coarse = readParam(ParamIDs::oscBCoarse);
    oscs[1].fine = readParam(ParamIDs::oscBFine);
    oscs[1].symmetry = readParam(ParamIDs::oscBSymmetry);
    oscs[1].volume = readParam(ParamIDs::oscBVolume);
    oscs[2] = OscState{};

    oscCount = (oscs[0].enabled ? 1 : 0) + (oscs[1].enabled ? 1 : 0);
    if (oscCount == 0)
    {
        oscs[0] = OscState{ true, 0, 0.0, 0.0, 50.0, 1.0, 0.0 };
        oscCount = 1;
    }

    ampEnv.triggerOn();
    filtEnv.triggerOn();
    pitchEnv.triggerOn();
}

double SynthVoice::renderApvtsSample(double filtVal, double pitchVal)
{
    double mixed = 0.0;
    double sampleRate = getSampleRate();
    double pitchOctaves = pitchVal * pitchEnvAmount * pitchEnvRangeOctaves;

    for (int i = 0; i < 2; ++i)
    {
        auto& osc = oscs[static_cast<size_t>(i)];
        if (!osc.enabled)
            continue;

        double detuneSemitones = osc.coarse + (osc.fine / 100.0);
        double freq = baseFreq * std::pow(2.0, detuneSemitones / 12.0) * std::pow(2.0, pitchOctaves);
        double phaseInc = freq / sampleRate;
        osc.phase = std::fmod(osc.phase + phaseInc, 1.0);
        double phi = osc.phase;
        double sym = osc.symmetry / 100.0;

        double val = bandlimitedOsc(osc.waveform, phi, phaseInc, sym, random);
        mixed += val * osc.volume;
    }
    mixed /= static_cast<double>(oscCount);

    double cutoffPct = filterCutoffPct / 100.0;
    // Exact filter cutoff curve from RetroLib.dll (CAnalogConvert::FilterCutoffToHertz,
    // a CAnalogPowerTable): Hz = 22050 * knob^1.5, knob in 0..1.
    double baseCutoffHz = 22050.0 * std::pow(cutoffPct, 1.5);
    double modulatedCutoff = baseCutoffHz * std::pow(2.0, filtVal * 6.0 * filterEnvAmount);
    modulatedCutoff = std::min(modulatedCutoff, sampleRate / 2.2);

    // filterResonance is a 0..100 APVTS percentage; the filter takes 0..1.
    return filterDsp.process(mixed, modulatedCutoff, filterResonance / 100.0);
}

// ---------------------------------------------------------------------------

void SynthVoice::stopNote(float, bool allowTailOff)
{
    if (!allowTailOff)
    {
        clearCurrentNote();
        return;
    }

    if (usePreset)
    {
        for (size_t i = 0; i < srcEnvs.size(); ++i)
            if (!srcIsLfo[i]) srcEnvs[i].triggerOff();
    }
    else
    {
        ampEnv.triggerOff();
        filtEnv.triggerOff();
        pitchEnv.triggerOff();
    }
}

void SynthVoice::pitchWheelMoved(int) {}
void SynthVoice::controllerMoved(int, int) {}

void SynthVoice::renderNextBlock(juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples)
{
    int numChannels = outputBuffer.getNumChannels();
    double silenceThresh = getSampleRate() * 0.06; // ~60 ms of silence frees a decayed voice

    // Macro-strip vibrato is read fresh each block (NOT snapshotted at note-on)
    // so the bottom-band Vibrato / Vib-rate knobs act on held notes. It bends
    // baseFreq, so both render paths pick it up. Depth 100% = ±50 cents.
    double vibDepth = static_cast<double>(processor.apvts.getRawParameterValue(ParamIDs::vibratoDepth)->load()) / 100.0;
    double vibInc = static_cast<double>(processor.apvts.getRawParameterValue(ParamIDs::vibratoRate)->load()) / getSampleRate();

    for (int s = 0; s < numSamples; ++s)
    {
        double sample;
        double pan = 0.0;

        if (vibDepth > 0.0)
        {
            vibPhase = std::fmod(vibPhase + vibInc, 1.0);
            double vibOct = std::sin(2.0 * std::numbers::pi * vibPhase) * vibDepth * (0.5 / 12.0);
            baseFreq = noteBaseFreq * std::pow(2.0, vibOct);
        }
        else
        {
            baseFreq = noteBaseFreq;
        }

        if (usePreset)
        {
            sample = renderPresetSample(pan) * velocityGain;

            // Terminate once the voice has sounded and gone quiet, or when all
            // gating envelopes have released to idle.
            double mag = std::abs(sample);
            if (mag > 0.01) hasSounded = true;
            if (mag < 1.0e-4) ++silentSamples; else silentSamples = 0;

            bool decayedSilent = hasSounded && silentSamples > static_cast<int>(silenceThresh);
            if (decayedSilent || !presetVoiceAlive())
            {
                clearCurrentNote();
                return;
            }
        }
        else
        {
            double ampVal = ampEnv.tick();
            double filtVal = filtEnv.tick();
            double pitchVal = pitchEnv.tick();
            if (ampEnv.getState() == Adsr::State::Idle)
            {
                clearCurrentNote();
                return;
            }
            sample = renderApvtsSample(filtVal, pitchVal) * ampVal * velocityGain;
        }

        if (numChannels >= 2)
        {
            double p = juce::jlimit(-1.0, 1.0, pan);
            double angle = (p * 0.5 + 0.5) * (std::numbers::pi / 2.0);
            outputBuffer.addSample(0, startSample + s, static_cast<float>(sample * std::cos(angle)));
            outputBuffer.addSample(1, startSample + s, static_cast<float>(sample * std::sin(angle)));
            for (int ch = 2; ch < numChannels; ++ch)
                outputBuffer.addSample(ch, startSample + s, static_cast<float>(sample));
        }
        else
        {
            for (int ch = 0; ch < numChannels; ++ch)
                outputBuffer.addSample(ch, startSample + s, static_cast<float>(sample));
        }
    }
}

} // namespace as1
