---
phase: 10-effect-chain
plan: 02
type: summary
completed: 2026-05-14
commit: pending
---

# 10-02 Summary: Effect Chain Interpreter — ops.py + EffectChainEngine + dispatch routing

## What was built

- `python/server/remix/ops.py` — 6 standalone op functions + `OP_REGISTRY`:
  - `apply_time_stretch`, `apply_pitch_shift` — delegate to `base.py` stereo-safe helpers
  - `apply_reverb` — pedalboard `Reverb` (lazy import)
  - `apply_chop` — periodic-repeat logic (self-contained, no circular deps)
  - `apply_bass_boost` — pedalboard `LowShelfFilter` at 200 Hz (lazy import)
  - `apply_eq_highpass` — pedalboard `HighpassFilter` (lazy import)

- `python/server/remix/chain_interpreter.py` — `EffectChainEngine.process()`:
  - Loads 4 stem WAVs as stereo numpy arrays via librosa
  - Executes `preset.effects` in order; resolves targeting (`"all"`, single stem, list)
  - `"all"` targeting: sum all 4 stems → apply op → divide by 4 back into each stem
  - Length reconciliation after `time_stretch` (min_len truncation)
  - Applies `stem_mix` weights after chain completes
  - Writes result via soundfile; returns output_path

- `python/server/main.py` — dispatch routing:
  - `preset = _presets.get(req.engine_id)`
  - `if preset and preset.effects:` → `EffectChainEngine().process(...)`
  - `else:` → existing `get_engine(...).process(...)` unchanged

## Acceptance criteria results

| AC | Result |
|----|--------|
| AC-1: ops.py provides all 6 op functions | ✓ `len(OP_REGISTRY) == 6` |
| AC-2: EffectChainEngine processes non-empty effects list | ✓ imports clean |
| AC-3: "all" stems targeting sums then distributes | ✓ sum → op → /4 |
| AC-4: Individual stem targeting leaves others untouched | ✓ only named stems modified |
| AC-5: Dispatch routes to interpreter when effects non-empty | ✓ `preset.effects` branch |
| AC-6: Dispatch falls through to engine when effects empty | ✓ existing path unchanged |
| AC-7: Unknown op raises ValueError | ✓ `OP_REGISTRY.get(op_name)` → ValueError |

## Verification

```
ops OK: ['time_stretch', 'pitch_shift', 'reverb', 'chop', 'bass_boost', 'eq_highpass']
chain_interpreter OK
main dispatch OK
13 passed, 5 warnings in 2.60s
```

## Next

- 10-03: pytest tests for EffectChainEngine + release prep
