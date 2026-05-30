---
phase: 14-musical-chop
plan: 04
subsystem: ui
tags: [juce, combobox, chop-mode, python-api, remix-params]

requires:
  - phase: 14-02
    provides: 5 musical chop ops in OP_REGISTRY
  - phase: 14-03
    provides: effect-chain presets with non-empty effects arrays

provides:
  - chop_mode field in RemixRequest (Python) + RemixParams (C++)
  - _CHOP_MODE_OPS injection logic in POST /api/v1/remix
  - chop_mode_combo_ (6-item ComboBox) in plugin editor
  - chop_slider_ dimmed when non-Fixed mode selected

affects: []

tech-stack:
  added: []
  patterns:
    - "chop_mode injected only when preset.effects non-empty — legacy engine path unchanged"
    - "active_preset = copy.copy(preset) prevents _presets dict mutation"
    - "params.chop_mode set after struct brace-init (C++ string field assignment)"

key-files:
  created: []
  modified:
    - python/server/models.py
    - python/server/main.py
    - plugin/include/PluginTypes.h
    - plugin/src/AudioBridge.cpp
    - plugin/include/PluginEditor.h
    - plugin/src/AutoRemixEditor.cpp

key-decisions:
  - "Injection only for effect-chain presets (effects non-empty) — legacy engine path left unchanged to avoid mixing two processing systems"
  - "copy.copy(preset) shallow copy sufficient — effects list rebuilt as new list before assignment"
  - "chop_mode appended (not prepended) to effects — runs after existing ops so existing preset behaviour is preserved"

patterns-established:
  - "chop_mode_combo_ uses index 0='fixed' convention; kChopModes[] array maps index→string"

duration: ~20min
started: 2026-05-16T00:00:00Z
completed: 2026-05-16T00:00:00Z
---

# Phase 14 Plan 04: Chop Mode UI + API Wiring Summary

**`chop_mode` ComboBox (6 modes) added to plugin editor; field flows through RemixParams → AudioBridge JSON → sidecar, injecting the selected musical chop op into effect-chain presets at remix time.**

## Performance

| Metric | Value |
|--------|-------|
| Duration | ~20 min |
| Tasks | 3 completed |
| Files modified | 6 |
| Tests | 23 passed (no regressions) |
| Build | Green (pre-existing -Wshadow only) |

## Acceptance Criteria Results

| Criterion | Status | Notes |
|-----------|--------|-------|
| AC-1: chop_mode flows editor → sidecar JSON | Pass | kChopModes[] + params.chop_mode sent via AudioBridge |
| AC-2: sidecar injects chop op for effect-chain presets | Pass | copy.copy + list append; original preset unmutated |
| AC-3: legacy presets unaffected | Pass | empty effects branch unchanged |
| AC-4: Fixed mode keeps slider enabled | Pass | combo index 0 → isFixed=true → slider alpha 1.0 |
| AC-5: Plugin builds without error | Pass | Green, only pre-existing warnings |

## Files Created/Modified

| File | Change | Purpose |
|------|--------|---------|
| `python/server/models.py` | Modified | `chop_mode: str = "fixed"` added to RemixRequest |
| `python/server/main.py` | Modified | `_CHOP_MODE_OPS` dict + injection in remix endpoint |
| `plugin/include/PluginTypes.h` | Modified | `chop_mode` field added to RemixParams |
| `plugin/src/AudioBridge.cpp` | Modified | `{"chop_mode", params.chop_mode}` in remix JSON body |
| `plugin/include/PluginEditor.h` | Modified | `juce::ComboBox chop_mode_combo_` member added |
| `plugin/src/AutoRemixEditor.cpp` | Modified | ComboBox setup, slider dimming, chop_mode_str in onClick_Play |

## Decisions Made

| Decision | Rationale | Impact |
|----------|-----------|--------|
| Inject only when `preset.effects` non-empty | Mixing legacy engine output with a new op would require re-implementing the legacy engine in EffectChainEngine; out of scope | chop_mode selector visually works but has no audio effect on chopped_screwed/slowed_reverb/drum_and_bass — acceptable MVP limitation |
| Append chop op to end of effects chain | Existing preset ops (time_stretch, pitch_shift) run first; chop operates on already-processed audio | Correct — chop on slowed/pitched audio sounds more musical |
| `copy.copy()` shallow copy | Effects list is rebuilt as new list before assignment; deep copy unnecessary | Preset mutation safely avoided |

## Deviations from Plan

None — all 3 tasks executed exactly as specified.

## Issues Encountered

None.

## Next Phase Readiness

**Ready:**
- Phase 14 complete — all 4 plans delivered
- 5 musical chop ops, 3 new presets, full UI control end-to-end
- 23 tests green

**Known limitation:**
- chop_mode selector has no effect when legacy preset (empty effects) selected — status_lbl does not warn user. Could add a note in a future polish phase.

**Blockers:** None

---
*Phase: 14-musical-chop, Plan: 04*
*Completed: 2026-05-16*
