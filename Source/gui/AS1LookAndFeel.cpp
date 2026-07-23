#include "AS1LookAndFeel.h"
#include "BinaryData.h"

namespace as1
{

const AS1LookAndFeel::Palette AS1LookAndFeel::palette {};

AS1LookAndFeel::AS1LookAndFeel()
{
    auto typeface = juce::Typeface::createSystemTypefaceFor(BinaryData::NunitoRegular_ttf,
                                                              static_cast<size_t>(BinaryData::NunitoRegular_ttfSize));
    setDefaultSansSerifTypeface(typeface);

    setColour(juce::ResizableWindow::backgroundColourId, palette.bgBottom);
    setColour(juce::Slider::textBoxTextColourId, palette.text);
    setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour(juce::Label::textColourId, palette.text);
    setColour(juce::ComboBox::backgroundColourId, palette.panel);
    setColour(juce::ComboBox::textColourId, palette.text);
    setColour(juce::ComboBox::outlineColourId, palette.panelBorder);
    setColour(juce::PopupMenu::backgroundColourId, palette.panel);
    setColour(juce::PopupMenu::textColourId, palette.text);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, palette.highlight);
    setColour(juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
    setColour(juce::ListBox::textColourId, palette.text);
    setColour(juce::TextEditor::backgroundColourId, palette.panel);
    setColour(juce::TextEditor::textColourId, palette.text);
    setColour(juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
    setColour(juce::TextEditor::focusedOutlineColourId, palette.highlight);
    setColour(juce::ScrollBar::thumbColourId, palette.accentDim);
    setColour(juce::ToggleButton::textColourId, palette.text);
}

void AS1LookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                       float sliderPosProportional, float rotaryStartAngle,
                                       float rotaryEndAngle, juce::Slider& slider)
{
    auto bounds = juce::Rectangle<float>((float) x, (float) y, (float) width, (float) height).reduced(4.0f);
    auto centre = bounds.getCentre();
    float radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f - 1.0f;
    float angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

    // "accent" knobs are drawn as filled white discs (FLEX's main Cutoff/Mix
    // knobs); the rest are flat dark discs. Flag set from the editor via the
    // slider's property store.
    bool accent = static_cast<bool>(slider.getProperties().getWithDefault("accent", false));

    // Flat disc body — no gloss/gradient, matching FLEX's simplified knobs.
    float bodyRadius = radius * 0.82f;
    auto body = juce::Rectangle<float>(bodyRadius * 2.0f, bodyRadius * 2.0f).withCentre(centre);

    // Thin value ring around the disc: a dim full track plus a bright arc up to
    // the current position. Kept subtle for dark knobs, brighter for accents.
    float arcRadius = radius;
    juce::Path track;
    track.addCentredArc(centre.x, centre.y, arcRadius, arcRadius, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
    g.setColour(palette.panelBorder.withAlpha(0.55f));
    g.strokePath(track, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    if (sliderPosProportional > 0.001f)
    {
        juce::Path valueArc;
        valueArc.addCentredArc(centre.x, centre.y, arcRadius, arcRadius, 0.0f, rotaryStartAngle, angle, true);
        g.setColour(accent ? palette.knobLight : palette.accent.withAlpha(0.85f));
        g.strokePath(valueArc, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    g.setColour(accent ? palette.knobLight : palette.knobDark);
    g.fillEllipse(body);
    g.setColour(juce::Colours::black.withAlpha(accent ? 0.10f : 0.28f));
    g.drawEllipse(body, 1.0f);

    // Pointer dot near the rim — dark on white discs, light on dark discs.
    float dotRadius = bodyRadius * 0.10f;
    juce::Point<float> dotPos = centre.getPointOnCircumference(bodyRadius * 0.66f, angle);
    g.setColour(accent ? palette.knobDark : palette.knobLight);
    g.fillEllipse(juce::Rectangle<float>(dotRadius * 2.0f, dotRadius * 2.0f).withCentre(dotPos));
}

void AS1LookAndFeel::drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                                       bool shouldDrawButtonAsHighlighted, bool)
{
    auto bounds = button.getLocalBounds().toFloat();
    float d = bounds.getHeight() * 0.62f;
    auto circle = juce::Rectangle<float>(d, d).withCentre({ bounds.getX() + d * 0.5f + 2.0f, bounds.getCentreY() });

    bool on = button.getToggleState();
    g.setColour(on ? palette.highlight : palette.panelBorder);
    g.drawEllipse(circle, 1.6f);
    if (on)
    {
        g.setColour(palette.highlight);
        g.fillEllipse(circle.reduced(3.0f));
    }

    g.setColour(shouldDrawButtonAsHighlighted ? palette.text : palette.accentDim);
    g.setFont(juce::Font(juce::FontOptions(14.0f)).boldened());
    g.drawFittedText(button.getButtonText(), bounds.getX() + (int) d + 10, 0, bounds.getWidth() - d, bounds.getHeight(),
                      juce::Justification::centredLeft, 1);
}

void AS1LookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button, const juce::Colour&,
                                           bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown)
{
    // Tab buttons stay plain text over the backdrop, per FLEX.png; buttons
    // flagged "pill" (the mod-matrix Add/Delete) get a small filled body.
    if (! static_cast<bool>(button.getProperties().getWithDefault("pill", false)))
        return;

    auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
    auto fill = palette.panel;
    if (shouldDrawButtonAsDown)
        fill = palette.highlight.withAlpha(0.55f);
    else if (shouldDrawButtonAsHighlighted)
        fill = palette.panel.brighter(0.15f);
    g.setColour(fill.withMultipliedAlpha(button.isEnabled() ? 1.0f : 0.5f));
    g.fillRoundedRectangle(bounds, bounds.getHeight() * 0.5f);
    g.setColour(palette.panelBorder.withMultipliedAlpha(button.isEnabled() ? 1.0f : 0.5f));
    g.drawRoundedRectangle(bounds, bounds.getHeight() * 0.5f, 1.0f);
}

void AS1LookAndFeel::drawButtonText(juce::Graphics& g, juce::TextButton& button, bool, bool)
{
    g.setFont(getTextButtonFont(button, button.getHeight()));
    auto colourId = button.getToggleState() ? juce::TextButton::textColourOnId
                                            : juce::TextButton::textColourOffId;
    g.setColour(button.findColour(colourId).withMultipliedAlpha(button.isEnabled() ? 1.0f : 0.5f));
    g.drawText(button.getButtonText(), button.getLocalBounds(), juce::Justification::centred, false);
}

juce::Font AS1LookAndFeel::getTextButtonFont(juce::TextButton& button, int)
{
    // "pill" buttons are small utility buttons; tabs keep FLEX's large labels.
    if (static_cast<bool>(button.getProperties().getWithDefault("pill", false)))
        return juce::Font(juce::FontOptions(13.5f)).boldened();
    return juce::Font(juce::FontOptions(19.0f)).boldened();
}

void AS1LookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool,
                                   int, int, int, int, juce::ComboBox& box)
{
    auto bounds = juce::Rectangle<float>(0, 0, (float) width, (float) height);
    g.setColour(palette.panel);
    g.fillRoundedRectangle(bounds, height * 0.5f);
    g.setColour(palette.panelBorder);
    g.drawRoundedRectangle(bounds.reduced(0.5f), height * 0.5f, 1.0f);

    juce::Path arrow;
    float ax = width - 16.0f, ay = height * 0.5f;
    arrow.addTriangle(ax - 4.0f, ay - 2.5f, ax + 4.0f, ay - 2.5f, ax, ay + 3.0f);
    g.setColour(palette.accentDim);
    g.fillPath(arrow);
    juce::ignoreUnused(box);
}

void AS1LookAndFeel::fillTextEditorBackground(juce::Graphics& g, int width, int height, juce::TextEditor& editor)
{
    auto bounds = juce::Rectangle<float>(0, 0, (float) width, (float) height);
    g.setColour(editor.findColour(juce::TextEditor::backgroundColourId));
    g.fillRoundedRectangle(bounds, height * 0.5f);
}

void AS1LookAndFeel::drawTextEditorOutline(juce::Graphics& g, int width, int height, juce::TextEditor& editor)
{
    if (! editor.isEnabled())
        return;

    auto bounds = juce::Rectangle<float>(0, 0, (float) width, (float) height).reduced(0.5f);
    g.setColour(editor.hasKeyboardFocus(true) ? palette.highlight : palette.panelBorder.withAlpha(0.7f));
    g.drawRoundedRectangle(bounds, height * 0.5f, 1.0f);
}

juce::Font AS1LookAndFeel::getLabelFont(juce::Label&)
{
    return juce::Font(juce::FontOptions(13.5f)).withExtraKerningFactor(0.02f);
}

juce::Font AS1LookAndFeel::getComboBoxFont(juce::ComboBox&)
{
    return juce::Font(juce::FontOptions(13.0f));
}

} // namespace as1
