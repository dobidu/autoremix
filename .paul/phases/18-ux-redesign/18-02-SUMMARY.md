---
phase: 18-ux-redesign
plan: 02
status: complete
date: 2026-05-16
---

# 18-02 Summary — ScreenContext + ScreenBase + ScreenEmpty

## What was built
- `plugin/include/ScreenContext.h` (new file):
  - `ScreenId` enum: Empty, Separating, StemsReady, ModeParams, Render
  - `ScreenContext` shared mutable state bag: file_path, detected_bpm, detected_key, stems, output_path, presets, separators, per-stem gains/mute/solo, remix params (target_bpm, pitch_semi, reverb_mix, chop_beats, chop_mode_idx), navigate and set_status callbacks
- `plugin/include/ScreenBase.h` (new file):
  - `ScreenBase : public juce::Component` with ScreenContext& ref, virtual onEnter()/onExit()
- `plugin/include/ScreenEmpty.h` (new file):
  - FileDragAndDropTarget + ChangeListener
  - Drag-drop zone with ACCENT border on hover; waveform via AudioThumbnail
  - Analysis text row: detected BPM + key from ctx_
  - Load File button triggers loadFile_fn_ callback; Prepare Stems button navigates to Separating
  - `fileWasSelected(juce::File)` updates ctx_.file_path, sets thumbnail, calls runAnalysis() on background thread
  - `runAnalysis()` calls analyze_fn_, updates ctx_.detected_bpm/detected_key via callAsync

## Decisions
- ScreenContext is owned by the editor and referenced by all screens (not copied)
- navigate and set_status are std::function callbacks wired by the editor
- ScreenEmpty owns its own AudioThumbnail/FormatManager/ThumbnailCache (not shared with editor)
- analyze_fn_ takes juce::String path, returns FileAnalysis struct (same as AudioBridge::analyzeFile signature)

## Acceptance criteria
- [x] ScreenContext.h and ScreenBase.h created
- [x] ScreenEmpty.h created with full drag-drop, waveform, analysis flow
- [x] Build clean
