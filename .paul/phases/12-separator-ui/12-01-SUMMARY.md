---
phase: 12-separator-ui
plan: 01
subsystem: ui
tags: [juce, combobox, separator, cross-platform, windows]

requires:
  - phase: 11-demucs
    provides: DemucsSeparator registered in sidecar; health endpoint returns available_separators list
provides:
  - SeparatorInfo struct in PluginTypes.h
  - AudioBridge::getAvailableSeparators() from GET /api/v1/health
  - separator_combo_ in plugin header bar (dynamic, populated on sidecar connect)
  - sep_id flows from UI → AudioBridge::separateStems() — no more hardcoded "algorithmic"
  - Cross-platform temp paths via juce::File::tempDirectory
  - StyleTabBar.h deleted
affects: future UI phases, Windows testing

tech-stack:
  added: []
  patterns:
    - "Separator selection: UI combo → AudioBridge → separateStems(sep_id)"
    - "Temp dir: juce::File::getSpecialLocation(tempDirectory).getChildFile(\"autoremix\")"

key-files:
  created: []
  modified:
    - plugin/include/PluginTypes.h
    - plugin/include/AudioBridge.h
    - plugin/src/AudioBridge.cpp
    - plugin/include/PluginEditor.h
    - plugin/src/AutoRemixEditor.cpp
    - CMakeLists.txt
    - python/server/models.py
  deleted:
    - plugin/include/StyleTabBar.h

key-decisions:
  - "getAvailableSeparators() reuses GET /api/v1/health — no new endpoint needed"
  - "separator_combo_ seeded with 'Algorithmic FFT' fallback before sidecar connects"
  - "Separator combo uses AR::CYAN arrow to visually distinguish from style combo (AR::PURPLE)"

patterns-established:
  - "Background thread fetches both presets AND separators in one shot; callAsync updates both combos"

duration: ~20min
started: 2026-05-15T00:00:00Z
completed: 2026-05-15T00:20:00Z
---

# Phase 12 Plan 01: Separator UI + Cross-Platform Temp Dir Summary

**Separator ComboBox added to plugin header — user can now choose Demucs (ML) or Algorithmic FFT at runtime; hardcoded `/tmp/autoremix` replaced with JUCE cross-platform temp dir; v2.4.0.**

## Performance

| Metric | Value |
|--------|-------|
| Duration | ~20 min |
| Tasks | 3 / 3 completed |
| Files modified | 7 |
| Files deleted | 1 |

## Acceptance Criteria Results

| Criterion | Status | Notes |
|-----------|--------|-------|
| AC-1: Separator list from sidecar | Pass | `getAvailableSeparators()` parses health JSON; combo repopulates on connect |
| AC-2: Selected separator used in processing | Pass | `sep_id` from combo passed to `separateStems()`; no hardcoded value |
| AC-3: Cross-platform temp paths | Pass | `juce::File::tempDirectory` used for both stems and remix output dirs |

## Accomplishments

- `SeparatorInfo {id, display_name}` type added to `PluginTypes.h`
- `AudioBridge::getAvailableSeparators()` fetches from `/api/v1/health` with 500ms timeout; maps IDs → display names via static table
- `separator_combo_` at (406, 6, 162, 28) in header; style_combo_ narrowed from 400px → 250px to make room
- Background startup thread now fetches presets + separators concurrently; both combos populated in single `callAsync`
- `onClick_Play()` derives `sep_id` from selected combo index; falls back to `"algorithmic"` if separators list empty
- `/tmp/autoremix` fully replaced: `juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("autoremix")`
- `StyleTabBar.h` deleted — no longer referenced anywhere
- Python sidecar version string bumped `0.2.0 → 0.4.0`; CMakeLists.txt `2.3.0 → 2.4.0`

## Task Commits

| Task | Commit | Description |
|------|--------|-------------|
| All 3 tasks | `32f509e` | feat(ui): separator ComboBox + cross-platform temp dir (v2.4.0) |

## Files Created/Modified

| File | Change | Purpose |
|------|--------|---------|
| `plugin/include/PluginTypes.h` | Modified | Added `SeparatorInfo` struct |
| `plugin/include/AudioBridge.h` | Modified | Added `getAvailableSeparators()` declaration |
| `plugin/src/AudioBridge.cpp` | Modified | Implemented `getAvailableSeparators()`; added `<unordered_map>` include |
| `plugin/include/PluginEditor.h` | Modified | Added `separator_combo_` and `separators_` members |
| `plugin/src/AutoRemixEditor.cpp` | Modified | Separator combo setup; startup thread; temp dir; sep_id in onClick_Play |
| `CMakeLists.txt` | Modified | VERSION 2.3.0 → 2.4.0 |
| `python/server/models.py` | Modified | HealthResponse version "0.2.0" → "0.4.0" |
| `plugin/include/StyleTabBar.h` | Deleted | Orphaned file removed |

## Decisions Made

| Decision | Rationale | Impact |
|----------|-----------|--------|
| Reuse `/api/v1/health` for separator list | Already called; avoids new endpoint | No sidecar changes needed |
| Fallback to `"algorithmic"` when list empty | Sidecar may not be up at UI init | Graceful degradation before connect |
| CYAN arrow for separator combo | Visual distinction from style combo (PURPLE) | Quick scan differentiation |

## Deviations from Plan

None — plan executed exactly as written.

## Issues Encountered

None.

## Next Phase Readiness

**Ready:**
- Separator selection end-to-end: UI → bridge → sidecar
- Cross-platform temp paths in place for Windows
- Phase 12 complete; roadmap open for new phases

**Concerns:**
- Pre-existing `-Wmissing-field-initializers` warning in `applyRemix()` (unrelated to this phase; `ProcessResult` brace init missing `output_path`)

**Blockers:** None

---
*Phase: 12-separator-ui, Plan: 01*
*Completed: 2026-05-15*
