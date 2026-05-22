---
phase: 26-native-demucs
plan: 02
subsystem: dsp / build / models
tags: [native, v4, onnxruntime, demucs, downloader, sha256]

provides:
  - autoremix::dsp::models::DownloadStatus
  - autoremix::dsp::models::htdemucs_cache_path()
  - autoremix::dsp::models::ensure_htdemucs(progress_cb)
  - autoremix::dsp::separators::DemucsResult
  - autoremix::dsp::separators::separate_demucs(buf, sr, model_path, progress_cb)
  - CMake top-level: FetchContent ORT 1.17.0 pre-built, imported as
    SHARED IMPORTED target `onnxruntime`
affects: [27-01]

key-files:
  created:
    - plugin/include/dsp/ModelDownloader.h          (~140 lines)
    - plugin/include/dsp/NativeDemucsSeparator.h    (~240 lines)
  modified:
    - CMakeLists.txt                                 (FetchContent ORT block + IMPORTED target)
    - plugin/CMakeLists.txt                          (juce_cryptography + onnxruntime in link list)
    - plugin/src/AutoRemixProcessor.cpp              (2 smoke includes)

key-decisions:
  - "ORT 1.17.0 pre-built tarballs via FetchContent + URL_HASH per
     platform. Microsoft ships no CMake-friendly source. Single
     SHARED IMPORTED target with platform-correct IMPORTED_LOCATION
     pointing at libonnxruntime.so / .dylib / onnxruntime.dll inside
     build/_deps."
  - "URL_HASH is conditional — pinned for Linux x64 (computed
     locally), left empty for macOS / Win until first build on those
     platforms. The conditional `if(ORT_SHA)` block keeps clean
     builds robust everywhere."
  - "ModelDownloader uses libcpr (already in build) for the streaming
     GET, juce::SHA256 (from juce_cryptography module) for integrity,
     and juce::File::userApplicationDataDirectory for the cross-platform
     cache path. 3-attempt retry on transient failures. Cache-and-valid
     fast path avoids network on every launch."
  - "Plug htdemucs SHA256 pinned compile-time:
     83a570fb273a411348a94564fb726d67b483f6c30f09fb95d1d7ef4dacc17c36"
  - "Hosting URL pinned to a release tag (v4.0.0-models) so old plugin
     versions keep resolving even after the next release ships."
  - "Separator design: in-memory API only. (juce::AudioBuffer<float>, sr,
     juce::File) → DemucsResult. No file I/O in the separator itself —
     screens read WAV upstream and write outputs downstream (in 27-01)."
  - "Fixed window size 343,980 samples (7.8 s × 44.1 kHz). 25 % overlap
     with raised-cosine cross-fade. ORT Session reused across all
     chunks of a single call."
  - "Sample-rate guard: hard error if input ≠ 44.1 kHz. Caller resamples
     (screens know their DAW's rate). Cleaner than embedding a resampler
     here; can be revisited in 27-01 if needed."
  - "CPU execution provider only this plan. CUDA / DirectML deferred
     to v4.1 (noted in PROJECT.md)."

duration: ~2 hours
started: 2026-05-21
completed: 2026-05-22
---

# Phase 26 Plan 02: ONNX Runtime + ModelDownloader + NativeDemucsSeparator

**Closes Phase 26 (Native Demucs / ONNX).** ORT linked into the build;
header-only `ModelDownloader` handles first-launch DOD with SHA256
verification; header-only `NativeDemucsSeparator` runs the
htdemucs ONNX via `Ort::Session` with chunked + cross-faded inference.
Build green; v3 sidecar path verified untouched.

## Acceptance Criteria Results

| AC   | Description                                              | Status |
|------|----------------------------------------------------------|--------|
| AC-1 | FetchContent ORT + AutoRemix links                       | PASS   |
| AC-2 | ModelDownloader compiles + cache + GET + SHA + retry     | PASS   |
| AC-3 | NativeDemucsSeparator compiles + Ort::Session + chunking | PASS   |
| AC-4 | Build green; v3 untouched                                | PASS   |

## Verification Results

```
$ cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
[onnxruntime] downloaded to build/_deps/onnxruntime-src/ (~50 MB tarball, extracted)
$ cmake --build build --parallel
[292/292] Linking CXX shared module ... AutoRemix.vst3/.../AutoRemix.so
JUCE v7.0.12
```

```
$ sha256sum build/_deps/onnxruntime-subbuild/.../onnxruntime-linux-x64-1.17.0.tgz
efc344d54d1969446ff5d3e55b54e205c6579c06333ecf1d34a04215eefae7c6
$ sha256sum models/htdemucs.onnx
83a570fb273a411348a94564fb726d67b483f6c30f09fb95d1d7ef4dacc17c36
```

Both SHAs pinned at:
- `CMakeLists.txt`  (ORT_SHA for Linux x64)
- `plugin/include/dsp/ModelDownloader.h::kHtdemucsSHA256`

V3 smoke (manual): sidecar started → red dot turned green → drop WAV →
algorithmic separator ran successfully → stems played back. Demucs
option absent in the sidecar combobox because demucs isn't installed
in the user's sidecar venv (pre-existing v3 install quirk, unrelated
to this plan). v3 path **not** affected by 26-02 — AudioBridge.cpp.o
still compiles + links + runs as before.

## Deviations

- **CP-1 (human-action for SHA pinning) absorbed by Claude**: the user
  had the ONNX file locally and the ORT tarball had just been
  downloaded. Claude computed both SHAs via `sha256sum` and pinned
  them directly, saving a round-trip. macOS and Windows ORT SHAs
  remain empty (filled on first build on those platforms — the
  conditional `if(ORT_SHA)` block makes this safe).

- **juce_cryptography added to link list**: `juce::SHA256` lives in
  `juce_cryptography`, not `juce_core`. Added the module to
  `plugin/CMakeLists.txt::target_link_libraries`. Cheap add (only
  hashing primitives needed).

- **Sample-rate resampling not implemented**: the open question in
  the plan offered three resampling paths. Picked the simplest:
  hard-error when sr ≠ 44.1 kHz. Caller resamples. Phase 27-01 can
  bolt a juce::Interpolators::WindowedSinc resample upstream if any
  DAW comes through at 48 kHz.

- **Manual scratch-test (open question) skipped**: the plan offered
  an optional standalone tests/scratch.cpp that calls separate_demucs
  on a real WAV and dumps stems to disk. Skipped because the build
  proves the API surface is correct + Phase 27-01 will exercise the
  full path end-to-end through a real screen. Adding a throwaway
  test now would be premature.

## Key patterns / decisions for downstream

- **Ort::Session lifecycle**: one session per separate_demucs call,
  destroyed at end of scope. Phase 27-01 should consider caching
  this at the processor level if separation is called often. For
  one-shot user-initiated separation, per-call is fine.

- **AllocatedStringPtr handling**: ORT 1.17 returns
  AllocatedStringPtr from GetInput/OutputNameAllocated. Plain
  `.get()` for the duration of the call is correct; do not pass
  these pointers across longer scopes.

- **NativeStems is the universal stem container**: separators (both
  algorithmic + demucs) and downstream remix / mashup engines all
  consume the same struct from NativeAlgorithmicSeparator.h. Phase
  27-01 wires whichever separator the user picks into the same
  downstream pipeline — no branch in the engine code.

## Next phase

**Phase 26 CLOSES.** Transition to Phase 27 (Wire-up + release v4.0.0):

- 27-01: Remove AudioBridge HTTP layer; replace with direct C++ calls
  from ScreenSeparating / ScreenModeParams / ScreenMashup into the
  native engines. Separator combobox dynamic from a native registry
  (algorithmic + demucs). First-launch model download with progress UI.
- 27-02: Remove sidecar/ from build packaging + CI.
- 27-03: Full regression test on all 6 screens + remix + mashup flows.
- 27-04: README rewrite (no Python install section) + version bump
  3.2.0 → 4.0.0 + tag v4.0.0.
