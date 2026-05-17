import numpy as np
import pytest
import soundfile as sf
from pathlib import Path
from server.remix.chain_interpreter import EffectChainEngine
from server.models import RemixPreset, PresetParams, StemMix
from server.separators.base import StemPaths

SR = 44100
DURATION = 2.0


@pytest.fixture
def stem_wavs(tmp_path):
    """Write 4 synthetic 2s stereo stem WAVs; return StemPaths."""
    rng = np.random.default_rng(1)
    paths = {}
    for name in ["vocals", "drums", "bass", "other"]:
        audio = (rng.standard_normal((2, int(SR * DURATION))) * 0.05).astype(np.float32)
        p = tmp_path / f"{name}.wav"
        sf.write(str(p), audio.T, SR)
        paths[name] = p
    return StemPaths(
        vocals=paths["vocals"],
        drums=paths["drums"],
        bass=paths["bass"],
        other=paths["other"],
    )


def make_preset(effects):
    return RemixPreset(
        id="test_chain", version="2.0", name="Test",
        engine="chopped_screwed",
        params=PresetParams(),
        stem_mix=StemMix(),
        effects=effects,
    )


def test_chain_writes_output(stem_wavs, tmp_path):
    out = tmp_path / "out.wav"
    preset = make_preset([
        {"op": "reverb", "stems": "all", "params": {"mix": 0.1}},
    ])
    result = EffectChainEngine().process(stem_wavs, preset, out)
    assert result == out
    assert out.exists()
    assert out.stat().st_size > 0


def test_time_stretch_changes_duration(stem_wavs, tmp_path):
    out = tmp_path / "out.wav"
    preset = make_preset([
        {"op": "time_stretch", "stems": "all", "params": {"factor": 0.7}},
    ])
    EffectChainEngine().process(stem_wavs, preset, out)
    audio, sr = sf.read(str(out))
    expected = DURATION / 0.7
    actual = len(audio) / sr
    assert abs(actual - expected) / expected < 0.05


def test_individual_stem_targeting(stem_wavs, tmp_path):
    out = tmp_path / "out.wav"
    preset = make_preset([
        {"op": "bass_boost", "stems": ["bass"], "params": {"db": 6.0}},
    ])
    EffectChainEngine().process(stem_wavs, preset, out)
    assert out.exists()
    assert out.stat().st_size > 0


def test_unknown_op_raises(stem_wavs, tmp_path):
    preset = make_preset([
        {"op": "nonexistent", "stems": "all", "params": {}},
    ])
    with pytest.raises(ValueError, match="nonexistent"):
        EffectChainEngine().process(stem_wavs, preset, tmp_path / "out.wav")


async def test_dispatch_chain_via_http(client, stem_wavs, tmp_path, monkeypatch):
    import server.main as main_mod
    chain_preset = make_preset([
        {"op": "bass_boost", "stems": "all", "params": {"db": 3.0}},
    ])
    monkeypatch.setitem(main_mod._presets, "test_chain_dispatch", chain_preset)
    out = str(tmp_path / "chain_out.wav")
    r = await client.post("/api/v1/remix", json={
        "vocals_path": str(stem_wavs.vocals),
        "drums_path":  str(stem_wavs.drums),
        "bass_path":   str(stem_wavs.bass),
        "other_path":  str(stem_wavs.other),
        "output_path": out,
        "engine_id":   "test_chain_dispatch",
    })
    assert r.status_code == 200
    data = r.json()
    assert data["success"] is True, f"chain dispatch failed: {data.get('error')}"
    assert Path(data["output_path"]).exists()
