#pragma once

#include <algorithm>
#include <vector>

namespace as1
{

// Streaming version of the delay effect from generate_audio() in
// metro-as1/play_ras.py. The Python version allocates a ring buffer sized
// exactly to the (fixed, offline-known) delay time; here the plugin needs to
// support the delay time changing live, so a fixed-capacity ring buffer is
// used instead and indexed by the current delay length in samples. The
// per-sample feedback/mix arithmetic is otherwise identical to the Python
// reference.
class DelayEffect
{
public:
    void prepare(double sampleRate)
    {
        fs = sampleRate;
        buffer.assign(static_cast<size_t>(fs * 2.0) + 1, 0.0f);
        writePos = 0;
    }

    void reset()
    {
        std::fill(buffer.begin(), buffer.end(), 0.0f);
        writePos = 0;
    }

    float process(float input, bool enabled, double delayTimeMs, double feedbackPct, double mixPct)
    {
        if (buffer.empty())
            return input;

        if (!enabled)
            return input;

        size_t delaySamples = static_cast<size_t>(std::clamp(delayTimeMs, 1.0, 1500.0) * fs / 1000.0);
        delaySamples = std::min(delaySamples, buffer.size() - 1);

        size_t readPos = (writePos + buffer.size() - delaySamples) % buffer.size();
        float delayedVal = buffer[readPos];
        float feedback = static_cast<float>(feedbackPct / 100.0);
        float mix = static_cast<float>(mixPct / 100.0);

        buffer[writePos] = input + delayedVal * feedback;
        writePos = (writePos + 1) % buffer.size();

        return input + delayedVal * mix;
    }

private:
    double fs = 44100.0;
    std::vector<float> buffer;
    size_t writePos = 0;
};

} // namespace as1
