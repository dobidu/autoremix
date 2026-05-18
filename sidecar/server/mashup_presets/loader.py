import logging
import os
import sys
from pathlib import Path

from ..models import MashupPreset

logger = logging.getLogger(__name__)


def _user_preset_dir() -> Path:
    if sys.platform == "win32":
        base = Path(os.environ.get("APPDATA", Path.home()))
        return base / "autoremix" / "mashup"
    return Path.home() / ".config" / "autoremix" / "mashup"


class MashupPresetLoader:
    BUILTIN_DIR = Path(__file__).parent
    USER_DIR = _user_preset_dir()

    def load_all(self) -> dict[str, MashupPreset]:
        presets: dict[str, MashupPreset] = {}

        for path in sorted(self.BUILTIN_DIR.glob("*.json")):
            try:
                preset = MashupPreset.model_validate_json(path.read_text())
                presets[preset.id] = preset
            except Exception as e:
                logger.warning("Failed to load built-in mashup preset %s: %s", path.name, e)

        if self.USER_DIR.exists():
            for path in sorted(self.USER_DIR.glob("*.json")):
                try:
                    preset = MashupPreset.model_validate_json(path.read_text())
                    presets[preset.id] = preset  # user overrides built-in on id collision
                    logger.info("Loaded user mashup preset: %s", preset.id)
                except Exception as e:
                    logger.warning("Failed to load user mashup preset %s: %s", path.name, e)

        return presets
