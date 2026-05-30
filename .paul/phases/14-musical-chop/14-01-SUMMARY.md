---
phase: 14-musical-chop
plan: 01
subsystem: audio-analysis
tags: [librosa, beat-detection, onset-detection, structural-segmentation, energy-gating]

requires:
  - phase: 10-effect-chain-dsl
    provides: ops.py + OP_REGISTRY pattern that plan 14-02 will extend

provides:
  - python/server/remix/analysis.py — 5 musical analysis functions
  - python/tests/test_analysis.py — 11 pytest tests (all passing)

affects: [14-02-ops, 14-03-presets]

tech-stack:
  added: []
  patterns:
    - "analysis.py functions are pure: accept (audio, sr, **kwargs), return new arrays, never mutate input"
    - "_to_mono() helper centralises stereo→mono conversion"

key-files:
  created:
    - python/server/remix/analysis.py
    - python/tests/test_analysis.py
  modified: []

key-decisions:
  - "hold_ms applied at frame level then upsampled → avoids per-sample iteration cost"
  - "detect_structure uses librosa.segment.agglomerative on MFCC (13 coefficients)"
  - "detect_onsets returns [0.0] fallback (not empty) to prevent downstream index errors"

patterns-established:
  - "Analysis functions guard empty/short audio with len < sr check → return safe fallback"

duration: ~15min
started: 2026-05-16T00:00:00Z
completed: 2026-05-16T00:00:00Z
---

# Phase 14 Plan 01: Musical Analysis Module Summary

**`python/server/remix/analysis.py` — 5 librosa-backed detection functions with 11 passing pytest tests, powering beat/onset/bar/energy/structural chop ops in plan 14-02.**

## Performance

| Metric | Value |
|--------|-------|
| Duration | ~15 min |
| Tasks | 2 completed |
| Files created | 2 |
| Tests | 11 passed, 0 failed |

## Acceptance Criteria Results

| Criterion | Status | Notes |
|-----------|--------|-------|
| AC-1: detect_beats returns sorted timestamps | Pass | sorted ascending, non-empty for 5s audio |
| AC-2: detect_onsets respects min_gap | Pass | 400ms gap test verified |
| AC-3: detect_bars aligns to beat grid | Pass | all bar times within 5ms of a beat |
| AC-4: detect_energy_gates sample-aligned bool mask | Pass | shape==audio.shape[-1], dtype==bool |
| AC-5: detect_structure non-overlapping ordered segments | Pass | covers [0, duration), 4 segments from 10s audio |

## Files Created/Modified

| File | Change | Purpose |
|------|--------|---------|
| `python/server/remix/analysis.py` | Created | 5 detection functions + `_to_mono` helper |
| `python/tests/test_analysis.py` | Created | 11 pytest tests using synthetic audio |

## Decisions Made

| Decision | Rationale | Impact |
|----------|-----------|--------|
| Energy gate hold applied at frame level | Frame = 512 samples; per-sample hold would be O(n²) | Efficient; minor precision loss at hold boundaries (acceptable) |
| detect_onsets returns `[0.0]` on empty | Prevents IndexError in ops that slice by onset positions | 14-02 ops can always assume ≥ 1 onset |
| MFCC n_mfcc=13 for structure | Standard mel-cepstral dimensionality; sufficient for agglomerative clustering | Consistent with librosa convention |

## Deviations from Plan

Plan specified 8 tests; 11 written — added `test_detect_onsets_returns_floats`, `test_detect_bars_returns_subset`, `test_detect_energy_gates_loud_is_true` for extra coverage. No scope violations; all test the specified functions.

## Issues Encountered

None — straight execution.

## Next Phase Readiness

**Ready:**
- `analysis.py` importable as `from server.remix.analysis import ...`
- All 5 functions tested and stable — 14-02 can call them directly from new ops
- `_to_mono` reusable internally

**Concerns:**
- `detect_structure` segment count can be < n_segments if audio is very short (< 1s) — 14-03 presets should use audio ≥ 5s

**Blockers:** None

---
*Phase: 14-musical-chop, Plan: 01*
*Completed: 2026-05-16*
