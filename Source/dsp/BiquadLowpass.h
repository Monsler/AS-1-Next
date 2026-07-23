#pragma once

#include <algorithm>
#include <numbers>
#include <cmath>

namespace as1
{

// Literal C++ port of the BiquadLP class in metro-as1/play_ras.py — an RBJ
// biquad lowpass recomputed every sample, exactly like the Python reference.
class BiquadLowpass
{
public:
    explicit BiquadLowpass(double sampleRate = 44100.0) : fs(sampleRate) {}

    void setSampleRate(double sampleRate) { fs = sampleRate; }

    void reset()
    {
        x1 = x2 = y1 = y2 = 0.0;
    }

    double process(double x, double fc, double qDb)
    {
        // Map resonance (0..100) to Q (0.5..10.0).
        double q = 0.5 + 9.5 * (qDb / 100.0);
        fc = std::max(20.0, std::min(fc, fs / 2.1));

        double w0 = 2.0 * std::numbers::pi * fc / fs;
        double alpha = std::sin(w0) / (2.0 * q);
        double cosW0 = std::cos(w0);

        double b0 = (1.0 - cosW0) / 2.0;
        double b1 = 1.0 - cosW0;
        double b2 = (1.0 - cosW0) / 2.0;
        double a0 = 1.0 + alpha;
        double a1 = -2.0 * cosW0;
        double a2 = 1.0 - alpha;

        b0 /= a0;
        b1 /= a0;
        b2 /= a0;
        a1 /= a0;
        a2 /= a0;

        double out = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;

        x2 = x1;
        x1 = x;
        y2 = y1;
        y1 = out;

        return out;
    }

private:
    double fs;
    double x1 = 0.0, x2 = 0.0, y1 = 0.0, y2 = 0.0;
};

} // namespace as1
