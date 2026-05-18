---
phase: 21-automashup
plan: 01
subsystem: sidecar
tags: [mashup, librosa, time-stretch, pitch-shift, fastapi, pydantic]

requires:
  - phase: 11-demucs-separator
    provides: IStemSeparator abstraction (algorithmic + demucs)
  - phase: 14-musical-chop
    provides: analysis.py (detect_bpm, detect_key, librosa helpers)
provides:
  - MashupEngine (sidecar) — pairwise mashup of two tracks
  - semitone_delta(key_from, key_to) — shortest-path semitone calc
  - POST /api/v1/mashup endpoint
  - MashupRequest / MashupResponse Pydantic models
affects: [phase 21-02, 21-03, 21-04, 21-05]

tech-stack:
  added: []
  patterns:
    - MashupEngine instances accept separator at process() time (DI, not constructor)

key-files:
  created:
    - sidecar/server/remix/mashup.py
    - sidecar/tests/test_mashup.py
  modified:
    - sidecar/server/models.py
    - sidecar/server/remix/analysis.py
    - sidecar/server/main.py

key-decisions:
  - "Anchor = track A. target_bpm/key default to A's analysis if absent in request."
  - "Mode-aware key matching deferred — semitone_delta strips 'm' suffix, root-only shift."
  - "Sample-rate mismatch between A and B = hard error (no resampling for MVP)."
  - "Length policy = truncate to shorter (min(len_a_stretched, len_b_stretched)). No loop/crossfade."
  - "stretch clamp 0.5–2.0 — librosa pitch artifacts beyond this become unusable."
  - "Stem source map keys: vocals/drums/bass/other; Literal['a','b'] in Pydantic for validation."

patterns-established:
  - "Engine returns dict with diagnostic fields (source_bpm_a/b, source_key_a/b, stretch_factor_b, semitone_shift_b) — endpoint exposes the user-facing subset only"
  - "MashupEngine is stateless (constructor takes only clamp bounds)"

duration: ~25min
started: 2026-05-18
completed: 2026-05-18
---

# Phase 21 Plan 01: AutoMashup Sidecar Summary

**Sidecar-only mashup pipeline: separate both tracks, analyze BPM/key, tempo-match and key-match track B to track A, mux stems per user selection, mix, output WAV. 7 pytest cases green.**

## Performance

| Metric | Value |
|--------|-------|
| Duration | ~25min |
| Tasks | 5/5 completed |
| Files modified | 3 |
| Files created | 2 |
| Tests | 7 new (6 planned, +1 stretch-clamp regression) |
| Total suite | 61 passing (was 54) |

## Acceptance Criteria Results

| Criterion | Status | Notes |
|-----------|--------|-------|
| POST /api/v1/mashup returns target_bpm, target_key, length_sec | Pass | All four fields in MashupResponse |
| Pydantic MashupRequest validation | Pass | Literal['a','b'] rejects invalid sources |
| stem_sources accepts per-stem A/B map | Pass | Schema validates; engine respects |
| target_bpm / target_key optional, anchor to A | Pass | None → A's analysis |
| Both tracks use same separator_id | Pass | One separator instance, two calls |
| Stretch factor clamp 0.5–2.0 | Pass | regression test confirms |
| Semitone delta computed via key_b vs target | Pass | 3 cases (same, +3, -3) |
| Length = min(stretched_a, stretched_b) | Pass | trim before stack-sum |
| 4 muxed stems summed + LUFS-normalized | Pass | normalize_lufs from base.py |
| 6+ pytest tests pass | Pass | 7/7 green |
| Heavy work in asyncio.to_thread | Pass | endpoint wraps engine.process |

## Accomplishments

- End-to-end mashup pipeline runs on synthetic sine inputs (5s + 4s WAVs) and produces a valid output with truncated length and no exceptions.
- `semitone_delta` correctly returns shortest signed semitone path in (-6, +6]; mode (major/minor) ignored as designed.
- Stretch-clamp regression test catches any future change that lets `bpm_target / bpm_b` escape `[0.5, 2.0]`.
- Sample-rate mismatch detected up front with a clear `ValueError` instead of silently mixing at the wrong rate.
- No regression in 54 pre-existing tests.

## Deviations from Plan

- Plan called for 6 tests; shipped 7 (added `test_mashup_stretch_factor_clamped` as a regression guard).
- Engine returns more diagnostic fields than spec'd (source_bpm_a, source_bpm_b, source_key_a, source_key_b, stretch_factor_b, semitone_shift_b). Endpoint exposes only the four user-facing fields the spec required; extras kept for downstream UI use and future logging.

## Open Items / Deferred

- **C++ glue** (21-02): `AudioBridge::mashup()`, `MashupParams`, `ScreenContext` mashup fields.
- **UI screen** (21-03): `ScreenMashup` — per-stem A/B toggles + target BPM/key sliders.
- **Entry-point flow** (21-04): Mashup button on `ScreenStemsReady` + 2nd-file load.
- **Render integration + release** (21-05): reuse `ScreenRender` Done state; release v3.1.0.
- **Mode-aware key matching** (Camelot wheel): explicit deferred.
- **Manual beat-offset slider**: explicit deferred.
- **N-track mashup (>2)**: explicit deferred.
- **Stretch fallback when key detection low-confidence**: not implemented; noisy delta accepted.

## Files Changed

```
sidecar/server/models.py          +18 lines  (MashupRequest, MashupResponse)
sidecar/server/remix/analysis.py  +22 lines  (semitone_delta + _key_root_index)
sidecar/server/remix/mashup.py    +90 lines  (new — MashupEngine class)
sidecar/server/main.py            +49 lines  (POST /api/v1/mashup handler)
sidecar/tests/test_mashup.py     +114 lines  (new — 7 tests)
```

## Verification

```bash
cd sidecar && .venv/bin/python -m pytest tests/test_mashup.py -v
# 7 passed in 3.66s

cd sidecar && .venv/bin/python -m pytest tests/
# 61 passed in 8.82s
```

## Next

`/paul:plan 21-02` — C++ glue (`AudioBridge::mashup()`, `MashupParams`, `ScreenContext` fields).
