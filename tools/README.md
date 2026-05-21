# tools/ — Offline build/release tooling

These scripts run **once** at release / model-upgrade time. They are not
shipped with the plugin and never run on a user's machine. Plugin runtime
is pure C++ — see `plugin/` and Phase 26-02 for the ONNX Runtime
integration.

## export_demucs_onnx.py

Exports the `htdemucs` model from the official `demucs` PyPI package to
ONNX. Produces a file that Phase 26-02's `NativeDemucsSeparator` consumes.

### Setup

```bash
cd tools
uv venv --python 3.12
source .venv/bin/activate
uv pip install -r requirements.txt
```

(Plain `pip` works too; `uv` is faster.)

### Run

```bash
python export_demucs_onnx.py --out ../models/htdemucs.onnx
```

Options:

| Flag                 | Default                    | Purpose                                          |
|----------------------|----------------------------|--------------------------------------------------|
| `--out`              | `models/htdemucs.onnx`     | Output path                                      |
| `--opset`            | `17`                       | ONNX opset version                               |
| `--window-seconds`   | `7.8`                      | htdemucs native segment length (44.1 kHz)        |
| `--skip-ort-check`   | off                        | Skip the onnxruntime CPU smoke pass              |

### Expected output

```
[export] loading htdemucs weights via demucs.pretrained...
[export] sample_rate=44100, window_samples=343980
[export] running torch.onnx.export (opset=17)...
[export] wrote ../models/htdemucs.onnx (XXX.X MB)
[export] check_model: OK
[export] input  : name=audio,  dims=['batch', 2, 'samples']
[export] output : name=stems,  dims=['batch', 4, 2, 'samples']
[export] running ORT CPU smoke inference...
[export] ORT output shape: (1, 4, 2, 343980)
[export] ORT check: OK
[export] DONE  out=../models/htdemucs.onnx  size=XXX.XMB  opset=17
```

The output tensor's source dimension MUST equal 4 — htdemucs predicts
`vocals / drums / bass / other` in that order.

### Known issues

- **`ModuleNotFoundError: No module named 'onnxscript'`**: newer PyTorch
  (>= 2.5) routes `torch.onnx.export` through `onnxscript` and does not
  auto-install it. Fix is in `requirements.txt` (`onnxscript>=0.1.0`).
  If you set up the venv before that pin was added, run:
  ```
  uv pip install onnxscript
  ```

- **`GuardOnDataDependentSymNode: Could not guard on data-dependent
  expression Eq(u0, 1)`** at `demucs/hdemucs.py:39 in pad1d`: the new
  dynamo-based ONNX exporter (torch >= 2.5 default) cannot lower
  demucs's internal data-dependent `assert (padded == original).all()`.
  Fix: this script defaults to the legacy TorchScript tracer
  (`dynamo=False`). Pass `--dynamo` only if you know the upstream
  demucs patch landed. The legacy tracer executes Python eagerly so
  the assert resolves at trace time and disappears from the graph.

- **`SymbolicValueError: STFT does not currently support complex types`**
  at `demucs/spec.py:17 in spectro` → `torch.stft`: htdemucs is a
  hybrid time/frequency architecture. The freq branch calls
  `torch.stft` returning a complex tensor; neither the legacy nor
  the dynamo ONNX exporter has symbolic support for complex-tensor
  `stft` at any current opset.

  **Resolution (chosen path A from the plan's open questions):** the
  script installs monkey-patches that replace `demucs.spec.spectro` /
  `demucs.spec.ispectro` with conv1d / conv_transpose1d-based DFT
  implementations operating on real tensors with last-dim 2 (re/im).
  Four `HTDemucs` methods are also patched (`_spec`, `_ispec`,
  `_magnitude`, `_mask`) so the entire model graph stays on real
  tensors end-to-end.

  A numerical parity check runs before export: the patched model is
  compared to a `copy.deepcopy` of the unmodified model on a 2-second
  random input. Max absolute diff must be ≤ `--parity-tol` (default
  1e-3). If parity fails the export is aborted with a clear error —
  no silent quality regressions.

- **`UnsupportedOperatorError: Exporting the operator
  'aten::_native_multi_head_attention' to ONNX opset 18`**: PyTorch's
  `nn.MultiheadAttention.forward` takes a fused fast-path
  (`torch._native_multi_head_attention`) when in eval mode + no grad,
  and the ONNX exporter has no symbolic for it. The script monkey-
  patches `nn.MultiheadAttention.forward` with a manual primitive-op
  implementation (`F.linear` QKV projection → reshape → matmul →
  softmax → matmul → out `F.linear`), which lowers cleanly to standard
  ONNX MatMul / Softmax / Add ops. Numerically equivalent (covered by
  the parity check).

### Export result (2026-05-21)

| Field         | Value                                       |
|---------------|---------------------------------------------|
| File          | `models/htdemucs.onnx`                      |
| Size          | 352.9 MB                                    |
| Opset         | 18                                          |
| Input         | `audio : [batch, 2, samples]`               |
| Output        | `stems : [batch, 4, 2, samples]`            |
| ORT backend   | CPUExecutionProvider — round-trip OK        |
| Parity        | PASS (max abs diff < 1e-3 vs unpatched)     |

Note: the trace bakes the export-time window length (`343980` samples
at 44.1 kHz × 7.8 s) into the graph. `dynamic_axes` flags `samples` as
dynamic in the ONNX schema, but the model is only numerically correct
at exactly that window size. The C++ runtime in Phase 26-02 chunks
input audio into fixed-size windows and stitches the outputs.

### Distribution

**Download-on-demand (DOD).** The exported `models/htdemucs.onnx`
(352.9 MB) is **not** embedded in the plugin binary. On first launch
the plugin downloads it from a GitHub release URL into a per-user
cache directory, verifies a SHA256 checksum, and reuses it on
subsequent launches.

Cache locations (Phase 26-02 will wire these):

| Platform | Path                                                |
|----------|-----------------------------------------------------|
| Linux    | `~/.config/autoremix/models/htdemucs.onnx`          |
| macOS    | `~/Library/Application Support/autoremix/models/…`  |
| Windows  | `%APPDATA%\autoremix\models\htdemucs.onnx`          |

Why DOD over bundling at this size:
- VST3 + AU + Standalone would each carry their own copy (~1 GB total)
- Single shared cache across formats
- Model updates can ship independently of plugin releases
- Standard ML practice (HuggingFace, demucs CLI, Stable Diffusion plugins)

TODO (Phase 26-02):
- Pin the hosting URL (default: `https://github.com/dobidu/autoremix/releases/download/v4.0.0-models/htdemucs.onnx`)
- Compute and pin the SHA256 of the produced .onnx file at release time
- Wire UI: "Downloading separation model… 352 MB" progress + retry on failure
- Offline fallback message if the download fails on first launch
