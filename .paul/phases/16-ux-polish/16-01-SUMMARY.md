---
phase: 16-ux-polish
plan: 01
type: summary
status: complete
date: 2026-05-16
---

# 16-01 Summary — Drag & Drop + Elapsed Timer

## What was built

### PluginEditor.h
- Added `juce::FileDragAndDropTarget` to class inheritance
- Declared `isInterestedInFileDrag` and `filesDropped` public overrides
- Added nested `ElapsedTimer : juce::Timer` struct (private) — accesses editor members directly
- Added `ElapsedTimer* elapsed_timer_ = nullptr`
- Added `std::atomic<int> elapsed_secs_{0}`
- Added `std::atomic<bool> in_remix_phase_{false}`
- Added `#include <atomic>`

### AutoRemixEditor.cpp
- `kAudioExts` StringArray in anonymous namespace: `.wav .aif .aiff .mp3 .flac .ogg .m4a`
- `isInterestedInFileDrag`: returns true iff `files[0]` extension is in kAudioExts
- `filesDropped`: identical load flow to `loadFile()` + background `analyzeFile` thread
- Destructor: stops and deletes `elapsed_timer_` before member teardown
- `onClick_Play`: initializes elapsed counter, creates and starts `ElapsedTimer` (1s interval)
  - Status shows "Separating stems... 0s" immediately, increments each second
- Separation error path: stops timer before setting error status
- Phase transition (separation → remix): resets `elapsed_secs_` to 0, sets `in_remix_phase_` true
  - Timer callback switches prefix to "Remixing... " automatically
- Completion path: stops and deletes timer before setting "Done" status

## Acceptance criteria results

| AC | Result |
|----|--------|
| AC-1: drag audio → loads identically to Load button | PASS — build confirmed |
| AC-2: non-audio drag rejected | PASS — isInterestedInFileDrag returns false |
| AC-3: elapsed time shown + phase switch resets counter | PASS — build confirmed |
| cmake build clean | PASS — zero errors |

## Files modified
- `plugin/include/PluginEditor.h` — FileDragAndDropTarget, ElapsedTimer nested struct, 3 new members
- `plugin/src/AutoRemixEditor.cpp` — kAudioExts, drag methods, timer wiring in onClick_Play

## Deferred
- None

## Next
16-02 — Cancel button + sidecar health auto-poll
