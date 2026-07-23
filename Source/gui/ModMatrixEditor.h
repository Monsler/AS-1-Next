#pragma once

#include "KnobView.h"
#include "../presets/RasPreset.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <memory>

namespace as1
{

class AS1AudioProcessor;

// Editable modulation matrix — the modern take on the original Retro AS-1
// editor's "Modulation" page (see the Bitheadz screenshot): a Routings list
// (source -> destination x amount) on the left and a Modulators list (every
// envelope / LFO the preset carries) on the right, each with Add / Delete and
// a detail editor underneath.
//
// Edits copy the processor's current preset snapshot, mutate its ModMatrix and
// republish it via setPresetSnapshot — voices pick the new matrix up at the
// next note-on, exactly like a preset load. The flat APVTS parameters are not
// involved (the general matrix can't fit them; see ModMatrix.h).
class ModMatrixEditor : public juce::Component
{
public:
    explicit ModMatrixEditor(AS1AudioProcessor&);
    ~ModMatrixEditor() override;

    // Called regularly by the editor's timer: rebuilds the UI whenever the
    // processor's snapshot was replaced from outside (i.e. a preset load).
    void refresh();

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    // One ListBoxModel serving both lists via callbacks into the owner.
    struct ListModel : juce::ListBoxModel
    {
        std::function<int()> numRows;
        std::function<juce::String(int)> textForRow;
        std::function<void(int)> onSelect;

        int getNumRows() override { return numRows ? numRows() : 0; }
        void paintListBoxItem(int row, juce::Graphics&, int width, int height, bool rowIsSelected) override;
        void selectedRowsChanged(int lastRowSelected) override { if (onSelect) onSelect(lastRowSelected); }
    };

    void rebuildAll();
    void publish();
    void updateRoutingEditor();
    void updateSourceEditor();
    void setupKnob(KnobView&, double min, double max, double skew, std::function<void(double)> apply);

    juce::String sourceName(int index) const;
    int selectedRouting() const { return routingList.getSelectedRow(); }
    int selectedSource() const { return sourceList.getSelectedRow(); }

    AS1AudioProcessor& processor;

    std::shared_ptr<const RasPreset> shown; // last snapshot reflected in the UI
    RasPreset edited;                       // working copy of *shown
    bool hasPreset = false;
    bool updating = false;                  // guard: programmatic control updates mustn't republish

    ListModel routingModel, sourceModel;
    juce::ListBox routingList { "routings" }, sourceList { "modulators" };

    juce::TextButton addRoutingBtn { "Add" }, delRoutingBtn { "Delete" };
    juce::TextButton addSourceBtn { "Add" }, delSourceBtn { "Delete" };

    juce::Label sourceCaption { {}, "Source" }, destCaption { {}, "Destination" };
    juce::Label typeCaption { {}, "Type" }, shapeCaption { {}, "Shape" };
    juce::ComboBox sourceBox, destBox, typeBox, shapeBox;

    KnobView amountKnob { "Amount" };
    KnobView envAttack { "Attack" }, envDecay { "Decay" }, envSustain { "Sus Lvl" },
             envSusDecay { "Sus Dec" }, envRelease { "Release" };
    KnobView lfoRate { "Rate" }, lfoDelay { "Delay" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ModMatrixEditor)
};

} // namespace as1
