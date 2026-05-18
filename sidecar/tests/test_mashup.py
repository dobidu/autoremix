"""Phase 21-01 — MashupEngine + endpoint tests."""

from pathlib import Path

import numpy as np
import pytest
import soundfile as sf
from pydantic import ValidationError

from server.models import MashupRequest
from server.remix.analysis import semitone_delta


# ── T1: schema validation ────────────────────────────────────────────────────

def test_mashup_request_schema_valid():
    req = MashupRequest(
        file_a="/tmp/a.wav",
        file_b="/tmp/b.wav",
        separator_id="algorithmic",
        stem_gains_a={"vocals": 1.0, "drums": 0.0, "bass": 0.0, "other": 0.0},
        stem_gains_b={"vocals": 0.0, "drums": 1.0, "bass": 1.0, "other": 1.0},
        target_bpm=120.0,
        target_key="C",
    )
    assert req.stem_gains_a["vocals"] == 1.0
    assert req.stem_gains_b["drums"] == 1.0
    assert req.target_bpm == 120.0


def test_mashup_request_defaults_to_unity_gains():
    req = MashupRequest(file_a="/tmp/a.wav", file_b="/tmp/b.wav")
    assert req.stem_gains_a == {"vocals": 1.0, "drums": 1.0, "bass": 1.0, "other": 1.0}
    assert req.stem_gains_b == {"vocals": 1.0, "drums": 1.0, "bass": 1.0, "other": 1.0}


# ── T2: semitone_delta ───────────────────────────────────────────────────────

def test_semitone_delta_same_key():
    assert semitone_delta("C", "C") == 0
    assert semitone_delta("Am", "Am") == 0


def test_semitone_delta_up_minor_third():
    assert semitone_delta("C", "Eb") == 3


def test_semitone_delta_wraps_negative():
    # C → A: forward = +9, back = -3; pick -3
    assert semitone_delta("C", "A") == -3


# ── T3: end-to-end mashup ────────────────────────────────────────────────────

def _write_sine(path: Path, freq_hz: float, sr: int = 44100, dur_s: float = 5.0):
    t = np.linspace(0.0, dur_s, int(sr * dur_s), endpoint=False)
    mono = 0.3 * np.sin(2.0 * np.pi * freq_hz * t).astype(np.float32)
    stereo = np.stack([mono, mono], axis=1)  # (samples, channels)
    sf.write(str(path), stereo, sr)


def test_mashup_end_to_end(tmp_path):
    librosa = pytest.importorskip("librosa")
    from server.registry import get_separator
    from server.remix.mashup import MashupEngine

    file_a = tmp_path / "a.wav"
    file_b = tmp_path / "b.wav"
    _write_sine(file_a, 220.0, dur_s=5.0)
    _write_sine(file_b, 330.0, dur_s=4.0)  # shorter — sets truncation length

    separator = get_separator("algorithmic")
    out_dir_a = tmp_path / "stems_a"
    out_dir_b = tmp_path / "stems_b"
    out_path = tmp_path / "mashup_out.wav"

    engine = MashupEngine()
    gains_a = {"vocals": 1.0, "drums": 0.0, "bass": 0.0, "other": 0.0}
    gains_b = {"vocals": 0.0, "drums": 1.0, "bass": 1.0, "other": 1.0}
    result = engine.process(
        file_a=file_a,
        file_b=file_b,
        separator=separator,
        stem_gains_a=gains_a,
        stem_gains_b=gains_b,
        target_bpm=None,
        target_key=None,
        out_dir_a=out_dir_a,
        out_dir_b=out_dir_b,
        output_path=out_path,
    )

    assert out_path.exists()
    assert result["length_sec"] > 0
    # Truncated to shorter input; allow 10% slack for stretch + analysis windowing
    assert result["length_sec"] <= 4.5
    assert "target_bpm" in result and result["target_bpm"] > 0
    assert isinstance(result["target_key"], str) and len(result["target_key"]) >= 1


def test_mashup_stretch_factor_clamped(tmp_path):
    """If target_bpm is wildly different from B's BPM, stretch must clamp to [0.5, 2.0]."""
    pytest.importorskip("librosa")
    from server.registry import get_separator
    from server.remix.mashup import MashupEngine

    file_a = tmp_path / "a.wav"
    file_b = tmp_path / "b.wav"
    _write_sine(file_a, 220.0, dur_s=3.0)
    _write_sine(file_b, 330.0, dur_s=3.0)

    separator = get_separator("algorithmic")
    engine = MashupEngine()
    unity = {"vocals": 1.0, "drums": 1.0, "bass": 1.0, "other": 1.0}
    result = engine.process(
        file_a=file_a,
        file_b=file_b,
        separator=separator,
        stem_gains_a=unity,
        stem_gains_b=unity,
        target_bpm=1000.0,  # absurd — should clamp to 2.0
        target_key=None,
        out_dir_a=tmp_path / "stems_a",
        out_dir_b=tmp_path / "stems_b",
        output_path=tmp_path / "out.wav",
    )
    assert result["stretch_factor_b"] <= 2.0 + 1e-6
    assert result["stretch_factor_b"] >= 0.5 - 1e-6
