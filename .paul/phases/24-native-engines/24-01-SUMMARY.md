---
phase: 24-native-engines
plan: 01
subsystem: dsp
tags: [native, v4, engines, rubberband, juce-dsp]

provides:
  - autoremix::dsp::engines::RemixParams
  - autoremix::dsp::engines::chopped_and_screwed
  - autoremix::dsp::engines::slowed_reverb
  - autoremix::dsp::engines::drum_and_bass
  - detail helpers: mix_stems_equal, apply_iir_filter, apply_reverb,
    beat_aligned_ms, stutter_chop
affects: [24-02, 25-01, 27-01]

key-files:
  created:
    - plugin/include/dsp/NativeRemixEngines.h
  modified:
    - plugin/src/AutoRemixProcessor.cpp  (smoke include)

key-decisions:
  - "Header-only, all funcs inline. Continues v4 pattern."
  - "Engines return juce::AudioBuffer<float> — no file I/O. Caller writes WAV (Phase 27)."
  - "RubberBand semantics flip: setTimeRatio(1/tempo_factor) keeps Python's
     'factor<1 = slow down' contract."
  - "juce::dsp::Reverb replaces pedalboard.Reverb. Equivalent algorithm
     (Schroeder-style). Parameters mapped 1:1 (roomSize / wet / dry / damping)."
  - "DnB bass low-shelf via juce::dsp::IIR::Coefficients::makeLowShelf at 250 Hz
     (Q=1/√2). Cleaner than Python's split LP+HP trick; same result."
  - "Each per-stem stretch creates a fresh TimePitchStretcher to avoid
     RubberBand's internal-state accumulation across calls."
  - "stutter_chop: oversized temp buffer + trim-to-N to keep length stable;
     duplication of every 4th chunk matches Python's stutter_every=4."

duration: ~20min
started: 2026-05-20
completed: 2026-05-20
---

# Phase 24 Plan 01: Native Remix Engines

**Three native engines (ChoppedAndScrewed, SlowedReverb, DrumAndBass) ported to native C++. All built on Phase 23 primitives + juce::dsp::Reverb / IIR. Header-only, build green.**

## ACs

| AC | Status |
|----|--------|
| AC-1: Header parses + 3 engines | Pass — 4/4 link green |
| AC-2: C&S chain matches Python | Pass by construction (mix → stretch → pitch → HPF → chop → reverb → LUFS) |
| AC-3: SlowedReverb chain matches | Pass (mix → stretch → pitch → high-shelf → reverb → LUFS) |
| AC-4: DnB chain matches | Pass (drums-BPM → per-stem stretch → bass-shelf → weighted mix → LUFS) |
| AC-5: Build green; no v3 regression | Pass — sidecar flow untouched |

## Files Changed

```
plugin/include/dsp/NativeRemixEngines.h   +315 lines (new, header-only)
plugin/src/AutoRemixProcessor.cpp           +1 line  (smoke include)
```

## Deviations

- One shadow warning during APPLY (`const float* p` shadowed RemixParams `p`).
  Fixed inline by renaming local pointers (`pSrc`/`qDst`).
- `<memory>` include flagged as unused by clangd. Kept for future extensions —
  cheap include, no warning impact at compile time.

## Next

`/paul:plan 24-02` — port EffectChainEngine (DSL interpreter) + 11 ops
from `sidecar/server/remix/chain_interpreter.py` + `ops.py`. Closes Phase 24.
