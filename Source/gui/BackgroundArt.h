#pragma once

#include "AS1LookAndFeel.h"
#include "ImageBlur.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace as1
{

// Blurred backdrop photo behind the main content panels — built once per
// resize (real blur math, not a pre-baked asset) and blitted every paint().
// Mirrors FLEX.png: the soft-focus artwork shows plainly in the top bar and
// the bottom strip, while the central "panel band" — set from the editor's
// resized() via setPanelBand() — is covered by an opaque flat panel, so the
// controls sit on a calm surface with no photo bleeding through (FLEX's Edit
// panel is fully opaque).
class BackgroundArt : public juce::Component
{
public:
    BackgroundArt()
    {
        auto data = getSourceImageData();
        sourceImage = juce::ImageFileFormat::loadFrom(data.data, data.size);
    }

    // Vertical range (in local coords) of the translucent content panel that
    // sits between the top bar and the bottom photo strip, per FLEX.png.
    void setPanelBand(juce::Range<int> band)
    {
        panelBand = band;
        repaint();
    }

    void resized() override
    {
        rebuild();
    }

    void paint(juce::Graphics& g) override
    {
        g.setColour(AS1LookAndFeel::palette.bgBottom);
        g.fillRect(getLocalBounds());

        if (blurred.isValid())
            g.drawImageAt(blurred, 0, 0);

        // Mild darkening so light text/knobs stay readable over the photo's
        // bright areas, without greying the colours out.
        g.setColour(juce::Colours::black.withAlpha(0.18f));
        g.fillRect(getLocalBounds());

        if (! panelBand.isEmpty())
        {
            auto band = juce::Rectangle<int>(0, panelBand.getStart(), getWidth(), panelBand.getLength());

            // Opaque and flat, per FLEX — any gradient here reads as the
            // photo blur continuing into the panel. The photo shows only
            // above and below this band.
            g.setColour(AS1LookAndFeel::palette.bgTop);
            g.fillRect(band);

            // Hairline separators where the panel meets the photo, as in FLEX.
            g.setColour(juce::Colours::white.withAlpha(0.06f));
            g.fillRect(band.getX(), band.getY(), band.getWidth(), 1);
            g.fillRect(band.getX(), band.getBottom() - 1, band.getWidth(), 1);
        }

        // Feather the photo into the flat background at the very top and bottom
        // edges so the blurred artwork fades in/out instead of ending on a hard
        // line. This is a plain opaque-to-transparent overlay of the background
        // colour — it does NOT touch the image's own alpha (an earlier attempt
        // made the whole image translucent, which is what greyed everything out).
        const int fade = 60;
        const auto bg = AS1LookAndFeel::palette.bgBottom;

        juce::ColourGradient topFade(bg, 0.0f, 0.0f, bg.withAlpha(0.0f), 0.0f, (float) fade, false);
        g.setGradientFill(topFade);
        g.fillRect(0, 0, getWidth(), fade);

        juce::ColourGradient botFade(bg.withAlpha(0.0f), 0.0f, (float) (getHeight() - fade),
                                     bg, 0.0f, (float) getHeight(), false);
        g.setGradientFill(botFade);
        g.fillRect(0, getHeight() - fade, getWidth(), fade);
    }

private:
    struct RawData { const void* data; size_t size; };
    static RawData getSourceImageData();

    void rebuild()
    {
        int w = getWidth(), h = getHeight();
        if (w <= 0 || h <= 0 || !sourceImage.isValid())
        {
            blurred = {};
            return;
        }

        blurred = boxBlurImage(coverFit(sourceImage, w, h), 32);
    }

    static juce::Image coverFit(const juce::Image& src, int w, int h)
    {
        float srcAspect = static_cast<float>(src.getWidth()) / static_cast<float>(src.getHeight());
        float dstAspect = static_cast<float>(w) / static_cast<float>(h);

        juce::Rectangle<int> srcRect;
        if (srcAspect > dstAspect)
        {
            int newWidth = juce::roundToInt(src.getHeight() * dstAspect);
            int x = (src.getWidth() - newWidth) / 2;
            srcRect = { x, 0, newWidth, src.getHeight() };
        }
        else
        {
            int newHeight = juce::roundToInt(src.getWidth() / dstAspect);
            int y = (src.getHeight() - newHeight) / 2;
            srcRect = { 0, y, src.getWidth(), newHeight };
        }

        return src.getClippedImage(srcRect).rescaled(w, h, juce::Graphics::highResamplingQuality);
    }

    juce::Image sourceImage;
    juce::Image blurred;
    juce::Range<int> panelBand;
};

} // namespace as1
