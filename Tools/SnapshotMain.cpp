// Offscreen GUI snapshot tool: instantiates the processor + editor without
// showing a window, optionally loads a preset and switches tabs, and writes
// PNG snapshots. Used to eyeball layout changes headlessly:
//   AS1Snapshot <outDir> [presetIndex]
#include "PluginProcessor.h"

#include <juce_gui_basics/juce_gui_basics.h>

static juce::TextButton* findButton(juce::Component* c, const juce::String& text)
{
    if (auto* b = dynamic_cast<juce::TextButton*>(c))
        if (b->getButtonText() == text)
            return b;
    for (auto* child : c->getChildren())
        if (auto* found = findButton(child, text))
            return found;
    return nullptr;
}

int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    juce::File outDir(argc > 1 ? juce::String(juce::CharPointer_UTF8(argv[1]))
                               : juce::File::getCurrentWorkingDirectory().getFullPathName());
    outDir.createDirectory();

    as1::AS1AudioProcessor proc;
    proc.prepareToPlay(44100.0, 512);
    if (argc > 2)
        proc.getPresetManager().loadPresetByIndex(juce::String(argv[2]).getIntValue());

    std::unique_ptr<juce::AudioProcessorEditor> editor(proc.createEditor());
    editor->setSize(1180, 820);

    auto snap = [&editor, &outDir](const juce::String& name)
    {
        // Let pending timers/async updates (preset list, mod-matrix refresh) run.
        juce::MessageManager::getInstance()->runDispatchLoopUntil(400);
        auto img = editor->createComponentSnapshot(editor->getLocalBounds());
        juce::File f = outDir.getChildFile(name + ".png");
        f.deleteFile();
        juce::FileOutputStream os(f);
        juce::PNGImageFormat().writeImageToStream(img, os);
        std::cout << f.getFullPathName() << std::endl;
    };

    snap("main");
    for (const char* tab : { "Modulation", "Effects", "Global" })
    {
        if (auto* btn = findButton(editor.get(), tab))
            btn->onClick();
        snap(juce::String(tab).toLowerCase());

        // On the Modulation tab, also exercise the editor: the first "Add"
        // button is the routings one — click it and snapshot the result.
        if (juce::String(tab) == "Modulation")
        {
            if (auto* add = findButton(editor.get(), "Add"))
                add->onClick();
            snap("modulation_add");
        }
    }

    editor = nullptr;

    // --- audio checks: for every macro knob, compare a rendered note with the
    // knob at default vs turned, and require an audible difference. ---
    auto set = [&proc](const char* id, float v)
    {
        auto* par = proc.apvts.getParameter(id);
        par->setValueNotifyingHost(par->convertTo0to1(v));
    };
    auto reset = [&proc](const char* id)
    {
        auto* par = proc.apvts.getParameter(id);
        par->setValueNotifyingHost(par->getDefaultValue());
    };

    auto renderNote = [&proc]
    {
        std::vector<float> out;
        juce::AudioBuffer<float> buf(2, 512);
        proc.keyboardState.noteOn(1, 60, 0.9f);
        for (int b = 0; b < 40; ++b) // ~0.46 s held
        {
            juce::MidiBuffer midi;
            proc.processBlock(buf, midi);
            out.insert(out.end(), buf.getReadPointer(0), buf.getReadPointer(0) + 512);
        }
        proc.keyboardState.noteOff(1, 60, 0.0f);
        for (int b = 0; b < 60; ++b) // ~0.7 s of release/FX tail (captured too)
        {
            juce::MidiBuffer midi;
            proc.processBlock(buf, midi);
            out.insert(out.end(), buf.getReadPointer(0), buf.getReadPointer(0) + 512);
        }
        for (int b = 0; b < 80; ++b) // drain any remaining tail between tests
        {
            juce::MidiBuffer midi;
            proc.processBlock(buf, midi);
        }
        return out;
    };

    auto baseline = renderNote();
    struct Test { const char* name; std::vector<std::pair<const char*, float>> params; };
    const Test tests[] = {
        { "Vibrato",  { { as1::ParamIDs::vibratoDepth, 100.0f } } },
        { "Vib-rate", { { as1::ParamIDs::vibratoDepth, 100.0f }, { as1::ParamIDs::vibratoRate, 1.0f } } },
        { "Cutoff",   { { as1::ParamIDs::filterCutoff, 8.0f } } },
        { "Res",      { { as1::ParamIDs::filterResonance, 95.0f } } },
        { "Attack",   { { as1::ParamIDs::ampAttack, 2.0f } } },
        { "Release",  { { as1::ParamIDs::ampRelease, 5.0f } } },
        { "Reverb",   { { as1::ParamIDs::reverbOn, 1.0f }, { as1::ParamIDs::reverbMix, 90.0f } } },
        { "Gain",     { { as1::ParamIDs::masterGain, 0.1f } } },
    };

    bool allPass = true;
    for (const auto& t : tests)
    {
        for (auto& pv : t.params) set(pv.first, pv.second);
        auto out = renderNote();
        for (auto& pv : t.params) reset(pv.first);

        double num = 0.0, den = 1.0e-9;
        for (size_t i = 0; i < baseline.size() && i < out.size(); ++i)
        {
            num += std::abs((double) out[i] - (double) baseline[i]);
            den += std::abs((double) baseline[i]);
        }
        double d = num / den;
        bool pass = d > 0.05;
        allPass = allPass && pass;
        std::cout << (pass ? "PASS " : "FAIL ") << t.name << " diff=" << d << std::endl;
    }
    return allPass ? 0 : 1;
}
