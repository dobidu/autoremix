# AutoRemix v2 — Project Context

## Vision
Audio plugin for automatic creative remixing. Load a track, choose a remix style,
get a processed version. Both stem separation and remix are pluggable — algorithmic
and ML backends coexist behind stable interfaces.

## Stack
- JUCE 7+ (C++20): plugin host (VST3/AU/LV2 via juce_audio_plugin_client)
- libcpr: HTTP client for plugin→Python IPC
- nlohmann/json: JSON serialization
- FastAPI + uvicorn: Python sidecar
- librosa + soundfile + pedalboard: audio processing in Python
- Note: Spleeter deferred (TF/cp312 incompatibility); pyrubberband dropped (requires CLI binary); algorithmic FFT separator used in MVP

## MVP Remix Modes
1. Chopped & Screwed: slow tempo (0.7x), pitch down (-4 semi), chop every N bars
2. Slowed Reverb: slow tempo (0.75x), heavy reverb (IR or algorithmic), slight pitch down
3. Drum and Bass: double drums tempo, bass boost, high-pass rest, re-sequence

## Non-goals (v2)
- Real-time processing (offline batch only in MVP)
- Cloud processing
- GUI animation
- MIDI output

## Success Criteria
- [x] Load a WAV/AIFF, press "Separate" → 4 stems in temp dir (algorithmic FFT separator; Spleeter deferred — TF/cp312)
- [x] Select "Chopped & Screwed" → press "Remix" → output WAV generated (all 3 engines verified Phase 03 + pytest)
- [ ] Plugin loads in REAPER without crash (VST3) — Deferred: pending manual REAPER smoke test
- [x] Python sidecar starts/stops cleanly from plugin lifecycle (AudioBridge::startSidecar/stopSidecar, Phase 05-02)
- [x] IStemSeparator implementors can be swapped via config without recompile (SeparatorRegistry + RemixRegistry, Phase 01)
- [x] Stem mix sliders (Vocals/Drums/Bass/Other 0–2×) send pre-weighting to sidecar — Phase 13-01
- [x] In-plugin audio preview via AudioTransportSource plays remixed WAV through plugin output — Phase 13-02
- [x] Save-as-preset writes custom preset JSON to user dir; style_combo_ refreshes live — Phase 13-03
- [x] Musical chop modes (beat/onset/bar/energy/structural) selectable from UI; ops inject into effect chains — Phase 14
- [x] Pairwise mashup: load 2 files, two-column 8-stem mixer with per-stem volume, 8 built-in templates, 5 feel knobs (tempo mod, master pitch, reverb mix + room, HPF B); auto BPM + key alignment — Phase 21
- [x] Native preset loading: 17 JSONs (9 remix + 8 mashup) embedded in plugin binary via `juce_add_binary_data`; user JSONs read from `~/.config/autoremix/{modes,mashup}` (or platform equivalent) override built-ins — Phase 25
- [x] Native ML separation foundation: `htdemucs` model exported to ONNX (352.9 MB) via offline tooling under `tools/`; ONNX Runtime 1.17.0 linked into the plugin; `ModelDownloader` provides first-launch SHA256-verified DOD; `NativeDemucsSeparator` runs chunked inference via `Ort::Session` — Phase 26 (smoke includes only; screen wiring lands in 27-01)
- [x] GPU EP acceleration: CUDA (Linux) + DirectML (Windows) via `-DAUTOREMIX_GPU=ON`; CPU fallback transparent; blue model-status dot when GPU active; separate GPU release artifacts — Phase 28 (v4.1.0)
- [x] CoreML EP acceleration on macOS (Apple Silicon / Neural Engine) via same `-DAUTOREMIX_GPU=ON` flag; `AUTOREMIX_COREML=1` defined on Darwin GPU builds; Intel fallback to CPU — Phase 29 (v4.2.0)

## Constraints
- GPL-3.0 compatible deps only (Spleeter=MIT, librosa=ISC, JUCE=GPL ok)
- No JUCE_MODAL_LOOPS in audio thread (already defined in CMakeLists)
- Python sidecar ≤ 500ms startup time after model load

## Phase 13 Key Decisions
- Stem pre-weighting in Python before engine dispatch (universal; no engine changes needed)
- AudioTransportSource in processor (not editor); processBlock routes transport audio to output
- AlertWindow runModalLoop() safe: JUCE_MODAL_LOOPS_PERMITTED=1 in CMakeLists
- POST /api/v1/presets updates module-level _presets in-place (MVP-safe, no lock needed)

## Phase 14 Key Decisions
- analysis.py functions pure (no mutation): accept (audio, sr, **kwargs), return new arrays
- Musical chop injection only for effect-chain presets (non-empty effects) — legacy engine path unchanged
- copy.copy(preset) before injection prevents _presets dict mutation (shallow copy + new list)
- Effect-chain presets use `"engine": "effect_chain"` string — RemixPreset.engine is str, not Optional[str]
- chop_mode appended (not prepended) to effects — existing ops run first, chop on post-processed audio
- OP_REGISTRY grows 6 → 11 entries; _chop_at_boundaries private helper shared by 3 boundary-based ops

## Phase 25 Key Decisions (v4 native)
- Built-in presets embedded in plugin binary via `juce_add_binary_data(AutoRemixPresetsData)` — no filesystem dependency for shipped defaults; 17 JSONs (9 remix + 8 mashup) compiled into the .a/.so/.vst3
- Routing built-ins by suffix `_json` + allow-list of 8 known mashup resource ids (BinaryData name mangling: `drum_swap.json` → `drum_swap_json`)
- User dir via `juce::File::userApplicationDataDirectory` → cross-platform (Linux ~/.config, macOS Application Support, Windows %APPDATA%). Slight macOS divergence from v3 (which used ~/.config everywhere)
- nlohmann/json `j.value(key, default)` for permissive parsing — unknown keys ignored, malformed JSONs logged via juce::Logger + skipped (no throw across the boundary)
- De-dup by id, last-wins: built-ins load first, user dir appended, final vector contains the latest entry per id
- Forward-declare `BinaryData::namedResourceList[]` / `namedResourceListSize` / `getNamedResource` rather than `#include "BinaryData.h"` — multiple juce_add_binary_data targets share the same `BinaryData` namespace; forward decls keep header decoupled from include ordering
- MashupEngine ported header-only: in-memory `(NativeStems, NativeStems, sr, MashupParams) → MashupResult` (no file I/O at engine layer; caller owns WAV writing)
- RubberBand semantics: `setTimeRatio(target_bpm / source_bpm)` matches v3 librosa `rate>1 = output longer = slowed`; per-stem fresh stretcher to avoid state accumulation

## Phase 26 Key Decisions (v4 native — ML separation)
- ONNX export approach: keep htdemucs (best quality). Three classes of monkey-patches in `tools/export_demucs_onnx.py` solve the export blockers:
    1. conv1d/conv_transpose1d DFT replacement for `torch.stft` / `torch.istft` (real tensors, last-dim = re/im) — bypasses ONNX's lack of symbolic complex support
    2. `HTDemucs._spec` / `_ispec` / `_magnitude` / `_mask` patched to operate on the real-tensor format end-to-end (cac=True path)
    3. `torch.nn.MultiheadAttention.forward` replaced with manual primitive-op MHA (QKV linear → reshape → matmul → softmax → matmul → out linear) to bypass `torch._native_multi_head_attention` fast path
- Numerical parity check gates the export — max abs diff < 1e-3 vs unpatched model on a 2 s random input. No silent quality regressions.
- Static `n_fft` is mandatory through the export path. Threaded as Python int from `self.nfft` (HTDemucs attribute) through patched_ispec → conv_istft so `torch.arange(n_fft)` builds static-shape Conv kernels at trace time.
- Trace-time window length (343,980 samples @ 44.1 kHz × 7.8 s) is baked into the ONNX graph despite `dynamic_axes=samples`. Plugin must feed exactly window-sized chunks at runtime; chunking + 25 % overlap + raised-cosine crossfade live in the C++ `NativeDemucsSeparator`.
- Shipping strategy: **download-on-demand**. 352.9 MB file too large to bundle into 3 plugin formats (~1 GB total otherwise). Cache: `juce::File::userApplicationDataDirectory/autoremix/models/htdemucs.onnx`. SHA256 pinned compile-time. Hosting at github.com release tag `v4.0.0-models`.
- ONNX Runtime 1.17.0: CPU EP default; CUDA EP (Linux) + DirectML EP (Windows) + CoreML EP (macOS) via `-DAUTOREMIX_GPU=ON`. GPU EP complete across all 3 platforms as of v4.2.0. Linked as SHARED IMPORTED via FetchContent per-platform tarball. GPU EP selection propagated via `DemucsResult.gpu_used`; UI dot shows blue when active.
- `NativeDemucsSeparator` requires 44.1 kHz input (hard error otherwise). Resampling lives in screens (caller knows DAW rate). Simpler + cleaner than embedding a resampler in the separator.
- One `Ort::Session` per `separate_demucs` call, destroyed at end of scope. If usage profile warrants, Phase 27-01 may move the session to the processor.

## Phase 28 Key Decisions (v4.1 — GPU EP)
- `AUTOREMIX_GPU=ON` flag selects GPU ORT package per platform at configure time
- Linux GPU: `onnxruntime-linux-x64-gpu-1.17.0.tgz` (CUDA 11.8 EP). CUDA EP .so libs copied into VST3 bundle via POST_BUILD so ORT can dlopen them.
- Windows GPU: `Microsoft.ML.OnnxRuntime.DirectML` NuGet. DirectML.dll is a Windows OS component (Win10 1903+) — NOT bundled; ORT loads from System32 via LoadLibrary.
- macOS: same CPU tarball regardless of flag. CoreML EP via `AUTOREMIX_COREML=1` (Phase 29 — ships in standard universal2 tarball, flags=0 default).
- `DemucsResult.gpu_used` bool: captured by ref in `make_session` lambda, set on GPU EP success, propagated to caller. `health_dot_.setState(gpu_active)` in `separateNative()`.
- `ModelStatusDot::State::gpu_active` (blue `#4A9EDB`): poll guard prevents 3s timer overwrite.
- SHA256 for GPU ORT packages left empty (`ORT_SHA ""`); pin after confirming no breaking changes.

---
## Phase 29 Key Decisions (v4.2 — macOS CoreML EP)
- `coreml_provider_factory.h` ships in `onnxruntime-osx-universal2` tarball `include/` — no new dep
- `AUTOREMIX_COREML=1` compile definition gated by `$<$<AND:$<BOOL:${AUTOREMIX_GPU}>,$<PLATFORM_ID:Darwin>>:...>` generator expression
- CoreML EP flags=0 (default ORT subgraph delegation)
- Intel Mac: CoreML EP throws → CPU EP fallback (same pattern as other platforms)
- `coreml_provider_factory.h` include guarded by `AUTOREMIX_GPU && __APPLE__ && AUTOREMIX_COREML`

---
*Last updated: 2026-05-29 after Phase 29 (CoreML EP — v4.2.0 released)*
