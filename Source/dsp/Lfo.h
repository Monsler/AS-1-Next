#pragma once

#include "ModMatrix.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <numbers>
#include <cmath>

namespace as1
{

// A single low-frequency oscillator, ticked once per sample like everything else
// in the voice. Free-running (phase starts at 0 on note-on), matching the way
// the factory presets serialise their LFOs (trailing sync field is always the
// same "key sync" value across the bank). Output is bipolar [-1, 1] for the
// periodic shapes and stepped-random for Sample & Hold.
class Lfo
{
public:
    void configure(const LfoConfig& cfg, double sampleRateHz)
    {
        waveform = cfg.waveform;
        sampleRate = sampleRateHz;
        phaseInc = cfg.rateHz / sampleRateHz;
        delayFrames = static_cast<int>(cfg.delaySeconds * sampleRateHz);
        phase = 0.0;
        frame = 0;
        heldValue = randomBipolar();
    }

    // Returns the LFO value scaled by its fade-in envelope (0..1 over the delay
    // time). Call exactly once per sample.
    double tick()
    {
        double raw;
        switch (waveform)
        {
            case LfoWaveform::Sine:
                raw = std::sin(2.0 * std::numbers::pi * phase);
                break;
            case LfoWaveform::Square:
                raw = phase < 0.5 ? 1.0 : -1.0;
                break;
            case LfoWaveform::Sawtooth:
                raw = 2.0 * phase - 1.0;
                break;
            case LfoWaveform::SampleHold:
                raw = heldValue;
                break;
            case LfoWaveform::Triangle:
            default:
                raw = 2.0 * std::abs(2.0 * phase - 1.0) - 1.0; // -1..1 triangle
                break;
        }

        double prevPhase = phase;
        phase += phaseInc;
        if (phase >= 1.0)
        {
            phase -= 1.0;
            heldValue = randomBipolar(); // new step each cycle for Sample & Hold
        }
        (void) prevPhase;

        double fade = 1.0;
        if (delayFrames > 0 && frame < delayFrames)
            fade = static_cast<double>(frame) / static_cast<double>(delayFrames);
        ++frame;

        return raw * fade;
    }

private:
    double randomBipolar() { return random.nextDouble() * 2.0 - 1.0; }

    LfoWaveform waveform = LfoWaveform::Triangle;
    double sampleRate = 44100.0;
    double phaseInc = 0.0;
    double phase = 0.0;
    int delayFrames = 0;
    int frame = 0;
    double heldValue = 0.0;
    juce::Random random;
};

} // namespace as1
