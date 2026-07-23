#pragma once

#include "RasPreset.h"
#include <cstdint>
#include <vector>

namespace as1
{

// Literal C++ port of metro-as1/parse_ras.py — reads a Retro AS-1 .ras preset
// file and maps its binary chunk tree onto a RasPreset.
class RasParser
{
public:
    static RasPreset parseFile(const std::string& path);
    static RasPreset parseBytes(const std::vector<uint8_t>& data, const std::string& presetName);

private:
    struct Chunk
    {
        std::string tag;
        std::vector<uint8_t> data; // only for leaf chunks
        std::vector<Chunk> children; // only for "list" chunks
        bool isList = false;
    };

    static int getChunkSize(const std::string& tagStr, const std::vector<uint8_t>& data, size_t offset);
    static Chunk parseChunk(const std::vector<uint8_t>& data, size_t offset, size_t& outOffset);
    static void walkTree(const Chunk& node, std::vector<const Chunk*>& outChunks);

    // Big-endian primitive readers (mirrors Python's struct.unpack(">...")).
    static uint16_t readU16BE(const std::vector<uint8_t>& d, size_t off);
    static uint32_t readU32BE(const std::vector<uint8_t>& d, size_t off);
    static int16_t readI16BE(const std::vector<uint8_t>& d, size_t off);
    static double readF64BE(const std::vector<uint8_t>& d, size_t off);

    static const std::vector<std::string>& knownTags();
};

} // namespace as1
