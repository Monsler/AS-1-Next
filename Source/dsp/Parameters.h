#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace as1::ParamIDs
{
    inline constexpr auto ampAttack       = "ampAttack";
    inline constexpr auto ampDecay        = "ampDecay";
    inline constexpr auto ampSustain      = "ampSustain";
    inline constexpr auto ampSustainDecay = "ampSustainDecay";
    inline constexpr auto ampRelease      = "ampRelease";

    inline constexpr auto filtAttack       = "filtAttack";
    inline constexpr auto filtDecay        = "filtDecay";
    inline constexpr auto filtSustain      = "filtSustain";
    inline constexpr auto filtSustainDecay = "filtSustainDecay";
    inline constexpr auto filtRelease      = "filtRelease";
    inline constexpr auto filtEnvAmount    = "filtEnvAmount";

    inline constexpr auto pitchAttack       = "pitchAttack";
    inline constexpr auto pitchDecay        = "pitchDecay";
    inline constexpr auto pitchSustain      = "pitchSustain";
    inline constexpr auto pitchSustainDecay = "pitchSustainDecay";
    inline constexpr auto pitchRelease      = "pitchRelease";
    inline constexpr auto pitchEnvAmount    = "pitchEnvAmount";

    inline constexpr auto filterCutoff    = "filterCutoff";
    inline constexpr auto filterResonance = "filterResonance";
    inline constexpr auto filterType      = "filterType";

    inline constexpr auto oscAEnabled  = "oscAEnabled";
    inline constexpr auto oscAWaveform = "oscAWaveform";
    inline constexpr auto oscACoarse   = "oscACoarse";
    inline constexpr auto oscAFine     = "oscAFine";
    inline constexpr auto oscASymmetry = "oscASymmetry";
    inline constexpr auto oscAVolume   = "oscAVolume";

    inline constexpr auto oscBEnabled  = "oscBEnabled";
    inline constexpr auto oscBWaveform = "oscBWaveform";
    inline constexpr auto oscBCoarse   = "oscBCoarse";
    inline constexpr auto oscBFine     = "oscBFine";
    inline constexpr auto oscBSymmetry = "oscBSymmetry";
    inline constexpr auto oscBVolume   = "oscBVolume";

    inline constexpr auto chorusOn       = "chorusOn";
    inline constexpr auto chorusDelayMs  = "chorusDelayMs";
    inline constexpr auto chorusFeedback = "chorusFeedback";
    inline constexpr auto chorusRate     = "chorusRate";
    inline constexpr auto chorusDepth    = "chorusDepth";
    inline constexpr auto chorusMix      = "chorusMix";

    inline constexpr auto delayOn       = "delayOn";
    inline constexpr auto delayTimeMs   = "delayTimeMs";
    inline constexpr auto delayFeedback = "delayFeedback";
    inline constexpr auto delayMix      = "delayMix";

    inline constexpr auto reverbOn         = "reverbOn";
    inline constexpr auto reverbType       = "reverbType"; // Room / Chamber / Small Hall / Large Hall
    inline constexpr auto reverbBrightness = "reverbBrightness";
    inline constexpr auto reverbDecay      = "reverbDecay";
    inline constexpr auto reverbMix        = "reverbMix";

    // Macro-strip vibrato: a live (not note-snapshotted) pitch LFO applied by
    // every voice, so the bottom-band knobs work mid-note like FLEX's macros.
    inline constexpr auto vibratoDepth = "vibratoDepth";
    inline constexpr auto vibratoRate  = "vibratoRate";

    inline constexpr auto masterGain = "masterGain";

    inline constexpr auto voiceMode = "voiceMode"; // 0 = Poly, 1 = Mono
}

namespace as1
{

inline const juce::StringArray waveformChoices { "Saw", "Triangle", "Square", "Sine", "Noise" };

// Order matches the FilterType enum in RasPreset.h (== the `filt` byte-7 code),
// which is the Retro AS-1 Editor's own filter-type dropdown.
inline const juce::StringArray filterTypeChoices {
    "LP 1-Pole", "LP 2-Pole", "LP 4-Pole Res",
    "HP 1-Pole", "HP 2-Pole", "HP 4-Pole Res",
    "AP 1-Pole", "AP 2-Pole", "AP 4-Pole",
    "SV LP", "SV BP", "SV BR", "SV HP"
};

inline const juce::StringArray voiceModeChoices { "Poly", "Mono" };

// Room list mirrors the original Retro AS-1 "Global Effect 2: Reverb" radio
// group (Room / Chamber / Small Hall / Large Hall).
inline const juce::StringArray reverbTypeChoices { "Room", "Chamber", "Small Hall", "Large Hall" };

inline juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    using namespace ParamIDs;
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Envelope times run to 20s (RampSpeedToSeconds' slowest); heavy skew keeps
    // the musically common sub-second range usable on the knobs.
    auto secRange = juce::NormalisableRange<float>(0.0f, 20.0f, 0.0001f, 0.25f);
    auto pctRange = juce::NormalisableRange<float>(0.0f, 100.0f, 0.01f);
    auto sustainRange = juce::NormalisableRange<float>(0.0f, 1.0f, 0.0001f);

    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(ampAttack, 1), "Amp Attack", secRange, 0.01f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(ampDecay, 1), "Amp Decay", secRange, 0.2f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(ampSustain, 1), "Amp Sustain", sustainRange, 0.8f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(ampSustainDecay, 1), "Amp Sustain Decay", secRange, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(ampRelease, 1), "Amp Release", secRange, 0.3f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(filtAttack, 1), "Filter Attack", secRange, 0.01f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(filtDecay, 1), "Filter Decay", secRange, 0.2f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(filtSustain, 1), "Filter Sustain", sustainRange, 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(filtSustainDecay, 1), "Filter Sustain Decay", secRange, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(filtRelease, 1), "Filter Release", secRange, 0.3f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(filtEnvAmount, 1), "Filter Env Amount",
                        juce::NormalisableRange<float>(-2.0f, 2.0f, 0.001f), 1.0f));

    // Pitch envelope — driven by presets' env->Pitch routings (e.g. a 909
    // kick's pitch drop). Not surfaced in the GUI yet; loaded from the preset.
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(pitchAttack, 1), "Pitch Attack", secRange, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(pitchDecay, 1), "Pitch Decay", secRange, 0.2f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(pitchSustain, 1), "Pitch Sustain", sustainRange, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(pitchSustainDecay, 1), "Pitch Sustain Decay", secRange, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(pitchRelease, 1), "Pitch Release", secRange, 0.2f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(pitchEnvAmount, 1), "Pitch Env Amount",
                        juce::NormalisableRange<float>(-1.0f, 1.0f, 0.001f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(filterCutoff, 1), "Filter Cutoff", pctRange, 100.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(filterResonance, 1), "Filter Resonance", pctRange, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID(filterType, 1), "Filter Type", filterTypeChoices, 1));

    params.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID(oscAEnabled, 1), "Osc A Enabled", true));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID(oscAWaveform, 1), "Osc A Waveform", waveformChoices, 0));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(oscACoarse, 1), "Osc A Coarse",
                        juce::NormalisableRange<float>(-24.0f, 24.0f, 1.0f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(oscAFine, 1), "Osc A Fine",
                        juce::NormalisableRange<float>(-100.0f, 100.0f, 0.1f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(oscASymmetry, 1), "Osc A Symmetry", pctRange, 50.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(oscAVolume, 1), "Osc A Volume",
                        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 1.0f));

    params.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID(oscBEnabled, 1), "Osc B Enabled", false));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID(oscBWaveform, 1), "Osc B Waveform", waveformChoices, 0));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(oscBCoarse, 1), "Osc B Coarse",
                        juce::NormalisableRange<float>(-24.0f, 24.0f, 1.0f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(oscBFine, 1), "Osc B Fine",
                        juce::NormalisableRange<float>(-100.0f, 100.0f, 0.1f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(oscBSymmetry, 1), "Osc B Symmetry", pctRange, 50.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(oscBVolume, 1), "Osc B Volume",
                        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 1.0f));

    // Chorus knob set mirrors the original editor's "Insert Effect: Chorus"
    // fader row (Delay / Feedback / Speed / Depth / Mix).
    params.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID(chorusOn, 1), "Chorus On", false));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(chorusDelayMs, 1), "Chorus Delay",
                        juce::NormalisableRange<float>(1.0f, 30.0f, 0.1f), 9.1f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(chorusFeedback, 1), "Chorus Feedback",
                        juce::NormalisableRange<float>(0.0f, 95.0f, 0.1f), 10.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(chorusRate, 1), "Chorus Speed",
                        juce::NormalisableRange<float>(0.05f, 5.0f, 0.01f, 0.5f), 0.51f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(chorusDepth, 1), "Chorus Depth", pctRange, 50.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(chorusMix, 1), "Chorus Mix", pctRange, 50.0f));

    params.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID(delayOn, 1), "Delay On", false));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(delayTimeMs, 1), "Delay Time",
                        juce::NormalisableRange<float>(1.0f, 1500.0f, 1.0f, 0.4f), 300.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(delayFeedback, 1), "Delay Feedback", pctRange, 30.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(delayMix, 1), "Delay Mix", pctRange, 50.0f));

    // Reverb mirrors the original "Global Effect: Reverb" (room-type selector
    // + Brightness / Decay faders), realised with juce::Reverb underneath.
    params.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID(reverbOn, 1), "Reverb On", false));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID(reverbType, 1), "Reverb Type", reverbTypeChoices, 2));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(reverbBrightness, 1), "Reverb Brightness", pctRange, 60.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(reverbDecay, 1), "Reverb Decay", pctRange, 40.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(reverbMix, 1), "Reverb Mix", pctRange, 30.0f));

    // Depth 100% = ±50 cents, the classic performance-vibrato sweep.
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(vibratoDepth, 1), "Vibrato", pctRange, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(vibratoRate, 1), "Vibrato Rate",
                        juce::NormalisableRange<float>(0.1f, 12.0f, 0.01f, 0.5f), 5.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(masterGain, 1), "Master Gain",
                        juce::NormalisableRange<float>(0.0f, 1.5f, 0.001f), 0.8f));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID(voiceMode, 1), "Voice Mode", voiceModeChoices, 0));

    return { params.begin(), params.end() };
}

} // namespace as1
