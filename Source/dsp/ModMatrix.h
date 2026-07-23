#pragma once

#include <vector>

namespace as1
{

// LFO shape codes come straight from the `.ras` LFO chunk (byte 7). The factory
// bank uses 1/2/3/5 heavily and 4 rarely; the mapping mirrors the Retro AS-1
// editor's LFO waveform list.
enum class LfoWaveform
{
    Triangle,   // code 1 (by far the most common — classic vibrato)
    Sine,       // code 2
    Square,     // code 3
    SampleHold, // code 4 (stepped random)
    Sawtooth,   // code 5
};

// One parsed LFO. Rate is already resolved to Hz (the file stores a normalised
// 0..1 "speed" knob, higher = faster; see RasParser::lfoRateToHz). `delaySeconds`
// is a fade-in during which the LFO depth ramps from 0 to full (the editor's LFO
// Delay knob); 0 = full depth immediately.
struct LfoConfig
{
    LfoWaveform waveform = LfoWaveform::Triangle;
    double rateHz = 5.0;
    double delaySeconds = 0.0;
};

// A modulation source: one envelope OR one LFO. The `.ras` format addresses
// sources by a fixed index (source 8 = first modulator in the preset, 9 = next,
// …, counting envelopes AND LFOs together in file order). The routing matrix
// then says "source S drives destination D by amount A" — so the SAME engine
// treats envelopes and LFOs uniformly, exactly like the original hardware. This
// is what lets a 909 snare give each of its three oscillators an independent
// decay envelope, which the old fixed amp/filter/pitch-role model could not.
struct ModSource
{
    bool isLfo = false;
    LfoConfig lfo;  // meaningful when isLfo
    // Envelope times/levels (seconds / 0..1), meaningful when !isLfo. Held as
    // plain doubles so ModMatrix stays independent of RasPreset's EnvelopeConfig.
    double envAttack = 0.0, envDecay = 0.0, envSustain = 0.0, envSustainDecay = 0.0, envRelease = 0.0;
};

// Where a routing points. Decoded from the `rout` chunk's destination code
// (matching the editor's routing-destination dropdown in order):
//   0            -> Pitch (all oscillators)
//   1            -> Volume (master voice amplitude)
//   100/105/110  -> oscillator block: +0 Frequency, +1 Random, +2 Symmetry
//                   (PWM), +3 FM Amount, +4 Volume (per-osc VCA)
//   200..204     -> Filter 1: Cutoff, Spread, CM Amount, Resonance, Overdrive
//   205..209     -> Filter 2: same five, uses filterIndex
//   300 + 4*i+s  -> output scaler of modulator i (how "mod wheel controls
//                   vibrato depth" is expressed in the factory bank)
//   400/401      -> Global effect 1/2 send
enum class ModDest
{
    None,
    Pitch,
    Volume,
    Pan,
    OscFreq,      // uses oscIndex
    OscPulse,     // uses oscIndex (symmetry / PWM)
    OscFMAmount,  // uses oscIndex
    OscLevel,     // uses oscIndex (per-oscillator VCA)
    FilterCutoff,     // uses filterIndex
    FilterSpread,     // uses filterIndex
    FilterCM,         // uses filterIndex
    FilterResonance,  // uses filterIndex
    FilterOverdrive,  // uses filterIndex
    ModScale,     // uses modIndex: scales modulator modIndex's output
    Send,
};

// A single routing. `sourceIndex` >= 0 indexes ModMatrix::sources (envelopes /
// LFOs, file source code 8+); `builtinSource` >= 0 instead selects one of the
// hardware's fixed performance sources (file source codes 0..7): 0 Note,
// 1 Velocity, 2 Mono Aftertouch, 3 Poly Aftertouch, 4..7 Controller A..D
// (Controller A = mod wheel).
struct ModRouting
{
    int sourceIndex = -1;
    int builtinSource = -1;
    ModDest dest = ModDest::None;
    int oscIndex = 0;             // 0/1/2 = osc A/B/C
    int filterIndex = 0;          // 0/1 = filter 1/2
    int modIndex = 0;             // target modulator for ModScale
    double amount = 0.0;
};

// The complete modulation description the flat APVTS parameter set can't hold:
// every envelope + LFO the preset carries, and every routing between them and a
// destination. The processor keeps the current preset's ModMatrix and each
// SynthVoice copies it at note-on. Oscillator/filter values still travel through
// APVTS + the RasPreset snapshot.
struct ModMatrix
{
    std::vector<ModSource> sources;
    std::vector<ModRouting> routings;
};

} // namespace as1
