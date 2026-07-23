#pragma once

#include "AS1LookAndFeel.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace as1
{

// A rotary slider with a caption underneath — the basic unit of every
// section in the FLEX-style layout (e.g. "Cutoff", "Res", "A", "D"...).
class KnobView : public juce::Component
{
public:
    explicit KnobView(const juce::String& captionText)
    {
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        slider.setRotaryParameters(juce::MathConstants<float>::pi * 1.2f,
                                    juce::MathConstants<float>::pi * 2.8f, true);
        // Plain (non-velocity) rotary drag relies on the OS supporting
        // unbounded/warped mouse movement while dragging; under XWayland this
        // is unreliable and makes the knob feel jumpy/broken. Velocity-based
        // mode drives the value from mouse *speed* instead, which sidesteps
        // that path entirely and feels correct on Linux too.
        // JUCE's velocity curve is `sin`-shaped against (offset + speed); with
        // offset at its default of 0.0 that curve starts at zero, so slow or
        // moderate drags (the common case) barely move the value at all. A
        // non-zero offset gives slow drags real, if gentle, motion, and the
        // higher sensitivity keeps a full sweep within a comfortable drag
        // distance.
        slider.setVelocityBasedMode(true);
        // Lower sensitivity so the knobs don't lurch on small mouse moves; a
        // small offset keeps slow drags responsive without over-reacting.
        slider.setVelocityModeParameters(1.0, 1, 0.12, false);
        addAndMakeVisible(slider);

        caption.setText(captionText, juce::dontSendNotification);
        caption.setJustificationType(juce::Justification::centred);
        caption.setColour(juce::Label::textColourId, AS1LookAndFeel::palette.textDim);
        caption.setFont(juce::Font(juce::FontOptions(12.5f)));
        addAndMakeVisible(caption);
    }

    void resized() override
    {
        auto area = getLocalBounds();
        auto captionArea = area.removeFromBottom(16);
        slider.setBounds(area);
        caption.setBounds(captionArea);
    }

    juce::Slider slider;

private:
    juce::Label caption;
};

} // namespace as1
