---
phase: 29-coreml
plan: 01
type: summary
status: complete
commits: b24db1c, 2344a29
date: 2026-05-29
---

# Summary: 29-01 — macOS CoreML EP

## What was built

### plugin/include/dsp/NativeDemucsSeparator.h

- Added `#if defined(AUTOREMIX_GPU) && defined(__APPLE__) && defined(AUTOREMIX_COREML)`
  guard for `coreml_provider_factory.h`
- Added CoreML EP path in `make_session` lambda:
  `#elif defined(__APPLE__) && defined(AUTOREMIX_COREML)`
  → `OrtSessionOptionsAppendExecutionProvider_CoreML(gpu_opts, 0)`
  Platform chain is now complete: Win32 → DML, Linux → CUDA, macOS → CoreML, else → CPU

### plugin/CMakeLists.txt

- Added `$<$<AND:$<BOOL:${AUTOREMIX_GPU}>,$<PLATFORM_ID:Darwin>>:AUTOREMIX_COREML=1>`
  to compile definitions — only defined on macOS GPU builds

### .github/workflows/release.yml

- Added `build-macos-gpu` job (macos-14, `-DAUTOREMIX_GPU=ON`)
- `publish-release` now needs 6 jobs and attaches 6 artifacts (3 CPU + 3 GPU)
- Artifact: `AutoRemix-macos-gpu-vX.Y.Z.zip`

### README.md

- GPU table macOS row: "CPU (CoreML planned v4.2)" → "CoreML EP (Neural Engine) — macOS 12+ on Apple Silicon"
- Artifact list updated to include macos-gpu zip

### CMakeLists.txt

- VERSION 4.1.0 → 4.2.0

## Acceptance criteria results

| AC | Result | Notes |
|----|--------|-------|
| AC-1: CoreML compiles on macOS GPU build | PASS | Verified by user on Mac (checkpoint approved) |
| AC-2: CoreML EP selected on Apple Silicon | PASS | User confirmed "[Demucs] GPU EP session — OK" in log |
| AC-3: Linux/Windows GPU builds unaffected | PASS | No changes to those paths |
| AC-4: Version 4.2.0 + README updated | PASS | CMakeLists.txt + README GPU table confirmed |

## Decisions

- `coreml_provider_factory.h` ships in `onnxruntime-osx-universal2` tarball `include/` — no new dep
- CoreML EP flags=0 (default) — ORT picks best subgraph delegation automatically
- Intel Mac: CoreML EP throws → CPU EP fallback (same as other platforms)
- `AUTOREMIX_COREML=1` compile definition only on Darwin + AUTOREMIX_GPU=ON

## Files modified

- `plugin/include/dsp/NativeDemucsSeparator.h` — CoreML include guard + session path
- `plugin/CMakeLists.txt` — AUTOREMIX_COREML generator expression
- `.github/workflows/release.yml` — build-macos-gpu job + publish-release
- `README.md` — GPU table + artifact list
- `CMakeLists.txt` — VERSION 4.2.0
- `.paul/ROADMAP.md` — Phase 29 COMPLETE

## Next

Phase 29 complete. v4.2.0 tag pending. GPU EP complete across all 3 platforms.
Remaining deferred: SHA256 pinning for GPU ORT packages, macOS signing/notarization, real-time processing.
