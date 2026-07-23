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

enum class FilterType
{
    Lowpass2Pole,
    Lowpass4Pole,
    Bandpass4Pole,
    Highpass4Pole,
    Unknown
};

struct OscillatorConfig
{
    bool enabled = false;
    Waveform waveform = Waveform::Saw;
    int coarseTune = 0;      // semitones
    double fineTune = 0.0;   // cents
    double symmetry = 50.0;  // 0..100 pulse duty (50 = square)
    double volume = 1.0;     // 0..1 mix level
};

struct FilterConfig
{
    bool enabled = false;
    FilterType type = FilterType::Lowpass4Pole;
    double cutoff = 100.0;        // 0..100
    double resonance = 0.0;       // 0..100
    double polyModAmount = 0.0;   // 0..100
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
