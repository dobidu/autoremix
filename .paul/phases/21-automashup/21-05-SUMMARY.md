---
phase: 21-automashup
plan: 05
subsystem: full-stack
tags: [mashup, presets, templates, pedalboard, reverb, juce, fastapi]

requires:
  - phase: 21-automashup
    provides: MashupRequest schema + MashupEngine + ScreenMashup (21-01..21-04)
provides:
  - MashupPreset Pydantic model + 5 feel-knob fields on MashupRequest
  - MashupPresetLoader (mirrors PresetLoader)
  - 8 built-in template JSONs in sidecar/server/mashup_presets/
  - GET /api/v1/mashup_presets endpoint
  - MashupPresetInfo C++ struct + AudioBridge::getMashupPresets()
  - ScreenContext mashup_presets + selected_mashup_preset_idx + 5 feel-knob fields
  - Editor fetches mashup presets at startup
  - ScreenMashup: TEMPLATE combo at top, Advanced ▾ disclosure with 5 sliders
  - 4 new pytest cases (65 total green)
affects: [phase 21-06 release]

tech-stack:
  added: []
  patterns:
    - "Template combo applies preset on selection: copies gains_a/b + feel knobs + targets to ctx_, refreshes every slider"
    - "Advanced ▾ toggle button uses ▾/▴ UTF-8 indicators (\xE2\x96\xBE / \xE2\x96\xB4)"
    - "Feel-knob defaults preserve pre-21-05 behavior (bpm_modifier=1.0, all reverb/pitch/hpf=0)"
    - "MashupEngine applies stretches per-track (A's stretch_a + B's stretch from anchor BPM × modifier)"

key-files:
  created:
    - sidecar/server/mashup_presets/__init__.py
    - sidecar/server/mashup_presets/loader.py
    - sidecar/server/mashup_presets/vocal_acapella.json
    - sidecar/server/mashup_presets/drum_swap.json
    - sidecar/server/mashup_presets/slowed_mashup.json
    - sidecar/server/mashup_presets/nightcore_mashup.json
    - sidecar/server/mashup_presets/dub_echo.json
    - sidecar/server/mashup_presets/instrumental_layer.json
    - sidecar/server/mashup_presets/bass_swap.json
    - sidecar/server/mashup_presets/frankenstein.json
    - sidecar/tests/test_mashup_presets.py
  modified:
    - sidecar/server/models.py
    - sidecar/server/remix/mashup.py
    - sidecar/server/main.py
    - plugin/include/PluginTypes.h
    - plugin/include/AudioBridge.h
    - plugin/src/AudioBridge.cpp
    - plugin/include/ScreenContext.h
    - plugin/src/AutoRemixEditor.cpp
    - plugin/include/ScreenMashup.h

key-decisions:
  - "Template combo at top, NOT replacing the per-stem mixer — templates are starting points, mixer stays manual."
  - "5 feel knobs picked for max creative range / min knob count: bpm_modifier, master_pitch_offset_semi, master_reverb_mix, master_reverb_room, highpass_b_hz."
  - "Advanced ▾ collapsed by default; reveals 5 feel-knob sliders. Keeps default screen clean."
  - "Track A also gets stretched when bpm_modifier ≠ 1 (since target = anchor × modifier diverges from A's native BPM). Stretch_a clamped separately to [0.5, 2.0]."
  - "Pedalboard for highpass + reverb — avoids hand-rolling biquads, matches existing ops.py pattern."
  - "Order of master FX: pitch_shift → reverb → normalize_lufs. Reverb after pitch so the reverb tail isn't double-shifted."
  - "MashupPreset stored as JSON files (BUILTIN_DIR + USER_DIR). User overrides on id collision — same pattern as PresetLoader."
  - "Anchor mode = anchor_a only in v1 built-ins. anchor_b / average / absolute fields exist in schema for future user templates."

patterns-established:
  - "Editor's startup background thread now fetches: presets, separators, mashup_presets in one round-trip."

duration: ~30min
started: 2026-05-18
completed: 2026-05-18
---

# Phase 21 Plan 05: Mashup Templates Summary

**8 built-in mashup templates instantly configure all 8 stem gains + 5 feel knobs (bpm modifier, master pitch, reverb mix + room, HPF on track B). Sidecar loads from JSON + user dir, C++ fetches at startup, ScreenMashup surfaces via TEMPLATE combo + Advanced ▾ disclosure. 65/65 tests green; user approved live flow.**

## Performance

| Metric | Value |
|--------|-------|
| Duration | ~30min |
| Tasks | 3/3 + checkpoint approved |
| Files created | 10 (loader + __init__ + 8 JSONs + tests) |
| Files modified | 9 |
| Tests | 4 new, 65 total green (was 61) |

## Acceptance Criteria Results

| Criterion | Status | Notes |
|-----------|--------|-------|
| AC-1: Template JSON loads + validates | Pass | curl returns 8 presets, loader handles missing keys |
| AC-2: New feel knobs applied | Pass | bpm_modifier multiplies target, reverb wet level applied, hpf cuts B's lows, master pitch shifts mix |
| AC-3: Defaults preserve behavior | Pass | MashupRequest() has all knobs at no-op defaults |
| AC-4: C++ surfaces templates | Pass | AudioBridge::getMashupPresets fetched at startup; ctx_.mashup_presets populated |
| AC-5: Combo applies template | Pass | applyTemplate() copies all values to ctx_ + refreshes 13 sliders |
| AC-6: Advanced ▾ disclosure | Pass | collapsed by default; toggle reveals 5 sliders; resized() respects state |
| AC-7: Generate uses Advanced values | Pass | params carries all 5 feel knobs to sidecar |
| AC-8: Build + tests green | Pass | 65 pytest; cmake 3 Linux targets link |

## Accomplishments

- Templates as first-class data: JSON files in `sidecar/server/mashup_presets/`, loadable from user dir (`~/.config/autoremix/mashup/`).
- One click on "Slowed Mashup" / "Nightcore Mashup" / "Vocal Acapella" instantly produces a recognizable sonic identity.
- HPF B knob solves the "two-bass mud" problem in layered mashups.
- bpm_modifier × anchor allows "user picks 128, template multiplies × 0.75 = 96" — composable controls.
- Live verified: templates audibly change the mashup character.

## Deviations from Plan

- None. Schema, knob set, and 8 templates shipped as proposed.
- One bonus: Track A also gets a separate `stretch_a` factor when `bpm_modifier != 1.0`, so slowed/nightcore templates affect both tracks. Plan implicitly assumed this; spelled out in implementation.

## Open Items / Deferred

- **Save as Template button** (21-06 or future): UI to write a user's current config to `~/.config/autoremix/mashup/`.
- **anchor_b / average / absolute target modes**: schema supports them; no built-in uses them yet.
- **Per-stem reverb / per-stem pitch**: scope-limited to master FX in v1.
- **README mashup tutorial + screenshots**: 21-06.

## Files Changed

```
sidecar/server/models.py                       +27 lines  (MashupPreset + 5 feel knobs on MashupRequest)
sidecar/server/mashup_presets/__init__.py        +0       (new, empty)
sidecar/server/mashup_presets/loader.py         +35 lines (new)
sidecar/server/mashup_presets/*.json (8 files)  +15-17 ea (new)
sidecar/server/remix/mashup.py                  +40 lines (feel-knob handling + helpers)
sidecar/server/main.py                          +10 lines (import + loader + endpoint + forward)
sidecar/tests/test_mashup_presets.py            +85 lines (4 new pytest cases)
plugin/include/PluginTypes.h                    +27 lines (MashupPresetInfo + 5 feel knobs on MashupParams)
plugin/include/AudioBridge.h                     +3 lines (getMashupPresets decl)
plugin/src/AudioBridge.cpp                      +52 lines (impl + 5 fields in mashup body)
plugin/include/ScreenContext.h                  +10 lines (mashup_presets + selected idx + 5 feel-knob fields)
plugin/src/AutoRemixEditor.cpp                   +8 lines (parallel fetch + ctx_ wiring)
plugin/include/ScreenMashup.h                  +130 lines (TEMPLATE combo + Advanced section + applyTemplate + 5 sliders)
```

## Verification

```bash
cd sidecar && .venv/bin/python -m pytest tests/
# 65 passed in 7.73s

cmake --build build --parallel
# 3 Linux targets linked, no errors, no new warnings

curl -s http://127.0.0.1:17432/api/v1/mashup_presets | jq length
# 8
```

Live: user picked all 8 templates, confirmed slider sync + audibly different outputs. Approved.

## Next

`/paul:plan 21-06` — release prep:
- README mashup tutorial section (5-screen → 6-screen flow)
- Screenshot placeholders
- Version bump 2.6.0 → 3.1.0 (or whatever the current scheme is)
- git tag + release notes
- CI verification
