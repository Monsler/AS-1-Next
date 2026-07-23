# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

AS-1 Next is a JUCE-based audio plugin (VST3 + Standalone): a software recreation of the
Alesis Andromeda A6-derived "Retro AS-1" synth voice, capable of loading its factory `.ras`
preset bank (bundled in `RetroAS1/`).

Critically, the DSP and preset-parsing code (`Source/dsp/`, `Source/presets/`) is a **literal
C++ port of a Python reference implementation** (`metro-as1/play_ras.py` and
`metro-as1/parse_ras.py`, not in this repo). File-level comments throughout the codebase call
out which Python function/behavior a given class mirrors. When touching DSP or the `.ras`
parser, preserve behavioral parity with that reference rather than "improving" the algorithm —
e.g. `Adsr` is a literal-segment (not exponential) envelope, `RasParser` reads a big-endian
binary chunk tree matching the original hardware format, and `DelayEffect`'s per-sample
feedback/mix math is intentionally identical to the Python version (only the ring-buffer
sizing differs, since the plugin needs to support live delay-time changes).

## Build

```
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build                 # or: ninja -C build
```

JUCE 8.0.15 is fetched automatically via CMake `FetchContent` (git) on first configure — no
system JUCE install needed. C++ standard is C++26.

Useful ninja targets: `AS1Next_VST3`, `AS1Next_Standalone`. Build artifacts land in
`build/AS1Next_artefacts/Debug/{VST3,Standalone}/`. Run the Standalone artifact directly to
exercise the plugin without a DAW.

There are no automated tests in this repo currently.

## Architecture

**Namespace:** everything lives under `as1::`.

**Parameter system is the spine of the plugin.** `Source/dsp/Parameters.h` is the single
source of truth: it defines every `ParamIDs::*` string constant and builds the
`AudioProcessorValueTreeState` layout (`createParameterLayout()`). Both the DSP
(`SynthVoice`) and the GUI (`PluginEditor`) read/write through the processor's `apvts` —
there is no separate parameter struct passed around. When adding a parameter, it's added in
exactly one place (`Parameters.h`) and then wired up independently in `SynthVoice::startNote`
and in `PluginEditor`'s knob/combo construction.

**Audio path** (`Source/PluginProcessor.cpp`):
`juce::Synthesiser` (16 `SynthVoice`s + one shared `SynthSound`) renders into the buffer, then
a stereo pair of `DelayEffect`s and a master gain stage are applied per-sample directly in
`processBlock` (not via `juce::dsp::ProcessorChain`).

**`SynthVoice`** (`Source/dsp/SynthVoice.cpp`) snapshots *all* oscillator/filter/envelope
parameters once at `startNote` (matching the Python reference's `Voice.__init__` snapshotting
a preset) — parameter changes during a held note do not affect that voice until the next
note-on. Per-sample synthesis (`renderOneSample`) mirrors the Python `Voice.tick()`: up to two
detunable oscillators (Saw/Triangle/Square/Sine/Noise) are mixed and run through
`BiquadLowpass`, whose cutoff is exponentially modulated by the filter envelope
(`filtEnvAmount` sets modulation depth in octaves).

**Presets** (`Source/presets/`):
- `RasParser` parses the binary `.ras` chunk format into a `RasPreset` (plain data struct,
  independent of JUCE parameter types).
- `PresetManager` scans `AS1_PRESET_DIR` (a compile-time define set by CMake to
  `RetroAS1/`, see `CMakeLists.txt`) for `*.ras` files, and on `loadPresetByIndex` parses one
  and pushes its fields into the APVTS via `setValueNotifyingHost` — this is the only bridge
  between the `.ras` file format and the live parameter state. GUI and DSP never talk to
  `RasParser`/`RasPreset` directly.
- Preset directory paths may contain non-ASCII characters; `AS1_PRESET_DIR` is decoded via
  `juce::CharPointer_UTF8` rather than JUCE's ASCII-assuming `juce::String(const char*)` ctor.

**GUI** (`Source/gui/`, `Source/PluginEditor.cpp`):
- `SectionPanel` is the reusable titled/bordered group box (optionally with a power toggle)
  that all knob clusters (OSC A/B, envelopes, filter, delay, master) are built from.
- `PresetBrowser` is the searchable sidebar list over the scanned `.ras` files.
- `BackgroundArt` builds a real-time dual-radius box-blurred, edge-feathered backdrop from a
  bundled source image (not a pre-baked blurred asset) — the blur is recomputed in `resized()`.
- `AS1LookAndFeel` centralizes the color palette (`AS1LookAndFeel::palette`) and custom
  control drawing; reference the existing palette rather than hardcoding colors elsewhere.
- `FLEX.png` (repo root) is the visual reference mockup several GUI comments point back to —
  check it when making layout/styling decisions.
- Editor tabs (`Tab::Main/Modulation/Effects/Global`) are mostly scaffolding today; only
  `Main` is fully built out, `Modulation` is a placeholder.

**Assets:** icons/fonts are compiled into the binary via `juce_add_binary_data` (target
`AS1NextAssets` in `CMakeLists.txt`), not loaded from disk at runtime.
