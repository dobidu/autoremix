from pydantic import BaseModel
from pathlib import Path
from typing import Literal, Optional

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
    stem_mix_override: Optional[dict[str, float]] = None
    chop_mode: str = "fixed"   # "fixed"|"beat"|"onset"|"bar"|"energy"|"structural"

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
    engine: str = "chopped_screwed"
    params: PresetParams
    stem_mix: StemMix
    effects: list = []


class MashupRequest(BaseModel):
    file_a: str
    file_b: str
    separator_id: str = "algorithmic"
    # Per-stem gains for each track (0.0 = silent, 1.0 = unity, 2.0 = +6dB).
    # Keys: "vocals", "drums", "bass", "other". Missing keys default to 1.0.
    stem_gains_a: dict[str, float] = {"vocals": 1.0, "drums": 1.0, "bass": 1.0, "other": 1.0}
    stem_gains_b: dict[str, float] = {"vocals": 1.0, "drums": 1.0, "bass": 1.0, "other": 1.0}
    target_bpm: Optional[float] = None
    target_key: Optional[str] = None
    output_dir: Optional[str] = None
    # Feel knobs (Phase 21-05). Defaults preserve pre-21-05 behavior.
    bpm_modifier:             float = 1.0   # multiplier on anchored BPM
    master_pitch_offset_semi: float = 0.0   # extra shift on top of key match
    master_reverb_mix:        float = 0.0   # 0..1 wet, applied to final mix
    master_reverb_room:       float = 0.5   # 0..1 room size
    highpass_b_hz:            float = 0.0   # cut B's lows before mix (0 = off)


class MashupPreset(BaseModel):
    id: str
    version: str = "1.0"
    name: str
    description: str = ""
    author: str = "AutoRemix"
    tags: list[str] = []
    stem_gains_a: dict[str, float]
    stem_gains_b: dict[str, float]
    target_bpm_mode: Literal["anchor_a", "anchor_b", "average", "absolute"] = "anchor_a"
    target_bpm_absolute: Optional[float] = None
    target_key_mode: Literal["anchor_a", "anchor_b", "absolute"] = "anchor_a"
    target_key_absolute: Optional[str] = None
    bpm_modifier:             float = 1.0
    master_pitch_offset_semi: float = 0.0
    master_reverb_mix:        float = 0.0
    master_reverb_room:       float = 0.5
    highpass_b_hz:            float = 0.0


class MashupResponse(BaseModel):
    success: bool
    output_path: Optional[str] = None
    target_bpm: Optional[float] = None
    target_key: Optional[str] = None
    length_sec: Optional[float] = None
    error: Optional[str] = None


class CreatePresetRequest(BaseModel):
    name: str
    engine_id: str
    tempo_factor: float
    pitch_shift_semi: float
    reverb_mix: float
    chop_interval_ms: float
    bass_boost_db: float = 0.0
    drums_tempo_factor: float = 1.0
    vocals_gain: float = 1.0
    drums_gain: float = 1.0
    bass_gain: float = 1.0
    other_gain: float = 1.0
