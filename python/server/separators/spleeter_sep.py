from pathlib import Path
from .base import IStemSeparator, StemPaths
import logging

logger = logging.getLogger(__name__)

class SpleeterSeparator(IStemSeparator):
    """
    Stem separator using Deezer's Spleeter (4stems model).
    Lazy-loads the model on first call to avoid startup delay.
    """

    def __init__(self):
        self._separator = None

    @property
    def separator_id(self) -> str:
        return "spleeter_4stems"

    @property
    def display_name(self) -> str:
        return "Spleeter (4 stems - Deezer)"

    def is_available(self) -> bool:
        try:
            from spleeter.separator import Separator
            return True
        except ImportError:
            return False

    def _get_separator(self):
        if self._separator is None:
            from spleeter.separator import Separator
            logger.info("Loading Spleeter 4stems model...")
            self._separator = Separator("spleeter:4stems")
            logger.info("Spleeter model loaded.")
        return self._separator

    def separate(self, input_path: Path, output_dir: Path) -> StemPaths:
        output_dir.mkdir(parents=True, exist_ok=True)
        sep = self._get_separator()

        # Spleeter writes to output_dir/track_name/vocals.wav etc.
        sep.separate_to_file(str(input_path), str(output_dir))

        # Spleeter creates a subdir named after the input file (sans extension)
        stem_dir = output_dir / input_path.stem

        stems = StemPaths(
            vocals=stem_dir / "vocals.wav",
            drums=stem_dir / "drums.wav",
            bass=stem_dir / "bass.wav",
            other=stem_dir / "other.wav",
        )

        for stem_path in [stems.vocals, stems.drums, stems.bass, stems.other]:
            if not stem_path.exists():
                raise FileNotFoundError(f"Spleeter did not produce {stem_path}")

        return stems
