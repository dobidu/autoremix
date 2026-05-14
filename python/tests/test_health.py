async def test_health(client):
    r = await client.get("/api/v1/health")
    assert r.status_code == 200
    data = r.json()
    assert data["status"] == "ok"
    assert len(data["available_engines"]) == 3
    assert len(data["available_separators"]) >= 1
    assert "chopped_screwed" in data["available_engines"]
    assert "slowed_reverb" in data["available_engines"]
    assert "drum_and_bass" in data["available_engines"]
    assert "algorithmic" in data["available_separators"]
