from abc import ABC, abstractmethod
from pathlib import Path
from dataclasses import dataclass

@dataclass
class StemPaths:
    vocals: Path
    drums: Path
    bass: Path
    other: Path

class IStemSeparator(ABC):
    """Abstract base for stem separation backends."""

    @property
    @abstractmethod
    def separator_id(self) -> str:
        """Unique identifier, e.g. 'spleeter_4stems'"""
        ...

    @property
    @abstractmethod
    def display_name(self) -> str:
        ...

    @abstractmethod
    def is_available(self) -> bool:
        """Return True if model/deps are ready."""
        ...

    @abstractmethod
    def separate(self, input_path: Path, output_dir: Path) -> StemPaths:
        """
        Separate input_path into 4 stems.
        Write stems to output_dir/vocals.wav, drums.wav, bass.wav, other.wav
        Return StemPaths with absolute paths.
        """
        ...
