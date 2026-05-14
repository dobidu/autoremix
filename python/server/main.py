"""
AutoRemix Python Sidecar
Serves stem separation and remix endpoints for the JUCE plugin.
Default port: 17432
"""

import os
import logging
from pathlib import Path
from fastapi import FastAPI, HTTPException
from .models import (
    SeparateRequest, SeparateResponse, StemPaths as StemPathsModel,
    RemixRequest, RemixResponse, HealthResponse,
    PresetSummary,
)
from .registry import get_separator, get_engine, list_separators, list_engines
from .presets.loader import PresetLoader

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

app = FastAPI(title="AutoRemix Sidecar", version="0.2.0")

TEMP_DIR = Path(os.environ.get("AUTOREMIX_TEMP_DIR", "/tmp/autoremix"))
_presets = PresetLoader().load_all()


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
        PresetSummary(id=p.id, name=p.name, params=p.params, stem_mix=p.stem_mix)
        for p in _presets.values()
    ]


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


@app.post("/api/v1/remix", response_model=RemixResponse)
async def remix(req: RemixRequest):
    try:
        stems = req.to_stems()
        output_path = Path(req.output_path)
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
