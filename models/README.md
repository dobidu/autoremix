# models/

ONNX (and any future native ML) artifacts consumed by the plugin at
runtime. Files in this directory are produced by tools under `tools/`
and are git-ignored (too large to commit). Regenerate them with:

```
python tools/export_demucs_onnx.py --out models/htdemucs.onnx
```

See `tools/README.md` for setup details.
