---
phase: 25-native-mashup
plan: 02
subsystem: dsp
tags: [native, v4, presets, binary-data, nlohmann-json]

provides:
  - autoremix::dsp::presets::NativeRemixPreset
  - autoremix::dsp::presets::NativeMashupPreset
  - autoremix::dsp::presets::load_remix_presets()  → std::vector<NativeRemixPreset>
  - autoremix::dsp::presets::load_mashup_presets() → std::vector<NativeMashupPreset>
  - CMake target AutoRemixPresetsData (17 JSONs embedded in plugin binary)
affects: [27-01]

key-files:
  created:
    - plugin/include/dsp/NativePresetTypes.h
    - plugin/include/dsp/NativePresetLoaders.h
  modified:
    - plugin/CMakeLists.txt              (new juce_add_binary_data target + link)
    - plugin/src/AutoRemixProcessor.cpp  (2 smoke includes)

key-decisions:
  - "Built-ins embedded via juce_add_binary_data — no filesystem
     required for the default 9 + 8 presets. Mashup vs remix routing
     by filename suffix '_json' + a small allow-list of 8 known
     mashup resource names."
  - "User dirs: juce::File::userApplicationDataDirectory + autoremix/{modes,mashup}.
     Cross-platform via JUCE. Linux=~/.config/autoremix, macOS=Application Support,
     Windows=%APPDATA%. Slight macOS divergence from v3 (which used ~/.config
     everywhere); v4 follows the platform convention."
  - "Permissive JSON parsing: each preset field reads only known keys via
     nlohmann::json::value(key, default). Unknown keys ignored; malformed JSONs
     logged via juce::Logger and skipped (no throw across the boundary)."
  - "BinaryData symbols are forward-declared rather than #include 'BinaryData.h'
     to avoid ambiguity between AutoRemixFonts' BinaryData.h and AutoRemixPresetsData's
     BinaryData.h — both generated headers share the same 'BinaryData' namespace."
  - "De-dup by id, last-wins. Built-ins load first, user dir appended; the
     dedup_by_id helper preserves the order of last-seen ids."

duration: ~10min
started: 2026-05-20
completed: 2026-05-20
---

# Phase 25 Plan 02: NativePresetTypes + NativePresetLoaders

**Closes Phase 25.** 17 preset JSONs (9 remix + 8 mashup) now embedded in
the plugin binary. Native loaders return populated vectors merged with
any user-supplied JSONs from `~/.config/autoremix/{modes,mashup}` (or
platform equivalent). Build green, v3 main untouched.

## ACs

| AC   | Status |
|------|--------|
| AC-1: CMake target ships all 17 JSONs                       | Pass — AutoRemixPresetsData links 17 sources |
| AC-2: NativePresetTypes.h struct fields match v3            | Pass — both structs compile, fields mirror RemixPreset / MashupPreset |
| AC-3: Loaders return populated vectors + user dir + dedup   | Pass — binary iteration + user dir + last-wins dedup all in place |
| AC-4: Build green, v3 untouched                             | Pass — full cmake --build green, only pre-existing warnings |

## Files Changed

```
plugin/CMakeLists.txt                          +21 lines
plugin/include/dsp/NativePresetTypes.h         +59 lines (new)
plugin/include/dsp/NativePresetLoaders.h       +254 lines (new)
plugin/src/AutoRemixProcessor.cpp              +2 lines (smoke includes)
```

## Verify

```
$ grep -c "juce_add_binary_data(AutoRemixPresetsData" plugin/CMakeLists.txt
1
$ grep -c "NativeRemixPreset\|NativeMashupPreset" plugin/include/dsp/NativePresetTypes.h
3
$ grep -c "load_remix_presets\|load_mashup_presets\|namedResourceList" plugin/include/dsp/NativePresetLoaders.h
8
$ cmake --build build --parallel
[41/41] Linking CXX shared module plugin/AutoRemix_artefacts/Debug/VST3/AutoRemix.vst3/Contents/x86_64-linux/AutoRemix.so
```

## Next

Phase 25 complete (mashup engine + preset loaders both done). Next:
`/paul:plan 26-01` — export Demucs to ONNX. Then 26-02 wires
ONNX Runtime into a NativeDemucsSeparator.
