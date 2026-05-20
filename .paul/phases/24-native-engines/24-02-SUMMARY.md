---
phase: 24-native-engines
plan: 02
subsystem: dsp
tags: [native, v4, effect-chain, ops, dsl, juce-dsp]

provides:
  - autoremix::dsp::ops — 11 inline op functions + OpParams struct + detail::chop_at_boundaries
  - autoremix::dsp::engines::EffectStep, StemMix, process_effect_chain
affects: [25-01, 25-02, 27-01]

key-files:
  created:
    - plugin/include/dsp/NativeEffectOps.h
    - plugin/include/dsp/NativeEffectChainEngine.h
  modified:
    - plugin/src/AutoRemixProcessor.cpp  (smoke includes)

key-decisions:
  - "OpParams: single struct with all fields default-initialized. Mirrors v3 dict
     semantics (extra/unused fields ignored). Simpler than std::variant per-op."
  - "structural_cut uses uniform-N segmentation as v4 fallback. True MFCC-clustered
     segmentation (Python's detect_structure) deferred."
  - "RubberBand convention flip preserved: time_stretch passes 1/factor to setTimeRatio
     so v3's librosa rate<1 = slower contract holds."
  - "chain interpreter mirrors v3 exactly: 'all' → sum, process, /4, redistribute.
     Specific stem names → process in place. Final mix uses StemMix weights /4."
  - "engines::detail namespace reused (named, not anonymous) for cross-header access
     to apply_iir_filter + apply_reverb. No code duplication."

duration: ~20min
started: 2026-05-20
completed: 2026-05-20
---

# Phase 24 Plan 02: Native Effect-Chain DSL

**11 native ops + EffectChainEngine interpreter ported. Closes Phase 24. Native engines + chain DSL now feature-complete in C++.**

## ACs

| AC | Status |
|----|--------|
| AC-1: 6 basic ops + OpParams | Pass — time_stretch / pitch_shift / reverb / chop / bass_boost / eq_highpass |
| AC-2: 5 musical ops + structural | Pass — chop_beats / chop_onsets / chop_bars / gate_energy / structural_cut (uniform fallback) |
| AC-3: Chain interpreter | Pass — dispatch_op + sum_buffers + 'all' redistribute + weighted mix |
| AC-4: Build green | Pass — 4/4 link, no new warnings |

## Files Changed

```
plugin/include/dsp/NativeEffectOps.h          +295 lines (new, header-only)
plugin/include/dsp/NativeEffectChainEngine.h  +135 lines (new, header-only)
plugin/src/AutoRemixProcessor.cpp               +2 lines (smoke includes)
```

## Phase 24 Wrap

| Sub-plan | Deliverable |
|----------|-------------|
| 24-01 | NativeRemixEngines.h — ChoppedAndScrewed / SlowedReverb / DrumAndBass |
| 24-02 | NativeEffectOps.h (11 ops) + NativeEffectChainEngine.h (interpreter) |

All v3 remix paths now have native C++ equivalents. v4 progress: 5/13.

## Deferred

- detect_structure (MFCC + agglomerative clustering). structural_cut
  uses uniform N-segment fallback. If audible drift vs v3 is reported
  during Phase 27, port the real segmenter.

## Next

`/paul:plan 25-01` — port MashupEngine (sidecar/server/remix/mashup.py).
Uses TimePitchStretcher + analysis::semitone_delta + engines::detail
helpers. Then 25-02 ports the JSON preset loaders.
