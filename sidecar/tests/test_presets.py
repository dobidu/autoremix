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


async def test_new_styles_present(client):
    r = await client.get("/api/v1/presets")
    ids = {p["id"] for p in r.json()}
    for style in ("phonk", "jersey_club", "nightcore"):
        assert style in ids, f"Missing preset: {style}"


async def test_effect_chain_presets_have_effects(client):
    r = await client.get("/api/v1/presets")
    for p in r.json():
        if p.get("engine") == "effect_chain":
            assert p["effects"], f"Preset {p['id']} has empty effects"


async def test_phonk_effect_chain_structure(client):
    r = await client.get("/api/v1/presets")
    phonk = next(p for p in r.json() if p["id"] == "phonk")
    assert phonk["engine"] == "effect_chain"
    ops = [e["op"] for e in phonk["effects"]]
    assert "time_stretch" in ops
    assert "bass_boost" in ops


async def test_nightcore_sped_up(client):
    r = await client.get("/api/v1/presets")
    nc = next(p for p in r.json() if p["id"] == "nightcore")
    ts = next(e for e in nc["effects"] if e["op"] == "time_stretch")
    assert ts["params"]["factor"] > 1.0, "Nightcore must speed up"
    ps = next(e for e in nc["effects"] if e["op"] == "pitch_shift")
    assert ps["params"]["semitones"] > 0, "Nightcore must pitch up"
