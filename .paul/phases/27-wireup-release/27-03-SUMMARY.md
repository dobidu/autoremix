---
phase: 27-wireup-release
plan: 03
subsystem: qa
tags: [regression, human-verify, ui-flows, native]

requires:
  - phase: 27-01
    provides: Native wiring of all 7 AudioBridge call sites
  - phase: 27-02
    provides: AudioBridge deleted; CI + release.yml updated; commit 4b40bfc

provides:
  - Full regression pass on all 6 screens confirmed PASS
  - Algorithmic + Demucs remix flows verified
  - Mashup flow verified (MASHUP DONE + 3 play buttons)
  - Static audit: zero AudioBridge refs, zero port 17432 refs
  - No sidecar process spawned; port 17432 never bound
  - v4 native plugin cleared for 27-04 release

affects: [27-04-release]

tech-stack:
  added: []
  patterns:
    - "sidecar_lbl_ variable retained (displays 'MODEL'); benign stale name — not a spawn reference"

key-files:
  modified: []
  verified:
    - plugin/include/ (zero AudioBridge refs)
    - plugin/src/ (zero AudioBridge refs)
    - build-rel/plugin/AutoRemix_artefacts/Release/Standalone/AutoRemix

key-decisions:
  - "sidecar_lbl_ variable name: stale but harmless — 13 grep hits were all comments or this var; none spawn a process. Not renamed (out of scope for regression pass)"

duration: ~20min
started: 2026-05-28T14:30:00Z
completed: 2026-05-28T14:50:00Z
---

# Phase 27 Plan 03: Full Regression Test Summary

**All 6 screens + remix + mashup flows verified PASS on Release build. Zero sidecar spawn. v4 native plugin cleared for 27-04 release.**

## Performance

| Metric | Value |
|--------|-------|
| Duration | ~20 min |
| Completed | 2026-05-28 |
| Tasks | 5 of 5 PASS |
| Regressions found | 0 |
| Blockers | 0 |

## Acceptance Criteria Results

| Criterion | Status | Notes |
|-----------|--------|-------|
| AC-1: Static audit clean | PASS | Zero AudioBridge hits; zero port 17432 hits; build green; no sidecar process |
| AC-2: ScreenEmpty + file analysis | PASS | BPM + key displayed; waveform renders |
| AC-3: Algorithmic remix flow | PASS | Separate → stems → preset → render → audio plays |
| AC-4: Demucs (ML) flow | PASS | ML inference → stems → render OK |
| AC-5: ScreenStemsReady features | PASS | Per-stem play, mute, drag-to-DAW, back navigation |
| AC-6: Mashup flow | PASS | MASHUP DONE + ▶ Track A / B / Mashup + New Mashup |
| AC-7: No crash / no port / no subprocess | PASS | Port 17432 clean; 3 cold-start cycles no crash |

## Task Results

| Task | Type | Result | Notes |
|------|------|--------|-------|
| T1: Static audit | auto | PASS | 4/4 checks clean |
| T2: Algorithmic remix | human-verify | PASS | All 5 sub-steps confirmed |
| T3: Demucs (ML) | human-verify | PASS | Model cached; inference + render OK |
| T4: Mashup flow | human-verify | PASS | All 6 sub-steps confirmed |
| T5: Process audit | human-verify | PASS | pgrep CLEAN, lsof CLEAN, 3 restarts OK |

## Static Audit Detail

```
grep -r "AudioBridge" plugin/  →  EXIT 1 (zero matches) ✓
grep -r "17432" plugin/        →  EXIT 1 (zero matches) ✓
cmake --build build-rel        →  10/10 targets green ✓
pgrep sidecar/python           →  system unattended-upgrades only ✓
lsof -i :17432                 →  CLEAN ✓
```

**Note on `sidecar` grep:** 13 hits found in plugin/ — all benign:
- `sidecar_lbl_` UI label variable (displays text "MODEL", not "SIDECAR")
- Comments documenting origin files (`// Port of sidecar/server/...`)
- `AutoRemixProcessor.cpp`: comment `// Python sidecar lifecycle removed`
No spawn mechanism, no port reference, no IPC code.

## Deviations from Plan

None. All tasks completed as specified. No regressions found.

## Issues Encountered

None.

## Next Phase Readiness

**Ready for 27-04:**
- Release build green
- All UI flows verified
- No sidecar dependency anywhere in the plugin
- Clean process state confirmed

**Deferred (noted, not blocking):**
- `sidecar_lbl_` variable rename: cosmetic, no functional impact. Can be renamed to `model_lbl_` in 27-04 or post-v4.0.0 cleanup.

**Blockers:**
None.

---
*Phase: 27-wireup-release, Plan: 03*
*Completed: 2026-05-28*
