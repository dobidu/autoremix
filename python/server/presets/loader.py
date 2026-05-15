import sys
import os
import logging
from pathlib import Path
from ..models import RemixPreset

logger = logging.getLogger(__name__)


def _user_preset_dir() -> Path:
    if sys.platform == "win32":
        base = Path(os.environ.get("APPDATA", Path.home()))
        return base / "autoremix" / "modes"
    return Path.home() / ".config" / "autoremix" / "modes"


class PresetLoader:
    BUILTIN_DIR = Path(__file__).parent
    USER_DIR = _user_preset_dir()

    def load_all(self) -> dict[str, RemixPreset]:
        presets: dict[str, RemixPreset] = {}

        for path in sorted(self.BUILTIN_DIR.glob("*.json")):
            if path.name == "schema.json":
                continue
            try:
                preset = RemixPreset.model_validate_json(path.read_text())
                presets[preset.id] = preset
            except Exception as e:
                logger.warning("Failed to load built-in preset %s: %s", path.name, e)

        if self.USER_DIR.exists():
            for path in sorted(self.USER_DIR.glob("*.json")):
                try:
                    preset = RemixPreset.model_validate_json(path.read_text())
                    presets[preset.id] = preset  # user overrides built-in on id collision
                    logger.info("Loaded user preset: %s", preset.id)
                except Exception as e:
                    logger.warning("Failed to load user preset %s: %s", path.name, e)

        return presets
