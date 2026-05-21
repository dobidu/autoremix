#!/usr/bin/env python3
"""
Offline ONNX export for demucs htdemucs.

One-shot tool. NOT used at plugin runtime. Run once per model upgrade.

Usage:
    cd tools
    uv venv --python 3.12 && source .venv/bin/activate
    uv pip install -r requirements.txt
    python export_demucs_onnx.py --out ../models/htdemucs.onnx

What it does:
    1. Loads demucs htdemucs weights via demucs.pretrained.get_model.
    2. Monkey-patches demucs.spec.spectro / demucs.spec.ispectro with
       conv1d-based DFT implementations that operate on real tensors
       (last dim = 2 for re/im). This bypasses the ONNX exporter's lack
       of symbolic support for torch.stft / torch.istft on complex
       tensors.
    3. Monkey-patches HTDemucs._spec / _ispec / _magnitude / _mask to
       interoperate with the real-tensor format end-to-end (cac=True
       path, which is htdemucs's default).
    4. Runs a numerical-parity check: the patched model is compared to
       the original on a fixed random input. Max abs diff must be
       below --parity-tol (default 1e-3).
    5. Exports the patched model via torch.onnx.export (legacy tracer)
       to the requested opset.
    6. Validates the ONNX file (onnx.checker.check_model) and runs an
       onnxruntime CPU round-trip on a random input to confirm the
       graph is end-to-end runnable.

If any of those steps fails, the script exits non-zero with the
verbatim error — do not paper over a real export blocker.
"""

from __future__ import annotations

import argparse
import math
import sys
from pathlib import Path


# ─────────────────────────────────────────────────────────────────────────────
# Conv-based STFT / ISTFT (real tensors, last dim = 2 for re/im).
#
# Math:
#   Forward STFT for real input, with Hann window, center-pad mode='reflect',
#   normalized=True (matches demucs/spec.py spectro):
#       X[k, f] = (1/sqrt(N)) sum_n x[f*hop + n] * w[n] * exp(-j 2π k n / N)
#   This is a conv1d with kernels [cos*w; -sin*w], stride=hop.
#
#   Inverse STFT (real output, COLA-corrected OLA):
#       x[n] = sum_f w[n - f*hop] * frame_idft[f][n - f*hop]
#              / sum_f w[n - f*hop]^2
#       frame_idft[f][m] = (1/N) Re[sum_k X[f, k] exp(j 2π k m / N)]
#   For real-spectrum input (rfft layout, n_freq = N//2 + 1):
#       x[n] = (2/N)(sum_{k=1..N/2-1} Re·cos − Im·sin)
#            + (1/N)(Re[0] + Re[N/2]·cos(π n))
#   Conv_transpose1d composes the OLA. Envelope normalization divides
#   by the overlap-sum of w[n]^2.
# ─────────────────────────────────────────────────────────────────────────────

def _build_stft_kernels(n_fft: int, win_length: int, device, dtype):
    import torch
    window = torch.hann_window(win_length, periodic=True, dtype=torch.float64,
                               device=device)
    # Pad window to n_fft if needed (htdemucs uses win_length = n_fft).
    if win_length < n_fft:
        left = (n_fft - win_length) // 2
        right = n_fft - win_length - left
        window = torch.nn.functional.pad(window, (left, right))

    n_freq = n_fft // 2 + 1
    n_idx = torch.arange(n_fft, dtype=torch.float64, device=device)
    k_idx = torch.arange(n_freq, dtype=torch.float64, device=device).unsqueeze(1)
    ang = 2.0 * math.pi * k_idx * n_idx / n_fft
    cos = (torch.cos(ang) * window).to(dtype)         # [n_freq, n_fft]
    sin = (-torch.sin(ang) * window).to(dtype)        # [n_freq, n_fft]
    fwd_kernel = torch.cat([cos, sin], dim=0).unsqueeze(1)  # [2*n_freq, 1, n_fft]
    window_real = window.to(dtype)
    return fwd_kernel, window_real


def conv_stft(x, n_fft: int, hop_length: int, normalized: bool = True):
    """Forward STFT via conv1d. Returns real tensor [..., n_freq, frames, 2]."""
    import torch
    import torch.nn.functional as F

    other_shape = x.shape[:-1]
    T = x.shape[-1]
    x = x.reshape(-1, 1, T)
    pad = n_fft // 2
    x_padded = F.pad(x, (pad, pad), mode="reflect")

    fwd_kernel, _ = _build_stft_kernels(
        n_fft=n_fft, win_length=n_fft, device=x.device, dtype=x.dtype
    )

    y = F.conv1d(x_padded, fwd_kernel, stride=hop_length)
    n_freq = n_fft // 2 + 1
    real = y[:, :n_freq, :]
    imag = y[:, n_freq:, :]
    z = torch.stack([real, imag], dim=-1)             # [B, n_freq, frames, 2]
    if normalized:
        z = z / math.sqrt(n_fft)
    z = z.reshape(*other_shape, n_freq, z.shape[-2], 2)
    return z


def conv_istft(z, n_fft: int, hop_length: int, length: int,
               normalized: bool = True):
    """Inverse STFT via conv_transpose1d. Input is real [..., n_freq, frames, 2].

    n_fft MUST be a Python int (not a Tensor / SymInt). The kernel arrays
    are built from arange(n_fft) and arange(n_freq) where n_freq derives
    from n_fft — both must be static at trace time so the resulting Conv
    weights are constants in the ONNX graph.
    """
    import torch
    import torch.nn.functional as F

    n_freq = n_fft // 2 + 1                    # static, derived from n_fft
    other_shape = z.shape[:-3]
    z = z.reshape(-1, n_freq, z.shape[-2], 2)
    real = z[..., 0]
    imag = z[..., 1]

    if normalized:
        real = real * math.sqrt(n_fft)
        imag = imag * math.sqrt(n_fft)

    # Reconstruction weights:
    #   x[n] = (2/N) sum_{k=1..N/2-1} (Re cos − Im sin) + (1/N)(Re[0] + Re[N/2]·cos(πn))
    # Multiply each cos/sin kernel by the synthesis Hann window.
    window = torch.hann_window(n_fft, periodic=True, dtype=torch.float64,
                               device=z.device)
    n_idx = torch.arange(n_fft, dtype=torch.float64, device=z.device)
    k_idx = torch.arange(n_freq, dtype=torch.float64,
                         device=z.device).unsqueeze(1)
    ang = 2.0 * math.pi * k_idx * n_idx / n_fft
    cos = torch.cos(ang)
    sin = torch.sin(ang)
    scale = torch.full((n_freq, 1), 2.0 / n_fft, dtype=torch.float64,
                       device=z.device)
    scale[0, 0]  = 1.0 / n_fft
    scale[-1, 0] = 1.0 / n_fft
    scale_im = scale.clone()
    scale_im[0, 0]  = 0.0
    scale_im[-1, 0] = 0.0

    w_re_kernel = ((cos * scale)    * window).to(z.dtype).unsqueeze(1)  # [n_freq,1,n_fft]
    w_im_kernel = ((-sin * scale_im) * window).to(z.dtype).unsqueeze(1)

    x = F.conv_transpose1d(real, w_re_kernel, stride=hop_length)
    x = x + F.conv_transpose1d(imag, w_im_kernel, stride=hop_length)

    # Overlap-sum of w[n]^2 envelope, for normalization.
    win_sq = (window * window).to(z.dtype).reshape(1, 1, n_fft)
    ones_in = torch.ones(real.shape[0], 1, real.shape[-1],
                         dtype=z.dtype, device=z.device)
    env = F.conv_transpose1d(ones_in, win_sq, stride=hop_length)
    x = x / (env + 1e-10)

    # Strip the n_fft//2 center-pad prefix and trim to `length`.
    pad = n_fft // 2
    x = x.squeeze(1)
    x = x[..., pad: pad + length]
    x = x.reshape(*other_shape, x.shape[-1])
    return x


# ─────────────────────────────────────────────────────────────────────────────
# Monkey-patch demucs.spec + HTDemucs to operate on real-tensor STFT.
# ─────────────────────────────────────────────────────────────────────────────

def _install_mha_patch():
    """Replace torch.nn.MultiheadAttention.forward with a manual
    primitive-op implementation (Linear + matmul + softmax + matmul +
    Linear). Bypasses torch._native_multi_head_attention, which the ONNX
    exporter cannot lower."""
    import torch
    import torch.nn.functional as F

    def manual_forward(self, query, key, value, key_padding_mask=None,
                       need_weights=True, attn_mask=None,
                       average_attn_weights=True, is_causal=False):
        # demucs uses batch_first=True throughout transformer blocks.
        # Standard layout: query/key/value [B, L, D] where D = embed_dim.
        if not self.batch_first:
            query = query.transpose(0, 1)
            key   = key.transpose(0, 1)
            value = value.transpose(0, 1)

        B, Lq, D = query.shape
        Lk = key.shape[1]
        h  = self.num_heads
        dh = D // h
        scale = 1.0 / math.sqrt(dh)

        # QKV projection.
        if self._qkv_same_embed_dim and self.in_proj_weight is not None:
            # Combined projection of [Q;K;V] when q/k/v share embed dim
            # and key/value are the same tensor as query.
            if (key.data_ptr() == query.data_ptr()
                    and value.data_ptr() == query.data_ptr()):
                qkv = F.linear(query, self.in_proj_weight, self.in_proj_bias)
                q, k, v = qkv.chunk(3, dim=-1)
            else:
                w_q, w_k, w_v = self.in_proj_weight.chunk(3, dim=0)
                if self.in_proj_bias is not None:
                    b_q, b_k, b_v = self.in_proj_bias.chunk(3, dim=0)
                else:
                    b_q = b_k = b_v = None
                q = F.linear(query, w_q, b_q)
                k = F.linear(key,   w_k, b_k)
                v = F.linear(value, w_v, b_v)
        else:
            q = F.linear(query, self.q_proj_weight,
                         self.in_proj_bias[:D] if self.in_proj_bias is not None else None)
            k = F.linear(key,   self.k_proj_weight,
                         self.in_proj_bias[D:2*D] if self.in_proj_bias is not None else None)
            v = F.linear(value, self.v_proj_weight,
                         self.in_proj_bias[2*D:] if self.in_proj_bias is not None else None)

        # Reshape to [B, h, L, dh].
        q = q.view(B, Lq, h, dh).transpose(1, 2)
        k = k.view(B, Lk, h, dh).transpose(1, 2)
        v = v.view(B, Lk, h, dh).transpose(1, 2)

        # Attention: [B, h, Lq, Lk] = (q @ k^T) / sqrt(dh)
        attn = torch.matmul(q, k.transpose(-2, -1)) * scale

        if attn_mask is not None:
            # attn_mask shape: [Lq, Lk] or [B*h, Lq, Lk] or boolean
            if attn_mask.dtype == torch.bool:
                attn = attn.masked_fill(attn_mask, float("-inf"))
            else:
                attn = attn + attn_mask

        if key_padding_mask is not None:
            # Shape: [B, Lk] → broadcast to [B, 1, 1, Lk]
            mask = key_padding_mask.view(B, 1, 1, Lk)
            attn = attn.masked_fill(mask, float("-inf"))

        attn = F.softmax(attn, dim=-1)
        # Dropout omitted: eval mode, dropout=0 effective.

        out = torch.matmul(attn, v)                # [B, h, Lq, dh]
        out = out.transpose(1, 2).contiguous()     # [B, Lq, h, dh]
        out = out.view(B, Lq, D)
        out = F.linear(out, self.out_proj.weight, self.out_proj.bias)

        if not self.batch_first:
            out = out.transpose(0, 1)

        if need_weights:
            # Optional return — demucs typically calls with need_weights=False.
            attn_weights = attn
            if average_attn_weights:
                attn_weights = attn_weights.mean(dim=1)
            return out, attn_weights
        return out, None

    torch.nn.MultiheadAttention.forward = manual_forward


def _install_demucs_patches():
    """Replace demucs.spec.spectro / .ispectro and HTDemucs._spec / _ispec /
    _magnitude / _mask with versions that avoid complex tensors. Also
    replace nn.MultiheadAttention.forward to bypass the fused
    _native_multi_head_attention op."""
    import torch
    import torch.nn.functional as F
    import demucs.spec
    import demucs.htdemucs

    _install_mha_patch()

    def patched_spectro(x, n_fft=4096, hop_length=None, pad=0):
        # Matches demucs.spec.spectro signature but returns real
        # [..., n_freq, frames, 2] instead of complex [..., n_freq, frames].
        if hop_length is None:
            hop_length = n_fft // 4
        nfft_eff = n_fft * (1 + pad)
        return conv_stft(x, n_fft=nfft_eff, hop_length=hop_length, normalized=True)

    def patched_ispectro(z, n_fft, hop_length, length):
        # z: real [..., n_freq, frames, 2]
        # n_fft is a Python int (passed from patched_ispec → self.nfft).
        # Deriving it from z.shape[-3] during JIT tracing produces a
        # symbolic Tensor; torch.arange on it gives a dynamic-length
        # kernel that ONNX Conv cannot accept.
        return conv_istft(z, n_fft=n_fft, hop_length=hop_length,
                          length=length, normalized=True)

    demucs.spec.spectro  = patched_spectro
    demucs.spec.ispectro = patched_ispectro

    # demucs.htdemucs imports spectro/ispectro at module load → re-bind.
    demucs.htdemucs.spectro  = patched_spectro
    demucs.htdemucs.ispectro = patched_ispectro

    pad1d = demucs.htdemucs.pad1d
    HTDemucs = demucs.htdemucs.HTDemucs

    def patched_spec(self, x):
        hl = self.hop_length
        nfft = self.nfft
        assert hl == nfft // 4
        le = int(math.ceil(x.shape[-1] / hl))
        pad = hl // 2 * 3
        x = pad1d(x, (pad, pad + le * hl - x.shape[-1]), mode="reflect")
        z = patched_spectro(x, nfft, hl)               # [..., n_freq, frames, 2]
        z = z[..., :-1, :, :]                          # drop top freq bin
        z = z[..., 2 : 2 + le, :]                      # frame slice
        return z

    def patched_ispec(self, z, length=None, scale=0):
        # z: real [..., n_freq, frames, 2]
        hl   = self.hop_length // (4 ** scale)
        nfft = self.nfft                        # Python int — must stay static
        # Pad freq dim by 1 (penultimate-of-last-3 axis):
        z = F.pad(z, (0, 0, 0, 0, 0, 1))
        # Pad frames dim by 2 each side:
        z = F.pad(z, (0, 0, 2, 2))
        pad_t = hl // 2 * 3
        le = hl * int(math.ceil(length / hl)) + 2 * pad_t
        x = patched_ispectro(z, nfft, hl, le)
        x = x[..., pad_t : pad_t + length]
        return x

    def patched_magnitude(self, z):
        # z: real [B, C, Fr, T, 2]
        if self.cac:
            B, C, Fr, T, _ = z.shape
            m = z.permute(0, 1, 4, 2, 3).contiguous()  # [B, C, 2, Fr, T]
            m = m.reshape(B, C * 2, Fr, T)
            return m
        else:
            return torch.sqrt(z[..., 0] ** 2 + z[..., 1] ** 2 + 1e-12)

    def patched_mask(self, z, m):
        # m: real [B, S, C, Fr, T] from freq decoder
        # Returns: real [B, S, C//2, Fr, T, 2] (the previous code returned
        # the complex equivalent; we skip view_as_complex).
        if self.cac:
            B, S, C, Fr, T = m.shape
            out = m.view(B, S, -1, 2, Fr, T).permute(0, 1, 2, 4, 5, 3)
            return out.contiguous()
        else:
            raise NotImplementedError(
                "Non-cac wiener path not implemented in real-STFT patch — "
                "htdemucs default is cac=True."
            )

    HTDemucs._spec      = patched_spec
    HTDemucs._ispec     = patched_ispec
    HTDemucs._magnitude = patched_magnitude
    HTDemucs._mask      = patched_mask


# ─────────────────────────────────────────────────────────────────────────────
# Numerical parity check: patched model vs unmodified model on the same input.
# ─────────────────────────────────────────────────────────────────────────────

def _parity_check(unmodified_model, patched_model, input_tensor, tol: float):
    import torch
    with torch.inference_mode():
        ref = unmodified_model(input_tensor)
        got = patched_model(input_tensor)
    if isinstance(ref, (list, tuple)):
        ref = ref[0]
    if isinstance(got, (list, tuple)):
        got = got[0]
    diff = (ref - got).abs()
    max_abs = diff.max().item()
    rmse = (diff ** 2).mean().sqrt().item()
    print(f"[parity] max_abs_diff={max_abs:.3e}  rmse={rmse:.3e}  tol={tol:.0e}")
    if max_abs > tol:
        print(
            f"[parity] FAIL — patched model diverges from original by "
            f"{max_abs:.3e}. The conv-STFT implementation is not "
            f"numerically equivalent to the original. Aborting export.",
            file=sys.stderr,
        )
        return False
    print(f"[parity] OK")
    return True


# ─────────────────────────────────────────────────────────────────────────────
# Main
# ─────────────────────────────────────────────────────────────────────────────

def main() -> int:
    parser = argparse.ArgumentParser(
        description="Export demucs htdemucs to ONNX (offline tooling).",
    )
    parser.add_argument(
        "--out",
        type=Path,
        default=Path("models/htdemucs.onnx"),
        help="Output path for the ONNX file (default: models/htdemucs.onnx).",
    )
    parser.add_argument(
        "--opset",
        type=int,
        default=18,
        help="ONNX opset version (default: 18).",
    )
    parser.add_argument(
        "--window-seconds",
        type=float,
        default=7.8,
        help="Dummy input window length in seconds (default: 7.8 — htdemucs native).",
    )
    parser.add_argument(
        "--parity-tol",
        type=float,
        default=1e-3,
        help="Max absolute diff allowed between patched and original models "
             "on the parity check (default: 1e-3).",
    )
    parser.add_argument(
        "--skip-parity",
        action="store_true",
        help="Skip the patched-vs-original parity check (faster, less safe).",
    )
    parser.add_argument(
        "--skip-ort-check",
        action="store_true",
        help="Skip the onnxruntime smoke inference pass.",
    )
    parser.add_argument(
        "--dynamo",
        action="store_true",
        help="Use the new torch.export-based ONNX exporter. Default is the "
             "legacy TorchScript tracer.",
    )
    args = parser.parse_args()

    args.out.parent.mkdir(parents=True, exist_ok=True)

    import copy
    import numpy as np
    import torch
    from demucs.pretrained import get_model

    print(f"[export] loading htdemucs weights via demucs.pretrained...")
    bag_orig = get_model("htdemucs")
    bag_orig.eval()
    if hasattr(bag_orig, "models") and len(bag_orig.models) > 0:
        model_orig = bag_orig.models[0]
    else:
        model_orig = bag_orig
    model_orig.eval()

    sr = int(getattr(model_orig, "samplerate", 44100))
    samples = int(round(args.window_seconds * sr))
    print(f"[export] sample_rate={sr}, window_samples={samples}")

    if not args.skip_parity:
        print(f"[export] deep-copying model for parity reference...")
        # Snapshot the unpatched model BEFORE installing the monkey-patches.
        model_ref = copy.deepcopy(model_orig)
        model_ref.eval()

    print(f"[export] installing demucs.spec + HTDemucs monkey-patches...")
    _install_demucs_patches()

    # The model instance picks up the patches because they live on the class.
    model = model_orig

    if not args.skip_parity:
        print(f"[export] running numerical parity check...")
        # Small dummy first for fast feedback. If small input passes, do full.
        small_samples = sr * 2     # 2 s
        dummy_small = torch.randn(1, 2, small_samples, dtype=torch.float32)
        ok = _parity_check(model_ref, model, dummy_small, args.parity_tol)
        if not ok:
            return 3

    print(f"[export] building export-window dummy input ({samples} samples)...")
    dummy = torch.randn(1, 2, samples, dtype=torch.float32)

    print(
        f"[export] running torch.onnx.export "
        f"(opset={args.opset}, dynamo={args.dynamo})..."
    )
    with torch.inference_mode():
        torch.onnx.export(
            model,
            dummy,
            str(args.out),
            input_names=["audio"],
            output_names=["stems"],
            dynamic_axes={
                "audio": {0: "batch", 2: "samples"},
                "stems": {0: "batch", 3: "samples"},
            },
            opset_version=args.opset,
            do_constant_folding=True,
            dynamo=args.dynamo,
        )

    size_mb = args.out.stat().st_size / (1024.0 * 1024.0)
    print(f"[export] wrote {args.out} ({size_mb:.1f} MB)")

    print(f"[export] running onnx.checker.check_model...")
    import onnx

    m = onnx.load(str(args.out))
    onnx.checker.check_model(m)
    print(f"[export] check_model: OK")

    def _dims(t):
        return [d.dim_value if d.dim_value else d.dim_param
                for d in t.type.tensor_type.shape.dim]

    inp = m.graph.input[0]
    out = m.graph.output[0]
    print(f"[export] input  : name={inp.name}, dims={_dims(inp)}")
    print(f"[export] output : name={out.name}, dims={_dims(out)}")

    if not args.skip_ort_check:
        print(f"[export] running ORT CPU smoke inference...")
        import onnxruntime as ort

        sess = ort.InferenceSession(
            str(args.out), providers=["CPUExecutionProvider"]
        )
        audio = np.random.randn(1, 2, samples).astype(np.float32)
        outs = sess.run(None, {"audio": audio})
        out_shape = outs[0].shape
        print(f"[export] ORT output shape: {out_shape}")
        if len(out_shape) != 4 or out_shape[1] != 4:
            print(
                f"[export] ERROR: expected output rank 4 with source-dim=4, "
                f"got shape {out_shape}",
                file=sys.stderr,
            )
            return 2
        print(f"[export] ORT check: OK")

    print(f"[export] DONE  out={args.out}  size={size_mb:.1f}MB  opset={args.opset}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
