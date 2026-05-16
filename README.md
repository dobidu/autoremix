# AutoRemix v2.5.0

JUCE VST3/Standalone plugin + Python FastAPI sidecar for creative audio remixing.
Load a WAV, choose a remix style, get a processed output file.

Stem separation and remix engines are pluggable — new backends register via
`SeparatorRegistry` / `RemixRegistry` without modifying existing code.

---

## Requirements

### Linux / macOS
- CMake ≥ 3.22, Ninja
- C++20 compiler (GCC 12+ or Clang 14+)
- Python 3.12
- [uv](https://github.com/astral-sh/uv)
- JUCE 7 (included as git submodule)
- libcpr, nlohmann/json (fetched automatically via CMake FetchContent)

### Windows
- CMake ≥ 3.22
- Visual Studio 2022 (MSVC) with C++ workload
- Python 3.12
- [uv](https://github.com/astral-sh/uv)
- JUCE 7 (included as git submodule)
- libcpr uses WinHTTP automatically — no libcurl needed

---

## Build

### Linux / macOS

```bash
git clone --recurse-submodules https://github.com/dobidu/autoremix.git
cd autoremix
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
```

### Windows

```bat
git clone --recurse-submodules https://github.com/dobidu/autoremix.git
cd autoremix
cmake -B build -G "Visual Studio 17 2022" -DCMAKE_BUILD_TYPE=Debug
cmake --build build --config Debug --parallel
```

**Artifacts (Linux/macOS):**
```
build/AutoRemix_artefacts/Debug/Standalone/AutoRemix
build/AutoRemix_artefacts/Debug/VST3/AutoRemix.vst3/
```

**Artifacts (Windows):**
```
build\AutoRemix_artefacts\Debug\Standalone\AutoRemix.exe
build\AutoRemix_artefacts\Debug\VST3\AutoRemix.vst3\
```

---

## Python Sidecar

```bash
cd python
uv venv
uv pip install -r requirements.txt

# Run manually:
python server/main.py        # listens on port 17432

# Or let the plugin auto-start it (set before launching plugin):
export AUTOREMIX_SERVER_PATH=$(pwd)/server/main.py   # Linux/macOS
# set AUTOREMIX_SERVER_PATH=C:\path\to\python\server\main.py  # Windows
```

**Optional — Demucs ML separator (recommended for quality):**

```bash
pip install demucs   # ~2 GB download (PyTorch)
```

Without demucs, the algorithmic FFT separator is used. With demucs installed,
`DemucsSeparator` is automatically available at `separator_id: "demucs"`.

---

## Running the Plugin (Standalone)

**Linux/macOS:**
```bash
AUTOREMIX_SERVER_PATH=/path/to/autoremix/python/server/main.py \
  ./build/AutoRemix_artefacts/Debug/Standalone/AutoRemix
```

**Windows:**
```bat
set AUTOREMIX_SERVER_PATH=C:\path\to\autoremix\python\server\main.py
build\AutoRemix_artefacts\Debug\Standalone\AutoRemix.exe
```

Workflow: **Load** → select a WAV/AIFF/FLAC/MP3 → **choose remix style** → **choose chop mode** → **Remix** → **Save**

---

## UI

600 × 400 px dark-themed interface (Dracula-inspired palette):

```
┌─────────────────────────────────────────────────────────────────┐
│  AutoRemix  [Trap Stutter ▼──────────]  [Algorithmic ▼]  [Save]●│  ← header
├─────────────────────────────────────────────────────────────────┤
│  ▓▓▒░▒▒▓▓▒░░▒▒▓▒░▒▒▓▓░                       waveform          │
├────────┬────────────────────────────┬───────────────────────────┤
│  Load  │  Tempo ──●──────── 0.70   │  Vocals ────●──── 1.0     │
│        │  Pitch ─●───────── -4.0   │  Drums  ────●──── 1.0     │
│ Remix  │  Reverb ────●───── 0.05   │  Bass   ────●──── 1.0     │
│        │  [Beat-Aligned ▼]          │  Other  ────●──── 1.0     │
│  Save  │  Chop ms ──────●── 2000   │                            │
│Preview │                            │                            │
├────────┴────────────────────────────┴───────────────────────────┤
│  Ready                                                          │
└─────────────────────────────────────────────────────────────────┘
```

- **Preset combo**: dynamically populated from sidecar (6 built-ins + user presets)
- **Chop mode combo**: Fixed (ms) / Beat-Aligned / Onset-Triggered / Bar-Locked / Energy Gate / Structural — selects musical chop algorithm applied on top of preset effects
- **Stem mix sliders**: per-stem gain 0–2× applied as pre-weighting before remix
- **Chop ms slider**: active only in Fixed (ms) mode; grayed out for musical modes
- Waveform display updates on file load (AudioThumbnail)
- Sidecar health dot: green = connected, red = down
- On **Windows/macOS**: native OS file dialog. On **Linux/WSL2**: JUCE file browser.

---

## Stem Separators

| ID | Method | Quality | Notes |
|----|--------|---------|-------|
| `algorithmic` | FFT band-split | Low — leaky stems | Always available, zero deps |
| `demucs` | Demucs `htdemucs` ML model | High — clean isolation | Requires `pip install demucs` (~2 GB) |

The sidecar exposes only available separators in `GET /api/v1/health`.
`DemucsSeparator.is_available()` returns `False` when torch/demucs are absent —
the sidecar starts and serves normally without them.

Model download (~80 MB) happens automatically to `~/.cache/torch/hub/` on first use.

---

## Remix Engines

| Engine ID | Description | Key Params |
|-----------|-------------|------------|
| `chopped_screwed` | Slow tempo, pitch down, chop every N ms | tempo 0.7×, −4 semitones |
| `slowed_reverb` | Slow tempo, heavy algorithmic reverb | tempo 0.75×, reverb mix 60% |
| `drum_and_bass` | Double drum tempo, bass boost, high-pass other | drums 2×, bass +6 dB |

---

## Remix Preset System

Remix modes are data-driven JSON presets — no code changes required to add new styles.
The sidecar discovers presets at startup from two locations:

| Location | Platform |
|----------|----------|
| `python/server/presets/*.json` | Built-in (6 presets) |
| `~/.config/autoremix/modes/*.json` | Linux / macOS user presets |
| `%APPDATA%\autoremix\modes\*.json` | Windows user presets |

User presets override built-ins on ID collision. Restarting the sidecar picks up
new files — no plugin recompile needed.

**Preset format v1.x (engine-based):**
```json
{
  "id": "my_style",
  "version": "1.0",
  "name": "My Style",
  "engine": "chopped_screwed",
  "params": {
    "tempo_factor": 0.80,
    "pitch_shift_semi": -2.0,
    "reverb_mix": 0.20,
    "chop_interval_ms": 1500.0,
    "bass_boost_db": 0.0,
    "drums_tempo_factor": 1.0
  },
  "stem_mix": { "vocals": 1.0, "drums": 1.0, "bass": 1.0, "other": 1.0 },
  "effects": []
}
```

---

## Effect Chain DSL

Presets can use a declarative `effects` array instead of an engine. When `effects`
is non-empty, the `engine` field is ignored and the chain interpreter runs each step
in sequence.

**Preset format v2.0 (chain-based):**
```json
{
  "id": "my_chain",
  "version": "2.0",
  "name": "My Chain",
  "params": { "tempo_factor": 0.75, "pitch_shift_semi": -2.0, "reverb_mix": 0.1,
              "chop_interval_ms": 0.0, "bass_boost_db": 0.0, "drums_tempo_factor": 1.0 },
  "stem_mix": { "vocals": 1.0, "drums": 1.0, "bass": 1.2, "other": 0.8 },
  "effects": [
    { "op": "time_stretch", "stems": "all",      "params": { "factor": 0.75 } },
    { "op": "reverb",       "stems": "all",      "params": { "mix": 0.20, "room_size": 0.7 } },
    { "op": "bass_boost",   "stems": ["bass"],   "params": { "db": 6.0 } },
    { "op": "eq_highpass",  "stems": ["other"],  "params": { "cutoff_hz": 300 } }
  ]
}
```

**Available ops:**

| Op | Stems target | Key param(s) |
|----|-------------|--------------|
| `time_stretch` | any | `factor` (0.1–4.0, <1 = slower) |
| `pitch_shift` | any | `semitones` (−24–24) |
| `reverb` | any | `mix` (0–1), `room_size` (0–1) |
| `chop` | any | `interval_ms` (100–10000) |
| `bass_boost` | any | `db` (−24–24) |
| `eq_highpass` | any | `cutoff_hz` (20–20000) |
| `chop_beats` | any | `division` (0.25–4.0, fraction of beat), `repeat` (1–8), `offset_beats` (int) |
| `chop_onsets` | any | `min_gap_ms` (20–500), `threshold` (0–1), `repeat` (1–8) |
| `chop_bars` | any | `beats_per_bar` (2–8), `repeat` (1–8) |
| `gate_energy` | any | `threshold_db` (−60–0), `hold_ms` (0–500) |
| `structural_cut` | any | `n_segments` (2–16), `mode` (`"keep"` or `"reverse"`) |

`stems` can be `"all"` (sum all 4 stems → apply → distribute), a single name
(`"vocals"`, `"drums"`, `"bass"`, `"other"`), or a list (`["bass", "drums"]`).

---

## Musical Chop Intelligence

The **Chop Mode** combo in the plugin header selects how effect-chain presets chop audio:

| Mode | Description |
|------|-------------|
| Fixed (ms) | Fixed-interval chop every N milliseconds (original behavior) |
| Beat-Aligned | Cuts at beat positions detected by librosa's beat tracker |
| Onset-Triggered | Cuts at transient onsets (drum hits, note attacks) |
| Bar-Locked | Cuts every N beats (default 4 = one bar) |
| Energy Gate | Silences low-energy regions below threshold |
| Structural | Reorders structural segments (verse/chorus-level form analysis) |

Chop mode applies **only to effect-chain presets** (non-empty `effects` array). Legacy
engine presets (`chopped_screwed`, `slowed_reverb`, `drum_and_bass`) are unaffected.

**Built-in effect-chain presets** (Phase 14):

| Preset | Style | Key ops |
|--------|-------|---------|
| `trap_stutter` | Trap Stutter | chop_beats on vocals (×3), chop_onsets on drums (×2), bass boost |
| `onset_drill` | Onset Drill | chop_onsets on vocals, energy gate on other, chop_beats on bass |
| `structural_loop` | Structural Loop | structural_cut vocals+other (reverse), reverb, time stretch |

---

## Running Tests

```bash
cd python
source .venv/bin/activate          # Linux/macOS
# .venv\Scripts\activate           # Windows
python -m pytest tests/ -v
```

**50 tests** covering: health, algorithmic separation, demucs separation (skipped when
demucs absent), 3 remix engines, 6 legacy effect ops (unit), 5 musical analysis functions
(11 tests), 5 new musical chop ops (12 tests), 5 chain interpreter tests
(including HTTP dispatch), error paths.

CI runs on every push/PR to `v2` and `main` via GitHub Actions (Linux, pytest only —
demucs excluded from CI to avoid 2 GB download).

---

## Architecture

```
┌─────────────────────────┐     HTTP/JSON       ┌──────────────────────────┐
│  JUCE Plugin (C++20)    │ ◄─────────────────► │  Python Sidecar (FastAPI)│
│                         │   port 17432        │                          │
│  PluginProcessor        │                     │  POST /api/v1/separate   │
│  PluginEditor           │                     │  POST /api/v1/remix      │
│  AudioBridge            │                     │  GET  /api/v1/health     │
│                         │                     │  GET  /api/v1/presets    │
│  IStemSeparator ──────► SeparatorRegistry     │                          │
│  IRemixEngine   ──────► RemixRegistry         │  AlgorithmicSeparator    │
│                         │                     │  DemucsSeparator         │
│                         │                     │  ChoppedAndScrewedEngine │
│                         │                     │  SlowedReverbEngine      │
│                         │                     │  DrumAndBassEngine       │
│                         │                     │  EffectChainEngine (DSL) │
└─────────────────────────┘                     └──────────────────────────┘
```

**Plugin** manages sidecar lifecycle:
- **Linux/macOS:** `fork()` + `execl("python3", ...)` on startup, `SIGTERM` on shutdown
- **Windows:** `CreateProcess` (no console window), `TerminateProcess` on shutdown

**Pluggability:** implement `IStemSeparator` or `IRemixEngine`, register in the
respective registry — no changes to existing code required.

---

## Known Limitations

- Algorithmic FFT separator produces rough stems (band-split only). Use `demucs`
  separator for production-quality stem isolation.
- No real-time processing — offline batch only.
- Demucs CPU inference is slow for long tracks (~1–5 min for a 3-minute song).
  GPU acceleration requires CUDA-enabled torch installation.
- AudioBridge separation timeout is 300 s — sufficient for CPU demucs on most tracks.

---

## License

GPL-3.0. See [LICENSE](LICENSE).
