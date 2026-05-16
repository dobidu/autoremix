"""
AutoRemix Python Sidecar
Serves stem separation and remix endpoints for the JUCE plugin.
Default port: 17432
"""

import os
import re
import logging
import tempfile
from pathlib import Path
import librosa
from fastapi import FastAPI, HTTPException
from .models import (
    SeparateRequest, SeparateResponse, StemPaths as StemPathsModel,
    RemixRequest, RemixResponse, HealthResponse,
    PresetSummary, CreatePresetRequest,
    RemixPreset, PresetParams, StemMix,
)
from .registry import get_separator, get_engine, list_separators, list_engines
from .presets.loader import PresetLoader

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

app = FastAPI(title="AutoRemix Sidecar", version="0.4.0")

TEMP_DIR = Path(os.environ.get("AUTOREMIX_TEMP_DIR",
    str(Path(tempfile.gettempdir()) / "autoremix")))
_presets = PresetLoader().load_all()

_CHOP_MODE_OPS: dict[str, dict] = {
    "beat":       {"op": "chop_beats",    "stems": "vocals", "params": {"division": 1.0, "repeat": 2}},
    "onset":      {"op": "chop_onsets",   "stems": "vocals", "params": {"min_gap_ms": 80, "threshold": 0.3, "repeat": 2}},
    "bar":        {"op": "chop_bars",     "stems": "vocals", "params": {"beats_per_bar": 4, "repeat": 2}},
    "energy":     {"op": "gate_energy",   "stems": "other",  "params": {"threshold_db": -20.0, "hold_ms": 50.0}},
    "structural": {"op": "structural_cut","stems": "vocals",  "params": {"n_segments": 4, "mode": "reverse"}},
}


@app.get("/api/v1/analyze")
async def analyze(path: str):
    import soundfile as sf
    from .remix.base import detect_bpm
    from .remix.analysis import detect_key
    file_path = Path(path)
    if not file_path.exists():
        raise HTTPException(status_code=400, detail=f"File not found: {path}")
    try:
        duration_sec = float(sf.info(str(file_path)).duration)
        y, sr = librosa.load(str(file_path), sr=None, mono=True, duration=60.0)
        bpm = detect_bpm(y, sr)
        key = detect_key(y, sr)
        return {"bpm": round(float(bpm), 2), "key": key, "duration_sec": round(duration_sec, 3)}
    except Exception as e:
        raise HTTPException(status_code=400, detail=f"Analysis failed: {e}")


@app.get("/api/v1/health", response_model=HealthResponse)
async def health():
    return HealthResponse(
        status="ok",
        available_separators=list_separators(),
        available_engines=list_engines(),
    )


@app.get("/api/v1/presets", response_model=list[PresetSummary])
async def list_presets():
    return [
        PresetSummary(id=p.id, name=p.name, engine=p.engine,
                      params=p.params, stem_mix=p.stem_mix,
                      effects=p.effects)
        for p in _presets.values()
    ]


@app.post("/api/v1/presets")
async def create_preset(req: CreatePresetRequest):
    preset_id = re.sub(r"[^a-z0-9]+", "_", req.name.lower().strip()).strip("_")
    preset = RemixPreset(
        id=preset_id,
        version="1.0",
        name=req.name,
        engine=req.engine_id,
        params=PresetParams(
            tempo_factor=req.tempo_factor,
            pitch_shift_semi=req.pitch_shift_semi,
            reverb_mix=req.reverb_mix,
            chop_interval_ms=req.chop_interval_ms,
            bass_boost_db=req.bass_boost_db,
            drums_tempo_factor=req.drums_tempo_factor,
        ),
        stem_mix=StemMix(
            vocals=req.vocals_gain,
            drums=req.drums_gain,
            bass=req.bass_gain,
            other=req.other_gain,
        ),
    )
    user_dir = PresetLoader.USER_DIR
    user_dir.mkdir(parents=True, exist_ok=True)
    (user_dir / f"{preset_id}.json").write_text(preset.model_dump_json(indent=2))
    _presets[preset_id] = preset
    return {"success": True, "id": preset_id}


@app.post("/api/v1/separate", response_model=SeparateResponse)
async def separate(req: SeparateRequest):
    try:
        input_path = Path(req.input_path)
        if not input_path.exists():
            raise HTTPException(status_code=400, detail=f"File not found: {req.input_path}")

        output_dir = TEMP_DIR / "stems" / input_path.stem
        separator = get_separator(req.separator_id)

        if not separator.is_available():
            raise HTTPException(status_code=503, detail=f"Separator {req.separator_id} not available")

        logger.info(f"Separating {input_path} with {req.separator_id}")
        stems: StemPaths = separator.separate(input_path, output_dir)

        return SeparateResponse(success=True, stems=StemPathsModel.from_domain(stems))
    except HTTPException:
        raise
    except KeyError as e:
        raise HTTPException(status_code=400, detail=str(e))
    except Exception as e:
        logger.exception("Separation failed")
        return SeparateResponse(success=False, error=str(e))


def _apply_stem_weights(stems, weights: dict, out_dir: Path):
    """Scale each stem WAV by its weight factor; write to out_dir. Return new StemPaths."""
    import soundfile as sf
    import numpy as np
    from .separators.base import StemPaths as _StemPaths
    out_dir.mkdir(parents=True, exist_ok=True)
    result = {}
    for name in ("vocals", "drums", "bass", "other"):
        path = getattr(stems, name)
        audio, sr = sf.read(str(path), dtype="float32", always_2d=True)
        factor = float(weights.get(name, 1.0))
        out_path = out_dir / f"{name}.wav"
        sf.write(str(out_path), audio * factor, sr)
        result[name] = out_path
    return _StemPaths(
        vocals=result["vocals"], drums=result["drums"],
        bass=result["bass"],    other=result["other"],
    )


@app.post("/api/v1/remix", response_model=RemixResponse)
async def remix(req: RemixRequest):
    try:
        stems = req.to_stems()

        if req.stem_mix_override:
            weighted_dir = TEMP_DIR / "weighted" / Path(req.vocals_path).stem
            stems = _apply_stem_weights(stems, req.stem_mix_override, weighted_dir)

        output_path = Path(req.output_path)

        preset = _presets.get(req.engine_id)
        chop_op = _CHOP_MODE_OPS.get(req.chop_mode) if req.chop_mode != "fixed" else None

        if preset and preset.effects:
            active_preset = preset
            if chop_op:
                import copy
                active_preset = copy.copy(preset)
                active_preset.effects = list(preset.effects) + [chop_op]
            from .remix.chain_interpreter import EffectChainEngine
            logger.info(f"Remixing via effect chain {req.engine_id} (chop_mode={req.chop_mode}) → {output_path}")
            result = EffectChainEngine().process(stems, active_preset, output_path)
        else:
            engine = get_engine(req.engine_id)
            params = req.to_params()
            logger.info(f"Remixing with engine {req.engine_id} → {output_path}")
            result = engine.process(stems, params, output_path)

        return RemixResponse(success=True, output_path=str(result))
    except HTTPException:
        raise
    except KeyError as e:
        raise HTTPException(status_code=400, detail=str(e))
    except Exception as e:
        logger.exception("Remix failed")
        return RemixResponse(success=False, error=str(e))


if __name__ == "__main__":
    import uvicorn
    port = int(os.environ.get("AUTOREMIX_PORT", "17432"))
    uvicorn.run(app, host="127.0.0.1", port=port, log_level="info")
