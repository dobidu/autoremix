---
phase: 17-more-styles
plan: 01
type: summary
status: complete
date: 2026-05-16
---

# 17-01 Summary — New Preset JSONs

## What was built

Three new files in `python/server/presets/`:

### phonk.json
- 0.88× time stretch all stems
- -3 semitones on vocals, bass, other
- +8dB bass boost on bass, +4dB on drums
- Reverb on vocals: mix=0.25, room_size=0.85
- Half-beat vocal chop: division=0.5, repeat=2

### jersey_club.json
- 1.25× time stretch on drums, bass, other (vocals untouched — float over fast rhythm)
- Onset vocal chop: min_gap_ms=50, threshold=0.20, repeat=3
- +6dB bass, +3dB drums
- stem_mix drums 1.3, bass 1.2 forward; other 0.6 back

### nightcore.json
- 1.30× time stretch all stems
- +4 semitones on vocals and other only (drums/bass pitch-neutral)
- stem_mix vocals 1.2 forward

## Acceptance criteria results

| AC | Result |
|----|--------|
| AC-1: all three ids in /api/v1/presets | PASS — phonk OK, jersey_club OK, nightcore OK |
| AC-1: total >= 9 | PASS — 9 presets |
| AC-2: non-empty effects arrays | PASS — verified via curl |
| AC-3: 50 tests pass | PASS — no regressions |

## Files created
- `python/server/presets/phonk.json`
- `python/server/presets/jersey_club.json`
- `python/server/presets/nightcore.json`

## Next
17-02 — 4 new tests + version bump + README
