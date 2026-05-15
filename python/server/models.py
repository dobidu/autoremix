from pydantic import BaseModel
from pathlib import Path
from typing import Optional

class SeparateRequest(BaseModel):
    input_path: str
    output_dir: str
    separator_id: str = "spleeter_4stems"

class StemPaths(BaseModel):
    vocals: str
    drums: str
    bass: str
    other: str

    @classmethod
    def from_domain(cls, stems: object) -> "StemPaths":
        return cls(
            vocals=str(stems.vocals),
            drums=str(stems.drums),
            bass=str(stems.bass),
            other=str(stems.other),
        )

    def to_domain(self) -> object:
        from .separators.base import StemPaths as _StemPaths
        return _StemPaths(
            vocals=Path(self.vocals),
            drums=Path(self.drums),
            bass=Path(self.bass),
            other=Path(self.other),
        )

class SeparateResponse(BaseModel):
    success: bool
    stems: Optional[StemPaths] = None
    error: Optional[str] = None

class RemixRequest(BaseModel):
    vocals_path: str
    drums_path: str
    bass_path: str
    other_path: str
    output_path: str
    engine_id: str = "chopped_screwed"
    tempo_factor: float = 0.70
    pitch_shift_semi: float = -4.0
    reverb_mix: float = 0.0
    chop_interval_ms: float = 0.0
    bass_boost_db: float = 0.0
    drums_tempo_factor: float = 1.0

    def to_stems(self) -> object:
        from .separators.base import StemPaths as _StemPaths
        return _StemPaths(
            vocals=Path(self.vocals_path),
            drums=Path(self.drums_path),
            bass=Path(self.bass_path),
            other=Path(self.other_path),
        )

    def to_params(self) -> object:
        from .remix.base import RemixParams as _RemixParams
        return _RemixParams(
            tempo_factor=self.tempo_factor,
            pitch_shift_semi=self.pitch_shift_semi,
            reverb_mix=self.reverb_mix,
            chop_interval_ms=self.chop_interval_ms,
            bass_boost_db=self.bass_boost_db,
            drums_tempo_factor=self.drums_tempo_factor,
        )

class RemixResponse(BaseModel):
    success: bool
    output_path: Optional[str] = None
    error: Optional[str] = None

class HealthResponse(BaseModel):
    status: str
    version: str = "0.4.0"
    available_separators: list[str]
    available_engines: list[str]


class PresetParams(BaseModel):
    tempo_factor: float = 1.0
    pitch_shift_semi: float = 0.0
    reverb_mix: float = 0.0
    chop_interval_ms: float = 0.0
    bass_boost_db: float = 0.0
    drums_tempo_factor: float = 1.0


class StemMix(BaseModel):
    vocals: float = 1.0
    drums: float = 1.0
    bass: float = 1.0
    other: float = 1.0


class RemixPreset(BaseModel):
    id: str
    version: str
    name: str
    engine: str
    params: PresetParams
    description: str = ""
    author: str = ""
    tags: list[str] = []
    stem_mix: StemMix = StemMix()
    effects: list = []


class PresetSummary(BaseModel):
    id: str
    name: str
    params: PresetParams
    stem_mix: StemMix
