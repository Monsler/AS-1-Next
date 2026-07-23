#include "RasParser.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <fstream>
#include <map>

namespace as1
{

const std::vector<std::string>& RasParser::knownTags()
{
    static const std::vector<std::string> tags = {
        "list", "osci", "filt", "enve", "LFO ", "rout", "over", "flan", "shel", "gdel", "idel", "reve"
    };
    return tags;
}

uint16_t RasParser::readU16BE(const std::vector<uint8_t>& d, size_t off)
{
    return static_cast<uint16_t>((d[off] << 8) | d[off + 1]);
}

uint32_t RasParser::readU32BE(const std::vector<uint8_t>& d, size_t off)
{
    return (static_cast<uint32_t>(d[off]) << 24) | (static_cast<uint32_t>(d[off + 1]) << 16)
         | (static_cast<uint32_t>(d[off + 2]) << 8) | static_cast<uint32_t>(d[off + 3]);
}

int16_t RasParser::readI16BE(const std::vector<uint8_t>& d, size_t off)
{
    return static_cast<int16_t>(readU16BE(d, off));
}

double RasParser::readF64BE(const std::vector<uint8_t>& d, size_t off)
{
    uint64_t bits = 0;
    for (int i = 0; i < 8; ++i)
        bits = (bits << 8) | d[off + static_cast<size_t>(i)];
    double value;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

int RasParser::getChunkSize(const std::string& tagStr, const std::vector<uint8_t>& data, size_t offset)
{
    static const std::map<std::string, int> sizes = {
        { "osci", 54 }, { "filt", 54 }, { "enve", 46 }, { "LFO ", 30 }, { "rout", 16 },
        { "over", 46 }, { "flan", 46 }, { "shel", 38 }, { "gdel", 60 }, { "idel", 60 }, { "reve", 40 }
    };

    auto it = sizes.find(tagStr);
    if (it != sizes.end())
        return it->second;

    // Fallback scan: find where the next known chunk starts.
    size_t nextIdx = data.size();
    for (const auto& kt : knownTags())
    {
        if (offset + 4 >= data.size())
            continue;

        auto searchStart = data.begin() + static_cast<long>(offset + 4);
        auto found = std::search(searchStart, data.end(), kt.begin(), kt.end());
        if (found != data.end())
        {
            size_t idx = static_cast<size_t>(std::distance(data.begin(), found));
            if (idx < nextIdx)
                nextIdx = idx;
        }
    }
    return static_cast<int>(nextIdx - offset);
}

RasParser::Chunk RasParser::parseChunk(const std::vector<uint8_t>& data, size_t offset, size_t& outOffset)
{
    if (offset >= data.size())
    {
        outOffset = offset;
        return {};
    }

    std::string tagStr(reinterpret_cast<const char*>(data.data() + offset), 4);

    if (tagStr == "list")
    {
        uint32_t count = readU32BE(data, offset + 4);
        size_t currOffset = offset + 8;
        Chunk node;
        node.isList = true;
        node.tag = "list";
        for (uint32_t i = 0; i < count; ++i)
        {
            Chunk child = parseChunk(data, currOffset, currOffset);
            node.children.push_back(std::move(child));
        }
        outOffset = currOffset;
        return node;
    }

    int size = getChunkSize(tagStr, data, offset);
    if (size < 0)
        size = 0;

    Chunk leaf;
    leaf.tag = tagStr;
    size_t end = std::min(data.size(), offset + static_cast<size_t>(size));
    leaf.data.assign(data.begin() + static_cast<long>(offset), data.begin() + static_cast<long>(end));
    outOffset = offset + static_cast<size_t>(size);
    return leaf;
}

void RasParser::walkTree(const Chunk& node, std::vector<const Chunk*>& outChunks)
{
    if (node.isList)
    {
        for (const auto& child : node.children)
            walkTree(child, outChunks);
    }
    else if (!node.tag.empty())
    {
        outChunks.push_back(&node);
    }
}

static as1::Waveform waveformFromCode(uint8_t code)
{
    switch (code)
    {
        case 0: return as1::Waveform::Saw;
        case 1: return as1::Waveform::Triangle;
        case 2: return as1::Waveform::Square;
        case 3: return as1::Waveform::Sine;
        case 6: return as1::Waveform::Noise;
        default: return as1::Waveform::Unknown;
    }
}

static as1::FilterType filterTypeFromCode(uint8_t code)
{
    switch (code)
    {
        case 0: case 1: return as1::FilterType::Lowpass2Pole;
        case 2: case 9: return as1::FilterType::Lowpass4Pole;
        case 10: return as1::FilterType::Bandpass4Pole;
        case 12: return as1::FilterType::Highpass4Pole;
        default: return as1::FilterType::Unknown;
    }
}

static double roundN(double v, int ndigits)
{
    double mul = std::pow(10.0, ndigits);
    return std::round(v * mul) / mul;
}

// Envelope A/D/R values in a .ras file are normalised knob positions (~0..1.5),
// NOT seconds — and higher means FASTER. The original engine converts them via
// CAnalogConvert::RampSpeedToSeconds (RetroLib.dll), whose shape is
// seconds ∝ 1/exp(k·value) — an exponential where a high knob value gives a
// near-instant time and a low value a multi-second one. (The exact hardware
// constants are runtime globals, not recoverable from the static binary, so
// the endpoints below are fit to the musical range instead.) Treating the raw
// value as seconds — as the original Python port did — inverts this, giving
// nearly every preset a ~1s attack swell; that is the main reason percussive
// presets "don't play" and the 909 kick's pitch rises instead of dropping.
// EXACT curve recovered by running the original engine's own code: a 32-bit
// harness LoadLibrary'd RetroLib.dll and called CAnalogConvert::RampSpeedToSeconds
// across the input range (the curve lives in a CAnalogExp2Table filled at DLL
// init, unrecoverable statically). Result: seconds = 14.51247 * 2^(-10*v),
// floored at 0.01496 s (max speed). Higher knob value => faster, halving every
// 0.1. This replaces the earlier fitted approximation.
static double rampValueToSeconds(double value)
{
    double v = std::max(0.0, value);
    double t = 14.51247166 * std::exp2(-10.0 * v);
    return std::max(t, 0.01496096);
}

// EXACT curve from the same harness (CAnalogConvert::LFOSpeedToHertz): the
// reciprocal side of the same exp2 table — Hz = 0.0344531 * 2^(10*v), capped at
// 33.4203 Hz. Doubles every 0.1; a typical vibrato knob (~0.78) ≈ 6 Hz.
static double lfoRateToHz(double value)
{
    double v = std::max(0.0, value);
    double hz = 0.0344531 * std::exp2(10.0 * v);
    return std::min(hz, 33.4203);
}

static as1::LfoWaveform lfoWaveformFromCode(uint8_t code)
{
    switch (code)
    {
        case 1: return as1::LfoWaveform::Triangle;
        case 2: return as1::LfoWaveform::Sine;
        case 3: return as1::LfoWaveform::Square;
        case 4: return as1::LfoWaveform::SampleHold;
        case 5: return as1::LfoWaveform::Sawtooth;
        default: return as1::LfoWaveform::Triangle;
    }
}

// Decode a `rout` destination code into a runtime modulation target. Oscillator
// blocks are 100 (osc A), 105 (osc B), 110 (osc C) with sub-offsets 0 = freq,
// 2/3 = pulse width, 4 = level; osc C folds onto osc B (the engine has two
// oscillators). Filter block is 200 (cutoff) / 201 (resonance) / 205 (cutoff).
static as1::ModDest decodeDest(int d, int& oscIndex)
{
    oscIndex = 0;
    if (d == 0)   return as1::ModDest::Pitch;
    if (d == 1)   return as1::ModDest::Volume;
    if (d == 2)   return as1::ModDest::Pan;
    if (d >= 100 && d < 115)
    {
        int block = (d - 100) / 5; // 0,1,2 = osc A/B/C
        int sub = (d - 100) % 5;
        oscIndex = std::min(block, 2);
        if (sub <= 1) return as1::ModDest::OscFreq;
        if (sub <= 3) return as1::ModDest::OscPulse;
        return as1::ModDest::OscLevel;
    }
    if (d >= 200 && d < 210)
        return d == 201 ? as1::ModDest::FilterResonance : as1::ModDest::FilterCutoff;
    return as1::ModDest::None;
}

RasPreset RasParser::parseBytes(const std::vector<uint8_t>& data, const std::string& presetName)
{
    RasPreset preset;
    preset.name = presetName;

    if (data.size() < 62 || data[0] != 'd' || data[1] != 'a' || data[2] != 't' || data[3] != 'a')
    {
        preset.error = "Invalid .ras file format (missing 'data' header).";
        return preset;
    }

    uint16_t strLen = readU16BE(data, 60);
    size_t startOffset = 62 + static_cast<size_t>(strLen);

    std::vector<Chunk> topLevelChunks;
    std::vector<const Chunk*> flatChunks;
    size_t offset = startOffset;
    while (offset < data.size())
    {
        size_t nextOffset = offset;
        Chunk node = parseChunk(data, offset, nextOffset);
        topLevelChunks.push_back(std::move(node));
        if (nextOffset <= offset)
            break; // guard against zero-progress chunks
        offset = nextOffset;
    }
    for (const auto& node : topLevelChunks)
        walkTree(node, flatChunks);

    // 1. Oscillators
    for (const auto* c : flatChunks)
    {
        if (c->tag != "osci" || c->data.size() < 54)
            continue;
        const auto& cd = c->data;
        // osci doubles (per the editor's knob order): Fine@14, Random@22,
        // Symmetry@30, FM Amount@38, Volume@46. The original port mistakenly
        // read symmetry from @46 (the volume field) — for a Pulse oscillator
        // that set the duty near 1.0, collapsing the wave to DC (silence) and
        // leaving only noise/other oscillators audible (the 909 kick's "hiss").
        OscillatorConfig osc;
        osc.enabled = cd[4] != 0;
        osc.waveform = waveformFromCode(cd[7]);
        osc.coarseTune = readI16BE(cd, 12);
        osc.fineTune = std::round(readF64BE(cd, 14) * 100.0);
        double rawSym = std::clamp(readF64BE(cd, 30), -1.0, 1.0); // 0 = square
        osc.symmetry = std::round((0.5 + 0.5 * rawSym) * 100.0);  // -> 0..100 duty
        osc.volume = std::clamp(readF64BE(cd, 46), 0.0, 1.0);
        preset.oscillators.push_back(osc);
    }

    // 2. Filters (only enabled ones, matching parse_ras.py)
    for (const auto* c : flatChunks)
    {
        if (c->tag != "filt" || c->data.size() < 54)
            continue;
        const auto& cd = c->data;
        bool enabled = cd[4] != 0;
        if (!enabled)
            continue;
        FilterConfig filt;
        filt.enabled = true;
        filt.type = filterTypeFromCode(cd[7]);
        filt.polyModAmount = std::round(readF64BE(cd, 30) * 100.0);
        filt.cutoff = std::round(readF64BE(cd, 38) * 100.0);
        filt.resonance = std::round(readF64BE(cd, 46) * 100.0);
        preset.filters.push_back(filt);
    }

    // 3. Modulators (envelopes + LFOs, in file order).
    //
    // The hardware addresses modulators by a fixed source index: the first
    // modulator in the preset's modulator list is source 8, the next 9, and so
    // on, counting envelopes and LFOs together. A routing then says
    // "source S drives destination D by amount A". So which envelope is the
    // amplitude / filter / pitch envelope is decided by the routing matrix
    // (section 5), NOT by the order envelopes happen to appear in — an LFO can
    // sit between two envelopes and shift every later source index.
    //
    // We record every modulator here (envelopes carry their parsed ADSR; LFOs
    // are placeholders that still consume a source slot) so the routing pass
    // can map source index -> modulator.
    // Sources are indexed by the hardware's fixed scheme: source 8 = first
    // modulator in file order, 9 = next, etc., counting envelopes AND LFOs
    // together. We build `preset.mod.sources` in that exact order so a routing's
    // `source - 8` indexes straight into it. `modulators` mirrors it with the
    // parsed EnvelopeConfig kept for the amp/filter/pitch role resolution (GUI).
    struct Modulator { bool isEnvelope = false; EnvelopeConfig env; };
    std::vector<Modulator> modulators;
    for (const auto* c : flatChunks)
    {
        if (c->tag == "enve" && c->data.size() >= 46)
        {
            const auto& cd = c->data;
            double doubles[5];
            for (int i = 0; i < 5; ++i)
                doubles[i] = readF64BE(cd, 6 + static_cast<size_t>(i) * 8);

            Modulator m;
            m.isEnvelope = true;
            // Double order is [Attack, Decay, SustainLevel, SustainDecay, Release].
            // A/D/R are normalised knob values converted to seconds; SustainLevel is
            // 0..1. SustainDecay is a second decay (toward 0) that runs while the
            // note is held; a value of ~0 means "no sustain decay" (hold forever),
            // so we only convert it when it is meaningfully non-zero.
            m.env.attack = roundN(rampValueToSeconds(doubles[0]), 5);
            m.env.decay = roundN(rampValueToSeconds(doubles[1]), 5);
            m.env.sustain = roundN(doubles[2], 4);
            m.env.sustainDecay = doubles[3] > 0.001 ? roundN(rampValueToSeconds(doubles[3]), 5) : 0.0;
            m.env.release = roundN(rampValueToSeconds(doubles[4]), 5);
            modulators.push_back(m);

            ModSource src;
            src.isLfo = false;
            src.envAttack = m.env.attack;
            src.envDecay = m.env.decay;
            src.envSustain = m.env.sustain;
            src.envSustainDecay = m.env.sustainDecay;
            src.envRelease = m.env.release;
            preset.mod.sources.push_back(src);
        }
        else if (c->tag == "LFO " && c->data.size() >= 30)
        {
            const auto& cd = c->data;
            LfoConfig lfo;
            lfo.waveform = lfoWaveformFromCode(cd[7]);
            lfo.rateHz = lfoRateToHz(readF64BE(cd, 18));
            // doubles[@10] is the LFO Delay (fade-in) knob — normalised, higher =
            // faster onset; convert with the same ramp curve, treating ~0 as none.
            double delayKnob = readF64BE(cd, 10);
            lfo.delaySeconds = (delayKnob > 0.001 && delayKnob < 2.0)
                                 ? roundN(rampValueToSeconds(delayKnob), 5) : 0.0;

            modulators.push_back(Modulator{}); // non-envelope: consumes a source slot

            ModSource src;
            src.isLfo = true;
            src.lfo = lfo;
            preset.mod.sources.push_back(src);
        }
    }

    // 4. Effects
    for (const auto* c : flatChunks)
    {
        if ((c->tag != "gdel" && c->tag != "idel") || c->data.size() < 60)
            continue;
        const auto& cd = c->data;
        if (cd[4] == 0)
            continue;
        preset.effects.delayEnabled = true;
        preset.effects.delayTimeMs = std::round(readF64BE(cd, 6) * 1000.0);
        preset.effects.delayFeedback = std::round(readF64BE(cd, 14) * 100.0);
        break;
    }

    for (const auto* c : flatChunks)
    {
        if (c->tag != "reve" || c->data.size() < 40)
            continue;
        const auto& cd = c->data;
        if (cd[4] == 0)
            continue;
        preset.effects.reverbEnabled = true;
        preset.effects.reverbMix = std::round(readF64BE(cd, 6) * 100.0);
        break;
    }

    // 5. Routings
    for (const auto* c : flatChunks)
    {
        if (c->tag != "rout" || c->data.size() < 16)
            continue;
        const auto& cd = c->data;
        RoutingConfig r;
        r.source = readU16BE(cd, 4);
        r.destination = readU16BE(cd, 6);
        r.amount = readF64BE(cd, 8);
        preset.routings.push_back(r);
    }

    // 6. Resolve envelope roles from the routing matrix.
    //
    // Destination codes: 1 = Volume (amp), 200 = Filter 1 Cutoff, 0 = Pitch,
    // 100/105/110 = individual oscillator frequency (also pitch for our
    // purposes). For each role we take the first routing to that destination
    // whose source is an envelope modulator (LFO-sourced routings — e.g. a
    // pitch vibrato — are left for a future LFO implementation).
    constexpr int destVolume = 1;
    constexpr int destCutoff = 200;

    auto envelopeForDest = [&](std::initializer_list<int> dests, double& outAmount) -> const EnvelopeConfig*
    {
        for (const auto& r : preset.routings)
        {
            bool destMatch = false;
            for (int d : dests)
                if (static_cast<int>(r.destination) == d) { destMatch = true; break; }
            if (!destMatch || r.source < 8)
                continue;
            size_t mi = static_cast<size_t>(r.source) - 8;
            if (mi < modulators.size() && modulators[mi].isEnvelope)
            {
                outAmount = r.amount;
                return &modulators[mi].env;
            }
        }
        return nullptr;
    };

    double ampAmount = 1.0, filtAmount = 1.0, pitchAmount = 0.0;
    const EnvelopeConfig* ampEnv = envelopeForDest({ destVolume }, ampAmount);
    const EnvelopeConfig* filtEnv = envelopeForDest({ destCutoff }, filtAmount);
    const EnvelopeConfig* pitchEnv = envelopeForDest({ 0, 100, 105, 110 }, pitchAmount);

    // Collect envelope modulators for the positional fallback used when a
    // preset has no explicit routing for a role.
    std::vector<const EnvelopeConfig*> envList;
    for (const auto& m : modulators)
        if (m.isEnvelope)
            envList.push_back(&m.env);

    preset.ampEnvelope = ampEnv ? *ampEnv
                                : (!envList.empty() ? *envList[0] : EnvelopeConfig{});
    preset.filterEnvelope = filtEnv ? *filtEnv
                                    : (envList.size() >= 2 ? *envList[1] : EnvelopeConfig{});
    preset.filterEnvAmount = filtEnv ? filtAmount : 1.0;

    if (pitchEnv != nullptr)
    {
        preset.pitchEnvelope = *pitchEnv;
        preset.pitchEnvAmount = pitchAmount;
    }

    // 7. Build the general runtime modulation matrix. EVERY routing whose source
    // is a modulator (index >= 8, envelope OR LFO) and whose destination decodes
    // to a supported target becomes a ModRouting into preset.mod.sources. This is
    // what makes multi-envelope percussion (e.g. the 909 snare's per-oscillator
    // decays) render correctly, instead of collapsing to fixed amp/filter/pitch
    // roles. The role resolution above is kept only to populate the GUI's flat
    // APVTS envelopes.
    for (const auto& r : preset.routings)
    {
        if (r.source < 8)
            continue; // sources 0..4 (note/velocity/AT/controllers) not modelled yet
        size_t si = static_cast<size_t>(r.source) - 8;
        if (si >= preset.mod.sources.size())
            continue;

        int oscIndex = 0;
        ModDest dest = decodeDest(static_cast<int>(r.destination), oscIndex);
        if (dest == ModDest::None || std::abs(r.amount) < 1.0e-6)
            continue;

        ModRouting mr;
        mr.sourceIndex = static_cast<int>(si);
        mr.dest = dest;
        mr.oscIndex = oscIndex;
        mr.amount = r.amount;
        preset.mod.routings.push_back(mr);
    }

    preset.valid = true;
    return preset;
}

RasPreset RasParser::parseFile(const std::string& path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open())
    {
        RasPreset preset;
        preset.error = "File '" + path + "' does not exist.";
        return preset;
    }

    std::vector<uint8_t> data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    // Derive preset name from the filename, stripping a ".ras" extension and
    // an optional "NNN " numeric prefix (mirrors parse_ras.py).
    std::string filename = path;
    auto slashPos = filename.find_last_of("/\\");
    if (slashPos != std::string::npos)
        filename = filename.substr(slashPos + 1);

    std::string presetName = filename;
    if (presetName.size() > 4)
    {
        std::string lower = presetName;
        for (auto& ch : lower) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        if (lower.size() >= 4 && lower.compare(lower.size() - 4, 4, ".ras") == 0)
            presetName = presetName.substr(0, presetName.size() - 4);
    }
    if (presetName.size() > 4 && std::isdigit(static_cast<unsigned char>(presetName[0]))
        && std::isdigit(static_cast<unsigned char>(presetName[1])) && std::isdigit(static_cast<unsigned char>(presetName[2]))
        && presetName[3] == ' ')
    {
        presetName = presetName.substr(4);
    }

    return parseBytes(data, presetName);
}

} // namespace as1
