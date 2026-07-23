#include "BackgroundArt.h"
#include "BinaryData.h"

namespace as1
{

BackgroundArt::RawData BackgroundArt::getSourceImageData()
{
    return { BinaryData::background_processed_png, static_cast<size_t>(BinaryData::background_processed_pngSize) };
}

} // namespace as1
