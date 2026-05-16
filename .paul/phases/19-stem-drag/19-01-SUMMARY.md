---
phase: 19-stem-drag
plan: 01
status: complete
date: 2026-05-16
---

# 19-01 Summary — Stem Drag-to-DAW

## What was built
- `ScreenStemsReady.h`: mouseDown + mouseDrag overrides enable OS-level file drag from stem rows
- `stemRowAtY(int y)`: maps y coordinate → row index [0-3] accounting for 40px section header
- `stemPath(int idx)`: maps index → juce::String path from ctx_.stems (vocals/drums/bass/other)
- 8px drag threshold before triggering; guards for ctx_.stems.valid and file existence
- drag_stem_idx_ reset to -1 before calling performExternalDragDropOfFiles (prevents re-trigger)
- Buttons/sliders in right column unaffected — JUCE child components consume their own mouse events

## Decisions
- Drag area: waveform + name column (left side). Right col buttons/sliders naturally excluded since children consume those events.
- canMoveFiles=false: always copy semantics — stems stay in temp dir
- No visual affordance added (drag cursor / highlight): discoverable by feel; avoids UI complexity

## Acceptance criteria
- [x] Drag from waveform/name area initiates OS file drag of stem WAV
- [x] Guards: no drag if stems invalid or file missing
- [x] Right col controls unaffected
- [x] Build clean: 4/4 targets
