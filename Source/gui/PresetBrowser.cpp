#include "PresetBrowser.h"
#include "AS1LookAndFeel.h"
#include "../PluginProcessor.h"
#include "BinaryData.h"

namespace as1
{

namespace
{
    constexpr int starHitWidth = 30;
    constexpr int starDrawSize = 15;

    // Downscale a large source icon to UI size by repeated halving, then a
    // final high-quality resample. A direct 512 -> ~15 rescale (even at
    // "high" quality) samples so sparsely it aliases into ragged edges;
    // halving keeps every step within the filter's footprint. Rendered at 2x
    // the draw size so the icon also stays crisp on hi-DPI displays.
    juce::Image downscaleForUi(juce::Image img, int targetSize)
    {
        if (!img.isValid())
            return img;
        while (img.getWidth() >= targetSize * 4)
            img = img.rescaled(img.getWidth() / 2, img.getHeight() / 2, juce::Graphics::highResamplingQuality);
        return img.rescaled(targetSize, targetSize, juce::Graphics::highResamplingQuality);
    }
}

PresetBrowser::PresetBrowser(AS1AudioProcessor& ownerProcessor) : processor(ownerProcessor)
{
    starOn = downscaleForUi(juce::ImageFileFormat::loadFrom(BinaryData::favoriteon_png, static_cast<size_t>(BinaryData::favoriteon_pngSize)),
                             starDrawSize * 2);
    starOff = downscaleForUi(juce::ImageFileFormat::loadFrom(BinaryData::favoriteoff_png, static_cast<size_t>(BinaryData::favoriteoff_pngSize)),
                              starDrawSize * 2);

    presetNameLabel.setFont(juce::Font(juce::FontOptions(18.0f)).boldened());
    presetNameLabel.setColour(juce::Label::textColourId, AS1LookAndFeel::palette.text);
    presetNameLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(presetNameLabel);

    // Owned/filtered here, but parented into the top bar by PluginEditor.
    searchBox.setFont(juce::Font(juce::FontOptions(15.0f)));
    searchBox.setIndents(16, 0);
    searchBox.setJustification(juce::Justification::centredLeft);
    searchBox.setTextToShowWhenEmpty("Search presets...", AS1LookAndFeel::palette.textDim);
    searchBox.addListener(this);

    listBox.setRowHeight(24);
    addAndMakeVisible(listBox);

    refresh();
}

void PresetBrowser::refresh()
{
    rebuildFilter();
    listBox.updateContent();
    listBox.repaint();

    auto& pm = processor.getPresetManager();
    int current = pm.getCurrentIndex();
    presetNameLabel.setText(current >= 0 ? pm.getPresetDisplayName(current) : juce::String("Init"),
                             juce::dontSendNotification);
}

void PresetBrowser::rebuildFilter()
{
    filteredIndices.clear();
    auto& pm = processor.getPresetManager();
    auto query = searchBox.getText().trim().toLowerCase();

    // Favourites first (in library order), then the rest — so starred presets
    // rise to the top of the list.
    juce::Array<int> rest;
    for (int i = 0; i < pm.getNumPresets(); ++i)
    {
        if (! (query.isEmpty() || pm.getPresetDisplayName(i).toLowerCase().contains(query)))
            continue;
        if (pm.isFavorite(i))
            filteredIndices.add(i);
        else
            rest.add(i);
    }
    filteredIndices.addArray(rest);
}

void PresetBrowser::resized()
{
    auto area = getLocalBounds().reduced(16, 14);
    presetNameLabel.setBounds(area.removeFromTop(30));
    area.removeFromTop(10);
    listBox.setBounds(area);
}

void PresetBrowser::paint(juce::Graphics& g)
{
    g.setColour(AS1LookAndFeel::palette.sidebar);
    g.fillRect(getLocalBounds());

    g.setColour(AS1LookAndFeel::palette.panelBorder.withAlpha(0.4f));
    g.drawLine(0, 60.0f, (float) getWidth(), 60.0f, 1.0f);
}

int PresetBrowser::getNumRows()
{
    return filteredIndices.size();
}

void PresetBrowser::paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected)
{
    if (rowNumber < 0 || rowNumber >= filteredIndices.size())
        return;

    auto& pm = processor.getPresetManager();
    int presetIndex = filteredIndices[rowNumber];
    bool isCurrent = presetIndex == pm.getCurrentIndex();

    if (isCurrent)
    {
        g.setColour(AS1LookAndFeel::palette.highlight.withAlpha(0.28f));
        g.fillRoundedRectangle(2.0f, 1.0f, (float) width - 4.0f, (float) height - 2.0f, 4.0f);
    }
    else if (rowIsSelected)
    {
        g.setColour(juce::Colours::white.withAlpha(0.05f));
        g.fillRoundedRectangle(2.0f, 1.0f, (float) width - 4.0f, (float) height - 2.0f, 4.0f);
    }

    g.setColour(isCurrent ? AS1LookAndFeel::palette.accent : AS1LookAndFeel::palette.text.withAlpha(0.85f));
    g.setFont(juce::Font(juce::FontOptions(14.0f)));
    g.drawText(pm.getPresetDisplayName(presetIndex), 10, 0, width - 14 - starHitWidth, height, juce::Justification::centredLeft);

    // Favourite star on the right. The filled star always shows; the empty
    // outline only shows on hover-less rows that are favourited vs not — here we
    // draw filled for favourites and a dim outline otherwise.
    bool fav = pm.isFavorite(presetIndex);
    const juce::Image& star = fav ? starOn : starOff;
    if (star.isValid())
    {
        int s = starDrawSize;
        int sx = width - starHitWidth + (starHitWidth - s) / 2;
        int sy = (height - s) / 2;
        g.setOpacity(fav ? 1.0f : 0.35f);
        g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
        g.drawImage(star, sx, sy, s, s, 0, 0, star.getWidth(), star.getHeight());
        g.setOpacity(1.0f);
    }
}

void PresetBrowser::listBoxItemClicked(int row, const juce::MouseEvent& e)
{
    if (row < 0 || row >= filteredIndices.size())
        return;

    int presetIndex = filteredIndices[row];

    // A click on the star column toggles the favourite (and re-sorts) instead of
    // loading the preset.
    if (e.x >= listBox.getWidth() - starHitWidth)
    {
        processor.getPresetManager().toggleFavorite(presetIndex);
        rebuildFilter();
        listBox.updateContent();
        listBox.repaint();
        return;
    }

    processor.getPresetManager().loadPresetByIndex(presetIndex);
    listBox.repaint();
}

void PresetBrowser::textEditorTextChanged(juce::TextEditor&)
{
    rebuildFilter();
    listBox.updateContent();
    listBox.repaint();
}

} // namespace as1
