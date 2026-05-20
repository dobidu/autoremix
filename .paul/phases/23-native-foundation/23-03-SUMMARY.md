---
phase: 23-native-foundation
plan: 03
subsystem: dsp
tags: [native, v4, separator, fft, band-split, juce-dsp]

provides:
  - autoremix::dsp::separators::NativeStems
  - autoremix::dsp::separators::separate_algorithmic
affects: [24-01, 27-01]

key-files:
  created:
    - plugin/include/dsp/NativeAlgorithmicSeparator.h
  modified:
    - plugin/src/AutoRemixProcessor.cpp  (smoke include)

key-decisions:
  - 4 fixed bands (0/250/2000/6000/Nyquist) verbatim from v3.
  - 2nd-order HP + 2nd-order LP cascade via juce::dsp::IIR — approximates
    scipy.butter(4, [lo,hi], 'band') closely enough for this separator's
    "low-quality, always-available" role.
  - Edge handling: lo≤0 skips HP; hi≥Nyquist-1 skips LP; lo≥hi returns silence.
  - Mono/stereo preserved (no fake stereo synthesis).
  - Always-available — no external deps. Matches v3 is_available()=True semantics.

duration: ~10min
started: 2026-05-20
completed: 2026-05-20
---

# Phase 23 Plan 03: Native AlgorithmicSeparator

**4-band native stem splitter ready. Phase 23 (Native foundation) closed — all primitives needed by v4 engines + mashup now exist in native C++.**

## ACs

| AC | Status |
|----|--------|
| AC-1: Header compiles + exposed API | Pass — 4/4 link, smoke include green |
| AC-2: Band partitioning matches v3 | Pass by construction (verbatim band edges) |
| AC-3: Shape preserved | Pass — `makeCopyOf` keeps channel count + length |
| AC-4: Build green; v3 untouched | Pass — sidecar flow unchanged |

## Files Changed

```
plugin/include/dsp/NativeAlgorithmicSeparator.h  +88 lines (new, header-only)
plugin/src/AutoRemixProcessor.cpp                 +1 line  (smoke include)
```

## Phase 23 Wrap-Up

| Sub-plan | Deliverable |
|----------|-------------|
| 23-01 | RubberBand integration + TimePitchStretcher wrapper |
| 23-02 | NativeAnalysis suite (BPM, beats, onsets, key, semitone_delta, LUFS, energy gates) |
| 23-03 | NativeAlgorithmicSeparator (4-band IIR cascade) |

All header-only under `plugin/include/dsp/`. v4 engines (Phase 24) and
mashup (Phase 25) can be built without touching Python.

## Next

`/paul:plan 24-01` — port ChoppedAndScrewedEngine + SlowedReverbEngine +
DrumAndBassEngine to native C++ using TimePitchStretcher + NativeAnalysis.
