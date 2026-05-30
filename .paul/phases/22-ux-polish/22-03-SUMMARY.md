---
phase: 22-ux-polish
plan: 03
subsystem: plugin-ui
tags: [juce, tooltips, slider-units, ux]

requires: []
provides:
  - juce::TooltipWindow on the editor (750ms hover)
  - Tooltips on health dot, separator combo, M/S/▶ stem buttons
  - "SIDECAR" header label next to the health dot
  - Slider unit suffixes (×, BPM, Hz, semi)
  - SidecarHealthDot now inherits juce::SettableTooltipClient
affects: [22-04 first-run, 22-05 polish]

tech-stack:
  added: []
  patterns:
    - "SettableTooltipClient inheritance for any juce::Component that needs setTooltip"
    - "initAdvancedSlider helper accepts optional suffix arg → uniform unit display"

key-files:
  created: []
  modified:
    - plugin/include/PluginEditor.h
    - plugin/src/AutoRemixEditor.cpp
    - plugin/include/SidecarHealthDot.h
    - plugin/include/ScreenStemsReady.h
    - plugin/include/ScreenMashup.h
    - plugin/include/ScreenModeParams.h

key-decisions:
  - "TooltipWindow at editor scope (750ms hover) — single instance covers all child widgets."
  - "Added explicit SIDECAR text label next to the dot (user request) — removes guesswork without requiring hover."
  - "Reverb sliders intentionally have NO unit suffix — already 0–1 ratios, suffix would be noise."
  - "ScreenStemsReady stem volume sliders have NoTextBox style — suffix would be invisible. Skipped."

patterns-established:
  - "When adding tooltip to a custom Component, also inherit juce::SettableTooltipClient."

duration: ~20min (plus follow-up SIDECAR label request)
started: 2026-05-19
completed: 2026-05-19
---

# Phase 22 Plan 03: Tooltips + Slider Units Summary

**Editor-scope TooltipWindow (750ms). Tooltips on health dot, separator combo, M, S, and play (▶/■) stem buttons. Explicit "SIDECAR" text label next to the dot. Slider unit suffixes: × on stem volumes + tempo mod, BPM on target sliders, semi on pitch sliders, Hz on HPF Track B.**

## Performance

| Metric | Value |
|--------|-------|
| Duration | ~20min |
| Tasks | 2/2 + checkpoint approved |
| Files modified | 6 |
| Files created | 0 |

## Acceptance Criteria Results

| Criterion | Status | Notes |
|-----------|--------|-------|
| AC-1: Tooltips on health dot + separator combo | Pass | hover ~750ms triggers |
| AC-2: Tooltips on M / S / ▶ stem buttons | Pass | per-row tooltips set in setup lambda |
| AC-3: Slider unit suffixes | Pass | ×, BPM, Hz, semi visible in value boxes |
| AC-4: Build green; no regression | Pass | both flows still work; live user approved |

## Accomplishments

- Unlabeled controls are now self-explanatory via hover OR direct label (SIDECAR).
- All numeric slider value boxes communicate their unit at a glance — no need to read the label above to know if a number is BPM, Hz, or a ratio.
- One reusable pattern documented: how to add tooltips to custom Components (SettableTooltipClient inheritance).
- User specifically requested "SIDECAR" label next to the dot; added inline during the checkpoint.

## Deviations from Plan

- **Bonus**: user requested a visible "SIDECAR" text label next to the dot beyond just the tooltip. Plan only specified hover tooltips; this is a small UX upgrade and was added inline before final approval.
- Slightly widened a few value-box text boxes (50→60–70px) to fit the unit suffixes without truncation.

## Open Items / Deferred

- 22-04: First-time orientation
- 22-05: Minor copy/visual polish (Anchor to A → Match A's key, Custom → Free mix, em-dash, render time hint, reveal-in-folder)
- StemsReady stem volume sliders still NoTextBox — value not visible. Could switch to compact TextBoxRight in a future polish if user requests.

## Files Changed

```
plugin/include/PluginEditor.h          +3 lines  (TooltipWindow + sidecar_lbl_)
plugin/src/AutoRemixEditor.cpp        +14 lines  (tooltips + SIDECAR label)
plugin/include/SidecarHealthDot.h      +2 lines  (SettableTooltipClient inheritance)
plugin/include/ScreenStemsReady.h      +3 lines  (M, S, ▶ tooltips)
plugin/include/ScreenMashup.h         +12 lines  (stem suffix, advanced suffix arg, target BPM suffix)
plugin/include/ScreenModeParams.h      +4 lines  (BPM + semi suffixes)
```

## Verification

- `cmake --build build --parallel` → green
- Live: user approved. SIDECAR label visible. All tooltips and suffixes confirmed.

## Next

`/paul:plan 22-04` — first-time orientation (drop zone headline + subtitle).
