from abc import ABC, abstractmethod
from pathlib import Path
from dataclasses import dataclass
import numpy as np
import librosa
from ..separators.base import StemPaths


def time_stretch(audio: np.ndarray, rate: float) -> np.ndarray:
    """Stereo-safe librosa time stretch. rate<1 = slower."""
    if audio.ndim == 1:
        return librosa.effects.time_stretch(audio, rate=rate)
    return np.stack([librosa.effects.time_stretch(ch, rate=rate) for ch in audio])


def pitch_shift(audio: np.ndarray, sr: int, n_steps: float) -> np.ndarray:
    """Stereo-safe librosa pitch shift."""
    if audio.ndim == 1:
        return librosa.effects.pitch_shift(audio, sr=sr, n_steps=n_steps)
    return np.stack([librosa.effects.pitch_shift(ch, sr=sr, n_steps=n_steps) for ch in audio])

@dataclass
class RemixParams:
    tempo_factor: float = 1.0
    pitch_shift_semi: float = 0.0
    reverb_mix: float = 0.0
    chop_interval_ms: float = 0.0
    bass_boost_db: float = 0.0
    drums_tempo_factor: float = 1.0

class IRemixEngine(ABC):
    """Abstract base for remix transformation backends."""

    @property
    @abstractmethod
    def engine_id(self) -> str:
        ...

    @property
    @abstractmethod
    def display_name(self) -> str:
        ...

    @abstractmethod
    def get_default_params(self) -> RemixParams:
        ...

    @abstractmethod
    def process(self, stems: StemPaths, params: RemixParams, output_path: Path) -> Path:
        """
        Transform stems according to params.
        Write result to output_path.
        Return output_path on success, raise on failure.
        """
        ...
