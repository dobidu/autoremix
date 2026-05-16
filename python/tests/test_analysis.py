import numpy as np
import pytest

from server.remix.analysis import (
    detect_bars,
    detect_beats,
    detect_energy_gates,
    detect_onsets,
    detect_structure,
)

SR = 22050


@pytest.fixture
def sine_audio():
    t = np.linspace(0, 5, SR * 5)
    ch = np.sin(2 * np.pi * 440 * t).astype(np.float32)
    return np.stack([ch, ch]), SR


@pytest.fixture
def percussive_audio():
    audio = np.zeros((2, SR * 5), dtype=np.float32)
    for i in range(10):
        idx = int(i * 0.5 * SR)
        audio[:, idx : idx + 100] = 1.0
    return audio, SR


@pytest.fixture
def loud_audio():
    """Audio well above -20 dB."""
    ch = np.ones(SR * 5, dtype=np.float32) * 0.9
    return np.stack([ch, ch]), SR


@pytest.fixture
def silent_audio():
    return np.zeros((2, SR * 5), dtype=np.float32), SR


# ── detect_beats ──────────────────────────────────────────────────────────────

def test_detect_beats_returns_sorted_floats(sine_audio):
    audio, sr = sine_audio
    beats = detect_beats(audio, sr)
    assert isinstance(beats, np.ndarray)
    assert beats.dtype.kind == "f"
    assert len(beats) > 0
    assert np.all(np.diff(beats) >= 0), "beats must be sorted ascending"


def test_detect_beats_short_audio_no_crash():
    audio = np.zeros((2, 100), dtype=np.float32)
    result = detect_beats(audio, SR)
    assert isinstance(result, np.ndarray)
    assert len(result) >= 1


# ── detect_onsets ─────────────────────────────────────────────────────────────

def test_detect_onsets_min_gap_respected(percussive_audio):
    audio, sr = percussive_audio
    min_gap_ms = 400.0
    onsets = detect_onsets(audio, sr, min_gap_ms=min_gap_ms)
    assert isinstance(onsets, np.ndarray)
    if len(onsets) > 1:
        diffs = np.diff(onsets)
        assert np.all(diffs >= min_gap_ms / 1000.0 - 1e-6), (
            f"min gap violated: {diffs.min():.4f}s < {min_gap_ms/1000:.4f}s"
        )


def test_detect_onsets_returns_floats(sine_audio):
    audio, sr = sine_audio
    onsets = detect_onsets(audio, sr)
    assert onsets.dtype.kind == "f"
    assert len(onsets) >= 1


# ── detect_bars ───────────────────────────────────────────────────────────────

def test_detect_bars_subset_of_beats(sine_audio):
    audio, sr = sine_audio
    beat_times = detect_beats(audio, sr)
    bar_times = detect_bars(audio, sr, beats_per_bar=4)
    # every bar timestamp must exist in beat_times within 5ms tolerance
    for bt in bar_times:
        diffs = np.abs(beat_times - bt)
        assert diffs.min() < 0.005, (
            f"bar time {bt:.4f}s not found in beat grid (closest diff={diffs.min():.4f}s)"
        )


def test_detect_bars_returns_subset(sine_audio):
    audio, sr = sine_audio
    beats = detect_beats(audio, sr)
    bars = detect_bars(audio, sr, beats_per_bar=4)
    assert len(bars) <= len(beats)


# ── detect_energy_gates ───────────────────────────────────────────────────────

def test_detect_energy_gates_shape(sine_audio):
    audio, sr = sine_audio
    mask = detect_energy_gates(audio, sr)
    assert mask.shape == (audio.shape[-1],)
    assert mask.dtype == bool


def test_detect_energy_gates_silent_is_false(silent_audio):
    audio, sr = silent_audio
    mask = detect_energy_gates(audio, sr, threshold_db=-20.0)
    assert mask.dtype == bool
    # silent audio far below threshold → all False
    assert not mask.any(), "silent audio should produce all-False gate mask"


def test_detect_energy_gates_loud_is_true(loud_audio):
    audio, sr = loud_audio
    mask = detect_energy_gates(audio, sr, threshold_db=-20.0)
    # loud audio should be mostly True (allow for edge frames)
    assert mask.mean() > 0.8, f"loud audio gate ratio too low: {mask.mean():.2f}"


# ── detect_structure ──────────────────────────────────────────────────────────

def test_detect_structure_covers_duration(sine_audio):
    audio, sr = sine_audio
    duration = audio.shape[-1] / sr
    segs = detect_structure(audio, sr, n_segments=4)
    assert isinstance(segs, list)
    assert len(segs) >= 1
    # first segment starts at 0
    assert abs(segs[0][0]) < 0.05, f"first segment starts at {segs[0][0]:.4f}s, expected ~0"
    # last segment ends at duration
    assert abs(segs[-1][1] - duration) < 0.1, (
        f"last segment ends at {segs[-1][1]:.4f}s, expected ~{duration:.4f}s"
    )
    # no gaps > 0.1s, no overlaps
    for i in range(len(segs) - 1):
        gap = segs[i + 1][0] - segs[i][1]
        assert abs(gap) < 0.1, f"gap between segments {i} and {i+1}: {gap:.4f}s"


def test_detect_structure_segment_count():
    # 10s audio
    audio = np.random.randn(2, SR * 10).astype(np.float32)
    segs = detect_structure(audio, SR, n_segments=4)
    assert len(segs) == 4, f"expected 4 segments, got {len(segs)}"
