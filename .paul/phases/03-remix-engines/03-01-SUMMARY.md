# Summary: Phase 03, Plan 01 — Remix Engines

## What Was Done

### AudioBridge::applyRemix() implemented
- File: `plugin/src/AudioBridge.cpp`
- POSTs JSON `{vocals_path, drums_path, bass_path, other_path, output_path,
  engine_id, tempo_factor, pitch_shift_semi, reverb_mix, chop_interval_ms,
  bass_boost_db, drums_tempo_factor}` to `/api/v1/remix`
- Timeout: 60s (remix of 3-min track can take 20–40s on CPU)
- Parses `output_path` from response; returns `ProcessResult{success=true}`
- Exception safety: any throw → empty `ProcessResult{success=false}`

### pyrubberband → librosa (no-CLI fix)
- Root cause: pyrubberband requires rubberband CLI binary (`rubberband`/`rubberband-r3`)
- Not installed on dev machine; `apt install rubberband-cli` requires sudo
- Resolution: replaced with `librosa.effects.time_stretch` + `librosa.effects.pitch_shift`
- Helpers `time_stretch()` and `pitch_shift()` added to `python/server/remix/base.py`
- Both helpers are stereo-safe (per-channel processing for 2D arrays)
- All 3 engines updated: removed `import pyrubberband`, use helpers from base

### Smoke test results
- `chopped_screwed` → 739K WAV (slowed 0.7x, pitch -4 semi, chopped)
- `slowed_reverb`   → 739K WAV (slowed 0.75x, pitch -2 semi, heavy reverb)
- `drum_and_bass`   → 517K WAV (drums 2x, bass boosted, vocals pitch-up)

## Verification Results

- [x] AC-1: applyRemix() POSTs correct JSON, parses output_path
- [x] AC-2: cmake --build exits 0 (VST3 + Standalone, 4/4 targets)
- [x] AC-3: all 3 engines produce valid WAV from test stems

## Decisions Made

- pyrubberband dropped in favor of librosa (no external CLI dependency)
- Stereo processing: per-channel time_stretch/pitch_shift (librosa limitation)

## Files Modified

- `plugin/src/AudioBridge.cpp` — applyRemix() implementation
- `python/server/remix/base.py` — added `time_stretch()` and `pitch_shift()` helpers
- `python/server/remix/chopped_screwed.py` — pyrubberband → librosa helpers
- `python/server/remix/slowed_reverb.py` — pyrubberband → librosa helpers
- `python/server/remix/drum_and_bass.py` — pyrubberband → librosa helpers
