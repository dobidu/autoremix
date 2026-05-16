---
phase: 15-musical-ui
plan: 01
type: summary
status: complete
date: 2026-05-16
---

# 15-01 Summary — Analysis Endpoint + AudioBridge::analyzeFile

## What was built

### Python sidecar
- `detect_key(audio, sr) -> str` added to `python/server/remix/analysis.py`
  - Krumhansl-Schmuckler chroma profiles (major + minor) over 12 roots
  - Returns note name + "" for major, "m" for minor (e.g. "C", "Am", "F#", "Ebm")
- `GET /api/v1/analyze?path=<absolute>` added to `python/server/main.py`
  - Validates path exists; raises 400 if not
  - Loads first 60s mono via `librosa.load(..., duration=60.0)`
  - Returns `{bpm: float, key: str, duration_sec: float}`
  - `librosa` import moved to module level

### C++ plugin
- `FileAnalysis` struct added to `plugin/include/PluginTypes.h`:
  `{float bpm, std::string key, float duration_sec, bool valid()}`
- `AudioBridge::analyzeFile(const juce::String& path)` declared in `AudioBridge.h`
  and implemented in `AudioBridge.cpp`
  - Manual percent-encoding of path (no new deps)
  - GET with 10s timeout
  - Returns `FileAnalysis{}` (zeroed/invalid) on any error
- `<juce_core/juce_core.h>` added to `AudioBridge.h`

## Acceptance criteria results

| AC | Result |
|----|--------|
| AC-1: endpoint returns bpm/key/duration_sec | PASS — blues/mix.wav → `{bpm:61.52, key:"G", duration_sec:341.629}` |
| AC-2: analyzeFile compiles and returns populated struct | PASS — build clean |
| AC-3: key field from valid note names | PASS — returns "G" (major), tested manually |
| 400 on missing file | PASS |
| 50 pytest pass | PASS |

## Files modified
- `python/server/remix/analysis.py` — added `detect_key`, `_NOTE_NAMES`, `_MAJOR_PROFILE`, `_MINOR_PROFILE`
- `python/server/main.py` — added `librosa` import, `GET /api/v1/analyze` handler
- `plugin/include/PluginTypes.h` — added `FileAnalysis` struct
- `plugin/include/AudioBridge.h` — added `juce_core` include, `analyzeFile` declaration
- `plugin/src/AudioBridge.cpp` — added `analyzeFile` implementation

## Deferred
- None. All scope completed as planned.

## Next
15-02 — UI wiring: detected BPM/key labels, BPM slider (40–200), chop in beats
