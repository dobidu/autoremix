---
phase: 13-interactive-mixing
plan: 03
subsystem: ui
tags: [juce, fastapi, preset, pydantic, modal-dialog]

requires:
  - phase: 13-01
    provides: stem gain fields in RemixParams (vocals_gain etc.) used in savePreset body
  - phase: 09
    provides: PresetLoader USER_DIR, RemixPreset/PresetParams/StemMix models, _presets dict

provides:
  - POST /api/v1/presets endpoint — writes JSON to ~/.config/autoremix/modes/, updates _presets
  - AudioBridge::savePreset(name, params) C++ bridge method
  - "Save" button in plugin header; AlertWindow name dialog; style_combo_ refresh on success

affects: [future-preset-management, merge-to-main]

tech-stack:
  added: []
  patterns:
    - "POST /api/v1/presets writes user preset + updates module-level _presets in-place (no restart)"
    - "AlertWindow runModalLoop() safe because JUCE_MODAL_LOOPS_PERMITTED=1 defined"

key-files:
  created: []
  modified:
    - python/server/models.py
    - python/server/main.py
    - plugin/include/AudioBridge.h
    - plugin/src/AudioBridge.cpp
    - plugin/include/PluginEditor.h
    - plugin/src/AutoRemixEditor.cpp

key-decisions:
  - "_presets updated in-place: no lock needed (FastAPI single-process, GIL covers MVP)"
  - "separator_combo_ narrowed 162→116px to fit Save button at (526,6,44,28) before health_dot_"
  - "AlertWindow modal on UI thread: correct since JUCE_MODAL_LOOPS_PERMITTED=1 in CMakeLists"

patterns-established:
  - "savePreset posts all RemixParams fields; Python reconstructs full RemixPreset JSON"
  - "Background thread: savePreset → getPresets → callAsync repopulate combo pattern"

duration: ~25min
started: 2026-05-16T00:00:00Z
completed: 2026-05-16T00:00:00Z
---

# Phase 13 Plan 03: Save-as-Preset Summary

**`POST /api/v1/presets` writes user preset JSON to `~/.config/autoremix/modes/` and updates `_presets` live; "Save" button in plugin header opens name dialog, posts via `AudioBridge::savePreset()`, and refreshes style_combo_ immediately.**

## Performance

| Metric | Value |
|--------|-------|
| Duration | ~25 min |
| Tasks | 3/3 completed |
| Files modified | 6 |
| Commit | ea7253a |

## Acceptance Criteria Results

| Criterion | Status | Notes |
|-----------|--------|-------|
| AC-1: Save dialog collects name and posts to sidecar | Pass | AlertWindow modal → background thread → savePreset() → POST /api/v1/presets |
| AC-2: Sidecar writes JSON and reloads in-memory list | Pass | `~/.config/autoremix/modes/test_preset.json` written; GET /api/v1/presets returned it immediately |
| AC-3: Style combo refreshes with new preset selected | Pass | callAsync repopulates style_combo_ and selects by name match |

## Files Modified

| File | Change |
|------|--------|
| `python/server/models.py` | Added `CreatePresetRequest` Pydantic model |
| `python/server/main.py` | Added `import re`; added `CreatePresetRequest/RemixPreset/PresetParams/StemMix` to imports; added `POST /api/v1/presets` route |
| `plugin/include/AudioBridge.h` | Added `savePreset(name, params)` declaration |
| `plugin/src/AudioBridge.cpp` | Implemented `savePreset()` — POST with all RemixParams fields |
| `plugin/include/PluginEditor.h` | Added `save_preset_btn`; added `onClick_SavePreset()` declaration |
| `plugin/src/AutoRemixEditor.cpp` | Button setup (PURPLE, 526,6,44,28); separator_combo_ narrowed 162→116; `onClick_SavePreset()` implementation |

## Decisions Made

| Decision | Rationale | Impact |
|----------|-----------|--------|
| `_presets` updated in-place without lock | FastAPI is single-process; GIL covers concurrent reads from GET route; MVP-safe | GET /api/v1/presets reflects new preset within same process run |
| separator_combo_ narrowed 162→116 | Needed 44px for Save button before health_dot_ at x=576 | Separator display names truncated if long; acceptable for MVP |
| `runModalLoop()` on UI thread | `JUCE_MODAL_LOOPS_PERMITTED=1` already in CMakeLists | No Timer/async complexity needed; blocking modal is correct here |
| ID slugified via `re.sub(r"[^a-z0-9]+", "_", ...)` | Simple, safe, deterministic | IDs may collide if two presets share a slugified name — last write wins (acceptable) |

## Deviations from Plan

None — plan executed as specified.

## Issues Encountered

None.

## Next Phase Readiness

**Ready:**
- Phase 13 deliverable complete: stem mix sliders + audio preview + preset save
- All three interactive mixing features functional end-to-end
- Branch v2 ready to merge to main

**Concerns:**
- separator_combo_ display truncation if separator name > ~14 chars (cosmetic only)
- No preset delete/rename UI (explicitly deferred in plan)
- Button-text-stays-"Stop" after transport EOF (noted in 13-02 SUMMARY, still deferred)

**Blockers:** None — Phase 13 complete, merge to main pending.
