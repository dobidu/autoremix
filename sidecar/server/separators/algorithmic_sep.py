from pathlib import Path
from .base import IStemSeparator, StemPaths
import numpy as np
import soundfile as sf
import librosa
import logging

logger = logging.getLogger(__name__)

class AlgorithmicSeparator(IStemSeparator):
    """
    Simple frequency-band separator (no ML).
    Useful for offline use / fallback when Spleeter not available.
    Quality is low — meant for testing the pipeline end-to-end.

    Band assignments (approximate):
      bass   : < 250 Hz
      drums  : 250–2000 Hz (transients)
      vocals : 2000–6000 Hz
      other  : 6000+ Hz
    """

    @property
    def separator_id(self) -> str:
        return "algorithmic"

    @property
    def display_name(self) -> str:
        return "Algorithmic (FFT band-split, low quality)"

    def is_available(self) -> bool:
        return True  # pure Python, always available

    def separate(self, input_path: Path, output_dir: Path) -> StemPaths:
        output_dir.mkdir(parents=True, exist_ok=True)

        audio, sr = librosa.load(str(input_path), sr=None, mono=False)
        if audio.ndim == 1:
            audio = np.stack([audio, audio])

        stems = {
            "bass": self._bandpass(audio, sr, 0, 250),
            "drums": self._bandpass(audio, sr, 250, 2000),
            "vocals": self._bandpass(audio, sr, 2000, 6000),
            "other": self._bandpass(audio, sr, 6000, sr // 2),
        }

        paths = {}
        for name, data in stems.items():
            out_path = output_dir / f"{name}.wav"
            sf.write(str(out_path), data.T, sr)
            paths[name] = out_path

        return StemPaths(**paths)

    def _bandpass(self, audio: np.ndarray, sr: int, low_hz: float, high_hz: float) -> np.ndarray:
        from scipy.signal import butter, sosfilt
        nyq = sr / 2
        low = max(low_hz / nyq, 1e-6)
        high = min(high_hz / nyq, 1.0 - 1e-6)
        if low >= high:
            return np.zeros_like(audio)
        sos = butter(4, [low, high], btype="band", output="sos")
        return sosfilt(sos, audio)
