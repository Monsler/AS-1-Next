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

    preset = processor.getPresetSnapshot();
    usePreset = preset != nullptr && preset->valid && !preset->oscillators.empty();

    filterDsp.setSampleRate(getSampleRate());
    filterDsp.reset();

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
            if (o.enabled)
                ++oscCount;
        }
    }
    if (oscCount == 0) { oscs[0] = OscState{ true, 0, 0.0, 0.0, 50.0, 1.0, 0.0 }; oscCount = 1; }

    // Filter. Cutoff/resonance are read from the APVTS, not the snapshot:
    // PresetManager mirrors the preset's values into the APVTS on load, so this
    // is identity until the user turns a knob — and it makes the Main-tab and
    // macro-strip Cutoff/Res knobs work while a preset is loaded.
    filterDsp.setType(!preset->filters.empty() ? preset->filters[0].type : FilterType::Lowpass4Pole);
    filterCutoffPct = static_cast<double>(processor.apvts.getRawParameterValue(ParamIDs::filterCutoff)->load());
    filterResonance = static_cast<double>(processor.apvts.getRawParameterValue(ParamIDs::filterResonance)->load());

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

double SynthVoice::renderPresetSample(double& outPan)
{
    // 1. Evaluate every modulation source once for this sample.
    size_t n = srcVal.size();
    for (size_t i = 0; i < n; ++i)
        srcVal[i] = srcIsLfo[i] ? srcLfos[i].tick() : srcEnvs[i].tick();

    // 2. Accumulate routings by destination.
    double pitchOct = 0.0, cutoffOct = 0.0, resAdd = 0.0, pan = 0.0, masterVol = 0.0;
    bool hasMasterVol = false;
    std::array<double, kMaxOscillators> oscPitch { 0.0, 0.0, 0.0 };
    std::array<double, kMaxOscillators> oscPwm { 0.0, 0.0, 0.0 };
    std::array<double, kMaxOscillators> oscLevel { 0.0, 0.0, 0.0 };
    std::array<bool, kMaxOscillators> oscHasLevel { false, false, false };

    for (const auto& r : routings)
    {
        if (r.sourceIndex < 0 || static_cast<size_t>(r.sourceIndex) >= n)
            continue;
        double v = srcVal[static_cast<size_t>(r.sourceIndex)] * r.amount;
        int o = juce::jlimit(0, kMaxOscillators - 1, r.oscIndex);
        switch (r.dest)
        {
            case ModDest::Pitch:          pitchOct += v * (pitchModSemitones / 12.0); break;
            case ModDest::Volume:         masterVol += v; hasMasterVol = true; break;
            case ModDest::Pan:            pan += v; break;
            case ModDest::OscFreq:        oscPitch[static_cast<size_t>(o)] += v * (pitchModSemitones / 12.0); break;
            case ModDest::OscPulse:       oscPwm[static_cast<size_t>(o)] += v * pwmModRange; break;
            case ModDest::OscLevel:       oscLevel[static_cast<size_t>(o)] += v; oscHasLevel[static_cast<size_t>(o)] = true; break;
            case ModDest::FilterCutoff:   cutoffOct += v * cutoffModOctaves; break;
            case ModDest::FilterResonance:resAdd += v * resonanceModRange; break;
            case ModDest::None: default: break;
        }
    }

    // 3. Oscillators.
    double sampleRate = getSampleRate();
    double mixed = 0.0;
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
        double phi = osc.phase;
        double sym = juce::jlimit(0.02, 0.98, (osc.symmetry + oscPwm[static_cast<size_t>(i)]) / 100.0);

        double val;
        switch (osc.waveform)
        {
            case 3: val = std::sin(2.0 * std::numbers::pi * phi); break;         // Sine
            case 1: val = 2.0 * std::abs(2.0 * phi - 1.0) - 1.0; break;           // Triangle
            case 2: val = (phi < sym) ? 1.0 : -1.0; break;                        // Square / Pulse
            case 4: val = random.nextDouble() * 2.0 - 1.0; break;                 // Noise
            default: val = 2.0 * phi - 1.0; break;                                // Saw
        }

        // Per-oscillator level: an envelope-driven VCA when routed, else the
        // oscillator's own static mix level.
        double gain = oscHasLevel[static_cast<size_t>(i)]
                        ? std::max(0.0, oscLevel[static_cast<size_t>(i)])
                        : osc.volume;
        mixed += val * gain;
    }
    mixed /= static_cast<double>(oscCount);

    // 4. Filter (base cutoff exponential, modulated in octaves).
    double cutoffPct = filterCutoffPct / 100.0;
    // Exact filter cutoff curve from RetroLib.dll (CAnalogConvert::FilterCutoffToHertz,
    // a CAnalogPowerTable): Hz = 22050 * knob^1.5, knob in 0..1.
    double baseCutoffHz = 22050.0 * std::pow(cutoffPct, 1.5);
    double modCutoff = baseCutoffHz * std::pow(2.0, cutoffOct);
    modCutoff = std::min(modCutoff, sampleRate / 2.2);
    double resonance = juce::jlimit(0.0, 100.0, filterResonance + resAdd);
    double filtered = filterDsp.process(mixed, modCutoff, resonance);

    // 5. Master amplitude. When a routing drives Volume, that is the voice VCA;
    // otherwise the per-oscillator level envelopes already shape the output.
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
        osc.phase = std::fmod(osc.phase + freq / sampleRate, 1.0);
        double phi = osc.phase;
        double sym = osc.symmetry / 100.0;

        double val;
        switch (osc.waveform)
        {
            case 3: val = std::sin(2.0 * std::numbers::pi * phi); break;
            case 1: val = 2.0 * std::abs(2.0 * phi - 1.0) - 1.0; break;
            case 2: val = (phi < sym) ? 1.0 : -1.0; break;
            case 4: val = random.nextDouble() * 2.0 - 1.0; break;
            default: val = 2.0 * phi - 1.0; break;
        }
        mixed += val * osc.volume;
    }
    mixed /= static_cast<double>(oscCount);

    double cutoffPct = filterCutoffPct / 100.0;
    // Exact filter cutoff curve from RetroLib.dll (CAnalogConvert::FilterCutoffToHertz,
    // a CAnalogPowerTable): Hz = 22050 * knob^1.5, knob in 0..1.
    double baseCutoffHz = 22050.0 * std::pow(cutoffPct, 1.5);
    double modulatedCutoff = baseCutoffHz * std::pow(2.0, filtVal * 6.0 * filterEnvAmount);
    modulatedCutoff = std::min(modulatedCutoff, sampleRate / 2.2);

    return filterDsp.process(mixed, modulatedCutoff, filterResonance);
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
