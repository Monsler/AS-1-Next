#pragma once

#include "PluginProcessor.h"
#include "gui/AS1LookAndFeel.h"
#include "gui/BackgroundArt.h"
#include "gui/KnobView.h"
#include "gui/ModMatrixEditor.h"
#include "gui/PresetBrowser.h"
#include "gui/SectionPanel.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>

namespace as1
{

class AS1AudioProcessorEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit AS1AudioProcessorEditor(AS1AudioProcessor&);
    ~AS1AudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    // Hold-G preset audition: G down plays a C4 with the current parameter
    // state through the normal synth path, G up releases it.
    bool keyPressed(const juce::KeyPress&) override;
    bool keyStateChanged(bool isKeyDown) override;
    void focusLost(FocusChangeType) override;

private:
    void stopAuditionNote();
    enum class Tab { Main, Modulation, Effects, Global };

    void timerCallback() override;
    void switchTab(Tab tab);

    KnobView& addKnob(SectionPanel& panel, const char* paramID, const juce::String& caption,
                      bool accent = false, float width = 64.0f);
    juce::ComboBox& addWaveformCombo(SectionPanel& panel, const char* paramID);

    AS1AudioProcessor& processorRef;
    AS1LookAndFeel lookAndFeel;

    BackgroundArt backgroundArt;
    PresetBrowser presetBrowser;

    // Flat on-screen piano along the bottom for auditioning sounds. Bound to the
    // processor's keyboardState, so clicks feed the synth exactly like the
    // hold-G audition and any external MIDI.
    juce::MidiKeyboardComponent keyboard;

    // logoImageSource is rescaled to its on-screen size with high-quality
    // resampling in resized() and handed to logoComponent — ImageComponent's
    // own drawImage() otherwise scales at a lower default quality, which
    // looks noticeably soft/aliased for a large up- or down-scale.
    juce::Image logoImageSource;
    juce::ImageComponent logoComponent;

    Tab currentTab = Tab::Main;
    juce::TextButton tabMainBtn, tabModBtn, tabFxBtn, tabGlobalBtn;
    // "|" between Modulation and Effects, echoing FLEX's "Edit | Library".
    juce::Label tabSeparator { "tabSep", "|" };
    ModMatrixEditor modMatrixEditor;

    juce::ToggleButton oscAToggle, oscBToggle, chorusToggle, delayToggle, reverbToggle;

    SectionPanel oscASection { "OSC A", &oscAToggle };
    SectionPanel oscBSection { "OSC B", &oscBToggle };
    SectionPanel filterEnvSection { "FILTER ENVELOPE" };
    SectionPanel filterSection { "FILTER" };
    SectionPanel ampEnvSection { "AMP ENVELOPE" };
    SectionPanel chorusSection { "CHORUS", &chorusToggle };
    SectionPanel delaySection { "DELAY", &delayToggle };
    SectionPanel reverbSection { "REVERB", &reverbToggle };
    SectionPanel masterSection { "MASTER" };

    // FLEX-style macro strip on the bottom photo band (between the content
    // panel and the piano): always visible, whatever tab is active.
    std::vector<KnobView*> macroKnobs;

    juce::OwnedArray<KnobView> knobViews;
    juce::OwnedArray<juce::AudioProcessorValueTreeState::SliderAttachment> sliderAttachments;
    juce::OwnedArray<juce::AudioProcessorValueTreeState::ButtonAttachment> buttonAttachments;
    juce::OwnedArray<juce::AudioProcessorValueTreeState::ComboBoxAttachment> comboAttachments;

    juce::ComboBox oscAWaveformBox, oscBWaveformBox, voiceModeBox, reverbTypeBox;

    int lastShownPresetIndex = -2;

    bool auditionNoteActive = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AS1AudioProcessorEditor)
};

} // namespace as1
