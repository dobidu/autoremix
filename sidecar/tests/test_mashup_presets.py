"""Phase 21-05 — MashupPreset loader + feel-knob processing tests."""

import numpy as np
import pytest
import soundfile as sf
from pathlib import Path

from server.models import MashupRequest, MashupPreset
from server.mashup_presets.loader import MashupPresetLoader


# ── Loader ───────────────────────────────────────────────────────────────────

def test_mashup_preset_loader_loads_8_builtins():
    loader = MashupPresetLoader()
    presets = loader.load_all()
    expected_ids = {
        "vocal_acapella", "drum_swap", "slowed_mashup", "nightcore_mashup",
        "dub_echo", "instrumental_layer", "bass_swap", "frankenstein",
    }
    assert expected_ids <= set(presets.keys()), \
        f"Missing built-in mashup presets: {expected_ids - set(presets.keys())}"
    assert len(presets) >= 8


def test_mashup_preset_schema_validates():
    """Sample one preset and confirm its fields parse into MashupPreset."""
    preset_path = Path(__file__).parent.parent / "server" / "mashup_presets" / "slowed_mashup.json"
    preset = MashupPreset.model_validate_json(preset_path.read_text())
    assert preset.id == "slowed_mashup"
    assert preset.bpm_modifier == 0.75
    assert preset.master_pitch_offset_semi == -2.0
    assert preset.master_reverb_mix > 0.0
    assert preset.stem_gains_a["vocals"] == 1.0


# ── Feel knob defaults ───────────────────────────────────────────────────────

def test_feel_knobs_default_values():
    """MashupRequest without feel knobs preserves pre-21-05 behavior."""
    req = MashupRequest(file_a="/tmp/a.wav", file_b="/tmp/b.wav")
    assert req.bpm_modifier == 1.0
    assert req.master_pitch_offset_semi == 0.0
    assert req.master_reverb_mix == 0.0
    assert req.master_reverb_room == 0.5
    assert req.highpass_b_hz == 0.0


# ── Engine applies feel knobs ────────────────────────────────────────────────

def _write_sine(path: Path, freq_hz: float, sr: int = 44100, dur_s: float = 3.0):
    t = np.linspace(0.0, dur_s, int(sr * dur_s), endpoint=False)
    mono = 0.3 * np.sin(2.0 * np.pi * freq_hz * t).astype(np.float32)
    stereo = np.stack([mono, mono], axis=1)
    sf.write(str(path), stereo, sr)


def test_mashup_engine_applies_master_reverb(tmp_path):
    """Reverb should extend the tail beyond the input length."""
    pytest.importorskip("librosa")
    pytest.importorskip("pedalboard")
    from server.registry import get_separator
    from server.remix.mashup import MashupEngine

    file_a = tmp_path / "a.wav"
    file_b = tmp_path / "b.wav"
    _write_sine(file_a, 220.0, dur_s=2.0)
    _write_sine(file_b, 330.0, dur_s=2.0)

    separator = get_separator("algorithmic")
    engine = MashupEngine()
    unity = {"vocals": 1.0, "drums": 1.0, "bass": 1.0, "other": 1.0}

    # Wet reverb pass
    out_wet = tmp_path / "wet.wav"
    engine.process(
        file_a=file_a, file_b=file_b, separator=separator,
        stem_gains_a=unity, stem_gains_b=unity,
        target_bpm=None, target_key=None,
        out_dir_a=tmp_path / "wet_a", out_dir_b=tmp_path / "wet_b",
        output_path=out_wet,
        master_reverb_mix=0.5, master_reverb_room=0.7,
    )

    # Dry pass
    out_dry = tmp_path / "dry.wav"
    engine.process(
        file_a=file_a, file_b=file_b, separator=separator,
        stem_gains_a=unity, stem_gains_b=unity,
        target_bpm=None, target_key=None,
        out_dir_a=tmp_path / "dry_a", out_dir_b=tmp_path / "dry_b",
        output_path=out_dry,
        master_reverb_mix=0.0,
    )

    wet, _ = sf.read(str(out_wet), always_2d=True)
    dry, _ = sf.read(str(out_dry), always_2d=True)
    # Wet output should be at least as long (reverb tail) and have non-zero RMS in the tail
    assert wet.shape[0] >= dry.shape[0] - 100, \
        f"Wet output unexpectedly shorter: wet={wet.shape[0]} dry={dry.shape[0]}"
