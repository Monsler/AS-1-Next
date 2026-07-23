#include "PresetManager.h"
#include "RasParser.h"
#include "../PluginProcessor.h"
#include "../dsp/Parameters.h"

#include <algorithm>

namespace as1
{

PresetManager::PresetManager(AS1AudioProcessor& ownerProcessor) : processor(ownerProcessor)
{
}

void PresetManager::scanPresetFolder()
{
    presetFiles.clear();

    // AS1_PRESET_DIR is a compile-time path that may contain non-ASCII bytes
    // (e.g. a Cyrillic directory name) — juce::String's `const char*` ctor
    // assumes ASCII and asserts on those, so decode it as UTF-8 explicitly.
    juce::File dir(juce::String(juce::CharPointer_UTF8(AS1_PRESET_DIR)));
    if (!dir.isDirectory())
        return;

    for (const auto& entry : juce::RangedDirectoryIterator(dir, false, "*.ras", juce::File::findFiles))
        presetFiles.add(entry.getFile());

    presetFiles.sort();
    loadFavorites();
}

juce::File PresetManager::favoritesFile() const
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
             .getChildFile("AS1Next").getChildFile("favorites.txt");
}

void PresetManager::loadFavorites()
{
    favoriteKeys.clear();
    auto f = favoritesFile();
    if (f.existsAsFile())
        favoriteKeys.addLines(f.loadFileAsString());
    favoriteKeys.removeEmptyStrings();
}

void PresetManager::saveFavorites()
{
    auto f = favoritesFile();
    f.getParentDirectory().createDirectory();
    f.replaceWithText(favoriteKeys.joinIntoString("\n"));
}

bool PresetManager::isFavorite(int index) const
{
    if (index < 0 || index >= presetFiles.size())
        return false;
    return favoriteKeys.contains(presetFiles[index].getFileName());
}

void PresetManager::toggleFavorite(int index)
{
    if (index < 0 || index >= presetFiles.size())
        return;
    auto key = presetFiles[index].getFileName();
    if (favoriteKeys.contains(key))
        favoriteKeys.removeString(key);
    else
        favoriteKeys.add(key);
    saveFavorites();
}

juce::String PresetManager::getPresetDisplayName(int index) const
{
    if (index < 0 || index >= presetFiles.size())
        return {};

    auto name = presetFiles[index].getFileNameWithoutExtension();
    // Strip a leading "NNN " numeric prefix, mirroring parse_ras.py's naming.
    if (name.length() > 4 && name.substring(0, 3).containsOnly("0123456789") && name[3] == ' ')
        name = name.substring(4);
    return name;
}

static void setNormalized(juce::AudioProcessorValueTreeState& apvts, const char* id, float rawValue)
{
    auto* p = apvts.getParameter(id);
    jassert(p != nullptr);
    p->setValueNotifyingHost(p->convertTo0to1(rawValue));
}

void PresetManager::applyPreset(const RasPreset& preset)
{
    using namespace ParamIDs;
    auto& apvts = processor.apvts;

    setNormalized(apvts, ampAttack, static_cast<float>(preset.ampEnvelope.attack));
    setNormalized(apvts, ampDecay, static_cast<float>(preset.ampEnvelope.decay));
    setNormalized(apvts, ampSustain, static_cast<float>(std::clamp(preset.ampEnvelope.sustain, 0.0, 1.0)));
    setNormalized(apvts, ampSustainDecay, static_cast<float>(preset.ampEnvelope.sustainDecay));
    setNormalized(apvts, ampRelease, static_cast<float>(preset.ampEnvelope.release));

    setNormalized(apvts, filtAttack, static_cast<float>(preset.filterEnvelope.attack));
    setNormalized(apvts, filtDecay, static_cast<float>(preset.filterEnvelope.decay));
    setNormalized(apvts, filtSustain, static_cast<float>(std::clamp(preset.filterEnvelope.sustain, 0.0, 1.0)));
    setNormalized(apvts, filtSustainDecay, static_cast<float>(preset.filterEnvelope.sustainDecay));
    setNormalized(apvts, filtRelease, static_cast<float>(preset.filterEnvelope.release));
    setNormalized(apvts, filtEnvAmount, static_cast<float>(std::clamp(preset.filterEnvAmount, -2.0, 2.0)));

    setNormalized(apvts, pitchAttack, static_cast<float>(preset.pitchEnvelope.attack));
    setNormalized(apvts, pitchDecay, static_cast<float>(preset.pitchEnvelope.decay));
    setNormalized(apvts, pitchSustain, static_cast<float>(std::clamp(preset.pitchEnvelope.sustain, 0.0, 1.0)));
    setNormalized(apvts, pitchSustainDecay, static_cast<float>(preset.pitchEnvelope.sustainDecay));
    setNormalized(apvts, pitchRelease, static_cast<float>(preset.pitchEnvelope.release));
    setNormalized(apvts, pitchEnvAmount, static_cast<float>(std::clamp(preset.pitchEnvAmount, -1.0, 1.0)));

    if (!preset.filters.empty())
    {
        setNormalized(apvts, filterCutoff, static_cast<float>(std::clamp(preset.filters[0].cutoff, 0.0, 100.0)));
        setNormalized(apvts, filterResonance, static_cast<float>(std::clamp(preset.filters[0].resonance, 0.0, 100.0)));
        int typeIdx = std::clamp(static_cast<int>(preset.filters[0].type), 0, filterTypeChoices.size() - 1);
        setNormalized(apvts, filterType, static_cast<float>(typeIdx));
    }
    else
    {
        setNormalized(apvts, filterCutoff, 100.0f);
        setNormalized(apvts, filterResonance, 0.0f);
        setNormalized(apvts, filterType, 1.0f); // LP 4-pole default
    }

    auto applyOsc = [&apvts](const char* enId, const char* wfId, const char* coarseId,
                              const char* fineId, const char* symId, const char* volId, const OscillatorConfig* osc)
    {
        if (osc == nullptr)
        {
            setNormalized(apvts, enId, 0.0f);
            return;
        }
        setNormalized(apvts, enId, osc->enabled ? 1.0f : 0.0f);
        setNormalized(apvts, wfId, static_cast<float>(std::min(4, static_cast<int>(osc->waveform))));
        setNormalized(apvts, coarseId, static_cast<float>(std::clamp(osc->coarseTune, -24, 24)));
        setNormalized(apvts, fineId, static_cast<float>(std::clamp(osc->fineTune, -100.0, 100.0)));
        setNormalized(apvts, symId, static_cast<float>(std::clamp(osc->symmetry, 0.0, 100.0)));
        setNormalized(apvts, volId, static_cast<float>(std::clamp(osc->volume, 0.0, 1.0)));
    };

    applyOsc(oscAEnabled, oscAWaveform, oscACoarse, oscAFine, oscASymmetry, oscAVolume,
             preset.oscillators.size() >= 1 ? &preset.oscillators[0] : nullptr);
    applyOsc(oscBEnabled, oscBWaveform, oscBCoarse, oscBFine, oscBSymmetry, oscBVolume,
             preset.oscillators.size() >= 2 ? &preset.oscillators[1] : nullptr);

    setNormalized(apvts, delayOn, preset.effects.delayEnabled ? 1.0f : 0.0f);
    if (preset.effects.delayEnabled)
    {
        setNormalized(apvts, delayTimeMs, static_cast<float>(std::clamp(preset.effects.delayTimeMs, 1.0, 1500.0)));
        setNormalized(apvts, delayFeedback, static_cast<float>(std::clamp(preset.effects.delayFeedback, 0.0, 100.0)));
        setNormalized(apvts, delayMix, 50.0f);
    }

    // .ras carries a reverb-enable + mix; the room/brightness/decay knobs keep
    // their current (or default) values. Chorus has no .ras field — switch it
    // off so a leftover insert doesn't colour the freshly loaded program.
    setNormalized(apvts, reverbOn, preset.effects.reverbEnabled ? 1.0f : 0.0f);
    if (preset.effects.reverbEnabled)
        setNormalized(apvts, reverbMix, static_cast<float>(std::clamp(preset.effects.reverbMix, 0.0, 100.0)));
    setNormalized(apvts, chorusOn, 0.0f);

    // Publish the full preset snapshot (oscillators + filter + general mod
    // matrix) for the voices to pick up on the next note-on. This is the
    // accurate playback path; the APVTS writes above keep the GUI in sync.
    processor.setPresetSnapshot(std::make_shared<const RasPreset>(preset));
}

void PresetManager::loadPresetByIndex(int index)
{
    if (index < 0 || index >= presetFiles.size())
        return;

    RasPreset preset = RasParser::parseFile(presetFiles[index].getFullPathName().toStdString());
    if (!preset.valid)
        return;

    currentIndex = index;
    applyPreset(preset);
}

} // namespace as1
