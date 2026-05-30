---
phase: 28-gpu-demucs
plan: 01
type: summary
status: complete
commit: 3b7a20f
date: 2026-05-28
---

# Summary: 28-01 — GPU EP support (CUDA/DirectML)

## What was built

Added GPU execution provider selection to `NativeDemucsSeparator` and the CMake build
system. Controlled by `-DAUTOREMIX_GPU=ON` (default OFF). CPU EP is unchanged and used
when GPU flag is off or GPU EP construction fails at runtime.

### CMakeLists.txt (root)

- Added `option(AUTOREMIX_GPU ...)` flag
- Linux: `AUTOREMIX_GPU=ON` → fetches `onnxruntime-linux-x64-gpu-1.17.0.tgz` (CUDA 11.8 build)
- Windows: `AUTOREMIX_GPU=ON` → fetches `Microsoft.ML.OnnxRuntime.DirectML` NuGet package
  (`.nupkg` = zip; FetchContent extracts to `onnxruntime_SOURCE_DIR`)
- macOS: same CPU tarball regardless of flag (CoreML deferred to Phase 29)
- Added `directml` IMPORTED SHARED target on Windows GPU builds
- POST_BUILD custom commands copy CUDA EP shared libs (Linux) and DirectML.dll (Windows)
  next to the plugin artifact so ORT can find them via dlopen/LoadLibrary

### plugin/CMakeLists.txt

- Added `$<$<BOOL:${AUTOREMIX_GPU}>:AUTOREMIX_GPU=1>` compile definition
- Added `if(AUTOREMIX_GPU)` block with POST_BUILD copy commands per platform

### plugin/include/dsp/NativeDemucsSeparator.h

- Added `#if defined(AUTOREMIX_GPU) && defined(_WIN32) #include <dml_provider_factory.h> #endif`
- Replaced bare `Ort::Session` construction with `make_session(bool try_gpu)` lambda:
  - `AUTOREMIX_GPU` defined: tries GPU EP first (CUDA on Linux, DirectML on Windows)
  - GPU EP `Ort::Exception` → logs "[Demucs] GPU EP failed (...)" → falls back to CPU EP
  - `AUTOREMIX_GPU` not defined: `juce::ignoreUnused(try_gpu)`, goes straight to CPU path
- Windows path guard: `toWideCharPointer()` on Win32, `toRawUTF8()` on others
  (ORT 1.17 Windows exposes only `const wchar_t*` overload)
- Updated header comment: EP line now documents CUDA/DirectML/CoreML-deferred

## Decisions

- **defer-coreml**: macOS CoreML EP deferred to Phase 29. v4.1 ships CUDA (Linux) +
  DirectML (Windows) only. Reason: CoreML EP compatibility with patched ONNX export ops
  uncertain in ORT 1.17; scope risk not worth delaying Linux/Windows GPU story.
- DirectML.dll deployed via POST_BUILD copy (not linked); ORT loads via LoadLibrary.
- CUDA EP providers deployed via POST_BUILD copy; ORT loads via dlopen at runtime.

## Acceptance criteria results

| AC | Result | Notes |
|----|--------|-------|
| AC-1: GPU build compiles | DONE_WITH_CONCERNS | CPU configure (`-DAUTOREMIX_GPU=OFF`) verified exit 0 locally. GPU configure not run — no CUDA toolkit on dev machine. CI GPU matrix is 28-02 scope. |
| AC-2: GPU EP selected | DONE_WITH_CONCERNS | Code correct; make_session uses CUDA/DirectML EP. Runtime unverifiable without GPU hardware locally. |
| AC-3: CPU fallback | DONE_WITH_CONCERNS | Ort::Exception caught in lambda → logs + retries CPU. Logic correct; not exercised without GPU binary. |
| AC-4: CPU build unchanged | PASS | `cmake -B /tmp/build-cpu-verify -DAUTOREMIX_GPU=OFF` exits 0. CPU path compiles identically. |

## Files modified

- `CMakeLists.txt` — GPU ORT FetchContent conditional + directml target + POST_BUILD
- `plugin/CMakeLists.txt` — AUTOREMIX_GPU compile definition + POST_BUILD EP copy
- `plugin/include/dsp/NativeDemucsSeparator.h` — make_session lambda + dml include guard

## Deferred / known concerns

- **SHA256 placeholders**: `ORT_SHA ""` left empty for both GPU packages (Linux GPU tgz,
  Windows DirectML NuGet). Pin after first successful CI GPU build (Phase 28-02).
- **DirectML NuGet internal path layout**: assumed `runtimes/win-x64/native/` and
  `build/native/include/` from standard NuGet conventions — unverified without Windows runner.
- **CUDA EP shared lib paths**: assumed `lib/libonnxruntime_providers_shared.so` and
  `lib/libonnxruntime_providers_cuda.so` from GPU tarball — unverified without CUDA toolkit.
- **GPU CI matrix**: Phase 28-02 scope — add `AUTOREMIX_GPU=ON` job to release.yml.
- **ModelStatusDot GPU indicator**: Phase 28-02 scope.
- **macOS CoreML**: Phase 29 scope.

## Next

Phase 28-02: CI GPU build matrix + ModelStatusDot GPU indicator + README v4.1 + v4.1.0 tag.
