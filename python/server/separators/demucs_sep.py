from pathlib import Path
from .base import IStemSeparator, StemPaths

_model = None  # module-level cache — loaded once on first separate() call


def _get_model():
    global _model
    if _model is None:
        from demucs.pretrained import get_model
        _model = get_model("htdemucs")
        _model.eval()
    return _model


class DemucsSeparator(IStemSeparator):
    @property
    def separator_id(self) -> str:
        return "demucs"

    @property
    def display_name(self) -> str:
        return "Demucs (ML)"

    def is_available(self) -> bool:
        try:
            import demucs      # noqa: F401
            import torch       # noqa: F401
            import torchaudio  # noqa: F401
            return True
        except ImportError:
            return False

    def separate(self, input_path: Path, output_dir: Path) -> StemPaths:
        import torch
        import torchaudio
        import soundfile as sf
        import numpy as np
        from demucs.apply import apply_model

        model = _get_model()
        output_dir.mkdir(parents=True, exist_ok=True)

        # Use soundfile to avoid torchaudio backend (torchcodec) dependency
        audio_np, sr = sf.read(str(input_path), dtype="float32", always_2d=True)
        wav = torch.from_numpy(audio_np.T)  # [channels, samples]

        if sr != model.samplerate:
            wav = torchaudio.functional.resample(wav, sr, model.samplerate)

        if wav.shape[0] == 1:
            wav = wav.repeat(2, 1)

        with torch.no_grad():
            sources = apply_model(model, wav.unsqueeze(0))
        # sources: [batch=1, num_sources, channels, samples]

        stem_paths = {}
        for i, name in enumerate(model.sources):
            stem = sources[0, i].cpu().numpy().T  # [samples, channels]
            out = output_dir / f"{name}.wav"
            sf.write(str(out), stem, model.samplerate)
            stem_paths[name] = out

        return StemPaths(
            vocals=stem_paths["vocals"],
            drums=stem_paths["drums"],
            bass=stem_paths["bass"],
            other=stem_paths["other"],
        )
