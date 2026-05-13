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

Workflow: **Load File** → select a WAV/AIFF → **choose remix style** → **Play** → **Save**

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
| `chopped_screwed` | Slow tempo, pitch down, chop every 2s           | tempo 0.7×, −4 semitones           |
| `slowed_reverb`   | Slow tempo, heavy algorithmic reverb             | tempo 0.75×, reverb mix 60%        |
| `drum_and_bass`   | Double drum tempo, bass boost, high-pass other   | drums 2×, bass +6 dB               |

## Known Limitations

- ML stem separator (Spleeter/demucs) deferred — TensorFlow incompatibility with Python 3.12; algorithmic FFT separator used in MVP
- Sidecar lifecycle implemented for Linux/macOS only (Windows `CreateProcess` not yet implemented)
- No real-time processing — offline batch only

## License

GPL-3.0. See [LICENSE](LICENSE).
