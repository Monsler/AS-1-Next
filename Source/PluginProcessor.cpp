#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "dsp/SynthVoice.h"
#include "dsp/SynthSound.h"

namespace as1
{

AS1AudioProcessor::AS1AudioProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMETERS", createParameterLayout()),
      presetManager(*this)
{
    for (int i = 0; i < numVoices; ++i)
        synth.addVoice(new SynthVoice(*this));
    synth.addSound(new SynthSound());

    presetManager.scanPresetFolder();
}

AS1AudioProcessor::~AS1AudioProcessor() = default;

void AS1AudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    synth.setCurrentPlaybackSampleRate(sampleRate);
    for (auto& d : delayEffects)
        d.prepare(sampleRate);

    chorus.prepare({ sampleRate, static_cast<juce::uint32>(samplesPerBlock), 2 });
    chorus.reset();
    reverb.setSampleRate(sampleRate);
    reverb.reset();
}

void AS1AudioProcessor::releaseResources()
{
}

bool AS1AudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void AS1AudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    keyboardState.processNextMidiBuffer(midiMessages, 0, buffer.getNumSamples(), true);

    // Monophonic mode: enforce last-note priority by turning off the previously
    // held note just before each new note-on, so only one note ever sounds.
    if (apvts.getRawParameterValue(ParamIDs::voiceMode)->load() > 0.5f)
    {
        juce::MidiBuffer mono;
        for (const auto meta : midiMessages)
        {
            auto m = meta.getMessage();
            if (m.isNoteOn())
            {
                if (monoLastNote >= 0 && monoLastNote != m.getNoteNumber())
                    mono.addEvent(juce::MidiMessage::noteOff(m.getChannel(), monoLastNote), meta.samplePosition);
                monoLastNote = m.getNoteNumber();
                mono.addEvent(m, meta.samplePosition);
            }
            else if (m.isNoteOff())
            {
                mono.addEvent(m, meta.samplePosition);
                if (m.getNoteNumber() == monoLastNote)
                    monoLastNote = -1;
            }
            else
            {
                mono.addEvent(m, meta.samplePosition);
            }
        }
        midiMessages.swapWith(mono);
    }
    else
    {
        monoLastNote = -1;
    }

    synth.renderNextBlock(buffer, midiMessages, 0, buffer.getNumSamples());

    auto raw = [this](const char* id) { return apvts.getRawParameterValue(id)->load(); };

    bool delayOn = raw(ParamIDs::delayOn) > 0.5f;
    double delayTimeMs = raw(ParamIDs::delayTimeMs);
    double delayFeedback = raw(ParamIDs::delayFeedback);
    double delayMix = raw(ParamIDs::delayMix);
    float masterGain = raw(ParamIDs::masterGain);

    int numChannels = buffer.getNumChannels();
    int numSamples = buffer.getNumSamples();

    // Chorus insert (juce::dsp) ahead of the ported delay, mirroring the
    // original's insert-then-global effect order.
    if (raw(ParamIDs::chorusOn) > 0.5f)
    {
        chorus.setCentreDelay(raw(ParamIDs::chorusDelayMs));
        chorus.setFeedback(raw(ParamIDs::chorusFeedback) * 0.01f);
        chorus.setRate(raw(ParamIDs::chorusRate));
        chorus.setDepth(raw(ParamIDs::chorusDepth) * 0.01f);
        chorus.setMix(raw(ParamIDs::chorusMix) * 0.01f);

        juce::dsp::AudioBlock<float> block(buffer);
        chorus.process(juce::dsp::ProcessContextReplacing<float>(block));
    }

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto& delay = delayEffects[static_cast<size_t>(ch % static_cast<int>(delayEffects.size()))];
        auto* data = buffer.getWritePointer(ch);
        for (int i = 0; i < numSamples; ++i)
            data[i] = delay.process(data[i], delayOn, delayTimeMs, delayFeedback, delayMix);
    }

    // Reverb "global effect": the room-type selector supplies the base
    // size/width (Room -> Large Hall), Decay stretches the size around that
    // base and Brightness is inverse damping — a modern take on the original's
    // Room/Chamber/Small Hall/Large Hall page.
    if (raw(ParamIDs::reverbOn) > 0.5f)
    {
        static constexpr float typeSize[]  = { 0.35f, 0.50f, 0.65f, 0.85f };
        static constexpr float typeWidth[] = { 0.60f, 0.80f, 1.00f, 1.00f };
        int type = juce::jlimit(0, 3, static_cast<int>(raw(ParamIDs::reverbType)));
        float mix = raw(ParamIDs::reverbMix) * 0.01f;

        juce::Reverb::Parameters rp;
        rp.roomSize = juce::jlimit(0.0f, 1.0f, typeSize[type] + (raw(ParamIDs::reverbDecay) * 0.01f - 0.5f) * 0.3f);
        rp.damping = 1.0f - raw(ParamIDs::reverbBrightness) * 0.01f;
        rp.width = typeWidth[type];
        rp.wetLevel = mix;
        rp.dryLevel = 1.0f - 0.4f * mix;
        reverb.setParameters(rp);

        if (numChannels >= 2)
            reverb.processStereo(buffer.getWritePointer(0), buffer.getWritePointer(1), numSamples);
        else if (numChannels == 1)
            reverb.processMono(buffer.getWritePointer(0), numSamples);
    }

    buffer.applyGain(masterGain);
}

juce::AudioProcessorEditor* AS1AudioProcessor::createEditor()
{
    return new AS1AudioProcessorEditor(*this);
}

void AS1AudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void AS1AudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml != nullptr && xml->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

} // namespace as1

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new as1::AS1AudioProcessor();
}
