---
phase: 28-gpu-demucs
plan: 02
type: summary
status: complete
commits: a0f4ab1, 8acc287, d7a344e
date: 2026-05-29
---

# Summary: 28-02 — CI GPU matrix + ModelStatusDot + README + v4.1.0

## What was built

### CI GPU build matrix (release.yml)

Added `build-linux-gpu` and `build-windows-gpu` jobs to release.yml:
- Both run with `-DAUTOREMIX_GPU=ON`
- Linux GPU: ubuntu-22.04, CUDA EP ORT tarball fetched via FetchContent
- Windows GPU: windows-latest, DirectML EP ORT NuGet fetched via FetchContent
- `publish-release` needs: expanded to 5 jobs; 5 ZIPs attached to each release
- Artifact naming: `AutoRemix-linux-gpu-vX.zip`, `AutoRemix-windows-gpu-vX.zip`

**Fix during CI:** DirectML.dll is a Windows OS component (Win10 1903+) — the NuGet
package does not bundle it. Removed `add_library(directml ...)` IMPORTED target,
POST_BUILD copy command, and `ORT_DML_LIB_SUBPATH` variable. ORT loads DirectML.dll
from System32 via LoadLibrary automatically.

### GPU runtime indicator (NativeDemucsSeparator.h + ModelStatusDot.h + AutoRemixEditor.cpp)

- `DemucsResult`: added `bool gpu_used = false`
- `separate_demucs()`: `bool gpu_used_` declared before `try {}` block; set to `true`
  in make_session lambda on GPU EP success path; propagated to `result.gpu_used`
- `ModelStatusDot::State`: added `gpu_active` (blue `#4A9EDB` dot)
- `refresh_()`: poll guard extended — won't overwrite `gpu_active` state on 3s timer
- `AutoRemixEditor::separateNative()`: calls `health_dot_.setState(gpu_active)` when
  `res.gpu_used` is true after successful demucs call

### README GPU section + VERSION 4.1.0

- `CMakeLists.txt`: `project(AutoRemix VERSION 4.1.0)`
- `README.md`: new "GPU Acceleration (v4.1+)" section with platform table, build
  commands, pre-built artifact naming, and blue dot indicator note
- Updated line 616: removed "GPU deferred to v4.1" → documents current state
- `ROADMAP.md`: Phase 28 and v4.1 milestone marked ✅ COMPLETE

## Acceptance criteria results

| AC | Result | Notes |
|----|--------|-------|
| AC-1: GPU CI jobs produce artifacts | PASS | build-linux-gpu + build-windows-gpu both green on rc2; publish-release attaches 5 ZIPs |
| AC-2: GPU EP reflected in UI dot | DONE | gpu_active state wired; untestable without GPU hardware locally |
| AC-3: CPU builds unchanged | PASS | build-linux / build-macos / build-windows all green; no regressions |
| AC-4: Version 4.1.0 + README GPU docs | PASS | VERSION confirmed; README GPU section present |

## Issues discovered and fixed

- **DirectML.dll POST_BUILD failure**: `Microsoft.ML.OnnxRuntime.DirectML` NuGet does not
  bundle DirectML.dll — it's a Windows system DLL. Removed bundling entirely (fixed in
  commit 8acc287). ORT finds it in System32 via LoadLibrary at runtime.

## Files modified

- `.github/workflows/release.yml` — GPU CI jobs + publish-release wiring (2 commits)
- `CMakeLists.txt` — directml target removed; VERSION 4.1.0
- `plugin/CMakeLists.txt` — DirectML POST_BUILD block removed
- `plugin/include/dsp/NativeDemucsSeparator.h` — DemucsResult.gpu_used + gpu_used_ bool
- `plugin/include/ModelStatusDot.h` — gpu_active state + poll guard + paint case
- `plugin/src/AutoRemixEditor.cpp` — setState(gpu_active) wiring
- `README.md` — GPU Acceleration section + line 616 update
- `.paul/ROADMAP.md` — Phase 28 → COMPLETE

## Phase 28 complete. Deferred / next.

- **SHA256 pinning**: `ORT_SHA ""` for both GPU packages (Linux GPU tgz, Windows DirectML
  NuGet). Pin after confirming no breaking changes in production.
- **macOS CoreML EP**: Phase 29 scope.
- **NativeDemucsSeparator.h `string` unused-include**: pre-existing clangd diagnostic;
  not introduced by these changes.
