---
phase: 26-native-demucs
plan: 01
subsystem: tools / models
tags: [native, v4, demucs, onnx, offline-tooling]

provides:
  - tools/export_demucs_onnx.py (offline torch.onnx.export tooling)
  - tools/requirements.txt (isolated build-time deps)
  - tools/README.md (setup + known issues + distribution)
  - models/.gitignore + models/README.md (artifact dir)
  - models/htdemucs.onnx (produced at runtime, git-ignored — 352.9 MB)
  - STATE.md ### Decisions: "Demucs ONNX shipping: download" + hosting URL placeholder + SHA256 TODO
affects: [26-02]

key-files:
  created:
    - tools/export_demucs_onnx.py        (~450 lines)
    - tools/requirements.txt             (8 pinned deps)
    - tools/README.md                    (setup, known issues, distribution)
    - models/.gitignore
    - models/README.md
  modified:
    - .paul/STATE.md  (Decisions entry + loop position)

key-decisions:
  - "Bundling: download-on-demand (DOD). Reason: 352.9 MB ONNX file is too
     large to bundle into 3 plugin formats (VST3 + AU + Standalone would carry
     ~1 GB total). DOD allows shared cache across formats + model updates
     without plugin re-release."
  - "Cache dir: juce::File::userApplicationDataDirectory/autoremix/models/htdemucs.onnx
     (Linux ~/.config, macOS Application Support, Windows %APPDATA%)."
  - "Hosting URL placeholder: github.com/dobidu/autoremix releases.
     Pinned + SHA256-checksummed in Phase 26-02."
  - "Three monkey-patches landed in the export script:
       1. demucs.spec.spectro/ispectro → conv1d/conv_transpose1d DFT
          on real tensors with last-dim 2 (re/im). Bypasses ONNX's lack
          of symbolic stft on complex types.
       2. HTDemucs._spec / _ispec / _magnitude / _mask — interop with
          the real-tensor format (cac=True path).
       3. torch.nn.MultiheadAttention.forward → manual primitive-op MHA
          (F.linear QKV → matmul → softmax → matmul → F.linear out).
          Bypasses torch._native_multi_head_attention fused fast path."
  - "n_fft must be threaded as a Python int (self.nfft = 4096) through
     patched_ispec → patched_ispectro → conv_istft to keep kernel shapes
     static at trace time. Deriving from z.shape during JIT trace
     produces symbolic Tensors that lower to dynamic-kernel Conv,
     which ONNX rejects."
  - "Numerical parity check: max abs diff < 1e-3 vs unpatched model on
     a 2 s random input. PASS before export proceeds (no silent quality
     regression)."
  - "Trace-time window length (343980 samples @ 44.1 kHz × 7.8 s) is
     baked into the graph despite dynamic_axes=samples. Plugin must
     feed exactly window-sized chunks at runtime. Phase 26-02 wires
     chunking on the C++ side."

duration: ~3 hours (debug-heavy: 3 export blockers + 3 monkey-patch iterations)
started: 2026-05-20
completed: 2026-05-21
---

# Phase 26 Plan 01: Demucs → ONNX Export Tooling

**Offline export pipeline for `htdemucs` → ONNX. Three torch-side
blockers solved via targeted monkey-patches. Numerical parity verified.
Output file size known + shipping strategy chosen.**

## Objective recap

Produce a runnable offline tool that exports the `htdemucs` model to
ONNX, validates it (`onnx.checker` + ORT round-trip), and records the
file size + shipping decision. Phase 26-02 (next) wires the resulting
artifact into a native `DemucsSeparator` via ONNX Runtime C++.

## What was built

| File                            | Purpose                                       | Lines |
|---------------------------------|-----------------------------------------------|-------|
| `tools/export_demucs_onnx.py`   | Offline export script with patches + parity  | ~450  |
| `tools/requirements.txt`        | Isolated build-time deps (torch, onnx, …)    | 8     |
| `tools/README.md`               | Setup + known issues + distribution          | ~100  |
| `models/.gitignore`             | Ignore artifacts, keep dir tracked           | 3     |
| `models/README.md`              | Pointer to regeneration script               | 8     |

The script is the load-bearing artifact. Three classes of patches inside:

1. **conv-based STFT/ISTFT** (`conv_stft`, `conv_istft`) — real-tensor
   DFT via `F.conv1d` / `F.conv_transpose1d`, kernels built from a
   static `n_fft` Python int. Replaces `torch.stft` / `torch.istft`
   which the ONNX exporter cannot lower on complex types.

2. **HTDemucs method patches** (`patched_spec`, `patched_ispec`,
   `patched_magnitude`, `patched_mask`) — keep the model's
   spectrogram branch on real `[..., freq, frames, 2]` tensors
   end-to-end, dropping `view_as_real` / `view_as_complex`.

3. **MHA primitive-op forward** (`manual_forward` patched onto
   `torch.nn.MultiheadAttention`) — manual QKV linear + reshape +
   matmul + softmax + matmul + out linear. Bypasses the fused
   `torch._native_multi_head_attention` op.

## Acceptance Criteria Results

| AC   | Description                                          | Status |
|------|------------------------------------------------------|--------|
| AC-1 | Export runs end-to-end + produces a valid ONNX file  | PASS   |
| AC-2 | onnx.checker + ORT round-trip green + 4 source dim   | PASS   |
| AC-3 | Plugin / sidecar / prior phases untouched            | PASS   |
| AC-4 | Bundling decision recorded                           | PASS   |

## Verification Results

```
$ python tools/export_demucs_onnx.py --out ../models/htdemucs.onnx
[export] loading htdemucs weights via demucs.pretrained...
[export] sample_rate=44100, window_samples=343980
[export] deep-copying model for parity reference...
[export] installing demucs.spec + HTDemucs monkey-patches...
[export] running numerical parity check...
[parity] OK
[export] running torch.onnx.export (opset=18, dynamo=False)...
[export] wrote ../models/htdemucs.onnx (352.9 MB)
[export] check_model: OK
[export] input  : name=audio,  dims=['batch', 2, 'samples']
[export] output : name=stems,  dims=['batch', 'Addstems_dim_1', 'Addstems_dim_2', 'samples']
[export] ORT output shape: (1, 4, 2, 343980)
[export] ORT check: OK
[export] DONE  out=../models/htdemucs.onnx  size=352.9MB  opset=18
```

C++ plugin: `cmake --build build --parallel` → ninja: no work to do.
v3 sidecar + main branch untouched.

## Deviations

- **`requirements.txt` pin added mid-run**: `onnxscript>=0.1.0`. Newer
  PyTorch (≥ 2.5) routes `torch.onnx.export` through onnxscript but
  doesn't auto-install. Added after the first run surfaced the
  `ModuleNotFoundError`.
- **Default opset bumped 17 → 18**: PyTorch auto-upgrades and warns
  if you pass < 18 — matched the actual behavior.
- **Three monkey-patch iterations**: the plan's "Open Questions"
  anticipated naive `torch.onnx.export` failure modes but did not
  pre-commit to a specific resolution. Path A (patch `demucs.spec`)
  was selected and expanded into the conv-DFT + MHA-manual + n_fft-
  static fixes during APPLY. No spec drift — the plan explicitly
  allowed this exploration in its scope.
- **Output tensor name `Addstems_dim_1` / `Addstems_dim_2`**: ONNX
  exporter generated synthetic axis names because the dynamic_axes
  hint only covered batch and samples dims. Cosmetic; functionally
  identical to `[batch, 4, 2, samples]`. Phase 26-02 reads by index,
  not by name.

## Key patterns / decisions for downstream

- **Fixed window size at runtime**: the traced graph is correct only at
  `samples = 343980` (= 7.8 s × 44.1 kHz). Phase 26-02 must chunk
  arbitrary-length input into 7.8 s windows with overlap (htdemucs
  natively uses 25 % overlap), run the ONNX session per chunk, and
  stitch the outputs.
- **No demucs at runtime**: the production plugin never imports the
  Python `demucs` package. Only `tools/` (build-time) needs it.
- **Patches are reversible**: monkey-patches install into the live
  `demucs.*` modules in the build venv. They do NOT modify the
  installed package files. Reinstalling the venv resets state.

## Next phase

`/paul:plan 26-02` — ONNX Runtime C++ integration. Concrete scope:

- CMake: FetchContent + link onnxruntime (CPU EP minimum, optional
  CUDA later)
- `plugin/include/dsp/NativeDemucsSeparator.h` — header-only, loads
  `models/htdemucs.onnx` from the user cache, runs `Ort::Session` on
  fixed-size windows, stitches outputs with overlap
- `plugin/include/dsp/ModelDownloader.h` — on first launch, GET the
  pinned URL into the cache dir, verify SHA256, retry on failure,
  surface progress to the UI
- Smoke include in `AutoRemixProcessor.cpp`
- AC: model downloads on cold start, session loads, separation runs
  end-to-end on a real WAV, output stems match the Python reference
  within ~3 dB SI-SDR
