# Phase 10 — Effect Chain DSL Design Spec

## 1. Overview

The `effects` array in a RemixPreset JSON enables a declarative, per-stem processing pipeline.
When `effects` is non-empty, the preset **bypasses the registered engine** and runs the chain
interpreter instead. Built-in presets (v1.x) keep `effects: []` and are unaffected.

This is **Option B** from the Phase 09 architecture decision: instead of mapping a preset to
a single monolithic `IRemixEngine`, a preset author composes primitive operations (ops) in
sequence — each targeting specific stems, with explicit parameters.

---

## 2. Execution Model

### 2a. Dispatch rule

```
if preset.effects:          # non-empty list
    route → EffectChainEngine.process(stems, preset, output_path)
else:                       # empty list or key absent
    route → registry.get_engine(preset.engine).process(...)   # Phase 09 behaviour
```

### 2b. Stem lifecycle

1. **Load** — four stem WAV files loaded as numpy arrays `shape=[2, N]` (stereo float32).
   Mono files are stacked to stereo: `np.stack([a, a])`.
2. **Chain** — ops executed in list order. Each op receives and returns numpy arrays.
   The working state is a dict `{stem_name: np.ndarray}`.
3. **Mix** — after chain completes, each stem array is multiplied by its `stem_mix` weight
   from the preset (default 1.0), then all stems are summed and length-normalised.
4. **Write** — result written to `output_path` at original sample rate via soundfile.

### 2c. Stem targeting

| `stems` value | What the op sees |
|---------------|-----------------|
| `"all"` | Summed mix of all 4 stems (mix-then-process). Result replaces ALL stem buffers equally (divide by 4 to avoid clipping). |
| `"vocals"` / `"drums"` / `"bass"` / `"other"` | That stem's buffer only. Other stems untouched. |
| `["drums", "bass"]` | Each listed stem processed independently with the same op+params. |

### 2d. Invariants

- Sample rate never changes (no resampling).
- Ops are applied in-place on the working state dict — each op sees the output of the previous.
- `stem_mix` weights are applied **after** the full chain, not inside individual ops.
- An op targeting `"all"` on a chain that has already processed individual stems sees the
  **current** (already-processed) versions of each stem summed.

---

## 3. Op Catalogue

### `time_stretch`

Slow down or speed up audio without changing pitch.

| param | type | range | default | notes |
|-------|------|-------|---------|-------|
| `factor` | float | 0.1 – 4.0 | — | <1 = slower, >1 = faster |

```python
librosa.effects.time_stretch(audio, rate=params["factor"])
# stereo: apply per channel
```

```json
{"op": "time_stretch", "stems": "all", "params": {"factor": 0.70}}
```

---

### `pitch_shift`

Shift pitch in semitones without changing tempo.

| param | type | range | default | notes |
|-------|------|-------|---------|-------|
| `semitones` | float | -24 – 24 | — | negative = lower |

```python
librosa.effects.pitch_shift(audio, sr=sr, n_steps=params["semitones"])
# stereo: apply per channel
```

```json
{"op": "pitch_shift", "stems": ["vocals", "other"], "params": {"semitones": -4.0}}
```

---

### `reverb`

Add algorithmic reverb via pedalboard.

| param | type | range | default | notes |
|-------|------|-------|---------|-------|
| `mix` | float | 0.0 – 1.0 | — | wet/dry ratio |
| `room_size` | float | 0.0 – 1.0 | 0.6 | larger = longer tail |

```python
from pedalboard import Pedalboard, Reverb
pb = Pedalboard([Reverb(room_size=params.get("room_size", 0.6), wet_level=params["mix"])])
pb(audio.astype(np.float32), sr)
```

```json
{"op": "reverb", "stems": "all", "params": {"mix": 0.60, "room_size": 0.7}}
```

---

### `chop`

Periodic silence-and-repeat (the "chop" effect). Every other interval is replaced by a
repeat of the previous chunk.

| param | type | range | default | notes |
|-------|------|-------|---------|-------|
| `interval_ms` | float | 100 – 10000 | — | chop period in milliseconds |

Implementation: reuse `ChoppedAndScrewedEngine._chop()` logic (extract to shared utility).

```json
{"op": "chop", "stems": "all", "params": {"interval_ms": 2000}}
```

---

### `bass_boost`

Shelf EQ boost/cut at 200 Hz.

| param | type | range | default | notes |
|-------|------|-------|---------|-------|
| `db` | float | -24 – 24 | — | positive = boost |

```python
from pedalboard import Pedalboard, LowShelfFilter
pb = Pedalboard([LowShelfFilter(cutoff_frequency_hz=200, gain_db=params["db"])])
pb(audio.astype(np.float32), sr)
```

```json
{"op": "bass_boost", "stems": ["bass", "drums"], "params": {"db": 6.0}}
```

---

### `eq_highpass`

High-pass filter to remove low-end mud from a stem.

| param | type | range | default | notes |
|-------|------|-------|---------|-------|
| `cutoff_hz` | float | 20 – 20000 | — | -3 dB point |

```python
from pedalboard import Pedalboard, HighpassFilter
pb = Pedalboard([HighpassFilter(cutoff_frequency_hz=params["cutoff_hz"])])
pb(audio.astype(np.float32), sr)
```

```json
{"op": "eq_highpass", "stems": ["other", "vocals"], "params": {"cutoff_hz": 300}}
```

---

## 4. Backward Compatibility

| Preset state | Behaviour |
|-------------|-----------|
| `effects` key absent | Engine dispatch (Phase 09 path) |
| `effects: []` (empty) | Engine dispatch (Phase 09 path) — no change from v1.x |
| `effects: [...]` (non-empty) | Chain interpreter; `engine` field ignored |
| `engine` absent + `effects` non-empty | Valid — interpreter runs, engine never consulted |
| `engine` absent + `effects: []` | **Invalid** at runtime — PresetLoader should warn; dispatcher falls back to error response |

Built-in presets (`chopped_screwed.json`, `slowed_reverb.json`, `drum_and_bass.json`) retain
`effects: []` and continue using engine dispatch. No migration needed.

---

## 5. Worked Examples

### Example A — Custom Chopped & Screwed (chain equivalent of built-in engine)

```json
{
  "id": "my_chop",
  "version": "2.0",
  "name": "My Chop",
  "params": {
    "tempo_factor": 0.70, "pitch_shift_semi": -4.0, "reverb_mix": 0.05,
    "chop_interval_ms": 2000.0, "bass_boost_db": 0.0, "drums_tempo_factor": 1.0
  },
  "stem_mix": {"vocals": 1.0, "drums": 1.0, "bass": 1.0, "other": 1.0},
  "effects": [
    {"op": "time_stretch", "stems": "all",  "params": {"factor": 0.70}},
    {"op": "pitch_shift",  "stems": "all",  "params": {"semitones": -4.0}},
    {"op": "chop",         "stems": "all",  "params": {"interval_ms": 2000}},
    {"op": "reverb",       "stems": "all",  "params": {"mix": 0.05}}
  ]
}
```

### Example B — Stems-targeted DnB

Drums double-speed, everything else 1.4×, bass boosted, high-pass on the other stem.

```json
{
  "id": "my_dnb",
  "version": "2.0",
  "name": "My DnB",
  "params": {
    "tempo_factor": 1.4, "pitch_shift_semi": 0.0, "reverb_mix": 0.0,
    "chop_interval_ms": 0.0, "bass_boost_db": 6.0, "drums_tempo_factor": 2.0
  },
  "stem_mix": {"vocals": 1.0, "drums": 1.0, "bass": 1.2, "other": 0.8},
  "effects": [
    {"op": "time_stretch", "stems": ["vocals", "bass", "other"], "params": {"factor": 1.4}},
    {"op": "time_stretch", "stems": ["drums"],                   "params": {"factor": 2.0}},
    {"op": "bass_boost",   "stems": ["bass", "drums"],           "params": {"db": 6.0}},
    {"op": "eq_highpass",  "stems": ["other"],                   "params": {"cutoff_hz": 300}}
  ]
}
```

---

## 6. Implementation Notes for 10-02

**New file:** `python/server/remix/chain_interpreter.py`

```python
class EffectChainEngine:
    """Executes a preset's effects array as a sequential DSL pipeline."""

    def process(self, stems: StemPaths, preset: RemixPreset, output_path: Path) -> Path:
        ...
```

**Routing in `main.py`** (or a new dispatch helper):
```python
if preset.effects:
    return EffectChainEngine().process(stems, preset, output_path)
else:
    engine = get_engine(preset.engine)
    return engine.process(stems, params, output_path)
```

**Shared utility** — extract `_chop()` from `ChoppedAndScrewedEngine` into
`python/server/remix/ops.py` (shared by both engine and interpreter).

**Error handling:**
- Unknown op name → `ValueError(f"Unknown effect op: {op!r}")`
- Missing required param → `ValueError(f"Op {op!r} missing required param {key!r}")`
- Wrong type/range → `ValueError` with descriptive message
- All errors propagate to the `/api/v1/remix` handler → 500 response with detail

**pedalboard note:** already in `requirements.txt`. No new dependencies needed.
**librosa per-channel:** `time_stretch` and `pitch_shift` must loop over channels
(librosa operates on 1-D arrays). Pattern established in `remix/base.py`.
