# AutoRemix v2

JUCE VST3/Standalone plugin + Python FastAPI sidecar for creative audio remixing.
Load a WAV, choose a remix style, get a processed output file.

Stem separation and remix engines are pluggable — new backends register via
`SeparatorRegistry` / `RemixRegistry` without modifying existing code.

## Requirements

- CMake ≥ 3.22, Ninja
- C++20 compiler (GCC 12+ or Clang 14+)
- Python 3.12
- [uv](https://github.com/astral-sh/uv)
- JUCE 7 (included as git submodule)
- libcpr, nlohmann/json (fetched automatically via CMake FetchContent)

## Build

```bash
git clone --recurse-submodules https://github.com/dobidu/autoremix.git
cd autoremix
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
```

Artifacts:
```
build/plugin/AutoRemix_artefacts/Debug/Standalone/AutoRemix
build/plugin/AutoRemix_artefacts/Debug/VST3/AutoRemix.vst3/
```

## Python Sidecar

```bash
cd python
uv venv
uv pip install -r requirements.txt

# Run manually:
python server/main.py        # listens on port 17432

# Or let the plugin auto-start it (set before launching plugin):
export AUTOREMIX_SERVER_PATH=$(pwd)/server/main.py
```

## Running the Plugin (Standalone)

```bash
# With auto-start sidecar:
AUTOREMIX_SERVER_PATH=/path/to/autoremix/python/server/main.py \
  ./build/plugin/AutoRemix_artefacts/Debug/Standalone/AutoRemix
```

Workflow: **Load** → select a WAV/AIFF/FLAC/MP3 → **choose remix style** → **Play** → **Save**

## UI

600 × 400 px dark-themed interface (Dracula-inspired palette):

```
┌──────────────────────────────────────────────────────────┐
│  AutoRemix          Style: [ Chopped & Screwed ▾ ]       │  ← header
├──────────────────────────────────────────────────────────┤
│                                                          │
│  ▓▓▒░▒▒▓▓▒░░▒▒▓▒░▒▒▓▓░▒▒▓▓▒░░▒▒▓▒░▒▒▓▓░  waveform      │  ← 160px
│                                                          │
├────────┬─────────────────────────────────────────────────┤
│  Load  │  filename.wav                                   │
│  Play  │                                                 │  ← controls
│  Save  │                                                 │
├────────┴─────────────────────────────────────────────────┤
│  Ready                                                   │  ← status
└──────────────────────────────────────────────────────────┘
```

- Waveform display updates after loading a file (AudioThumbnail)
- Progress spinner appears during stem separation / remix
- Sidecar connection status shown in status bar

## Running Tests

```bash
cd python
.venv/bin/python -m pytest tests/ -v
```

8 tests covering health, stem separation, all 3 remix engines, and error paths.
CI runs on every push/PR to `v2` and `main` via GitHub Actions.

## Architecture

```
┌─────────────────────────┐     HTTP/JSON      ┌──────────────────────────┐
│  JUCE Plugin (C++20)    │ ◄────────────────► │  Python Sidecar (FastAPI)│
│                         │   port 17432        │                          │
│  PluginProcessor        │                     │  POST /api/v1/separate   │
│  PluginEditor           │                     │  POST /api/v1/remix      │
│  AudioBridge            │                     │  GET  /api/v1/health     │
│                         │                     │                          │
│  IStemSeparator ──────► SeparatorRegistry     │  IStemSeparator impls    │
│  IRemixEngine   ──────► RemixRegistry         │  IRemixEngine impls      │
└─────────────────────────┘                     └──────────────────────────┘
```

**Plugin** manages sidecar lifecycle: spawns `python server/main.py` on startup
(if `AUTOREMIX_SERVER_PATH` is set), kills it on shutdown.

**Pluggability:** implement `IStemSeparator` or `IRemixEngine`, register in the
respective registry — no changes to existing code required.

## Remix Modes

| Engine ID         | Description                                      | Key Params                          |
|-------------------|--------------------------------------------------|-------------------------------------|
| `chopped_screwed` | Slow tempo, pitch down, chop every 2s            | tempo 0.7×, −4 semitones           |
| `slowed_reverb`   | Slow tempo, heavy algorithmic reverb             | tempo 0.75×, reverb mix 60%        |
| `drum_and_bass`   | Double drum tempo, bass boost, high-pass other   | drums 2×, bass +6 dB               |

## Remix Preset System

Remix modes are data-driven JSON presets — no code changes required to add new styles.
The sidecar discovers presets at startup from two locations:

- **Built-in:** `python/server/presets/*.json` (3 included: chopped_screwed, slowed_reverb, drum_and_bass)
- **User:** `~/.config/autoremix/modes/*.json` (user presets override built-ins on ID collision)

The plugin UI fetches available presets from the sidecar at startup via `GET /api/v1/presets`
and populates the style tab bar dynamically. Adding a new preset JSON and restarting the
sidecar is sufficient — no plugin recompile needed.

**Preset JSON format:**
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

The `engine` field maps to a registered remix engine. `effects` is reserved for a future
effect-chain DSL (Phase 10) and currently ignored.

## Known Limitations

- ML stem separator (Spleeter/demucs) deferred — TensorFlow incompatibility with Python 3.12; algorithmic FFT separator used in MVP
- Sidecar lifecycle implemented for Linux/macOS only (Windows `CreateProcess` not yet implemented)
- No real-time processing — offline batch only

## License

GPL-3.0. See [LICENSE](LICENSE).
