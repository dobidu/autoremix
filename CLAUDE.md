# AutoRemix v2 — Claude Code Context

## Project
JUCE audio plugin (VST3/AU, C++20) + Python FastAPI sidecar.
Pluggable stem separation + pluggable remix engines.
GPL-3.0. Build: cmake + ninja. C++ standard: 20.

## Architecture
- plugin/include/ → abstract interfaces (IStemSeparator, IRemixEngine), screens (ScreenEmpty/Separating/StemsReady/ModeParams/Render/Mashup)
- plugin/src/separators/ → concrete separators (Algorithmic FFT, Demucs ML)
- plugin/src/engines/ → concrete remix engines (C&S, SlowedReverb, DnB)
- plugin/src/AudioBridge.cpp → HTTP IPC with Python sidecar (libcpr + nlohmann/json)
- sidecar/server/ → FastAPI sidecar (port 17432 default)
- sidecar/server/presets/ → remix preset JSON files (9 built-ins + user dir)
- sidecar/server/mashup_presets/ → mashup template JSON files (8 built-ins + user dir, Phase 21)
- sidecar/server/remix/mashup.py → MashupEngine (pairwise mashup with feel knobs)

## Pluggability Rules
- NEW separator: implement IStemSeparator, register in SeparatorRegistry
- NEW engine: implement IRemixEngine, register in RemixRegistry
- Registry is factory+singleton. No modification of existing code required.

## Build

cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel

## Python sidecar
cd sidecar
uv venv --python 3.12 && source .venv/bin/activate
uv pip install -r requirements.txt
python -m server.main

## Key types (plugin/include/PluginTypes.h)
- StemPaths: paths to 4 separated WAVs (vocals/drums/bass/other)
- RemixParams: float tempo_factor, pitch_shift_semi, reverb_mix, chop_interval_ms, chop_mode
- MashupParams: file_a, file_b, separator_id, stem_gains_a/b, target_bpm, target_key, 5 feel knobs (Phase 21)
- MashupResult: success, output_path, target_bpm, target_key, length_sec
- MashupPresetInfo: id, name, gains_a/b, target policies, feel knobs (loaded from sidecar)
- ProcessResult: JSON response from Python sidecar

## Conventions
- C++: explicit std::, snake_case files, PascalCase classes, JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR
- Python: snake_case, type hints, Pydantic models, async FastAPI handlers; heavy work via asyncio.to_thread
- IPC: POST /api/v1/separate, POST /api/v1/remix, POST /api/v1/mashup, GET /api/v1/health, GET /api/v1/presets, GET /api/v1/mashup_presets, GET /api/v1/analyze
- Errors: never throw across plugin boundary; use juce::Result

## Current phase: see .paul/STATE.md
## Codebase graph: see graphify-out/GRAPH_REPORT.md (after /graphify .)
