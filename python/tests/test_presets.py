import pytest


async def test_presets_returns_200(client):
    r = await client.get("/api/v1/presets")
    assert r.status_code == 200


async def test_presets_has_three_builtins(client):
    r = await client.get("/api/v1/presets")
    data = r.json()
    assert isinstance(data, list)
    assert len(data) >= 3
    ids = {p["id"] for p in data}
    assert {"chopped_screwed", "slowed_reverb", "drum_and_bass"} <= ids


async def test_preset_params_structure(client):
    r = await client.get("/api/v1/presets")
    for preset in r.json():
        params = preset["params"]
        for field in ("tempo_factor", "pitch_shift_semi", "reverb_mix",
                      "chop_interval_ms", "bass_boost_db", "drums_tempo_factor"):
            assert field in params, f"Missing field {field} in preset {preset['id']}"
            assert isinstance(params[field], (int, float))


async def test_preset_stem_mix_structure(client):
    r = await client.get("/api/v1/presets")
    for preset in r.json():
        mix = preset["stem_mix"]
        for stem in ("vocals", "drums", "bass", "other"):
            assert stem in mix
            assert isinstance(mix[stem], (int, float))


async def test_chopped_screwed_default_params(client):
    r = await client.get("/api/v1/presets")
    cs = next(p for p in r.json() if p["id"] == "chopped_screwed")
    assert cs["params"]["tempo_factor"] == pytest.approx(0.70)
    assert cs["params"]["pitch_shift_semi"] == pytest.approx(-4.0)
    assert cs["params"]["chop_interval_ms"] == pytest.approx(500.0)
