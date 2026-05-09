from pathlib import Path


async def test_separate_algorithmic(client, test_wav, tmp_path):
    r = await client.post("/api/v1/separate", json={
        "input_path":   str(test_wav),
        "output_dir":   str(tmp_path),
        "separator_id": "algorithmic",
    })
    assert r.status_code == 200
    data = r.json()
    assert data["success"] is True, f"separation failed: {data.get('error')}"
    stems = data["stems"]
    for key in ("vocals", "drums", "bass", "other"):
        p = Path(stems[key])
        assert p.exists(), f"{key} stem missing at {p}"
        assert p.stat().st_size > 0, f"{key} stem is empty"


async def test_separate_missing_file(client):
    r = await client.post("/api/v1/separate", json={
        "input_path":   "/nonexistent/path/audio.wav",
        "output_dir":   "/tmp",
        "separator_id": "algorithmic",
    })
    assert r.status_code == 400, f"expected 400, got {r.status_code}: {r.text}"


async def test_separate_unknown_separator(client, test_wav):
    r = await client.post("/api/v1/separate", json={
        "input_path":   str(test_wav),
        "output_dir":   "/tmp",
        "separator_id": "does_not_exist",
    })
    assert r.status_code == 400, f"expected 400, got {r.status_code}: {r.text}"
