---
phase: 22-ux-polish
plan: 02
subsystem: plugin-ui
tags: [juce, cpp, screen-render, mashup, ux]

requires:
  - phase: 21-automashup
    provides: ScreenMashup + mashup_output_path (21-03)
  - phase: 22-ux-polish
    provides: mashup_mode_separating pattern (22-01)
provides:
  - ctx_.render_is_mashup flag (set by ScreenMashup, cleared by ScreenModeParams)
  - ScreenRender mashup-aware Done state: "MASHUP DONE" header, TRACK A / MASHUP wave labels, ▶ Track B button, New Mashup button (replaces New Remix)
  - PreviewMode::TrackB variant + handleTrackBPlay
  - Idle/Playing text helpers that read ctx_.render_is_mashup
affects: [22-03 tooltips/labels, 22-04 orientation, 22-05 polish]

tech-stack:
  added: []
  patterns:
    - "Render-state flag set deterministically at each entry point (Mashup → true, ModeParams → false)"
    - "Button-text helpers (originalIdleText/PlayingText etc.) centralize mashup/remix text switch — no duplication"

key-files:
  created: []
  modified:
    - plugin/include/ScreenContext.h
    - plugin/include/ScreenMashup.h
    - plugin/include/ScreenModeParams.h
    - plugin/include/ScreenRender.h

key-decisions:
  - "Reuse original_btn_ and remix_btn_ in mashup mode (renamed to Track A / Mashup via text helpers) — avoids duplicate widgets for the same semantic role."
  - "Only add ONE new play button (track_b_btn_) and ONE new action button (new_mashup_btn_). Total widget growth = 2."
  - "PreviewMode enum gains a TrackB variant; existing Original/Remix variants keep their semantics (Original = the input file, Remix = the output file; in mashup mode those are Track A and Mashup respectively)."
  - "New Mashup navigates to ScreenMashup (preserves stems_a/stems_b context for re-tweaking) rather than re-running the separation."
  - "Waveform labels switch in paint() based on ctx_.render_is_mashup — cheap and atomic."

patterns-established:
  - "Button-text helper methods (e.g. originalIdleText) return UTF-8 strings based on ctx_ flag — called from initial setup, applyState, button click handlers, and the playback-stopped reset path."

duration: ~25min
started: 2026-05-19
completed: 2026-05-19
---

# Phase 22 Plan 02: ScreenRender Mashup-Aware Summary

**ScreenRender Done state now reads `ctx_.render_is_mashup` and adapts: header "MASHUP DONE" (teal), wave labels TRACK A / MASHUP, 3 play buttons (Track A / Track B / Mashup) instead of 2, New Mashup button returns to ScreenMashup. Remix flow unchanged. User approved live.**

## Performance

| Metric | Value |
|--------|-------|
| Duration | ~25min |
| Tasks | 2/2 + checkpoint approved |
| Files modified | 4 |
| Files created | 0 |
| LOC delta | +75 |

## Acceptance Criteria Results

| Criterion | Status | Notes |
|-----------|--------|-------|
| AC-1: Mashup Done labels | Pass | "MASHUP DONE" teal, TRACK A / MASHUP, 3 play btns + New Mashup |
| AC-2: 3 exclusive preview buttons | Pass | Track B handler stops other previews, sets PreviewMode::TrackB |
| AC-3: New Mashup → ScreenMashup | Pass | stems_a/b preserved, user can re-tweak template + gains |
| AC-4: Remix flow unchanged | Pass | header DONE, ORIGINAL/REMIX labels, New Remix → ModeParams |
| AC-5: Flag lifecycle | Pass | set true by ScreenMashup before Render nav, cleared by ScreenModeParams render_btn_ click |
| AC-6: Build green + live smoke | Pass | user approved live flow |

## Accomplishments

- Single ScreenRender file handles both flows cleanly via one flag + 6 text helpers. No duplicate widgets, no second screen.
- Live verified: user ran both flows and confirmed no regressions.
- Done-state diff between modes is now visually obvious — header color + button text both shift.

## Deviations from Plan

- **Critical post-checkpoint fix**: original checkpoint approval was premature —
  user discovered ScreenRender.onEnter() was unconditionally calling startRender()
  even when arriving from ScreenMashup. That kicked off the remix engine on
  track A's stems, overwriting `ctx_.output_path` (which had been set to the
  mashup output). User heard C&S-on-track-A instead of the mashup.

  Fix: ScreenRender.onEnter() now branches on `ctx_.render_is_mashup`. In
  mashup mode it skips startRender entirely and jumps straight to Done state,
  loading the already-rendered output thumbnail. In remix mode it runs as before.

  Root cause: my plan assumed Done-state behavior was the only thing that
  needed mashup-awareness. Forgot that onEnter() also kicks off work.

## Open Items / Deferred

- 22-03: Tooltips + labels + slider units
- 22-04: First-time orientation
- 22-05: Minor copy/visual polish
- Future: Track B waveform display (currently no thumbnail for B in Done state — only the play button)

## Files Changed

```
plugin/include/ScreenContext.h        +1 line  (render_is_mashup flag)
plugin/include/ScreenMashup.h         +1 line  (set flag before Render nav)
plugin/include/ScreenModeParams.h     +4 lines (clear flag before Render nav)
plugin/include/ScreenRender.h        +75 lines (track_b_btn_, new_mashup_btn_,
                                                 6 text helpers, PreviewMode::TrackB,
                                                 handleTrackBPlay, mashup-aware
                                                 applyState/paint/resized)
```

## Verification

- `cmake --build build --parallel` → green
- Live user verification confirms both flows are correct.

## Next

`/paul:plan 22-03` — tooltips + labels + slider units (health dot, M/S, separator combo, `×`/`Hz` slider suffixes).
