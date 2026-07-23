#pragma once

#include "RasPreset.h"
#include <juce_core/juce_core.h>

namespace as1
{

class AS1AudioProcessor;

// Scans the bundled RetroAS1/*.ras factory library, parses them with
// RasParser and pushes the result onto the plugin's APVTS parameters so the
// DSP (SynthVoice) and the GUI both pick the new patch up uniformly.
class PresetManager
{
public:
    explicit PresetManager(AS1AudioProcessor& ownerProcessor);

    void scanPresetFolder();

    int getNumPresets() const { return presetFiles.size(); }
    juce::String getPresetDisplayName(int index) const;
    int getCurrentIndex() const { return currentIndex; }

    void loadPresetByIndex(int index);

    // Favourites — persisted across sessions; favourited presets are shown at
    // the top of the browser list.
    bool isFavorite(int index) const;
    void toggleFavorite(int index);

private:
    void applyPreset(const RasPreset& preset);
    juce::File favoritesFile() const;
    void loadFavorites();
    void saveFavorites();

    AS1AudioProcessor& processor;
    juce::Array<juce::File> presetFiles;
    int currentIndex = -1;
    juce::StringArray favoriteKeys; // preset file names marked as favourite
};

} // namespace as1
