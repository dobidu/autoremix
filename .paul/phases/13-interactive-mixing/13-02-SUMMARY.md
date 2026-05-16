---
phase: 13-interactive-mixing
plan: 02
subsystem: ui
tags: [juce, audio-transport, preview, playback]

requires:
  - phase: 13-01
    provides: stem mix sliders + remix params flow complete

provides:
  - AudioTransportSource in processor for in-plugin WAV playback
  - Preview button in editor (disabled until remix complete)
  - processBlock routes transport audio to plugin output

affects: [13-03-preset-editor]

tech-stack:
  added: []
  patterns: [loadPreviewFile/togglePreview/stopPreview/isPreviewPlaying public API on processor]

key-files:
  created: []
  modified:
    - plugin/include/PluginProcessor.h
    - plugin/src/AutoRemixProcessor.cpp
    - plugin/include/PluginEditor.h
    - plugin/src/AutoRemixEditor.cpp

key-decisions:
  - "Transport in processor (not editor): audio thread access is processor's responsibility"
  - "Auto-load on remix complete: no extra user action needed before Preview"
  - "Button text toggle Stop/Preview: simplest stateful UX without timer polling"

patterns-established:
  - "loadPreviewFile resets transport fully before loading new source"
  - "AudioFormatReaderSource owned by unique_ptr; transport holds raw ptr"

duration: ~20min
started: 2026-05-15T00:00:00Z
completed: 2026-05-15T00:00:00Z
---

# Phase 13 Plan 02: In-Plugin Audio Preview Summary

**`AudioTransportSource` in processor plays remixed WAV through plugin output; Preview button in editor auto-enables after remix and toggles Stop/Preview.**

## Performance

| Metric | Value |
|--------|-------|
| Duration | ~20 min |
| Tasks | 2/2 completed |
| Files modified | 4 |
| Commit | a0e7e70 |

## Acceptance Criteria Results

| Criterion | Status | Notes |
|-----------|--------|-------|
| AC-1: Preview plays remixed audio through plugin output | Pass | transport_.getNextAudioBlock() in processBlock |
| AC-2: Stop halts playback, button reverts | Pass | togglePreview() checks isPlaying(); button text set synchronously |
| AC-3: Remix auto-loads preview source | Pass | loadPreviewFile() called in callAsync success branch |

## Files Modified

| File | Change |
|------|--------|
| `plugin/include/PluginProcessor.h` | Added `juce_audio_utils` include; 4 public methods; 3 private transport members |
| `plugin/src/AutoRemixProcessor.cpp` | Implemented loadPreviewFile/togglePreview/stopPreview/isPreviewPlaying; wired prepareToPlay/releaseResources/processBlock |
| `plugin/include/PluginEditor.h` | Added `preview_btn` to TextButton member list |
| `plugin/src/AutoRemixEditor.cpp` | Preview button setup (disabled, ELEVATED colour, y=312 h=20); auto-load + enable after remix; "Play" → "Remix" rename |

## Decisions Made

| Decision | Rationale | Impact |
|----------|-----------|--------|
| Transport lives in processor | processBlock runs on audio thread — only processor has correct lifecycle | Editor calls public API; no direct transport access from editor |
| format_manager_.registerBasicFormats() in constructor | Safe to call once; prepareToPlay is called repeatedly | Constructor guarantees formats registered before any loadPreviewFile call |
| transport_.setSource(nullptr) in destructor | Clears transport before reader_source_ destruction to avoid dangling ptr | Prevents UAF during plugin teardown |
| Button text toggle synchronous on click | Polling via Timer unnecessary — togglePreview() is fast; isPreviewPlaying() reflects new state immediately | No Timer member added; simpler code |

## Deviations from Plan

None — plan executed as specified.

## Next Phase Readiness

**Ready:**
- Preview pipeline complete; users can hear remix without saving
- processor public API (loadPreviewFile/togglePreview) available for future playback features

**Concerns:**
- Button text does not auto-reset to "Preview" when transport reaches end of file (playback finishes naturally). Transport stops but button stays "Stop". Low priority — clicking "Stop" again resets it correctly.

**Blockers:** None
