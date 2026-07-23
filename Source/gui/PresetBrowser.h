#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace as1
{

class AS1AudioProcessor;

// Left-hand preset sidebar: current preset name + scrollable list of the
// RetroAS1 factory library, styled after FLEX.png's browser column. The
// search box lives in the top bar (as in FLEX) — this component still owns
// and filters it, but PluginEditor is responsible for parenting/positioning
// it there via getSearchBox().
class PresetBrowser : public juce::Component,
                       private juce::ListBoxModel,
                       private juce::TextEditor::Listener
{
public:
    explicit PresetBrowser(AS1AudioProcessor& ownerProcessor);

    void resized() override;
    void paint(juce::Graphics&) override;

    void refresh();

    juce::TextEditor& getSearchBox() { return searchBox; }

private:
    int getNumRows() override;
    void paintListBoxItem(int rowNumber, juce::Graphics&, int width, int height, bool rowIsSelected) override;
    void listBoxItemClicked(int row, const juce::MouseEvent&) override;

    void textEditorTextChanged(juce::TextEditor&) override;

    void rebuildFilter();

    AS1AudioProcessor& processor;
    juce::Label presetNameLabel { "presetName", "Init" };
    juce::TextEditor searchBox;
    juce::ListBox listBox { "presets", this };
    juce::Array<int> filteredIndices;
    juce::Image starOn, starOff;
};

} // namespace as1
