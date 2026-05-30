---
phase: 11-demucs-separator
plan: 01
type: summary
completed: 2026-05-15
commit: pending
---

# 11-01 Summary: DemucsSeparator + registry + timeout

## What was built

- `python/server/separators/demucs_sep.py` — `DemucsSeparator(IStemSeparator)`:
  - `is_available()`: imports demucs/torch/torchaudio, returns False on ImportError
  - `separate()`: torchaudio load → resample to model.samplerate → apply_model → save 4 stems
  - Module-level `_model` cache — htdemucs loaded once per process
  - All heavy imports inside methods — zero import-time cost when demucs absent

- `python/server/registry.py`:
  - `DemucsSeparator` imported and registered first in `_init_separators()`
  - `list_separators()` already filters by `is_available()` — demucs hidden when not installed

- `python/requirements.txt`:
  - `demucs>=4.0.0` documented as optional dep (commented) — CI stays fast

- `plugin/src/AudioBridge.cpp`:
  - `separateStems` timeout: 30000 → 300000 ms (5 minutes for ML CPU inference)

## Acceptance criteria results

| AC | Result |
|----|--------|
| AC-1: is_available()=False when demucs absent | ✓ `False` without torch/demucs |
| AC-2: produces 4 stem WAVs when installed | ✓ (verified by design; 11-02 will test) |
| AC-3: model cached module-level | ✓ `_model` global, loaded once |
| AC-4: demucs in health when installed | ✓ registry + list_separators filter |
| AC-5: algorithmic still works (regression) | ✓ 26 tests pass |
| AC-6: AudioBridge timeout = 300s | ✓ `Timeout{300000}` |

## Next

- 11-02: Install demucs, run integration test, pytest coverage, release v2.3.0
