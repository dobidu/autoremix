import pytest
from pathlib import Path


@pytest.fixture
async def stems(client, test_wav, tmp_path):
    """Separate test_wav once, return stem paths dict."""
    r = await client.post("/api/v1/separate", json={
        "input_path":   str(test_wav),
        "output_dir":   str(tmp_path),
        "separator_id": "algorithmic",
    })
    data = r.json()
    assert data["success"] is True, f"stem fixture failed: {data.get('error')}"
    return data["stems"]


@pytest.mark.parametrize("engine_id", [
    "chopped_screwed",
    "slowed_reverb",
    "drum_and_bass",
])
async def test_remix_engine(client, stems, tmp_path, engine_id):
    out = tmp_path / f"{engine_id}_out.wav"
    r = await client.post("/api/v1/remix", json={
        "vocals_path": stems["vocals"],
        "drums_path":  stems["drums"],
        "bass_path":   stems["bass"],
        "other_path":  stems["other"],
        "output_path": str(out),
        "engine_id":   engine_id,
    })
    assert r.status_code == 200
    data = r.json()
    assert data["success"] is True, f"engine {engine_id} failed: {data.get('error')}"
    result_path = Path(data["output_path"])
    assert result_path.exists(), f"output file missing: {result_path}"
    assert result_path.stat().st_size > 0, f"output file empty: {result_path}"


async def test_remix_unknown_engine(client, stems, tmp_path):
    r = await client.post("/api/v1/remix", json={
        "vocals_path": stems["vocals"],
        "drums_path":  stems["drums"],
        "bass_path":   stems["bass"],
        "other_path":  stems["other"],
        "output_path": str(tmp_path / "out.wav"),
        "engine_id":   "does_not_exist",
    })
    assert r.status_code == 400, f"expected 400, got {r.status_code}: {r.text}"
