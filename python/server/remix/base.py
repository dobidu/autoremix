from abc import ABC, abstractmethod
from pathlib import Path
from dataclasses import dataclass
from ..separators.base import StemPaths

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
