#pragma once

namespace as1
{

// Literal C++ port of the ADSR class in metro-as1/play_ras.py.
// Linear-segment envelope driven one sample ("frame") at a time.
class Adsr
{
public:
    enum class State { Idle, Attack, Decay, Sustain, Release };

    void configure(double attackSeconds, double decaySeconds, double sustainLevel,
                    double releaseSeconds, double sampleRate, double sustainDecaySeconds = 0.0)
    {
        attackFrames = static_cast<int>(attackSeconds * sampleRate);
        decayFrames = static_cast<int>(decaySeconds * sampleRate);
        sustain = sustainLevel;
        releaseFrames = static_cast<int>(releaseSeconds * sampleRate);
        // 0 (or non-positive) means "no sustain decay" — the level holds at the
        // sustain value for as long as the note is held.
        sustainDecayFrames = sustainDecaySeconds > 0.0 ? static_cast<int>(sustainDecaySeconds * sampleRate) : 0;
    }

    void triggerOn()
    {
        state = State::Attack;
        frameCount = 0;
    }

    void triggerOff()
    {
        state = State::Release;
        frameCount = 0;
        releaseStartVal = currentValue;
    }

    double tick()
    {
        switch (state)
        {
            case State::Idle:
                return 0.0;

            case State::Attack:
                if (attackFrames <= 0)
                {
                    currentValue = 1.0;
                    state = State::Decay;
                    frameCount = 0;
                }
                else
                {
                    currentValue = static_cast<double>(frameCount) / static_cast<double>(attackFrames);
                    ++frameCount;
                    if (frameCount >= attackFrames)
                    {
                        state = State::Decay;
                        frameCount = 0;
                    }
                }
                break;

            case State::Decay:
                if (decayFrames <= 0)
                {
                    currentValue = sustain;
                    state = State::Sustain;
                    frameCount = 0;
                }
                else
                {
                    double factor = static_cast<double>(frameCount) / static_cast<double>(decayFrames);
                    currentValue = 1.0 - (1.0 - sustain) * factor;
                    ++frameCount;
                    if (frameCount >= decayFrames)
                    {
                        state = State::Sustain;
                        frameCount = 0;
                    }
                }
                break;

            case State::Sustain:
                // With a sustain-decay stage, the held level ramps from the
                // sustain value down toward 0 (Andromeda-style DADSR); without
                // it the level simply holds.
                if (sustainDecayFrames > 0)
                {
                    if (frameCount < sustainDecayFrames)
                    {
                        double factor = static_cast<double>(frameCount) / static_cast<double>(sustainDecayFrames);
                        currentValue = sustain * (1.0 - factor);
                        ++frameCount;
                    }
                    else
                    {
                        currentValue = 0.0;
                    }
                }
                else
                {
                    currentValue = sustain;
                }
                break;

            case State::Release:
                if (releaseFrames <= 0)
                {
                    currentValue = 0.0;
                    state = State::Idle;
                }
                else
                {
                    double factor = static_cast<double>(frameCount) / static_cast<double>(releaseFrames);
                    currentValue = releaseStartVal * (1.0 - factor);
                    ++frameCount;
                    if (frameCount >= releaseFrames)
                    {
                        currentValue = 0.0;
                        state = State::Idle;
                    }
                }
                break;
        }
        return currentValue;
    }

    State getState() const { return state; }
    double getCurrentValue() const { return currentValue; }

private:
    int attackFrames = 0;
    int decayFrames = 0;
    double sustain = 0.0;
    int sustainDecayFrames = 0;
    int releaseFrames = 0;

    State state = State::Idle;
    double currentValue = 0.0;
    int frameCount = 0;
    double releaseStartVal = 0.0;
};

} // namespace as1
