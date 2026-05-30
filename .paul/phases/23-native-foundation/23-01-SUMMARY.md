---
phase: 23-native-foundation
plan: 01
subsystem: build + dsp
tags: [cmake, fetchcontent, rubberband, dsp, native, v4]

provides:
  - RubberBand v3.3.0 in the build (FetchContent + single-file source)
  - autoremix::dsp::TimePitchStretcher header-only wrapper
  - rubberband target linked into the AutoRemix plugin
affects: [24-01, 24-02, 25-01]  # all engines + mashup use time/pitch ops

key-files:
  created:
    - plugin/include/dsp/TimePitchStretcher.h
  modified:
    - CMakeLists.txt
    - plugin/CMakeLists.txt
    - plugin/src/AutoRemixProcessor.cpp  (smoke include only)

key-decisions:
  - Upstream RubberBand uses Meson, not CMake. Used its supported
    "single-file build" path (single/RubberBandSingle.cpp) — produces a
    self-contained TU we wrap in a tiny CMake add_library() target.
    Trade-off: built-in FFT + resampler (slightly slower than vDSP/IPP
    but zero external deps).
  - Engine: OptionEngineFiner (R3 engine, higher quality) +
    OptionProcessOffline. Defaults appropriate for offline render flows.
  - Wrapper is header-only to match project style; offline-only API for now
    (engines need full-buffer process). Streaming variant deferred.
  - Smoke include in AutoRemixProcessor.cpp proves the header compiles
    cleanly in a real TU.

duration: ~20min
started: 2026-05-20
completed: 2026-05-20
---

# Phase 23 Plan 01: RubberBand Integration

**RubberBand fetched + built (single-source path) + linked into the plugin. New `autoremix::dsp::TimePitchStretcher` wrapper offers a JUCE-buffer-shaped API for offline time-stretch + pitch-shift, replacing Python's librosa.effects.* calls used by every engine.**

## ACs

| AC | Status |
|----|--------|
| AC-1: RubberBand fetched + builds | Pass — librubberband.a builds clean |
| AC-2: TimePitchStretcher wrapper compiles | Pass — 41/41 link green |
| AC-3: No v3 regression | Pass — sidecar flow untouched; only additive build deps |

## Changes

```
CMakeLists.txt                        +21 lines (FetchContent + add_library + include dirs)
plugin/CMakeLists.txt                  +1 line  (link rubberband)
plugin/include/dsp/TimePitchStretcher.h +130 lines (new wrapper, header-only)
plugin/src/AutoRemixProcessor.cpp       +1 line  (smoke include — ensures wrapper compiles in real TU)
```

## Next

`/paul:plan 23-02` — native analysis module:
port detect_bpm, detect_key, detect_beats, detect_onsets, normalize_lufs
to C++ (header-only utility under plugin/include/dsp/).
