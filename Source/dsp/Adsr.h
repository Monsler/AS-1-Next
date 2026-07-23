#pragma once

#include <cmath>

namespace as1
{

// Exponential-segment envelope driven one sample ("frame") at a time.
//
// Originally a literal port of the linear ADSR in metro-as1/play_ras.py, but the
// real hardware (Andromeda-derived `CAnalogEnvelope`) — like every analog
// envelope — is EXPONENTIAL: the decay/release fall fast then trail off, and the
// attack rises fast then eases in. That curve is what gives percussion its
// characteristic click-thump: a linear pitch drop reads as a slide-whistle glide,
// a linear amp/filter decay has no punch. The specified segment *time* (from the
// hardware's RampSpeedToSeconds table, see RasParser) is preserved exactly; only
// the shape within each segment changed from a straight ramp to an RC-style curve.
//
// Each segment uses a normalised curve that hits its endpoints exactly at the
// segment boundary so the frame-based state machine (and the voice-free logic that
// depends on it) stays deterministic.
class Adsr
{
public:
    enum class State { Idle, Attack, Decay, Sustain, Release };

    // Curve steepness. Higher = more exponential (punchier). Decay/release want a
    // strong curve; attack is gentler so short attacks still read as a clean onset.
    static constexpr double kDecayShape = 5.0;
    static constexpr double kAttackShape = 3.0;

    // Normalised falling curve: f(0)=1, f(1)=0, convex (fast then slow).
    static double fallCurve(double frac)
    {
        const double em = std::exp(-kDecayShape);
        return (std::exp(-kDecayShape * frac) - em) / (1.0 - em);
    }

    // Normalised rising curve: g(0)=0, g(1)=1, concave (fast then slow).
    static double riseCurve(double frac)
    {
        const double em = std::exp(-kAttackShape);
        return (1.0 - std::exp(-kAttackShape * frac)) / (1.0 - em);
    }

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
                    double factor = static_cast<double>(frameCount) / static_cast<double>(attackFrames);
                    currentValue = riseCurve(factor);
                    ++frameCount;
                    if (frameCount >= attackFrames)
                    {
                        currentValue = 1.0;
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
                    currentValue = sustain + (1.0 - sustain) * fallCurve(factor);
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
                        currentValue = sustain * fallCurve(factor);
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
                    currentValue = releaseStartVal * fallCurve(factor);
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
