import numpy as np
import pytest

from server.remix.ops import (
    OP_REGISTRY,
    apply_chop_bars,
    apply_chop_beats,
    apply_chop_onsets,
    apply_gate_energy,
    apply_structural_cut,
)

SR = 22050


@pytest.fixture(scope="module")
def stereo_5s():
    t = np.linspace(0, 5, SR * 5)
    ch = np.sin(2 * np.pi * 220 * t).astype(np.float32)
    return np.stack([ch, ch])


@pytest.fixture(scope="module")
def silent_5s():
    return np.zeros((2, SR * 5), dtype=np.float32)


# ── apply_chop_beats ──────────────────────────────────────────────────────────

def test_chop_beats_output_shape(stereo_5s):
    out = apply_chop_beats(stereo_5s, SR, {"repeat": 2, "division": 1.0})
    assert out.shape == stereo_5s.shape


def test_chop_beats_half_division(stereo_5s):
    out = apply_chop_beats(stereo_5s, SR, {"repeat": 2, "division": 0.5})
    assert out.shape == stereo_5s.shape



# ── apply_chop_onsets ─────────────────────────────────────────────────────────

def test_chop_onsets_output_shape(stereo_5s):
    out = apply_chop_onsets(stereo_5s, SR, {"min_gap_ms": 80, "threshold": 0.3, "repeat": 2})
    assert out.shape == stereo_5s.shape


# ── apply_chop_bars ───────────────────────────────────────────────────────────

def test_chop_bars_output_shape(stereo_5s):
    out = apply_chop_bars(stereo_5s, SR, {"beats_per_bar": 4, "repeat": 2})
    assert out.shape == stereo_5s.shape


# ── apply_gate_energy ─────────────────────────────────────────────────────────

def test_gate_energy_silent_zeroed(silent_5s):
    out = apply_gate_energy(silent_5s, SR, {"threshold_db": -20.0, "hold_ms": 50.0})
    assert np.all(out == 0.0), "silent audio below threshold must be fully gated out"


def test_gate_energy_shape(stereo_5s):
    out = apply_gate_energy(stereo_5s, SR, {"threshold_db": -20.0, "hold_ms": 50.0})
    assert out.shape == stereo_5s.shape


# ── apply_structural_cut ──────────────────────────────────────────────────────

@pytest.mark.parametrize("mode", ["reverse", "shuffle", "repeat_first"])
def test_structural_cut_length_tolerance(stereo_5s, mode):
    out = apply_structural_cut(stereo_5s, SR, {"n_segments": 4, "mode": mode})
    original_len = stereo_5s.shape[-1]
    ratio = abs(out.shape[-1] - original_len) / original_len
    assert ratio < 0.05, (
        f"mode={mode}: output length {out.shape[-1]} deviates {ratio:.1%} from {original_len}"
    )


def test_structural_cut_reverse_different_from_input(stereo_5s):
    out = apply_structural_cut(stereo_5s, SR, {"n_segments": 4, "mode": "reverse"})
    # reversed segments should differ from original order
    assert not np.array_equal(out[..., :out.shape[-1]], stereo_5s[..., :out.shape[-1]])


# ── registry checks ───────────────────────────────────────────────────────────

def test_all_new_ops_in_registry():
    new_ops = {"chop_beats", "chop_onsets", "chop_bars", "gate_energy", "structural_cut"}
    assert new_ops.issubset(OP_REGISTRY.keys()), (
        f"missing from registry: {new_ops - set(OP_REGISTRY.keys())}"
    )


def test_existing_ops_still_in_registry():
    legacy_ops = {"time_stretch", "pitch_shift", "reverb", "chop", "bass_boost", "eq_highpass"}
    assert legacy_ops.issubset(OP_REGISTRY.keys()), (
        f"regression: missing legacy ops: {legacy_ops - set(OP_REGISTRY.keys())}"
    )
