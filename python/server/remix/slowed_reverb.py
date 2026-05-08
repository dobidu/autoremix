from pathlib import Path
import numpy as np
import soundfile as sf
import librosa
import pyrubberband as pyrb
from .base import IRemixEngine, RemixParams
from ..separators.base import StemPaths

class SlowedReverbEngine(IRemixEngine):
    """
    Slowed + Reverb: dreamy aesthetic, popularized on YouTube/TikTok.
    - Slight slow (0.75x)
    - Pitch down (-2 semi)
    - Heavy reverb (room_size 0.85, wet 0.6)
    """

    @property
    def engine_id(self) -> str:
        return "slowed_reverb"

    @property
    def display_name(self) -> str:
        return "Slowed + Reverb"

    def get_default_params(self) -> RemixParams:
        return RemixParams(
            tempo_factor=0.75,
            pitch_shift_semi=-2.0,
            reverb_mix=0.60,
        )

    def process(self, stems: StemPaths, params: RemixParams, output_path: Path) -> Path:
        output_path.parent.mkdir(parents=True, exist_ok=True)

        arrays, sr = [], None
        for path in [stems.vocals, stems.drums, stems.bass, stems.other]:
            a, s = librosa.load(str(path), sr=None, mono=False)
            if a.ndim == 1: a = np.stack([a, a])
            arrays.append(a)
            sr = s

        min_len = min(a.shape[1] for a in arrays)
        audio = sum(a[:, :min_len] for a in arrays) / len(arrays)

        audio = pyrb.time_stretch(audio, sr, params.tempo_factor)
        audio = pyrb.pitch_shift(audio, sr, params.pitch_shift_semi)

        try:
            from pedalboard import Pedalboard, Reverb
            pb = Pedalboard([Reverb(room_size=0.85, wet_level=params.reverb_mix, dry_level=0.4)])
            audio = pb(audio.astype(np.float32), sr)
        except ImportError:
            pass

        sf.write(str(output_path), audio.T, sr)
        return output_path
