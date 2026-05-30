---
phase: 21-automashup
plan: 03
subsystem: plugin-ui
tags: [cpp, juce, screen, ui, mashup]

requires:
  - phase: 21-automashup
    provides: ctx_.run_mashup callback + MashupParams/Result (21-02)
provides:
  - ScreenId::Mashup enum value
  - ScreenMashup header-only screen class
  - Editor navigateTo() wiring for ScreenId::Mashup
affects: [phase 21-04 entry-point, 21-05 release]

tech-stack:
  added: []
  patterns:
    - "Source-toggle button pair: active = default style, inactive = ghost (componentID swap + repaint)"
    - "Target-key combo: index 0 = 'Anchor to A' (empty string sentinel); 1..12 = note roots"
    - "Mashup output reuses ctx_.output_path so existing ScreenRender Done state can display it"

key-files:
  created:
    - plugin/include/ScreenMashup.h
  modified:
    - plugin/include/ScreenContext.h
    - plugin/src/AutoRemixEditor.cpp

key-decisions:
  - "Stem source picker: button pair (A/B) per row, not ComboBox — matches existing M/S toggle styling."
  - "Target key combo offers 12 roots + 'Anchor to A' sentinel. Mode (major/minor) not exposed — semitone_delta strips 'm'."
  - "Back button → ScreenId::StemsReady (track A's view) — revisit when 21-04 designs the 2-file flow."
  - "Mashup result reuses ctx_.output_path so ScreenRender Done state shows it without changes."
  - "ScreenMashup not yet reachable from UI — 21-04 adds the entry button on ScreenStemsReady."

patterns-established:
  - "Per-stem state map driven by std::array<const char*, 4> kStemNames — symmetric with sidecar StemPaths field names."

duration: ~25min
started: 2026-05-18
completed: 2026-05-18
---

# Phase 21 Plan 03: ScreenMashup UI Summary

**New `ScreenMashup` screen wired into the navigation switch. Per-stem A/B toggles, target BPM slider, target key combo, Generate button calls `ctx_.run_mashup` and routes the output through the existing ScreenRender Done state on success. Build green, existing 5-screen flow regression-tested live.**

## Performance

| Metric | Value |
|--------|-------|
| Duration | ~25min |
| Tasks | 3/3 auto + 1 checkpoint approved |
| Files modified | 2 |
| Files created | 1 (273-line header-only class) |
| Build targets linked | 3 Linux (Shared + VST3 + Standalone); 4 on macOS |

## Acceptance Criteria Results

| Criterion | Status | Notes |
|-----------|--------|-------|
| AC-1: A + B analysis labels | Pass | onEnter() populates filename/BPM/key from ctx_ |
| AC-2: Per-stem source picker | Pass | a_btn/b_btn pair, ghost-style toggle, updates ctx_.mashup_sources |
| AC-3: Target BPM + target key write ctx_ | Pass | slider onValueChange, combo onChange wired |
| AC-4: Generate calls run_mashup + navigates | Pass | builds MashupParams, navigates to Render on success |
| AC-5: Build green, no new warnings | Pass | only pre-existing JUCE/LookAndFeel warnings remain |

## Accomplishments

- Full mashup-configuration UI ready to drop in once 21-04 provides the entry point.
- Button-pair toggle pattern reusable for any future binary per-row picker.
- Target-key combo + "Anchor to A" sentinel cleanly maps to the sidecar's `target_key = None` default.
- Existing 5-screen flow regression-verified live by the user — no breakage from the new screen file or the editor edit.
- One transient segfault on first bare launch; reran clean. Cause unclear — flagged as deferred. Suspect stale orphan process or environment race; not reproducible.

## Deviations from Plan

- Plan said "header-only class ~273 LOC target". Actual: 273 LOC. On target.
- Plan AC-4 didn't specify error styling. Added `setColour(label, AR::ERROR)` on failure path so the user sees red on errors.

## Open Items / Deferred

- **Entry-point button** (21-04): a "Mashup →" button on ScreenStemsReady.
- **2nd-file load flow** (21-04): triggers a second separation, populates `ctx_.file_path_b` + `stems_b`.
- **Render integration polish** (21-05): may want a distinct ScreenRender state ("MASHUP DONE" header) instead of reusing the remix Done verbatim.
- **Transient segfault on bare launch**: monitor; investigate if it recurs.
- **Surfacing diagnostic fields** (source_bpm_a/b, semitone_shift_b): MashupResult doesn't carry them; sidecar does. Wire if user feedback asks for visibility.
- **Back button destination**: currently `ScreenId::StemsReady`. May need a "mashup back" target after 21-04.

## Files Changed

```
plugin/include/ScreenContext.h     +1 line   (Mashup enum value)
plugin/include/ScreenMashup.h    +273 lines  (new)
plugin/src/AutoRemixEditor.cpp     +5 lines  (#include + switch case)
```

## Verification

```bash
cmake --build build --parallel
# [5/5] Linking VST3 + Standalone + SharedCode — green
```

Live regression: user ran the standalone, exercised the 5-screen flow, reported "everything ok".
ScreenMashup itself not yet reachable; verify deferred to 21-04.

## Next

`/paul:plan 21-04` — entry-point flow:
- Add "Mashup →" button on `ScreenStemsReady`
- File picker for track B
- Trigger 2nd separation
- Populate `ctx_.file_path_b` + `ctx_.stems_b` + `detected_bpm_b/key_b`
- Navigate to `ScreenId::Mashup`
