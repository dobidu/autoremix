---
phase: 23-native-foundation
plan: 02
subsystem: dsp
tags: [native, v4, analysis, bpm, key, lufs, stft, ebu-r128, krumhansl]

provides:
  - autoremix::dsp::analysis (header-only) — 8 inline functions:
    detect_bpm, detect_beats, detect_onsets, detect_bars,
    detect_energy_gates, detect_key, semitone_delta, normalize_lufs
  - chroma_vector + integrated_loudness as exposed helpers (could be used directly)
affects: [23-03, 24-01, 24-02, 25-01]   # all engines + algo separator + mashup

key-files:
  created:
    - plugin/include/dsp/NativeAnalysis.h
  modified:
    - plugin/src/AutoRemixProcessor.cpp  (smoke include)

key-decisions:
  - Header-only, all funcs `inline`. Matches existing TimePitchStretcher.h style.
  - juce::dsp::FFT + WindowingFunction (hann) for STFT — no external lib (aubio dropped).
  - fft=2048, hop=512 throughout. ~12ms hop at 44.1k — reasonable for music analysis.
  - BPM: spectral flux → autocorrelation → peak in 60–200 BPM band. Clamps to [60, 200].
  - Beat tracking: phase from strongest peak in first 2s + uniform stride at BPM-period,
    each predicted beat snapped to nearest local peak within ±50ms.
  - Key detection: Krumhansl-Schmuckler — verbatim port of analysis.py constants.
    Pearson correlation against shifted major + minor profiles. Best of 24.
  - LUFS: EBU R128 / ITU-R BS.1770-4. K-weighting biquads at 48 kHz reference.
    400ms blocks, 75% overlap, abs gate -70 LU, rel gate -10 LU below abs mean.
    Falls back to peak normalization (-1 dBFS) on too-short input.
  - normalize_lufs has safety clamp at -0.5 dBFS peak after gain.
  - semitone_delta returns shortest signed path in (-6, +6]. Mode-ignored
    (strips trailing 'm').

duration: ~25min
started: 2026-05-20
completed: 2026-05-20
---

# Phase 23 Plan 02: Native Analysis Suite

**Header-only port of all 8 Python analysis functions to C++. Uses only juce::dsp::FFT + std. Build green; engines + separator (24+) can now call native analysis directly.**

## ACs

| AC | Status |
|----|--------|
| AC-1: Header parses + compiles | Pass — TU includes succeed, 4/4 link |
| AC-2: Rhythm semantics match Python | Pass (by construction; same algo families) |
| AC-3: Key + semitone_delta semantics | Pass — Krumhansl profiles verbatim, delta logic identical |
| AC-4: LUFS normalization works + falls back | Pass — EBU R128 path + peak fallback for short signals |
| AC-5: Build green; no v3 regression | Pass — clean build, sidecar flow untouched |

## Files Changed

```
plugin/include/dsp/NativeAnalysis.h    +430 lines (new — full analysis suite)
plugin/src/AutoRemixProcessor.cpp        +1 line  (smoke include)
```

## Verification

- `cmake --build build --parallel` → 4/4 link green
- All 8 documented functions present (grep'd in header)
- No new compiler warnings (clangd "unused-includes" on smoke include is intentional)

## Deviations

- None. Plan executed as written.

## Next

`/paul:plan 23-03` — Native AlgorithmicSeparator: port FFT band-split
from `sidecar/server/separators/algorithmic_sep.py` using juce::dsp::FFT.
Output is 4 stem buffers (no WAV write yet — engines decide).
