---
phase: 22-ux-polish
plan: 01
subsystem: plugin-ui
tags: [juce, cpp, screen-flow, mashup, ux]

requires:
  - phase: 21-automashup
    provides: ScreenMashup + start_mashup_flow callback (21-04)
provides:
  - ctx_.mashup_mode_separating flag
  - ScreenSeparating mashup-aware (header, file path, stems field, nav target)
  - Editor startMashupFlow delegates separation UI to ScreenSeparating
affects: [22-02 ScreenRender mashup-aware]

tech-stack:
  added: []
  patterns:
    - "Single bool flag on ctx_ to opt-in screen behavior — consumed by the screen and cleared after use."
    - "ScreenSeparating captures mode at onEnter so the value doesn't change mid-separation."

key-files:
  created: []
  modified:
    - plugin/include/ScreenContext.h
    - plugin/include/ScreenSeparating.h
    - plugin/src/AutoRemixEditor.cpp

key-decisions:
  - "mashup_mode_separating is a one-shot flag: set by editor before navigating, consumed by ScreenSeparating on success/cancel."
  - "Cancel from mashup mode returns to StemsReady (track A still loaded), NOT Empty (which would wipe track A's separation work)."
  - "ScreenSeparating uses distinct stems_b subdir for track B to avoid collision with stems/."
  - "Editor's startMashupFlow no longer runs separateStems inline — pure analyze + flag + navigate."

patterns-established:
  - "Use ctx_-side flags to repurpose existing screens for new flows instead of duplicating them."

duration: ~15min
started: 2026-05-19
completed: 2026-05-19
---

# Phase 22 Plan 01: Track B Progress Screen Summary

**ScreenSeparating now handles both track A (normal flow) and track B (mashup flow) via a one-shot `ctx_.mashup_mode_separating` flag. Track B no longer appears frozen during separation — gets the same 4-row + elapsed-timer + cancel UI as track A.**

## Performance

| Metric | Value |
|--------|-------|
| Duration | ~15min |
| Tasks | 3/3 + checkpoint approved |
| Files modified | 3 |
| Files created | 0 |
| LOC delta | +27 / -34 (editor simplified; ScreenSeparating got +25; ctx_ +1) |

## Acceptance Criteria Results

| Criterion | Status | Notes |
|-----------|--------|-------|
| AC-1: Mashup uses ScreenSeparating for B | Pass | header switches to "SEPARATING TRACK B", 4 rows animate |
| AC-2: Writes to stems_b not stems | Pass | flag consumed, stems_b populated, ctx_.stems untouched |
| AC-3: Non-mashup unchanged | Pass | track A flow identical to before |
| AC-4: Cancel from mashup → StemsReady | Pass | flag cleared, stems_b emptied, A's view restored |
| AC-5: Build green + live smoke | Pass | user approved live flow |

## Accomplishments

- Track B separation feels like first-class UI instead of background work.
- Single flag pattern is reusable for any future screen that needs to be repurposed.
- Editor's startMashupFlow shrank from ~45 lines to ~20 — separation logic moved to where it belongs.

## Deviations from Plan

- None. Plan executed as written.
- One transient ALSA segfault on first relaunch (same WSL2 race we've seen). Reran clean. Out of scope to fix.

## Open Items / Deferred (per Phase 22 roadmap)

- 22-02: ScreenRender mashup-aware Done state ("MASHUP DONE" header, swapped buttons)
- 22-03: Tooltips + labels + slider units
- 22-04: First-time orientation hint
- 22-05: Minor copy/visual polish

## Files Changed

```
plugin/include/ScreenContext.h        +1 line  (flag)
plugin/include/ScreenSeparating.h    +28 lines (mashup-aware onEnter/startSeparation/requestCancel + member)
plugin/src/AutoRemixEditor.cpp       -32 lines (startMashupFlow simplified)
```

## Verification

- `cmake --build build --parallel` → green, 3 Linux targets linked
- Live: user ran the full Empty → StemsReady → Mashup pick → SEPARATING TRACK B → Mashup flow and reported approved.

## Next

`/paul:plan 22-02` — ScreenRender mashup-aware Done state.
