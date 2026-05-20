---
phase: 25-native-mashup
plan: 01
subsystem: dsp
tags: [native, v4, mashup, rubberband, lufs]

provides:
  - autoremix::dsp::engines::MashupParams (gains_a/b + 5 feel knobs + target sentinels)
  - autoremix::dsp::engines::MashupResult (buffer + 9 diagnostic fields)
  - autoremix::dsp::engines::mashup(stems_a, stems_b, sr, params) → MashupResult
  - kMashupStretchMin/Max constants matching v3
affects: [25-02, 27-01]

key-files:
  created:
    - plugin/include/dsp/NativeMashupEngine.h
  modified:
    - plugin/src/AutoRemixProcessor.cpp  (smoke include)

key-decisions:
  - "In-memory API: takes two NativeStems + sr. No file I/O. Caller (Phase 27) writes WAV."
  - "BPM/key analysis runs on summed stems (matches v3 mix-of-stems approach)."
  - "RubberBand semantics: setTimeRatio(stretch) where stretch = target_bpm / source_bpm.
     Matches v3 librosa rate convention (rate>1 = output longer = slowed)."
  - "Per-stem fresh TimePitchStretcher to avoid RubberBand state accumulation across calls."
  - "Diagnostic fields (source_bpm_a/b, source_key_a/b, stretch_factor_b, semitone_shift_b)
     exposed for future UI display (v3 endpoint returned them in the response dict)."

duration: ~15min
started: 2026-05-20
completed: 2026-05-20
---

# Phase 25 Plan 01: Native MashupEngine

**Header-only port of v3 MashupEngine. Takes two NativeStems, returns a MashupResult with the mixed buffer + analysis metadata. Build green.**

## ACs

| AC | Status |
|----|--------|
| AC-1: Header parses + ports faithfully | Pass — 4/4 link, full pipeline ported |
| AC-2: Build green; v3 untouched | Pass |

## Files Changed

```
plugin/include/dsp/NativeMashupEngine.h    +220 lines (new, header-only)
plugin/src/AutoRemixProcessor.cpp            +1 line  (smoke include)
```

## Next

`/paul:plan 25-02` — port PresetLoader + MashupPresetLoader.
JSON files in sidecar/server/presets/ + sidecar/server/mashup_presets/.
Use nlohmann/json (already in build). Bundle as embedded BinaryData or
load from a user-chosen dir. Closes Phase 25.
