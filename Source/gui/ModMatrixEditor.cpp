#include "ModMatrixEditor.h"
#include "AS1LookAndFeel.h"
#include "../PluginProcessor.h"

namespace as1
{

namespace
{
    // Fixed destination menu, covering everything the engine's general matrix
    // can drive (ModMatrix.h). Order = combo order.
    struct DestItem { const char* name; ModDest dest; int osc; };
    const DestItem destItems[] = {
        { "Pitch",            ModDest::Pitch,           0 },
        { "Volume",           ModDest::Volume,          0 },
        { "Pan",              ModDest::Pan,             0 },
        { "Osc A Freq",       ModDest::OscFreq,         0 },
        { "Osc B Freq",       ModDest::OscFreq,         1 },
        { "Osc C Freq",       ModDest::OscFreq,         2 },
        { "Osc A PWM",        ModDest::OscPulse,        0 },
        { "Osc B PWM",        ModDest::OscPulse,        1 },
        { "Osc C PWM",        ModDest::OscPulse,        2 },
        { "Osc A Level",      ModDest::OscLevel,        0 },
        { "Osc B Level",      ModDest::OscLevel,        1 },
        { "Osc C Level",      ModDest::OscLevel,        2 },
        { "Filter Cutoff",    ModDest::FilterCutoff,    0 },
        { "Filter Resonance", ModDest::FilterResonance, 0 },
    };
    constexpr int numDestItems = static_cast<int>(std::size(destItems));

    int destToItem(ModDest d, int osc)
    {
        for (int i = 0; i < numDestItems; ++i)
            if (destItems[i].dest == d && (destItems[i].osc == osc
                    || (d != ModDest::OscFreq && d != ModDest::OscPulse && d != ModDest::OscLevel)))
                return i;
        return -1;
    }

    juce::String destDisplayName(ModDest d, int osc)
    {
        int item = destToItem(d, osc);
        return item >= 0 ? juce::String(destItems[item].name) : juce::String("-");
    }

    // Matches the LfoWaveform enum order (ModMatrix.h).
    const char* lfoShapeNames[] = { "Triangle", "Sine", "Square", "S && H", "Sawtooth" };

    ModSource makeDefaultEnvelope()
    {
        ModSource s;
        s.envAttack = 0.01;
        s.envDecay = 0.2;
        s.envSustain = 0.7;
        s.envSustainDecay = 0.0;
        s.envRelease = 0.3;
        return s;
    }
}

void ModMatrixEditor::ListModel::paintListBoxItem(int row, juce::Graphics& g, int width, int height, bool rowIsSelected)
{
    auto r = juce::Rectangle<int>(0, 0, width, height).reduced(2, 1).toFloat();
    if (rowIsSelected)
    {
        g.setColour(AS1LookAndFeel::palette.highlight.withAlpha(0.30f));
        g.fillRoundedRectangle(r, 5.0f);
    }
    else if (row % 2 == 1)
    {
        g.setColour(juce::Colours::white.withAlpha(0.03f));
        g.fillRoundedRectangle(r, 5.0f);
    }

    g.setColour(rowIsSelected ? AS1LookAndFeel::palette.text : AS1LookAndFeel::palette.text.withAlpha(0.85f));
    g.setFont(juce::Font(juce::FontOptions(13.5f)));
    g.drawText(textForRow ? textForRow(row) : juce::String(), 10, 0, width - 16, height,
               juce::Justification::centredLeft);
}

ModMatrixEditor::ModMatrixEditor(AS1AudioProcessor& p) : processor(p)
{
    // --- lists ---
    routingModel.numRows = [this] { return static_cast<int>(edited.mod.routings.size()); };
    routingModel.textForRow = [this](int row) -> juce::String
    {
        if (row < 0 || row >= (int) edited.mod.routings.size())
            return {};
        const auto& r = edited.mod.routings[(size_t) row];
        return sourceName(r.sourceIndex) + "  →  " + destDisplayName(r.dest, r.oscIndex)
             + "   (" + juce::String(r.amount, 2) + ")";
    };
    routingModel.onSelect = [this](int) { updateRoutingEditor(); };

    sourceModel.numRows = [this] { return static_cast<int>(edited.mod.sources.size()); };
    sourceModel.textForRow = [this](int row) -> juce::String
    {
        if (row < 0 || row >= (int) edited.mod.sources.size())
            return {};
        const auto& s = edited.mod.sources[(size_t) row];
        if (s.isLfo)
            return sourceName(row) + "   " + juce::String(s.lfo.rateHz, 2) + " Hz "
                 + lfoShapeNames[juce::jlimit(0, 4, (int) s.lfo.waveform)];
        return sourceName(row) + "   A " + juce::String(s.envAttack, 2) + "s  D "
             + juce::String(s.envDecay, 2) + "s  R " + juce::String(s.envRelease, 2) + "s";
    };
    sourceModel.onSelect = [this](int) { updateSourceEditor(); };

    for (auto* list : { &routingList, &sourceList })
    {
        list->setRowHeight(28);
        list->setColour(juce::ListBox::backgroundColourId, AS1LookAndFeel::palette.panel.withAlpha(0.35f));
        list->setColour(juce::ListBox::outlineColourId, AS1LookAndFeel::palette.panelBorder.withAlpha(0.6f));
        list->setOutlineThickness(1);
        addAndMakeVisible(*list);
    }
    routingList.setModel(&routingModel);
    sourceList.setModel(&sourceModel);

    // --- buttons ---
    for (auto* b : { &addRoutingBtn, &delRoutingBtn, &addSourceBtn, &delSourceBtn })
    {
        b->getProperties().set("pill", true);
        b->setColour(juce::TextButton::textColourOffId, AS1LookAndFeel::palette.text);
        addAndMakeVisible(*b);
    }

    addRoutingBtn.onClick = [this]
    {
        if (!hasPreset)
            return;
        if (edited.mod.sources.empty())
            edited.mod.sources.push_back(makeDefaultEnvelope());
        ModRouting nr;
        nr.sourceIndex = 0;
        nr.dest = ModDest::FilterCutoff;
        nr.amount = 0.5;
        edited.mod.routings.push_back(nr);
        publish();
        rebuildAll();
        routingList.selectRow((int) edited.mod.routings.size() - 1);
    };
    delRoutingBtn.onClick = [this]
    {
        int sel = selectedRouting();
        if (!hasPreset || sel < 0 || sel >= (int) edited.mod.routings.size())
            return;
        edited.mod.routings.erase(edited.mod.routings.begin() + sel);
        publish();
        rebuildAll();
    };
    addSourceBtn.onClick = [this]
    {
        if (!hasPreset)
            return;
        edited.mod.sources.push_back(makeDefaultEnvelope());
        publish();
        rebuildAll();
        sourceList.selectRow((int) edited.mod.sources.size() - 1);
    };
    delSourceBtn.onClick = [this]
    {
        int sel = selectedSource();
        if (!hasPreset || sel < 0 || sel >= (int) edited.mod.sources.size())
            return;
        edited.mod.sources.erase(edited.mod.sources.begin() + sel);
        // Routings addressing the removed source die with it; higher source
        // indices shift down by one.
        std::erase_if(edited.mod.routings, [sel](const ModRouting& r) { return r.sourceIndex == sel; });
        for (auto& r : edited.mod.routings)
            if (r.sourceIndex > sel)
                --r.sourceIndex;
        publish();
        rebuildAll();
    };

    // --- routing detail editor ---
    for (auto* l : { &sourceCaption, &destCaption, &typeCaption, &shapeCaption })
    {
        l->setColour(juce::Label::textColourId, AS1LookAndFeel::palette.textDim);
        addAndMakeVisible(*l);
    }

    destBox.clear();
    for (int i = 0; i < numDestItems; ++i)
        destBox.addItem(destItems[i].name, i + 1);
    addAndMakeVisible(sourceBox);
    addAndMakeVisible(destBox);

    sourceBox.onChange = [this]
    {
        int sel = selectedRouting();
        if (updating || !hasPreset || sel < 0 || sel >= (int) edited.mod.routings.size())
            return;
        int idx = sourceBox.getSelectedId() - 1;
        if (idx >= 0 && idx < (int) edited.mod.sources.size())
        {
            edited.mod.routings[(size_t) sel].sourceIndex = idx;
            publish();
            routingList.repaintRow(sel);
        }
    };
    destBox.onChange = [this]
    {
        int sel = selectedRouting();
        if (updating || !hasPreset || sel < 0 || sel >= (int) edited.mod.routings.size())
            return;
        int item = destBox.getSelectedId() - 1;
        if (item >= 0 && item < numDestItems)
        {
            edited.mod.routings[(size_t) sel].dest = destItems[item].dest;
            edited.mod.routings[(size_t) sel].oscIndex = destItems[item].osc;
            publish();
            routingList.repaintRow(sel);
        }
    };

    setupKnob(amountKnob, -1.0, 1.0, 1.0, [this](double v)
    {
        int sel = selectedRouting();
        if (sel >= 0 && sel < (int) edited.mod.routings.size())
        {
            edited.mod.routings[(size_t) sel].amount = v;
            routingList.repaintRow(sel);
        }
    });
    amountKnob.slider.setDoubleClickReturnValue(true, 0.0);

    // --- modulator detail editor ---
    typeBox.addItem("Envelope", 1);
    typeBox.addItem("LFO", 2);
    addAndMakeVisible(typeBox);
    typeBox.onChange = [this]
    {
        int sel = selectedSource();
        if (updating || !hasPreset || sel < 0 || sel >= (int) edited.mod.sources.size())
            return;
        edited.mod.sources[(size_t) sel].isLfo = (typeBox.getSelectedId() == 2);
        publish();
        rebuildAll();
        sourceList.selectRow(sel);
    };

    for (int i = 0; i < 5; ++i)
        shapeBox.addItem(lfoShapeNames[i], i + 1);
    addAndMakeVisible(shapeBox);
    shapeBox.onChange = [this]
    {
        int sel = selectedSource();
        if (updating || !hasPreset || sel < 0 || sel >= (int) edited.mod.sources.size())
            return;
        edited.mod.sources[(size_t) sel].lfo.waveform = static_cast<LfoWaveform>(shapeBox.getSelectedId() - 1);
        publish();
        sourceList.repaintRow(sel);
    };

    // Envelope knob ranges mirror the flat APVTS envelope parameters (0..20 s,
    // heavy skew; sustain level 0..1).
    auto envField = [this](double ModSource::* field)
    {
        return [this, field](double v)
        {
            int sel = selectedSource();
            if (sel >= 0 && sel < (int) edited.mod.sources.size())
            {
                edited.mod.sources[(size_t) sel].*field = v;
                sourceList.repaintRow(sel);
            }
        };
    };
    setupKnob(envAttack,   0.0, 20.0, 0.25, envField(&ModSource::envAttack));
    setupKnob(envDecay,    0.0, 20.0, 0.25, envField(&ModSource::envDecay));
    setupKnob(envSustain,  0.0, 1.0,  1.0,  envField(&ModSource::envSustain));
    setupKnob(envSusDecay, 0.0, 20.0, 0.25, envField(&ModSource::envSustainDecay));
    setupKnob(envRelease,  0.0, 20.0, 0.25, envField(&ModSource::envRelease));

    // LFO rate covers the hardware's 0.03..33 Hz curve (see lfoRateToHz).
    setupKnob(lfoRate, 0.03, 33.0, 0.35, [this](double v)
    {
        int sel = selectedSource();
        if (sel >= 0 && sel < (int) edited.mod.sources.size())
        {
            edited.mod.sources[(size_t) sel].lfo.rateHz = v;
            sourceList.repaintRow(sel);
        }
    });
    setupKnob(lfoDelay, 0.0, 5.0, 0.5, [this](double v)
    {
        int sel = selectedSource();
        if (sel >= 0 && sel < (int) edited.mod.sources.size())
            edited.mod.sources[(size_t) sel].lfo.delaySeconds = v;
    });

    rebuildAll();
}

ModMatrixEditor::~ModMatrixEditor()
{
    routingList.setModel(nullptr);
    sourceList.setModel(nullptr);
}

void ModMatrixEditor::setupKnob(KnobView& knob, double min, double max, double skew,
                                 std::function<void(double)> apply)
{
    knob.slider.setRange(min, max, 0.0001);
    knob.slider.setSkewFactor(skew);
    knob.slider.onValueChange = [this, &knob, apply = std::move(apply)]
    {
        if (updating || !hasPreset)
            return;
        apply(knob.slider.getValue());
        publish();
    };
    addAndMakeVisible(knob);
}

void ModMatrixEditor::refresh()
{
    auto snap = processor.getPresetSnapshot();
    if (snap == shown)
        return;
    shown = snap;
    hasPreset = (snap != nullptr && snap->valid);
    if (hasPreset)
        edited = *snap;
    else
        edited = RasPreset {};
    rebuildAll();
}

void ModMatrixEditor::publish()
{
    auto p = std::make_shared<const RasPreset>(edited);
    processor.setPresetSnapshot(p);
    shown = std::move(p); // our own snapshot — don't rebuild on the next refresh()
}

juce::String ModMatrixEditor::sourceName(int index) const
{
    if (index < 0 || index >= (int) edited.mod.sources.size())
        return "?";
    int envN = 0, lfoN = 0;
    for (int i = 0; i <= index; ++i)
        edited.mod.sources[(size_t) i].isLfo ? ++lfoN : ++envN;
    return edited.mod.sources[(size_t) index].isLfo ? "LFO " + juce::String(lfoN)
                                                    : "Envelope " + juce::String(envN);
}

void ModMatrixEditor::rebuildAll()
{
    updating = true;

    // Source combo mirrors the modulator list.
    sourceBox.clear(juce::dontSendNotification);
    for (int i = 0; i < (int) edited.mod.sources.size(); ++i)
        sourceBox.addItem(sourceName(i), i + 1);

    routingList.updateContent();
    sourceList.updateContent();
    routingList.deselectAllRows();
    sourceList.deselectAllRows();
    if (!edited.mod.routings.empty())
        routingList.selectRow(0);
    if (!edited.mod.sources.empty())
        sourceList.selectRow(0);

    addRoutingBtn.setEnabled(hasPreset);
    addSourceBtn.setEnabled(hasPreset);

    updating = false;
    updateRoutingEditor();
    updateSourceEditor();
    repaint();
}

void ModMatrixEditor::updateRoutingEditor()
{
    updating = true;
    int sel = selectedRouting();
    bool valid = hasPreset && sel >= 0 && sel < (int) edited.mod.routings.size();

    for (auto* c : std::initializer_list<juce::Component*> { &sourceBox, &destBox, &amountKnob, &delRoutingBtn })
        c->setEnabled(valid);

    if (valid)
    {
        const auto& r = edited.mod.routings[(size_t) sel];
        sourceBox.setSelectedId(r.sourceIndex + 1, juce::dontSendNotification);
        destBox.setSelectedId(destToItem(r.dest, r.oscIndex) + 1, juce::dontSendNotification);
        amountKnob.slider.setValue(r.amount, juce::dontSendNotification);
    }
    else
    {
        sourceBox.setSelectedId(0, juce::dontSendNotification);
        destBox.setSelectedId(0, juce::dontSendNotification);
        amountKnob.slider.setValue(0.0, juce::dontSendNotification);
    }
    updating = false;
}

void ModMatrixEditor::updateSourceEditor()
{
    updating = true;
    int sel = selectedSource();
    bool valid = hasPreset && sel >= 0 && sel < (int) edited.mod.sources.size();
    bool isLfo = valid && edited.mod.sources[(size_t) sel].isLfo;

    typeBox.setEnabled(valid);
    delSourceBtn.setEnabled(valid);

    // Envelope knobs vs LFO controls swap in place, like the original page
    // rebuilding its fader row per modulator type.
    for (auto* c : std::initializer_list<juce::Component*> { &envAttack, &envDecay, &envSustain, &envSusDecay, &envRelease })
        c->setVisible(!isLfo), c->setEnabled(valid && !isLfo);
    for (auto* c : std::initializer_list<juce::Component*> { &shapeCaption, &shapeBox, &lfoRate, &lfoDelay })
        c->setVisible(isLfo);

    if (valid)
    {
        const auto& s = edited.mod.sources[(size_t) sel];
        typeBox.setSelectedId(s.isLfo ? 2 : 1, juce::dontSendNotification);
        if (s.isLfo)
        {
            shapeBox.setSelectedId(juce::jlimit(0, 4, (int) s.lfo.waveform) + 1, juce::dontSendNotification);
            lfoRate.slider.setValue(s.lfo.rateHz, juce::dontSendNotification);
            lfoDelay.slider.setValue(s.lfo.delaySeconds, juce::dontSendNotification);
        }
        else
        {
            envAttack.slider.setValue(s.envAttack, juce::dontSendNotification);
            envDecay.slider.setValue(s.envDecay, juce::dontSendNotification);
            envSustain.slider.setValue(s.envSustain, juce::dontSendNotification);
            envSusDecay.slider.setValue(s.envSustainDecay, juce::dontSendNotification);
            envRelease.slider.setValue(s.envRelease, juce::dontSendNotification);
        }
    }
    else
    {
        typeBox.setSelectedId(0, juce::dontSendNotification);
    }
    updating = false;
}

void ModMatrixEditor::paint(juce::Graphics& g)
{
    auto area = getLocalBounds().reduced(18, 6);
    int halfW = area.getWidth() / 2 - 14;

    g.setColour(AS1LookAndFeel::palette.text);
    g.setFont(juce::Font(juce::FontOptions(16.0f)).boldened().withExtraKerningFactor(0.08f));
    g.drawText("ROUTINGS", area.getX(), area.getY() + 4, halfW, 22, juce::Justification::centred);
    g.drawText("MODULATORS", area.getRight() - halfW, area.getY() + 4, halfW, 22, juce::Justification::centred);

    // Thin centre divider, like the section separators on the Main tab.
    g.setColour(juce::Colours::white.withAlpha(0.08f));
    g.fillRect(area.getCentreX(), area.getY() + 12, 1, area.getHeight() - 24);

    if (!hasPreset)
    {
        g.setColour(AS1LookAndFeel::palette.textDim);
        g.setFont(juce::Font(juce::FontOptions(15.0f)));
        g.drawText("Load a preset to edit its modulation matrix.", getLocalBounds(),
                   juce::Justification::centred);
    }
}

void ModMatrixEditor::resized()
{
    auto area = getLocalBounds().reduced(18, 6);
    auto left = area.removeFromLeft(area.getWidth() / 2 - 14);
    area.removeFromLeft(28);
    auto right = area;

    auto layoutButtons = [](juce::Rectangle<int> row, juce::TextButton& add, juce::TextButton& del)
    {
        row = row.withSizeKeepingCentre(row.getWidth(), 24);
        add.setBounds(row.removeFromLeft(72));
        row.removeFromLeft(10);
        del.setBounds(row.removeFromLeft(72));
    };

    // ---- left: routings ----
    {
        auto col = left;
        col.removeFromTop(30); // painted title
        auto editorArea = col.removeFromBottom(104);
        auto btnRow = col.removeFromBottom(32);
        routingList.setBounds(col.reduced(0, 2));
        layoutButtons(btnRow, addRoutingBtn, delRoutingBtn);

        auto knobArea = editorArea.removeFromRight(86);
        amountKnob.setBounds(knobArea.withSizeKeepingCentre(72, 94));
        editorArea.removeFromRight(10);

        auto row1 = editorArea.removeFromTop(30).withTrimmedTop(2);
        sourceCaption.setBounds(row1.removeFromLeft(90));
        sourceBox.setBounds(row1.withHeight(26).withTrimmedRight(4));
        editorArea.removeFromTop(8);
        auto row2 = editorArea.removeFromTop(30).withTrimmedTop(2);
        destCaption.setBounds(row2.removeFromLeft(90));
        destBox.setBounds(row2.withHeight(26).withTrimmedRight(4));
    }

    // ---- right: modulators ----
    {
        auto col = right;
        col.removeFromTop(30);
        auto editorArea = col.removeFromBottom(134);
        auto btnRow = col.removeFromBottom(32);
        sourceList.setBounds(col.reduced(0, 2));
        layoutButtons(btnRow, addSourceBtn, delSourceBtn);

        auto typeRow = editorArea.removeFromTop(30).withTrimmedTop(2);
        typeCaption.setBounds(typeRow.removeFromLeft(46));
        typeBox.setBounds(typeRow.removeFromLeft(130).withHeight(26));
        typeRow.removeFromLeft(16);
        shapeCaption.setBounds(typeRow.removeFromLeft(52));
        shapeBox.setBounds(typeRow.removeFromLeft(130).withHeight(26));

        editorArea.removeFromTop(6);
        auto knobRow = editorArea;
        int kw = juce::jmin(76, knobRow.getWidth() / 5);
        auto place = [&knobRow, kw](KnobView& k) { k.setBounds(knobRow.removeFromLeft(kw).withHeight(94)); };
        // Envelope and LFO knob sets overlap — visibility switches per type.
        auto envRow = knobRow;
        place(envAttack); place(envDecay); place(envSustain); place(envSusDecay); place(envRelease);
        knobRow = envRow;
        place(lfoRate); place(lfoDelay);
    }
}

} // namespace as1
