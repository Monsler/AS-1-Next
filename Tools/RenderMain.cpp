// Headless offline audio-render harness. For each preset name (or index) passed
// on the command line — or all presets when given none — it loads the patch,
// plays a middle-C note for ~1.5 s (held ~1 s then released), renders through
// the full processor and reports level and a rough spectral centroid so a
// regression in the DSP / parser shows up as silence, DC, blow-ups or a wildly
// wrong brightness. Optionally writes a WAV per preset with --wav <dir>.

#include "PluginProcessor.h"
#include "presets/PresetManager.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

using namespace as1;

int main(int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    std::vector<std::string> wanted;
    juce::String wavDir;
    for (int i = 1; i < argc; ++i)
    {
        std::string a = argv[i];
        if (a == "--wav" && i + 1 < argc) { wavDir = argv[++i]; continue; }
        wanted.push_back(a);
    }

    const double sr = 44100.0;
    const int block = 512;

    // Enumeration processor (names/count only).
    AS1AudioProcessor enumProc;
    int total = enumProc.getPresetManager().getNumPresets();

    auto matches = [&](int idx) -> bool
    {
        if (wanted.empty())
            return true;
        auto name = enumProc.getPresetManager().getPresetDisplayName(idx).toLowerCase();
        for (auto& w : wanted)
        {
            juce::String ws = juce::String(w).toLowerCase();
            if (ws.containsOnly("0123456789") && ws.getIntValue() == idx)
                return true;
            if (name.contains(ws))
                return true;
        }
        return false;
    };

    std::cout << juce::String("preset").paddedRight(' ', 26)
              << "  rms      peak     centroidHz  status\n";

    for (int idx = 0; idx < total; ++idx)
    {
        if (!matches(idx))
            continue;

        // Fresh processor per preset so reverb/release tails from one patch can't
        // bleed into the next patch's measurement.
        AS1AudioProcessor proc;
        auto& pm = proc.getPresetManager();
        proc.setPlayConfigDetails(0, 2, sr, block);
        proc.prepareToPlay(sr, block);
        pm.loadPresetByIndex(idx);

        if (std::getenv("AS1_DUMP"))
        {
            auto snap = proc.getPresetSnapshot();
            if (snap)
            {
                std::cerr << "--- " << enumProc.getPresetManager().getPresetDisplayName(idx) << " sources=" << snap->mod.sources.size()
                          << " filters=" << snap->filters.size() << " oscs=" << snap->oscillators.size() << "\n";
                for (size_t si = 0; si < snap->mod.sources.size(); ++si)
                {
                    auto& s = snap->mod.sources[si];
                    if (s.isLfo)
                        std::cerr << "   SRC[" << si << "] LFO\n";
                    else
                        std::cerr << "   SRC[" << si << "] env A=" << s.envAttack << " D=" << s.envDecay
                                  << " S=" << s.envSustain << " SD=" << s.envSustainDecay
                                  << " R=" << s.envRelease << "\n";
                }
                for (size_t oi = 0; oi < snap->oscillators.size(); ++oi)
                {
                    auto& o = snap->oscillators[oi];
                    std::cerr << "   OSC[" << oi << "] en=" << o.enabled << " wf=" << (int) o.waveform
                              << " coarse=" << o.coarseTune << " fine=" << o.fineTune
                              << " sym=" << o.symmetry << " vol=" << o.volume
                              << " kt=" << o.keyTrack << "\n";
                }
                for (size_t fi = 0; fi < snap->filters.size(); ++fi)
                {
                    auto& f = snap->filters[fi];
                    std::cerr << "   FILT[" << fi << "] en=" << f.enabled << " type=" << (int) f.type
                              << " cutoff=" << f.cutoff << " reso=" << f.resonance
                              << " spread=" << f.spread << " od=" << f.overdrive
                              << " in=" << f.inputOsc[0] << f.inputOsc[1] << f.inputOsc[2]
                              << " chain=" << f.inputOtherFilter << "\n";
                }
                for (auto& r : snap->mod.routings)
                    std::cerr << "   src=" << r.sourceIndex << " builtin=" << r.builtinSource
                              << " dest=" << (int) r.dest << " osc=" << r.oscIndex
                              << " filt=" << r.filterIndex << " modIdx=" << r.modIndex
                              << " amt=" << r.amount << "\n";
            }
        }

        juce::AudioBuffer<float> buffer(2, block);
        std::vector<float> mono;

        double holdSec = std::getenv("AS1_HOLD") ? atof(std::getenv("AS1_HOLD")) : 1.0;
        double tailSec = std::getenv("AS1_TAIL") ? atof(std::getenv("AS1_TAIL")) : 0.6;
        const int noteFrames = static_cast<int>(holdSec * sr);
        const int tailFrames = static_cast<int>(tailSec * sr);
        int rendered = 0;
        bool released = false;

        while (rendered < noteFrames + tailFrames)
        {
            juce::MidiBuffer midi;
            if (rendered == 0)
                midi.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8) 100), 0);
            if (!released && rendered >= noteFrames)
            {
                midi.addEvent(juce::MidiMessage::noteOff(1, 60), 0);
                released = true;
            }
            buffer.clear();
            proc.processBlock(buffer, midi);
            for (int s = 0; s < block; ++s)
                mono.push_back(0.5f * (buffer.getSample(0, s) + buffer.getSample(1, s)));
            rendered += block;
        }

        // Level metrics.
        double sumSq = 0.0, peak = 0.0, dc = 0.0;
        for (float v : mono) { sumSq += (double) v * v; peak = std::max(peak, (double) std::abs(v)); dc += v; }
        double rms = std::sqrt(sumSq / mono.size());
        dc /= (double) mono.size();

        // Rough spectral centroid via zero-crossing-weighted energy: use a simple
        // DFT-free estimate — average absolute first difference vs signal energy.
        double diffSq = 0.0;
        for (size_t i = 1; i < mono.size(); ++i)
        {
            double d = (double) mono[i] - mono[i - 1];
            diffSq += d * d;
        }
        // centroid ≈ (sr / 2π) * sqrt(Σdiff² / Σsig²)
        double centroid = (rms > 1e-9)
            ? (sr / (2.0 * juce::MathConstants<double>::pi)) * std::sqrt(diffSq / (sumSq > 0 ? sumSq : 1))
            : 0.0;

        const char* status = "ok";
        if (rms < 1e-4) status = "SILENT";
        else if (std::abs(dc) > 0.2 * peak && peak > 1e-3) status = "DC-BIAS";
        else if (!std::isfinite(peak) || peak > 4.0) status = "BLOWUP";

        std::cout << enumProc.getPresetManager().getPresetDisplayName(idx).paddedRight(' ', 26) << "  "
                  << juce::String(rms, 4) << "  "
                  << juce::String(peak, 4) << "  "
                  << juce::String(centroid, 0).paddedLeft(' ', 8) << "  "
                  << status << "\n";

        if (wavDir.isNotEmpty())
        {
            juce::File dir(wavDir);
            dir.createDirectory();
            auto wav = dir.getChildFile(enumProc.getPresetManager().getPresetDisplayName(idx) + ".wav");
            juce::WavAudioFormat fmt;
            if (auto os = wav.createOutputStream())
            {
                std::unique_ptr<juce::AudioFormatWriter> w(
                    fmt.createWriterFor(os.release(), sr, 1, 16, {}, 0));
                if (w)
                {
                    juce::AudioBuffer<float> out(1, (int) mono.size());
                    for (int i = 0; i < (int) mono.size(); ++i)
                        out.setSample(0, i, mono[(size_t) i]);
                    w->writeFromAudioSampleBuffer(out, 0, out.getNumSamples());
                }
            }
        }
    }

    return 0;
}
