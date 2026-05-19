---
phase: 22-ux-polish
plan: 05
subsystem: plugin-ui
tags: [juce, ux, copy, polish, mashup, render]

provides:
  - ScreenMashup: "Free mix" + "Match A's key" + middle-dot headers + render time hint
  - ScreenRender: 3-waveform layout when mashup (Track A · B · Mashup) with distinct colors
  - ScreenRender Save: reveal-in-folder after copy + clear status messages
  - ScreenSeparating + ScreenMashup: set status "Track B ready" / "Mashup ready" on success

key-files:
  modified:
    - plugin/include/ScreenMashup.h
    - plugin/include/ScreenRender.h
    - plugin/include/ScreenSeparating.h

key-decisions:
  - "Free mix" replaces "Custom" — clearer that it's not a saved template.
  - "Match A's key" replaces "Anchor to A" — plain English.
  - Track A/B/Mashup waveforms each use a distinct color: ACCENT (orange), STEM_VOCALS (sage), MASHUP (teal).
  - 3-waveform layout only when ctx_.render_is_mashup; remix flow keeps 2-waveform layout untouched.
  - revealToUser() opens system file manager post-save; status bar reports success/failure.
  - Stale status messages cleared at each transition (Track B ready, Mashup ready: N.N s).

patterns-established:
  - "Mashup-aware UI elements branch on ctx_.render_is_mashup; remix code path untouched."

duration: ~30min (plus 2 follow-up patches: 3rd waveform, distinct colors, status clear)
started: 2026-05-19
completed: 2026-05-19
---

# Phase 22 Plan 05 Summary

**Final polish pass closing Phase 22. ScreenMashup combos use friendlier copy. Track headers use middle dot. Render overlay shows time estimate. ScreenRender Done state has 3 distinct-colored waveforms when mashup. Save reveals the file in the system file manager. Status bar no longer goes stale across the mashup flow.**

## ACs

| AC | Status |
|----|--------|
| AC-1: Friendlier copy (Free mix, Match A's key) | Pass |
| AC-2: Middle-dot track headers | Pass |
| AC-3: Render overlay time estimate | Pass |
| AC-4: Reveal-in-folder after Save | Pass |
| AC-5: Build green; no regression | Pass |

## Bonus (during checkpoint)

- 3 stacked waveforms in mashup Done state (Track A / B / Mashup) replacing the original 2-waveform layout.
- Distinct color per waveform (orange / sage / teal) so the user can tell them apart at a glance.
- Status bar messages set on every transition — "Track B ready", "Mashup ready: N.N s", "Stems ready" — replacing leftover "Track B: analyzing..." that was hanging around after the actual work finished.

## Files Changed

```
plugin/include/ScreenMashup.h        +30 lines  (combo copy, ·, time-hint label, status msg)
plugin/include/ScreenRender.h        +85 lines  (b_thumb_, 3-section paint branch, save status + revealToUser)
plugin/include/ScreenSeparating.h     +3 lines  (status set on success)
```

## Phase 22 Wrap-Up

All 5 sub-plans complete:

| # | Deliverable |
|---|-------------|
| 22-01 | Track B uses ScreenSeparating with mashup-aware mode |
| 22-02 | ScreenRender mashup-aware Done state (3 play btns + critical onEnter fix) |
| 22-03 | Tooltips + slider unit suffixes + SIDECAR label |
| 22-04 | Drop-zone first-run orientation copy |
| 22-05 | Free mix / Match A's key / · / time hint / 3 waveforms / reveal / status |

Mashup flow now end-to-end polished. Plugin feels like a finished product.

## Next

Phase 22 ready for v3.2.0 release tag. Open: do we cut a new release tag now,
or batch more polish into a Phase 23 first?
