# AutoRemix v2 — Claude Code Context

## Project
JUCE audio plugin (VST3/AU, C++20) + Python FastAPI sidecar.
Pluggable stem separation + pluggable remix engines.
GPL-3.0. Build: cmake + ninja. C++ standard: 20.

## Architecture
- plugin/include/ → abstract interfaces (IStemSeparator, IRemixEngine)
- plugin/src/separators/ → concrete separators (Spleeter via HTTP, Algorithmic FFT)
- plugin/src/engines/ → concrete remix engines (C&S, SlowedReverb, DnB)
- plugin/src/AudioBridge.cpp → HTTP IPC with Python sidecar (libcpr + nlohmann/json)
- python/server/ → FastAPI sidecar (port 17432 default)

## Pluggability Rules
- NEW separator: implement IStemSeparator, register in SeparatorRegistry
- NEW engine: implement IRemixEngine, register in RemixRegistry
- Registry is factory+singleton. No modification of existing code required.

## Build

cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel

## Python sidecar
cd python
uv venv && source .venv/bin/activate
uv pip install -r requirements.txt
python server/main.py

## Key types (plugin/include/PluginTypes.h)
- StemCollection: struct { AudioBuffer vocals, drums, bass, other; }
- RemixParams: struct { float tempo_factor, pitch_shift_semitones, reverb_mix, chop_interval_ms; }
- StemResult: JSON response from Python sidecar

## Conventions
- C++: explicit std::, snake_case files, PascalCase classes, JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR
- Python: snake_case, type hints, Pydantic models, async FastAPI handlers
- IPC: POST /api/v1/separate, POST /api/v1/remix, GET /api/v1/health
- Errors: never throw across plugin boundary; use juce::Result

## Current phase: see .paul/STATE.md
## Codebase graph: see graphify-out/GRAPH_REPORT.md (after /graphify .)
