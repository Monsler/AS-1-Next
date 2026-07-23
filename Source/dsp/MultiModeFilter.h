#pragma once

#include "../presets/RasPreset.h"
#include <algorithm>
#include <array>
#include <numbers>
#include <cmath>

namespace as1
{

// Multi-mode filter covering the Retro AS-1's full filter list (the `filt`
// byte-7 code, 0..12; see FilterType). The original Python reference only had a
// single lowpass biquad, but the factory bank uses 1/2/4-pole lowpass and
// highpass, 4-pole allpass, and the four state-variable modes (LP/BP/BR/HP) —
// the last group carries most of the acid/band-pass/reject presets.
//
// Everything is realised with a zero-delay-feedback (TPT) state-variable core
// (Zavalishin / Cytomic form), which produces LP, BP, HP, notch and allpass
// outputs from one stable topology. 1-pole modes use a single TPT one-pole;
// 2-pole modes use one SVF stage; 4-pole modes cascade two SVF stages (24 dB).
// `spread` detunes the two stages of a 4-pole filter; `overdrive` soft-clips
// the output (the editor's per-filter Overdrive knob).
class MultiModeFilter
{
public:
    void setSampleRate(double sampleRateHz) { fs = sampleRateHz; }
    void setType(FilterType t) { type = t; }

    void reset()
    {
        for (auto& s : svf) s = SvfState{};
        for (auto& s : onePole) s = 0.0;
    }

    // fc in Hz, resonance/overdrive as 0..1. Kept 3-arg-compatible (spread and
    // overdrive default off) for the manual APVTS render path.
    double process(double x, double fc, double resonance01, double spread01 = 0.0, double overdrive01 = 0.0)
    {
        fc = std::clamp(fc, 20.0, fs * 0.49);
        double q = std::clamp(resonance01, 0.0, 1.0);
        // k = 1/Q damping: q=0 -> k=2 (Q 0.5, gentle), q=1 -> k~0.05 (very
        // resonant, near self-oscillation for the acid presets).
        double k = 2.0 * (1.0 - 0.975 * q) + 0.03;

        double out;
        if (isFourPole())
        {
            double f2 = std::min(fc * std::pow(2.0, spread01), fs * 0.49);
            out = svfStage(0, x, fc, k);
            out = svfStage(1, out, f2, k);
        }
        else if (isOnePole())
        {
            out = onePoleStage(0, x, fc);
        }
        else // 2-pole
        {
            out = svfStage(0, x, fc, k);
        }

        if (overdrive01 > 1.0e-4)
        {
            double drive = 1.0 + overdrive01 * 6.0;
            out = std::tanh(out * drive) / std::tanh(drive);
        }
        return out;
    }

private:
    struct SvfState { double ic1 = 0.0, ic2 = 0.0; };

    // One TPT SVF stage; picks the LP/BP/HP/notch/allpass output for `type`.
    double svfStage(int idx, double x, double fc, double k)
    {
        auto& s = svf[static_cast<size_t>(idx)];
        double g = std::tan(std::numbers::pi * fc / fs);
        double a1 = 1.0 / (1.0 + g * (g + k));
        double a2 = g * a1;
        double a3 = g * a2;

        double v3 = x - s.ic2;
        double v1 = a1 * s.ic1 + a2 * v3;
        double v2 = s.ic2 + a2 * s.ic1 + a3 * v3;
        s.ic1 = 2.0 * v1 - s.ic1;
        s.ic2 = 2.0 * v2 - s.ic2;

        double lp = v2;
        double bp = v1;
        double hp = x - k * v1 - v2;
        switch (type)
        {
            case FilterType::Highpass1Pole:
            case FilterType::Highpass2Pole:
            case FilterType::Highpass4Pole:
            case FilterType::SVHighpass:      return hp;
            case FilterType::SVBandpass:      return bp * k; // normalise BP peak
            case FilterType::SVBandreject:    return x - k * v1; // notch
            case FilterType::Allpass1Pole:
            case FilterType::Allpass2Pole:
            case FilterType::Allpass4Pole:    return x - 2.0 * k * v1;
            default:                          return lp; // all lowpass + SV LP
        }
    }

    // One-pole TPT low/high pass (6 dB); resonance has no effect on a 1-pole.
    double onePoleStage(int idx, double x, double fc)
    {
        auto& z = onePole[static_cast<size_t>(idx)];
        double g = std::tan(std::numbers::pi * fc / fs);
        double v = (x - z) * g / (1.0 + g);
        double lp = v + z;
        z = lp + v;
        return isHighpass() ? (x - lp) : lp;
    }

    bool isOnePole() const
    {
        return type == FilterType::Lowpass1Pole || type == FilterType::Highpass1Pole
            || type == FilterType::Allpass1Pole;
    }
    bool isFourPole() const
    {
        return type == FilterType::Lowpass4Pole || type == FilterType::Highpass4Pole
            || type == FilterType::Allpass4Pole;
    }
    bool isHighpass() const
    {
        return type == FilterType::Highpass1Pole || type == FilterType::Highpass2Pole
            || type == FilterType::Highpass4Pole || type == FilterType::SVHighpass;
    }

    double fs = 44100.0;
    FilterType type = FilterType::Lowpass4Pole;
    std::array<SvfState, 2> svf;
    std::array<double, 2> onePole { 0.0, 0.0 };
};

} // namespace as1
