#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

namespace as1
{

// A single blanket "sound" — the plugin only has one instrument, so every
// voice is allowed to play every note/channel.
class SynthSound : public juce::SynthesiserSound
{
public:
    bool appliesToNote(int) override { return true; }
    bool appliesToChannel(int) override { return true; }
};

} // namespace as1
