#pragma once

#include "AS1LookAndFeel.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace as1
{

// A titled section that lays its children out in a centred left-to-right row,
// mirroring the "FILTER ENVELOPE" / "FILTER" / "AMP ENVELOPE" groups in
// FLEX.png: no boxed frame of its own — sections sit directly on the shared
// translucent panel band, with a bold centred title above the controls and an
// optional thin separator on the right edge dividing neighbouring sections.
// Optionally carries a small power toggle next to the title.
class SectionPanel : public juce::Component
{
public:
    explicit SectionPanel(const juce::String& titleText, juce::Button* powerToggle = nullptr)
        : title(titleText), toggle(powerToggle)
    {
        if (toggle != nullptr)
            addAndMakeVisible(toggle);

        itemsBox.flexDirection = juce::FlexBox::Direction::row;
        itemsBox.justifyContent = juce::FlexBox::JustifyContent::center;
        itemsBox.alignItems = juce::FlexBox::AlignItems::center;
    }

    void addKnob(juce::Component& c, float width = 64.0f)
    {
        addAndMakeVisible(c);
        itemsBox.items.add(juce::FlexItem(c).withWidth(width).withHeight(78.0f).withMargin({ 0, 5, 0, 5 }));
    }

    void addWideItem(juce::Component& c, float width)
    {
        addAndMakeVisible(c);
        itemsBox.items.add(juce::FlexItem(c).withWidth(width).withHeight(28.0f).withMargin({ 0, 5, 0, 5 }));
    }

    void paint(juce::Graphics& g) override
    {
        g.setColour(AS1LookAndFeel::palette.text);
        g.setFont(juce::Font(juce::FontOptions(16.0f)).boldened().withExtraKerningFactor(0.08f));
        g.drawText(title, 0, 8, getWidth(), 22, juce::Justification::centred);

        if (showRightSeparator)
        {
            g.setColour(juce::Colours::white.withAlpha(0.08f));
            g.fillRect(getWidth() - 1, 10, 1, getHeight() - 20);
        }
    }

    void resized() override
    {
        if (toggle != nullptr)
            toggle->setBounds(10, 8, 20, 22);

        itemsBox.performLayout(getLocalBounds().withTrimmedTop(36).withTrimmedLeft(14).withTrimmedRight(10).withTrimmedBottom(10));
    }

    bool showRightSeparator = false;

private:
    juce::String title;
    juce::Button* toggle;
    juce::FlexBox itemsBox;
};

} // namespace as1
