from pathlib import Path

import numpy as np
import soundfile as sf
import librosa

from .base import (IRemixEngine, RemixParams,
                   time_stretch, pitch_shift,
                   apply_high_shelf, normalize_lufs)
from ..separators.base import StemPaths


class SlowedReverbEngine(IRemixEngine):
    """
    Slowed + Reverb — dreamy/lo-fi aesthetic:
      1. Slow (tempo_factor ≈ 0.80)
      2. Pitch shift down slightly
      3. Presence boost: time-stretch dulls the top end, +2 dB at 5 kHz restores air
      4. Reverb wash — room_size 0.75, wet controlled by reverb_mix
      5. LUFS normalise to -14 LUFS
    """

    @property
    def engine_id(self) -> str:
        return "slowed_reverb"

    @property
    def display_name(self) -> str:
        return "Slowed + Reverb"

    def get_default_params(self) -> RemixParams:
        return RemixParams(
            tempo_factor=0.80,
            pitch_shift_semi=-2.0,
            reverb_mix=0.40,
        )

    def process(self, stems: StemPaths, params: RemixParams,
                output_path: Path) -> Path:
        output_path.parent.mkdir(parents=True, exist_ok=True)

        arrays, sr = [], None
        for path in [stems.vocals, stems.drums, stems.bass, stems.other]:
            a, s = librosa.load(str(path), sr=None, mono=False)
            if a.ndim == 1:
                a = np.stack([a, a])
            arrays.append(a)
            sr = s

        min_len = min(a.shape[1] for a in arrays)
        audio = (sum(a[:, :min_len] for a in arrays) / len(arrays)).astype(np.float32)

        # ── 1 & 2. Stretch + pitch
        audio = time_stretch(audio, params.tempo_factor)
        audio = pitch_shift(audio, sr, params.pitch_shift_semi)

        # ── 3. Restore presence lost to time-stretch (scale with slowdown depth)
        #       At 0.80× the top-end loses ~1.5 dB of perceived brightness
        presence_db = max(0.0, (1.0 - params.tempo_factor) * 8.0)   # 0→0 dB, 0.5→4 dB
        if presence_db > 0.1:
            audio = apply_high_shelf(audio, sr, gain_db=presence_db, freq_hz=5000.0)

        # ── 4. Reverb — headroom before reverb, then wet/dry sum ≤ 1
        try:
            from pedalboard import Pedalboard, Reverb
            dry = 1.0 - params.reverb_mix
            pb = Pedalboard([Reverb(room_size=0.75,
                                    wet_level=params.reverb_mix,
                                    dry_level=dry,
                                    damping=0.5)])
            audio = pb(audio.astype(np.float32), sr)
        except ImportError:
            pass

        # ── 5. LUFS normalise
        audio = normalize_lufs(audio, sr, target_lufs=-14.0)

        sf.write(str(output_path), audio.T, sr)
        return output_path
