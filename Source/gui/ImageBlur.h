#pragma once

#include <juce_graphics/juce_graphics.h>

namespace as1
{

// Cheap box blur (three passes ~= gaussian) over an ARGB juce::Image. Used to
// build cached backdrop art once per resize — never called from paint().
inline juce::Image boxBlurImage(const juce::Image& source, int radius)
{
    if (radius <= 0)
        return source;

    juce::Image img = source.convertedToFormat(juce::Image::ARGB);
    const int w = img.getWidth();
    const int h = img.getHeight();
    if (w <= 0 || h <= 0)
        return img;

    juce::Image::BitmapData bmp(img, juce::Image::BitmapData::readWrite);

    std::vector<uint8_t> row(static_cast<size_t>(w) * 4);
    std::vector<uint8_t> col(static_cast<size_t>(h) * 4);

    auto boxBlurPass = [&](int passes)
    {
        for (int pass = 0; pass < passes; ++pass)
        {
            // Horizontal pass.
            for (int y = 0; y < h; ++y)
            {
                auto* line = bmp.getLinePointer(y);
                std::memcpy(row.data(), line, row.size());

                for (int x = 0; x < w; ++x)
                {
                    int sum[4] = { 0, 0, 0, 0 };
                    int count = 0;
                    for (int k = -radius; k <= radius; ++k)
                    {
                        int xx = juce::jlimit(0, w - 1, x + k);
                        for (int c = 0; c < 4; ++c)
                            sum[c] += row[static_cast<size_t>(xx) * 4 + static_cast<size_t>(c)];
                        ++count;
                    }
                    for (int c = 0; c < 4; ++c)
                        line[x * 4 + c] = static_cast<uint8_t>(sum[c] / count);
                }
            }

            // Vertical pass.
            for (int x = 0; x < w; ++x)
            {
                for (int y = 0; y < h; ++y)
                {
                    auto* px = bmp.getPixelPointer(x, y);
                    for (int c = 0; c < 4; ++c)
                        col[static_cast<size_t>(y) * 4 + static_cast<size_t>(c)] = px[c];
                }
                for (int y = 0; y < h; ++y)
                {
                    int sum[4] = { 0, 0, 0, 0 };
                    int count = 0;
                    for (int k = -radius; k <= radius; ++k)
                    {
                        int yy = juce::jlimit(0, h - 1, y + k);
                        for (int c = 0; c < 4; ++c)
                            sum[c] += col[static_cast<size_t>(yy) * 4 + static_cast<size_t>(c)];
                        ++count;
                    }
                    auto* px = bmp.getPixelPointer(x, y);
                    for (int c = 0; c < 4; ++c)
                        px[c] = static_cast<uint8_t>(sum[c] / count);
                }
            }
        }
    };

    boxBlurPass(2);
    return img;
}

} // namespace as1
