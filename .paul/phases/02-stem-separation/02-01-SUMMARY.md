# Summary: Phase 02, Plan 01 — Stem Separation

## What Was Done

### AudioBridge::separateStems() implemented
- File: `plugin/src/AudioBridge.cpp`
- Posts JSON `{input_path, output_dir, separator_id}` to `/api/v1/separate`
- Parses response with nlohmann/json
- Returns `StemPaths{valid=true}` with 4 filesystem::path fields from response
- Exception safety: any throw → returns empty `StemPaths{valid=false}`
- Timeout: 30s (stem separation can be slow for large files)

Note: Python server ignores `output_dir` from request body; actual output paths
come back in response. C++ correctly uses response paths, not the input arg.

### Spleeter — blocked, resolution path documented
- Root cause: spleeter 2.4.0 requires tensorflow<2.9; no cp312 wheels for TF<2.9
- httpx conflict (fastapi vs spleeter) is secondary — TF is the real blocker
- Resolution: replace spleeter with **demucs** (PyTorch-based, Python 3.12 ok)
- See `libs/demucs.md`. Target: Phase 02.1 (inserted) or Phase 05

## Verification Results

- [x] AC-1: separateStems() contains cpr::Post + nlohmann parse + result.valid=true
- [x] AC-2: cmake --build exits 0 (261/261, VST3 + Standalone)
- [x] AC-3: spleeter blocked, documented with clear resolution path (demucs)
- [x] No regression: server imports ok, health endpoint 200

## Decisions Made

- spleeter → demucs replacement (tensorflow/cp312 incompatibility is hard blocker)
- AudioBridge timeout set to 30s (ML separation on CPU can exceed 10s)

## Files Modified

- `plugin/src/AudioBridge.cpp` — separateStems() real implementation
- `python/requirements.txt` — spleeter comment updated with root cause + path
