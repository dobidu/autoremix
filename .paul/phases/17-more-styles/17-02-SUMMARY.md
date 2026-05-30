---
phase: 17-more-styles
plan: 02
type: summary
status: complete
date: 2026-05-16
---

# 17-02 Summary — Tests + Version + README

## What was built

### python/tests/test_presets.py
4 new tests appended:
- `test_new_styles_present` — phonk, jersey_club, nightcore all in preset list
- `test_effect_chain_presets_have_effects` — all engine=effect_chain presets have non-empty effects
- `test_phonk_effect_chain_structure` — phonk has time_stretch + bass_boost ops
- `test_nightcore_sped_up` — time_stretch factor > 1.0, pitch_shift semitones > 0

### python/server/models.py (unplanned fix)
`PresetSummary` was missing `engine` and `effects` fields. Added:
- `engine: str = "chopped_screwed"`
- `effects: list = []`

### python/server/main.py
- `list_presets()` updated to pass `engine=p.engine, effects=p.effects` to `PresetSummary`
- Version bumped: `"0.3.0"` → `"0.4.0"`

### README.md
- Version header: v2.5.0 → v2.6.0
- Preset combo description: "6 built-ins" → "9 built-ins"
- Preset table location count: "Built-in (6 presets)" → "Built-in (9 presets)"
- Built-in effect-chain presets table: added phonk, jersey_club, nightcore rows
- Test count: 50 → 54; description updated

## Acceptance criteria results

| AC | Result |
|----|--------|
| AC-1: 54 tests pass | PASS |
| AC-2: version "0.4.0" in main.py | PASS |
| AC-3: README v2.6.0 + preset table + 54 tests | PASS |

## Unplanned fix
`PresetSummary.engine` and `PresetSummary.effects` were never in the API response.
This was a latent bug (hidden because tests only checked `params` / `stem_mix`).
Fixed as part of this plan.

## Files modified
- `python/tests/test_presets.py` — 4 new tests
- `python/server/models.py` — engine + effects fields on PresetSummary
- `python/server/main.py` — list_presets passes engine+effects; version bump
- `README.md` — version, preset count, preset table, test count

## Phase 17 complete
9 built-in presets (3 legacy + 3 Phase-14 effect-chain + 3 new). 54 tests green.
