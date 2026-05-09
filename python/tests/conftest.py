import pytest
import numpy as np
import soundfile as sf
from pathlib import Path
from httpx import AsyncClient, ASGITransport
from server.main import app


@pytest.fixture(scope="session")
def test_wav(tmp_path_factory) -> Path:
    """Synthetic 3s stereo 44100Hz WAV — no real audio needed."""
    sr = 44100
    rng = np.random.default_rng(42)
    audio = (rng.standard_normal((2, sr * 3)) * 0.05).astype(np.float32)
    path = tmp_path_factory.mktemp("audio") / "test_input.wav"
    sf.write(str(path), audio.T, sr)
    return path


@pytest.fixture
async def client():
    async with AsyncClient(
        transport=ASGITransport(app=app), base_url="http://test"
    ) as c:
        yield c
