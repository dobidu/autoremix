import numpy as np
import pytest
from server.remix.ops import (
    apply_time_stretch, apply_pitch_shift, apply_reverb,
    apply_chop, apply_bass_boost, apply_eq_highpass, OP_REGISTRY,
)

SR = 44100


def make_stereo(seconds=1.0):
    rng = np.random.default_rng(0)
    return (rng.standard_normal((2, int(SR * seconds))) * 0.05).astype(np.float32)


def test_registry_has_six_ops():
    assert len(OP_REGISTRY) == 6
    assert set(OP_REGISTRY) == {
        "time_stretch", "pitch_shift", "reverb",
        "chop", "bass_boost", "eq_highpass",
    }


def test_time_stretch_slows_down():
    audio = make_stereo(1.0)
    out = apply_time_stretch(audio, SR, {"factor": 0.5})
    assert out.shape[-1] > audio.shape[-1]


def test_pitch_shift_preserves_length():
    audio = make_stereo(0.5)
    out = apply_pitch_shift(audio, SR, {"semitones": -4.0})
    assert out.shape[-1] == audio.shape[-1]


def test_reverb_returns_float32():
    audio = make_stereo(0.5)
    out = apply_reverb(audio, SR, {"mix": 0.3})
    assert out.dtype == np.float32


def test_chop_zero_interval_returns_unchanged():
    audio = make_stereo(0.5)
    out = apply_chop(audio, SR, {"interval_ms": 0.0})
    np.testing.assert_array_equal(out, audio)


def test_chop_nonzero_preserves_shape():
    audio = make_stereo(1.0)
    out = apply_chop(audio, SR, {"interval_ms": 500.0})
    assert out.shape == audio.shape


def test_bass_boost_returns_float32():
    audio = make_stereo(0.5)
    out = apply_bass_boost(audio, SR, {"db": 6.0})
    assert out.dtype == np.float32


def test_eq_highpass_returns_float32():
    audio = make_stereo(0.5)
    out = apply_eq_highpass(audio, SR, {"cutoff_hz": 300.0})
    assert out.dtype == np.float32
