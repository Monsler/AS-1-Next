#include "PluginEditor.h"
#include "BinaryData.h"
#include "dsp/Parameters.h"

namespace as1
{

namespace
{
    constexpr int sidebarWidth = 260;
    constexpr int topBarHeight = 64;
    // Height of the photo strip left uncovered below the content panel,
    // mirroring FLEX's bottom macro-knob band.
    constexpr int bottomBandHeight = 120;
    // On-screen piano along the very bottom, full width.
    constexpr int keyboardHeight = 78;
    constexpr int auditionMidiNote = 60; // C4
}

AS1AudioProcessorEditor::AS1AudioProcessorEditor(AS1AudioProcessor& p)
    : juce::AudioProcessorEditor(&p), processorRef(p), presetBrowser(p),
      keyboard(p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard),
      modMatrixEditor(p)
{
    setLookAndFeel(&lookAndFeel);

    addAndMakeVisible(backgroundArt);
    addAndMakeVisible(presetBrowser);

    // Flat, Serum-like piano: solid keys, thin separators, no 3D shadow.
    using MK = juce::MidiKeyboardComponent;
    keyboard.setColour(MK::whiteNoteColourId, juce::Colour(0xffd8d4e2));
    keyboard.setColour(MK::blackNoteColourId, juce::Colour(0xff26232e));
    keyboard.setColour(MK::keySeparatorLineColourId, AS1LookAndFeel::palette.bgBottom);
    keyboard.setColour(MK::shadowColourId, juce::Colours::transparentBlack);
    keyboard.setColour(MK::mouseOverKeyOverlayColourId, AS1LookAndFeel::palette.highlight.withAlpha(0.35f));
    keyboard.setColour(MK::keyDownOverlayColourId, AS1LookAndFeel::palette.highlight.withAlpha(0.75f));
    keyboard.setColour(MK::upDownButtonBackgroundColourId, AS1LookAndFeel::palette.panel);
    keyboard.setColour(MK::upDownButtonArrowColourId, AS1LookAndFeel::palette.textDim);
    keyboard.setKeyWidth(22.0f);
    keyboard.setLowestVisibleKey(36); // C2
    keyboard.setScrollButtonsVisible(true);
    addAndMakeVisible(keyboard);

    // Added after backgroundArt so they paint on top of its full-height blur,
    // matching FLEX's top bar (logo + tabs + search, no background of its own).
    logoImageSource = juce::ImageFileFormat::loadFrom(BinaryData::logo_trimmed_png,
                                                        static_cast<size_t>(BinaryData::logo_trimmed_pngSize));
    logoComponent.setImagePlacement(juce::RectanglePlacement::xLeft | juce::RectanglePlacement::yMid
                                     | juce::RectanglePlacement::doNotResize);
    logoComponent.setAlpha(0.6f);
    addAndMakeVisible(logoComponent);

    addAndMakeVisible(presetBrowser.getSearchBox());

    auto setupTabButton = [this](juce::TextButton& btn, const juce::String& text, Tab tab)
    {
        btn.setButtonText(text);
        btn.setClickingTogglesState(false);
        btn.onClick = [this, tab] { switchTab(tab); };
        addAndMakeVisible(btn);
    };
    setupTabButton(tabMainBtn, "Main", Tab::Main);
    setupTabButton(tabModBtn, "Modulation", Tab::Modulation);
    setupTabButton(tabFxBtn, "Effects", Tab::Effects);
    setupTabButton(tabGlobalBtn, "Global", Tab::Global);

    tabSeparator.setJustificationType(juce::Justification::centred);
    // Label's default 5px side padding leaves "|" no room in its narrow slot,
    // so JUCE ellipsises it to "..." — kill the padding.
    tabSeparator.setBorderSize(juce::BorderSize<int>(0));
    tabSeparator.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.85f));
    tabSeparator.setFont(juce::Font(juce::FontOptions(19.0f)));
    addAndMakeVisible(tabSeparator);

    addAndMakeVisible(modMatrixEditor);

    addAndMakeVisible(oscASection);
    addAndMakeVisible(oscBSection);
    addAndMakeVisible(filterEnvSection);
    addAndMakeVisible(filterSection);
    addAndMakeVisible(ampEnvSection);
    addAndMakeVisible(chorusSection);
    addAndMakeVisible(delaySection);
    addAndMakeVisible(reverbSection);
    addAndMakeVisible(masterSection);

    auto& apvts = processorRef.apvts;

    addWaveformCombo(oscASection, ParamIDs::oscAWaveform);
    addKnob(oscASection, ParamIDs::oscACoarse, "Coarse");
    addKnob(oscASection, ParamIDs::oscAFine, "Fine");
    addKnob(oscASection, ParamIDs::oscASymmetry, "Sym");
    buttonAttachments.add(new juce::AudioProcessorValueTreeState::ButtonAttachment(apvts, ParamIDs::oscAEnabled, oscAToggle));

    addWaveformCombo(oscBSection, ParamIDs::oscBWaveform);
    addKnob(oscBSection, ParamIDs::oscBCoarse, "Coarse");
    addKnob(oscBSection, ParamIDs::oscBFine, "Fine");
    addKnob(oscBSection, ParamIDs::oscBSymmetry, "Sym");
    buttonAttachments.add(new juce::AudioProcessorValueTreeState::ButtonAttachment(apvts, ParamIDs::oscBEnabled, oscBToggle));

    addKnob(filterEnvSection, ParamIDs::filtAttack, "A");
    addKnob(filterEnvSection, ParamIDs::filtDecay, "D");
    addKnob(filterEnvSection, ParamIDs::filtSustain, "S");
    addKnob(filterEnvSection, ParamIDs::filtRelease, "R");
    addKnob(filterEnvSection, ParamIDs::filtEnvAmount, "Amt");

    addKnob(filterSection, ParamIDs::filterCutoff, "Cutoff", true);
    addKnob(filterSection, ParamIDs::filterResonance, "Res", true);

    addKnob(ampEnvSection, ParamIDs::ampAttack, "A");
    addKnob(ampEnvSection, ParamIDs::ampDecay, "D");
    addKnob(ampEnvSection, ParamIDs::ampSustain, "S");
    addKnob(ampEnvSection, ParamIDs::ampRelease, "R");

    // Effects tab — knob rows mirror the original's Chorus / Delay / Reverb
    // fader groups (see the Bitheadz "Effects" page screenshot).
    addKnob(chorusSection, ParamIDs::chorusDelayMs, "Delay");
    addKnob(chorusSection, ParamIDs::chorusFeedback, "Fdbk");
    addKnob(chorusSection, ParamIDs::chorusRate, "Speed");
    addKnob(chorusSection, ParamIDs::chorusDepth, "Depth");
    addKnob(chorusSection, ParamIDs::chorusMix, "Mix", true);
    buttonAttachments.add(new juce::AudioProcessorValueTreeState::ButtonAttachment(apvts, ParamIDs::chorusOn, chorusToggle));

    addKnob(delaySection, ParamIDs::delayTimeMs, "Time");
    addKnob(delaySection, ParamIDs::delayFeedback, "Fdbk");
    addKnob(delaySection, ParamIDs::delayMix, "Mix", true);
    buttonAttachments.add(new juce::AudioProcessorValueTreeState::ButtonAttachment(apvts, ParamIDs::delayOn, delayToggle));

    reverbTypeBox.addItemList(reverbTypeChoices, 1);
    reverbSection.addWideItem(reverbTypeBox, 116.0f);
    comboAttachments.add(new juce::AudioProcessorValueTreeState::ComboBoxAttachment(apvts, ParamIDs::reverbType, reverbTypeBox));
    addKnob(reverbSection, ParamIDs::reverbBrightness, "Bright");
    addKnob(reverbSection, ParamIDs::reverbDecay, "Decay");
    addKnob(reverbSection, ParamIDs::reverbMix, "Mix", true);
    buttonAttachments.add(new juce::AudioProcessorValueTreeState::ButtonAttachment(apvts, ParamIDs::reverbOn, reverbToggle));

    addKnob(masterSection, ParamIDs::masterGain, "Gain", true);

    // Voice mode (Poly / Mono) selector in the Global section.
    voiceModeBox.addItemList(voiceModeChoices, 1);
    masterSection.addWideItem(voiceModeBox, 110.0f);
    comboAttachments.add(new juce::AudioProcessorValueTreeState::ComboBoxAttachment(apvts, ParamIDs::voiceMode, voiceModeBox));

    // Macro strip on the bottom photo band, echoing FLEX's macro-knob row.
    // Vibrato / Vib-rate are live per-voice modulation; the rest are quick
    // handles on existing parameters.
    {
        struct Macro { const char* id; const char* caption; };
        for (const auto& m : std::initializer_list<Macro> {
                 { ParamIDs::vibratoDepth, "Vibrato" }, { ParamIDs::vibratoRate, "Vib-rate" },
                 { ParamIDs::filterCutoff, "Cutoff" }, { ParamIDs::filterResonance, "Res" },
                 { ParamIDs::ampAttack, "Attack" }, { ParamIDs::ampRelease, "Release" },
                 { ParamIDs::reverbMix, "Reverb" }, { ParamIDs::masterGain, "Gain" } })
        {
            auto* knob = new KnobView(m.caption);
            knobViews.add(knob);
            macroKnobs.push_back(knob);
            addAndMakeVisible(knob);
            sliderAttachments.add(new juce::AudioProcessorValueTreeState::SliderAttachment(apvts, m.id, knob->slider));
        }
    }

    // FLEX-style thin dividers between neighbouring sections in a row.
    oscASection.showRightSeparator = true;
    filterEnvSection.showRightSeparator = true;
    filterSection.showRightSeparator = true;
    chorusSection.showRightSeparator = true;

    switchTab(Tab::Main);

    setWantsKeyboardFocus(true); // for the hold-G preset audition

    setSize(1180, 820);
    startTimerHz(8);
}

AS1AudioProcessorEditor::~AS1AudioProcessorEditor()
{
    stopAuditionNote();
    setLookAndFeel(nullptr);
}

bool AS1AudioProcessorEditor::keyPressed(const juce::KeyPress& key)
{
    // Consume G (including key-repeat) so it doesn't leak to the host; the
    // actual note-on/off is driven by keyStateChanged.
    auto c = key.getTextCharacter();
    return c == 'g' || c == 'G';
}

bool AS1AudioProcessorEditor::keyStateChanged(bool)
{
    bool gDown = juce::KeyPress::isKeyCurrentlyDown('g') || juce::KeyPress::isKeyCurrentlyDown('G');
    if (gDown == auditionNoteActive)
        return false;

    if (gDown)
    {
        auditionNoteActive = true;
        processorRef.keyboardState.noteOn(1, auditionMidiNote, 0.9f);
    }
    else
    {
        stopAuditionNote();
    }
    return true;
}

void AS1AudioProcessorEditor::focusLost(FocusChangeType)
{
    // If focus moves away while G is held, the key-up will never reach us —
    // release the note now rather than leaving it hanging.
    stopAuditionNote();
}

void AS1AudioProcessorEditor::stopAuditionNote()
{
    if (auditionNoteActive)
    {
        auditionNoteActive = false;
        processorRef.keyboardState.noteOff(1, auditionMidiNote, 0.0f);
    }
}

KnobView& AS1AudioProcessorEditor::addKnob(SectionPanel& panel, const char* paramID, const juce::String& caption,
                                           bool accent, float width)
{
    auto* knob = new KnobView(caption);
    knobViews.add(knob);
    knob->slider.getProperties().set("accent", accent);
    panel.addKnob(*knob, width);
    sliderAttachments.add(new juce::AudioProcessorValueTreeState::SliderAttachment(processorRef.apvts, paramID, knob->slider));
    return *knob;
}

juce::ComboBox& AS1AudioProcessorEditor::addWaveformCombo(SectionPanel& panel, const char* paramID)
{
    auto& box = (juce::String(paramID) == ParamIDs::oscAWaveform) ? oscAWaveformBox : oscBWaveformBox;
    box.addItemList(waveformChoices, 1);
    panel.addWideItem(box, 110.0f);
    comboAttachments.add(new juce::AudioProcessorValueTreeState::ComboBoxAttachment(processorRef.apvts, paramID, box));
    return box;
}

void AS1AudioProcessorEditor::switchTab(Tab tab)
{
    currentTab = tab;

    bool main = tab == Tab::Main;
    bool mod = tab == Tab::Modulation;
    bool fx = tab == Tab::Effects;
    bool global = tab == Tab::Global;

    oscASection.setVisible(main);
    oscBSection.setVisible(main);
    filterEnvSection.setVisible(main);
    filterSection.setVisible(main);
    ampEnvSection.setVisible(main);

    chorusSection.setVisible(fx);
    delaySection.setVisible(fx);
    reverbSection.setVisible(fx);
    masterSection.setVisible(global);
    modMatrixEditor.setVisible(mod);

    // Selected tab = solid white; unselected = white at 60% opacity.
    auto tint = [this](juce::TextButton& b, bool selected)
    {
        b.setColour(juce::TextButton::textColourOffId,
                    selected ? juce::Colours::white : juce::Colours::white.withAlpha(0.6f));
    };
    tint(tabMainBtn, main);
    tint(tabModBtn, mod);
    tint(tabFxBtn, fx);
    tint(tabGlobalBtn, global);

    repaint();
}

void AS1AudioProcessorEditor::timerCallback()
{
    int current = processorRef.getPresetManager().getCurrentIndex();
    if (current != lastShownPresetIndex)
    {
        lastShownPresetIndex = current;
        presetBrowser.refresh();
    }
    // Cheap when nothing changed (one atomic load + pointer compare); rebuilds
    // whenever a preset load replaced the snapshot.
    modMatrixEditor.refresh();
}

void AS1AudioProcessorEditor::paint(juce::Graphics& g)
{
    g.setColour(AS1LookAndFeel::palette.bgBottom);
    g.fillRect(getLocalBounds());

    // The top bar itself paints nothing — only backgroundArt's fading blur
    // (a child drawn on top of this) shows through it, per FLEX.png.
    g.setColour(AS1LookAndFeel::palette.panelBorder.withAlpha(0.35f));
    g.drawLine((float) sidebarWidth, 0, (float) sidebarWidth, (float) getHeight(), 1.0f);
}

void AS1AudioProcessorEditor::resized()
{
    // The piano sits only under the content column, so the preset sidebar keeps
    // its full height and isn't covered by the keyboard.
    int contentH = getHeight() - keyboardHeight;
    keyboard.setBounds(sidebarWidth, contentH, getWidth() - sidebarWidth, keyboardHeight);

    presetBrowser.setBounds(0, 0, sidebarWidth, getHeight());

    // backgroundArt spans the full content column height (including behind
    // the top bar) so its top-edge blur band is what the top bar shows.
    auto contentBounds = juce::Rectangle<int>(sidebarWidth, 0, getWidth() - sidebarWidth, contentH);
    backgroundArt.setBounds(contentBounds);

    // Translucent content panel between the top bar and the bottom photo
    // strip, per FLEX.png (coords are local to backgroundArt, which starts
    // at y = 0).
    backgroundArt.setPanelBand({ topBarHeight, contentH - bottomBandHeight });

    auto top = juce::Rectangle<int>(sidebarWidth, 0, getWidth() - sidebarWidth, topBarHeight);
    auto bar = top.reduced(20, 14);
    auto logoArea = bar.removeFromLeft(120);
    logoComponent.setBounds(logoArea);
    if (logoImageSource.isValid() && logoArea.getWidth() > 0 && logoArea.getHeight() > 0)
    {
        float scale = juce::jmin(logoArea.getWidth() / (float) logoImageSource.getWidth(),
                                  logoArea.getHeight() / (float) logoImageSource.getHeight());
        int lw = juce::jmax(1, juce::roundToInt(logoImageSource.getWidth() * scale));
        int lh = juce::jmax(1, juce::roundToInt(logoImageSource.getHeight() * scale));
        logoComponent.setImage(logoImageSource.rescaled(lw, lh, juce::Graphics::highResamplingQuality));
    }
    bar.removeFromLeft(20);

    presetBrowser.getSearchBox().setBounds(bar.removeFromRight(260));
    bar.removeFromRight(20);

    // Tabs are sized to the exact width of their text (so the glyphs aren't
    // squashed) and packed close together, FLEX-style, with a "|" separator
    // between Modulation and Effects.
    juce::Font tabFont = juce::Font(juce::FontOptions(19.0f)).boldened();
    auto textWidth = [&tabFont](const juce::String& t)
    {
        juce::GlyphArrangement ga;
        ga.addLineOfText(tabFont, t, 0.0f, 0.0f);
        return juce::roundToInt(ga.getBoundingBox(0, -1, true).getWidth());
    };
    constexpr int tabGap = 10, tabPad = 8;
    auto placeTab = [&](juce::Component& c, const juce::String& t)
    {
        c.setBounds(bar.removeFromLeft(textWidth(t) + tabPad));
        bar.removeFromLeft(tabGap);
    };
    placeTab(tabMainBtn, "Main");
    placeTab(tabModBtn, "Modulation");
    tabSeparator.setBounds(bar.removeFromLeft(textWidth("|") + 6));
    bar.removeFromLeft(tabGap);
    placeTab(tabFxBtn, "Effects");
    placeTab(tabGlobalBtn, "Global");

    // The opaque panel band between the top bar and the bottom photo strip —
    // all tab content is centred vertically within it.
    auto bandArea = juce::Rectangle<int>(sidebarWidth, topBarHeight,
                                          getWidth() - sidebarWidth,
                                          contentH - topBarHeight - bottomBandHeight);
    modMatrixEditor.setBounds(bandArea.reduced(0, 4));

    constexpr int row1Height = 120, rowGap = 14, row2Height = 150;
    auto main = bandArea.reduced(16, 0)
                    .withSizeKeepingCentre(bandArea.getWidth() - 32, row1Height + rowGap + row2Height);

    auto row1 = main.removeFromTop(row1Height);
    int halfW = row1.getWidth() / 2 - 6;
    oscASection.setBounds(row1.removeFromLeft(halfW));
    row1.removeFromLeft(12);
    oscBSection.setBounds(row1);

    main.removeFromTop(rowGap);
    auto row2 = main.removeFromTop(row2Height);
    int w3 = (row2.getWidth() - 24) / 3;
    filterEnvSection.setBounds(row2.removeFromLeft(w3));
    row2.removeFromLeft(12);
    filterSection.setBounds(row2.removeFromLeft(w3));
    row2.removeFromLeft(12);
    ampEnvSection.setBounds(row2);

    // Effects tab: Chorus | Delay on the first row, Reverb below — echoing the
    // original's insert/global effect groups.
    auto fx = bandArea.reduced(16, 0).withSizeKeepingCentre(bandArea.getWidth() - 32, 150 + rowGap + 150);
    auto fxRow1 = fx.removeFromTop(150);
    int chorusW = juce::jmin(fxRow1.getWidth() - 300, fxRow1.getWidth() * 3 / 5);
    chorusSection.setBounds(fxRow1.removeFromLeft(chorusW));
    fxRow1.removeFromLeft(12);
    delaySection.setBounds(fxRow1);
    fx.removeFromTop(rowGap);
    reverbSection.setBounds(fx.withSizeKeepingCentre(juce::jmin(560, fx.getWidth()), 150));

    // Global tab: master section (gain + voice mode) centred in the panel band.
    masterSection.setBounds(bandArea.withSizeKeepingCentre(300, 150));

    // Macro strip: a centred knob row on the photo band between the content
    // panel and the piano.
    auto macroArea = juce::Rectangle<int>(sidebarWidth, contentH - bottomBandHeight,
                                           getWidth() - sidebarWidth, bottomBandHeight);
    int knobW = 74, knobGap = 16;
    int count = static_cast<int>(macroKnobs.size());
    auto macroRow = macroArea.withSizeKeepingCentre(count * knobW + (count - 1) * knobGap, 96);
    for (auto* knob : macroKnobs)
    {
        knob->setBounds(macroRow.removeFromLeft(knobW));
        macroRow.removeFromLeft(knobGap);
    }
}

} // namespace as1
