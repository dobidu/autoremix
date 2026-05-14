---
phase: 09-remix-presets
plan: 01
type: summary
completed: 2026-05-13
commit: pending
---

# 09-01 Summary: Remix Preset Format + Built-in Presets

## What was built

- `python/server/presets/schema.json` — JSON Schema draft-07; required fields: `id`, `version`, `name`, `engine`, `params`; optional: `description`, `author`, `tags`, `stem_mix`, `effects`; `params` keys are exact RemixRequest field names; `effects: []` reserved for Phase 10 DSL
- `python/server/presets/chopped_screwed.json` — tempo=0.70, pitch=-4.0, reverb=0.05, chop=2000ms
- `python/server/presets/slowed_reverb.json` — tempo=0.75, pitch=-2.0, reverb=0.60
- `python/server/presets/drum_and_bass.json` — tempo=1.40, pitch=+2.0, bass_boost=6dB, drums_tempo=2×; stem_mix gives bass 1.2× and other 0.8×

## Acceptance criteria results

| AC | Result |
|----|--------|
| AC-1: All 3 presets pass jsonschema validation | ✓ |
| AC-2: Param field names match RemixRequest exactly | ✓ |
| AC-3: Param values match existing hardcoded defaults | ✓ |

## Key decisions locked

- Param names in preset JSON = canonical RemixRequest field names → no translation layer needed in 09-02
- `stem_mix` is optional in schema; 09-02 PresetLoader defaults all stems to 1.0 when absent
- `effects` array is always empty in v1.x — Phase 10 interprets it
- `id` field uses same value as engine `engine_id` property → preset ID = engine ID in Option A

## Next

- 09-02: Python sidecar — PresetLoader + `GET /api/v1/presets` + `POST /api/v1/remix` uses preset params
