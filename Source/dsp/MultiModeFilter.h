#pragma once

#include "../presets/RasPreset.h"
#include <algorithm>
#include <array>
#include <numbers>
#include <cmath>

namespace as1
{

// Multi-mode extension of the Python reference's BiquadLP. The original port
// only implemented a lowpass; the factory bank also uses 2-pole lowpass,
// 4-pole (24 dB) lowpass, 4-pole bandpass and 4-pole highpass (filt byte-7
// codes 0/1, 2/9, 10, 12 respectively). Coefficients are the standard RBJ
// forms, recomputed every sample exactly like BiquadLP; the 4-pole modes
// cascade two identical biquad stages for a 24 dB/oct slope.
class MultiModeFilter
{
public:
    void setSampleRate(double sampleRateHz) { fs = sampleRateHz; }
    void setType(FilterType t) { type = t; }

    void reset()
    {
        for (auto& s : stages)
            s = Stage{};
    }

    double process(double x, double fc, double qDb)
    {
        double q = 0.5 + 9.5 * (qDb / 100.0);
        fc = std::max(20.0, std::min(fc, fs / 2.1));

        double w0 = 2.0 * std::numbers::pi * fc / fs;
        double cosW0 = std::cos(w0);
        double alpha = std::sin(w0) / (2.0 * q);

        double b0, b1, b2;
        switch (type)
        {
            case FilterType::Highpass4Pole:
                b0 = (1.0 + cosW0) / 2.0;
                b1 = -(1.0 + cosW0);
                b2 = (1.0 + cosW0) / 2.0;
                break;
            case FilterType::Bandpass4Pole:
                b0 = alpha;   // constant 0 dB peak (BPF)
                b1 = 0.0;
                b2 = -alpha;
                break;
            case FilterType::Lowpass2Pole:
            case FilterType::Lowpass4Pole:
            case FilterType::Unknown:
            default:
                b0 = (1.0 - cosW0) / 2.0;
                b1 = 1.0 - cosW0;
                b2 = (1.0 - cosW0) / 2.0;
                break;
        }

        double a0 = 1.0 + alpha;
        double a1 = -2.0 * cosW0;
        double a2 = 1.0 - alpha;

        b0 /= a0; b1 /= a0; b2 /= a0; a1 /= a0; a2 /= a0;

        int numStages = isFourPole() ? 2 : 1;
        double out = x;
        for (int i = 0; i < numStages; ++i)
            out = stages[static_cast<size_t>(i)].process(out, b0, b1, b2, a1, a2);
        return out;
    }

private:
    struct Stage
    {
        double x1 = 0.0, x2 = 0.0, y1 = 0.0, y2 = 0.0;
        double process(double x, double b0, double b1, double b2, double a1, double a2)
        {
            double out = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
            x2 = x1; x1 = x;
            y2 = y1; y1 = out;
            return out;
        }
    };

    bool isFourPole() const
    {
        return type == FilterType::Lowpass4Pole
            || type == FilterType::Bandpass4Pole
            || type == FilterType::Highpass4Pole;
    }

    double fs = 44100.0;
    FilterType type = FilterType::Lowpass4Pole;
    std::array<Stage, 2> stages;
};

} // namespace as1
