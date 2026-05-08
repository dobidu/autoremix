from pydantic import BaseModel
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

class RemixResponse(BaseModel):
    success: bool
    output_path: Optional[str] = None
    error: Optional[str] = None

class HealthResponse(BaseModel):
    status: str
    version: str = "0.2.0"
    available_separators: list[str]
    available_engines: list[str]
