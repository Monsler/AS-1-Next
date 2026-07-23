#pragma once

#include "Adsr.h"
#include "Lfo.h"
#include "ModMatrix.h"
#include "MultiModeFilter.h"
#include "../presets/RasPreset.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <memory>
#include <vector>

namespace as1
{

class AS1AudioProcessor;

// A synthesizer voice that reproduces the Retro AS-1 architecture: up to three
// oscillators run through a multi-mode filter, driven by a fully general
// modulation matrix (any number of envelopes + LFOs, each routable to any
// destination — pitch, per-oscillator frequency / pulse-width / level, filter
// cutoff / resonance, master volume, pan). This mirrors how the hardware treats
// modulation and is what lets multi-envelope percussion (e.g. a 909 snare with
// an independent decay per oscillator) render correctly.
//
// Two paths share the class: when a preset snapshot is present (a .ras was
// loaded) the voice renders from it with the general matrix; otherwise it falls
// back to the flat APVTS amp/filter/pitch model so the plugin still works as a
// standalone/manual instrument.
class SynthVoice : public juce::SynthesiserVoice
{
public:
    explicit SynthVoice(AS1AudioProcessor& ownerProcessor);

    bool canPlaySound(juce::SynthesiserSound* sound) override;
    void startNote(int midiNoteNumber, float velocity, juce::SynthesiserSound* sound, int currentPitchWheelPosition) override;
    void stopNote(float velocity, bool allowTailOff) override;
    void pitchWheelMoved(int newValue) override;
    void controllerMoved(int controllerNumber, int newValue) override;
    using SynthesiserVoice::renderNextBlock;
    void renderNextBlock(juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples) override;

    static constexpr int kMaxOscillators = 3;

private:
    struct OscState
    {
        bool enabled = false;
        int waveform = 0; // 0 Saw,1 Triangle,2 Square,3 Sine,4 Noise
        double coarse = 0.0;
        double fine = 0.0;
        double symmetry = 50.0;
        double volume = 1.0;
        double phase = 0.0;
        int fmSource = 0;      // AnalogAudioSource: 1..3 = osc 1..3
        double fmAmount = 0.0; // 0..1 linear (phase) FM depth
        double lastOut = 0.0;  // previous sample, for inter-oscillator FM
    };

    // One filter as configured from the preset snapshot. Cutoff/resonance for
    // filter 0 track the APVTS knobs (so the GUI stays live); filter 1 is fully
    // snapshot-driven. inputOsc/inputOtherFilter reproduce the hardware's
    // per-filter input routing (parallel split or serial chain of the two).
    struct FilterState
    {
        MultiModeFilter dsp;
        bool enabled = false;
        double cutoff01 = 1.0;
        double resonance01 = 0.0;
        double spread01 = 0.0;
        double overdrive01 = 0.0;
        bool inputOsc[kMaxOscillators] = { false, false, false };
        bool inputOtherFilter = false;
        double lastOutput = 0.0;   // previous sample, for serial filter chaining
    };

    static double getMidiFreq(int noteNumber);

    // --- APVTS (manual / no-preset) path ---
    void startNoteApvts();
    double renderApvtsSample(double filtVal, double pitchVal);

    // --- General preset-matrix path ---
    void startNotePreset();
    double renderPresetSample(double& outPan);
    bool presetVoiceAlive() const;

    AS1AudioProcessor& processor;

    bool usePreset = false;
    std::shared_ptr<const RasPreset> preset;

    double baseFreq = 440.0;
    double noteBaseFreq = 440.0; // baseFreq without the live vibrato applied
    double vibPhase = 0.0;
    double velocityGain = 1.0;

    std::array<OscState, kMaxOscillators> oscs;
    int oscCount = 0;
    int midiNote = 60;

    // Preset path uses up to two filters (dual-filter routing); the manual APVTS
    // path uses filters[0] only via filterDsp aliasing below.
    std::array<FilterState, 2> filters;
    int filterCount = 0;
    MultiModeFilter filterDsp;   // APVTS-path single filter
    double filterCutoffPct = 100.0;
    double filterResonance = 0.0;

    juce::Random random;

    // Built-in .ras modulation sources (file source codes 0..7), evaluated once
    // per sample: 0 Note, 1 Velocity, 2 Mono AT, 3 Poly AT, 4..7 Controller A..D.
    std::array<double, 8> builtinVal { 0, 0, 0, 0, 0, 0, 0, 0 };
    void evalBuiltins();

    // ---- APVTS-path envelopes ----
    Adsr ampEnv;
    Adsr filtEnv;
    Adsr pitchEnv;
    double filterEnvAmount = 1.0;
    double pitchEnvAmount = 0.0;
    static constexpr double pitchEnvRangeOctaves = 4.0;

    // ---- General modulation matrix (preset path) ----
    // srcEnvs / srcLfos are parallel to preset->mod.sources; only the member
    // matching each source's kind is used.
    std::vector<Adsr> srcEnvs;
    std::vector<Lfo> srcLfos;
    std::vector<char> srcIsLfo;
    std::vector<double> srcVal;          // per-sample evaluated source values
    std::vector<ModRouting> routings;
    std::vector<int> ampCriticalEnvs;    // env source indices gating voice lifetime

    // Preset-path voice termination: free the voice once it has produced sound
    // and then fallen silent (drums decay to zero long before their slow master
    // envelope idles).
    int silentSamples = 0;
    bool hasSounded = false;

    // Musical scalings for routing amounts (the hardware's exact units live in
    // runtime lookup tables and aren't statically recoverable, so these are fit
    // to a musical range — see the RasParser curve notes).
    // Pitch: amount 1.0 ≈ 1 octave (a 909 kick's env→pitch amt 1.0 gives an
    // octave drop; the bank's LFO→pitch vibratos use amounts ~0.01..0.02).
    static constexpr double pitchModSemitones = 12.0;
    // Cutoff: the modulator adds to the cutoff KNOB (0..1), which is then mapped
    // through Hz = 22050*knob^1.5 (see cutoffToHz). Depth 1.0 = amount 1.0 sweeps
    // the knob its full span, so an acid/pad preset with a resting cutoff of 0
    // opens all the way under its filter envelope and closes back as it decays.
    static constexpr double cutoffModDepth = 1.0;
    static constexpr double resonanceModRange = 0.8;    // 0..1 resonance per unit amount
    static constexpr double pwmModRange = 45.0;         // duty-% per unit amount
    static constexpr double fmModRange = 1.0;           // FM-amount per unit routing amount
    // Phase-modulation depth: FM Amount 1.0 -> ±0.25 cycle (≈1.6 rad) of phase
    // deviation. Treating FM Amount directly as cycles (±0.9 cycle here) makes
    // the cross-FM leads (70's Fusion Lead, Electric Violin) collapse into hiss.
    static constexpr double fmDepthCycles = 0.25;
};

} // namespace as1
