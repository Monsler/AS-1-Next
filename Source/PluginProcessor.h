#pragma once

#include "dsp/DelayEffect.h"
#include "dsp/ModMatrix.h"
#include "dsp/Parameters.h"
#include "presets/PresetManager.h"
#include "presets/RasPreset.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include <memory>

namespace as1
{

// libc++ (macOS) still lacks std::atomic<std::shared_ptr>; fall back to a
// spinlock-guarded pointer there. The audio thread only takes the lock for a
// pointer copy at note-on, so contention is negligible.
#if defined(__cpp_lib_atomic_shared_ptr)
using AtomicPresetPtr = std::atomic<std::shared_ptr<const RasPreset>>;
#else
class AtomicPresetPtr
{
public:
    std::shared_ptr<const RasPreset> load() const
    {
        const juce::SpinLock::ScopedLockType sl(lock);
        return ptr;
    }
    void store(std::shared_ptr<const RasPreset> newPtr)
    {
        const juce::SpinLock::ScopedLockType sl(lock);
        ptr = std::move(newPtr);
    }
private:
    mutable juce::SpinLock lock;
    std::shared_ptr<const RasPreset> ptr;
};
#endif

class AS1AudioProcessor : public juce::AudioProcessor
{
public:
    AS1AudioProcessor();
    ~AS1AudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    using AudioProcessor::processBlock;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    PresetManager& getPresetManager() { return presetManager; }

    juce::AudioProcessorValueTreeState apvts;

    // GUI-triggered notes (the editor's hold-G preset audition) are injected
    // into the MIDI stream through here at the top of processBlock.
    juce::MidiKeyboardState keyboardState;

    // The current preset in full (oscillators, filter, and the general
    // modulation matrix) — the parts the flat APVTS set can't hold. PresetManager
    // publishes a new snapshot on preset load; each SynthVoice grabs the current
    // one at note-on and renders from it. Swapped as a whole via an atomic
    // shared_ptr so the audio thread always sees a consistent, complete preset.
    std::shared_ptr<const RasPreset> getPresetSnapshot() const { return currentPreset.load(); }
    void setPresetSnapshot(std::shared_ptr<const RasPreset> p) { currentPreset.store(std::move(p)); }

private:
    static constexpr int numVoices = 16;

    juce::Synthesiser synth;
    PresetManager presetManager;

    // Effects chain after the synth: chorus (insert) -> the ported DelayEffect
    // pair -> reverb -> master gain. Chorus/reverb are new additions on top of
    // the Python-port core, so they use stock JUCE DSP rather than ported code.
    std::array<DelayEffect, 2> delayEffects;
    juce::dsp::Chorus<float> chorus;
    juce::Reverb reverb;

    // Last note held in mono mode (-1 = none), for last-note-priority stealing.
    int monoLastNote = -1;

    AtomicPresetPtr currentPreset {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AS1AudioProcessor)
};

} // namespace as1
