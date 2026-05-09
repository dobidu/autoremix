# Summary: Phase 00, Plan 01 — Setup & Scaffolding

## What Was Done

Established green build + running Python sidecar.

### C++ side
- Renamed `plugin/source/` → `plugin/src/`
- Renamed `PluginProcessor.cpp` → `AutoRemixProcessor.cpp`, `PluginEditor.cpp` → `AutoRemixEditor.cpp`
- Created `plugin/include/PluginProcessor.h` and `PluginEditor.h` with correct JUCE class declarations
- Fixed includes in processor/editor (removed old `autoremix/` subdir path)
- Created stub implementations: `AudioBridge.cpp`, `SpleeterSeparator.cpp`, `AlgorithmicSeparator.cpp`, `ChoppedAndScrewedEngine.cpp`, `SlowedReverbEngine.cpp`, `DrumAndBassEngine.cpp`

### Bug fixed in base class headers
- Added `IStemSeparator() = default` to `IStemSeparator.h`
- Added `IRemixEngine() = default` to `IRemixEngine.h`
- Root cause: `JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR` declares a deleted copy ctor (user-declared), which suppresses the implicit default ctor in C++11+. Derived classes need the base default ctor to be present.

### Python side
- Added `__init__.py` to `python/server/`, `python/server/separators/`, `python/server/remix/`
- Set up `python/.venv/` via `uv venv`
- Installed deps; two dependency issues resolved:
  1. `spleeter==2.4.0` conflicts with fastapi's httpx requirement → commented out; install separately in Phase 02 (lazy import already handles graceful unavailability)
  2. `pyrubberband==0.3.0` uses `imp` removed in Python 3.12 → upgraded to `>=0.4.0`

## Verification Results

- [x] `cmake --build build --parallel` exits 0 — VST3 + Standalone artifacts built
- [x] `python -c "from server.main import app"` exits 0
- [x] `GET /api/v1/health` → `{"status":"ok","version":"0.2.0","available_separators":["algorithmic"],"available_engines":["chopped_screwed","slowed_reverb","drum_and_bass"]}`

## Decisions Made

- `spleeter` deferred to Phase 02: resolve httpx conflict then
- `pyrubberband` pinned to `>=0.4.0` (Python 3.12 compat)
- Stubs return empty structs; no actual audio processing

## Files Modified

- `plugin/src/AutoRemixProcessor.cpp` (renamed + fixed includes)
- `plugin/src/AutoRemixEditor.cpp` (renamed + fixed includes)
- `plugin/include/PluginProcessor.h` (new)
- `plugin/include/PluginEditor.h` (new)
- `plugin/include/IStemSeparator.h` (added explicit default ctor)
- `plugin/include/IRemixEngine.h` (added explicit default ctor)
- `plugin/src/AudioBridge.cpp` (new stub)
- `plugin/src/separators/SpleeterSeparator.cpp` (new stub)
- `plugin/src/separators/AlgorithmicSeparator.cpp` (new stub)
- `plugin/src/engines/ChoppedAndScrewedEngine.cpp` (new stub)
- `plugin/src/engines/SlowedReverbEngine.cpp` (new stub)
- `plugin/src/engines/DrumAndBassEngine.cpp` (new stub)
- `python/server/__init__.py` (new)
- `python/server/separators/__init__.py` (new)
- `python/server/remix/__init__.py` (new)
- `python/requirements.txt` (spleeter commented out, pyrubberband unpinned)
