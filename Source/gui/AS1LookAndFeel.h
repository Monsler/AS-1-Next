#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace as1
{

// Dark, glassy synth theme inspired by FLEX.png: slate/purple panels,
// glossy spherical rotary knobs with a bright rim highlight.
class AS1LookAndFeel : public juce::LookAndFeel_V4
{
public:
    AS1LookAndFeel();

    void drawRotarySlider(juce::Graphics&, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider&) override;

    void drawToggleButton(juce::Graphics&, juce::ToggleButton&,
                           bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

    // Tab buttons are plain text over the backdrop, per FLEX.png — no filled
    // rounded body.
    void drawButtonBackground(juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
    // Tab labels drawn centred with no indent — the default's height-based
    // left/right indents ellipsise short tabs ("Main" -> "M...").
    void drawButtonText(juce::Graphics&, juce::TextButton&,
                         bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
    juce::Font getTextButtonFont(juce::TextButton&, int buttonHeight) override;

    void drawComboBox(juce::Graphics&, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox&) override;

    void fillTextEditorBackground(juce::Graphics&, int width, int height, juce::TextEditor&) override;
    void drawTextEditorOutline(juce::Graphics&, int width, int height, juce::TextEditor&) override;

    juce::Font getLabelFont(juce::Label&) override;
    juce::Font getComboBoxFont(juce::ComboBox&) override;

    struct Palette
    {
        juce::Colour bgTop        { 0xff2a2733 };
        juce::Colour bgBottom     { 0xff1c1a22 };
        juce::Colour panel        { 0xff353244 };
        juce::Colour panelBorder  { 0xff45414f };
        juce::Colour sidebar      { 0xff2b1f2a };
        juce::Colour accent       { 0xffe8e2f5 };
        juce::Colour accentDim    { 0xff8b86a0 };
        juce::Colour knobDark     { 0xff3a3648 };
        juce::Colour knobLight    { 0xffe9e6f2 };
        juce::Colour text         { 0xffe6e2f0 };
        juce::Colour textDim      { 0xff9691a6 };
        juce::Colour highlight    { 0xff7c6ff0 };
    };

    static const Palette palette;
};

} // namespace as1
