from pathlib import Path
import numpy as np
import librosa
import soundfile as sf
from ..separators.base import StemPaths
from ..models import RemixPreset
from .ops import OP_REGISTRY

STEM_NAMES = ["vocals", "drums", "bass", "other"]


class EffectChainEngine:
    def process(self, stems: StemPaths, preset: RemixPreset, output_path: Path) -> Path:
        output_path.parent.mkdir(parents=True, exist_ok=True)

        stem_arrays: dict[str, np.ndarray] = {}
        sr = None
        for name in STEM_NAMES:
            path = getattr(stems, name)
            audio, s = librosa.load(str(path), sr=None, mono=False)
            if audio.ndim == 1:
                audio = np.stack([audio, audio])
            stem_arrays[name] = audio
            sr = s

        for effect in preset.effects:
            op_name = effect["op"]
            stems_spec = effect["stems"]
            params = effect["params"]

            op_fn = OP_REGISTRY.get(op_name)
            if op_fn is None:
                raise ValueError(f"Unknown effect op: {op_name!r}")

            if stems_spec == "all":
                mix = sum(stem_arrays[n] for n in STEM_NAMES)
                processed = op_fn(mix, sr, params)
                share = processed / len(STEM_NAMES)
                for name in STEM_NAMES:
                    min_len = min(share.shape[-1], stem_arrays[name].shape[-1])
                    stem_arrays[name] = share[..., :min_len]
            elif isinstance(stems_spec, str):
                stem_arrays[stems_spec] = op_fn(stem_arrays[stems_spec], sr, params)
            else:
                for name in stems_spec:
                    stem_arrays[name] = op_fn(stem_arrays[name], sr, params)

        mix_cfg = preset.stem_mix
        weights = {
            "vocals": mix_cfg.vocals,
            "drums":  mix_cfg.drums,
            "bass":   mix_cfg.bass,
            "other":  mix_cfg.other,
        }
        min_len = min(a.shape[-1] for a in stem_arrays.values())
        result = sum(
            stem_arrays[n][..., :min_len] * weights[n]
            for n in STEM_NAMES
        ) / len(STEM_NAMES)

        sf.write(str(output_path), result.T if result.ndim > 1 else result, sr)
        return output_path
