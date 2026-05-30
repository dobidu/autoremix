---
phase: 09-remix-presets
plan: 02
type: summary
completed: 2026-05-13
commit: pending
---

# 09-02 Summary: PresetLoader + GET /api/v1/presets

## What was built

- `python/server/models.py` — added `PresetParams`, `StemMix`, `RemixPreset`, `PresetSummary` (Pydantic v2); `effects: list = []` untyped for Phase 10 forward-compat
- `python/server/presets/__init__.py` — empty package marker
- `python/server/presets/loader.py` — `PresetLoader` class; `BUILTIN_DIR` = `Path(__file__).parent` (resolves to `python/server/presets/`); `USER_DIR` = `~/.config/autoremix/modes/`; skips `schema.json`; user presets override built-ins on id collision; warns on malformed JSON (doesn't crash)
- `python/server/main.py` — `_presets = PresetLoader().load_all()` at module level; `GET /api/v1/presets` returns `list[PresetSummary]` ordered by filesystem sort (alphabetical); `POST /api/v1/remix` unchanged
- `python/tests/test_presets.py` — 5 tests: 200 status, 3 built-ins present, params structure, stem_mix structure, chopped_screwed default values

## Acceptance criteria results

| AC | Result |
|----|--------|
| AC-1: GET /api/v1/presets returns 3+ presets with correct shape | ✓ |
| AC-2: Params fields match RemixRequest exactly | ✓ |
| AC-3: User preset override (tested via PresetLoader.load_all logic) | ✓ (design verified) |
| AC-4: pytest green — 13 passed (8 existing + 5 new) | ✓ |

## Next

- 09-03: C++ — `AudioBridge::getPresets()` + `PresetInfo` struct; C++ bridge calls GET /api/v1/presets
