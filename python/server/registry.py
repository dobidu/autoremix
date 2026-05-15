from typing import Dict, Type
from .separators.base import IStemSeparator
from .remix.base import IRemixEngine
from .separators.spleeter_sep import SpleeterSeparator
from .separators.algorithmic_sep import AlgorithmicSeparator
from .separators.demucs_sep import DemucsSeparator
from .remix.chopped_screwed import ChoppedAndScrewedEngine
from .remix.slowed_reverb import SlowedReverbEngine
from .remix.drum_and_bass import DrumAndBassEngine

# --- Separator registry ---
_SEPARATORS: Dict[str, IStemSeparator] = {}

def _init_separators():
    for cls in [DemucsSeparator, SpleeterSeparator, AlgorithmicSeparator]:
        inst = cls()
        _SEPARATORS[inst.separator_id] = inst

def get_separator(sep_id: str) -> IStemSeparator:
    if not _SEPARATORS:
        _init_separators()
    sep = _SEPARATORS.get(sep_id)
    if sep is None:
        raise KeyError(f"Unknown separator: {sep_id}. Available: {list(_SEPARATORS.keys())}")
    return sep

def list_separators() -> list[str]:
    if not _SEPARATORS:
        _init_separators()
    return [k for k, v in _SEPARATORS.items() if v.is_available()]

# --- Engine registry ---
_ENGINES: Dict[str, IRemixEngine] = {}

def _init_engines():
    for cls in [ChoppedAndScrewedEngine, SlowedReverbEngine, DrumAndBassEngine]:
        inst = cls()
        _ENGINES[inst.engine_id] = inst

def get_engine(engine_id: str) -> IRemixEngine:
    if not _ENGINES:
        _init_engines()
    eng = _ENGINES.get(engine_id)
    if eng is None:
        raise KeyError(f"Unknown engine: {engine_id}. Available: {list(_ENGINES.keys())}")
    return eng

def list_engines() -> list[str]:
    if not _ENGINES:
        _init_engines()
    return list(_ENGINES.keys())
