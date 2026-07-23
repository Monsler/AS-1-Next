#pragma once

#include "../dsp/ModMatrix.h"

#include <cstdint>
#include <string>
#include <vector>

namespace as1
{

enum class Waveform
{
    Saw,
    Triangle,
    Square,
    Sine,
    Noise,
    Unknown
};

// Full filter-type list from the Retro AS-1 Editor's dropdown; the list index
// IS the `filt` byte-7 code (0..12). "SV" = state-variable (12 dB modes).
enum class FilterType
{
    Lowpass1Pole,   // 0  "1 Pole LP"
    Lowpass2Pole,   // 1  "2 Pole LP"
    Lowpass4Pole,   // 2  "4 Pole LP Resonant"
    Highpass1Pole,  // 3  "1 Pole HP"
    Highpass2Pole,  // 4  "2 Pole HP"
    Highpass4Pole,  // 5  "4 Pole HP Resonant"
    Allpass1Pole,   // 6  "1 Pole AP Resonant"
    Allpass2Pole,   // 7  "2 Pole AP Resonant"
    Allpass4Pole,   // 8  "4 Pole AP Resonant"
    SVLowpass,      // 9  "State Variable LP"
    SVBandpass,     // 10 "State Variable BP"
    SVBandreject,   // 11 "State Variable BR"
    SVHighpass,     // 12 "State Variable HP"
    Unknown
};

// Audio-rate source selector used by oscillator sync / FM and filter CM
// (editor dropdown order): 0 None, 1..3 Oscillator 1..3, 4 Filter 1 in,
// 5 Filter 1 out, 6 Filter 2 in, 7 Filter 2 out.
struct OscillatorConfig
{
    bool enabled = false;
    bool keyTrack = true;    // osci byte 5: oscillator follows the keyboard
    Waveform waveform = Waveform::Saw;
    int syncSource = 0;      // osci bytes 8-9 (AnalogAudioSource): hard sync master
    int fmSource = 0;        // osci bytes 10-11 (AnalogAudioSource): linear FM source
    int coarseTune = 0;      // semitones
    double fineTune = 0.0;   // cents
    double symmetry = 50.0;  // 0..100 pulse duty (50 = square)
    double fmAmount = 0.0;   // 0..1 FM depth (osci double @38)
    double volume = 1.0;     // 0..1 mix level
};

// One of the two filters. The five doubles follow the editor's knob order
// (verified against RetroLib.dll's export table: getter RVA -> member offset):
// Cutoff@14, Spread@22, CM Amt@30, Resonance@38, Overdrive@46. Bytes 10..12
// select which oscillators feed this filter; byte 13 chains in the OTHER
// filter's output (serial routing). Bytes 8-9 = CM (cutoff-modulation) audio
// source.
struct FilterConfig
{
    bool enabled = false;
    FilterType type = FilterType::Lowpass4Pole;
    double cutoff = 1.0;          // 0..1 knob; Hz = 22050 * cutoff^1.5
    double spread = 0.0;          // 0..1 separation of the two cascade stages
    double cmAmount = 0.0;        // 0..1 audio-rate cutoff modulation depth
    double resonance = 0.0;       // 0..1
    double overdrive = 0.0;       // 0..1 post-filter drive
    int cmSource = 0;             // AnalogAudioSource
    bool inputOsc[3] = { false, false, false };
    bool inputOtherFilter = false;
};

struct EnvelopeConfig
{
    double attack = 0.0;        // seconds
    double decay = 0.0;         // seconds
    double sustain = 0.0;       // 0..1 level
    double sustainDecay = 0.0;  // seconds to fall from sustain toward 0 while held; 0 = hold forever
    double release = 0.0;       // seconds
};

struct EffectsConfig
{
    bool delayEnabled = false;
    double delayTimeMs = 0.0;
    double delayFeedback = 0.0; // 0..100
    bool reverbEnabled = false;
    double reverbMix = 0.0;     // 0..100
};

struct RoutingConfig
{
    uint16_t source = 0;
    uint16_t destination = 0;
    double amount = 0.0;
};

struct RasPreset
{
    std::string name = "Init";

    std::vector<OscillatorConfig> oscillators;
    std::vector<FilterConfig> filters;

    EnvelopeConfig ampEnvelope;
    EnvelopeConfig filterEnvelope;
    EnvelopeConfig pitchEnvelope;

    // Modulation depths taken from the routing matrix (0..1 for the .ras
    // "amount"). filterEnvAmount scales the filter-cutoff sweep; pitchEnvAmount
    // scales the oscillator pitch sweep (0 = no pitch modulation).
    double filterEnvAmount = 1.0;
    double pitchEnvAmount = 0.0;

    EffectsConfig effects;
    std::vector<RoutingConfig> routings;

    // LFOs and their (resolved) routings — the part of the preset that doesn't
    // fit the flat APVTS parameter set. Carried to the voice as a snapshot.
    ModMatrix mod;

    bool valid = false;
    std::string error;
};

} // namespace as1
