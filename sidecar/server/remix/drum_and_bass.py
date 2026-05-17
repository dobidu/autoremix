from pathlib import Path

import numpy as np
import soundfile as sf
import librosa
from scipy.signal import butter, sosfilt

from .base import (IRemixEngine, RemixParams,
                   time_stretch, pitch_shift,
                   detect_bpm, normalize_lufs)
from ..separators.base import StemPaths

# DnB sits in 160–180 BPM. We target the midpoint.
_DnB_TARGET_BPM = 170.0


class DrumAndBassEngine(IRemixEngine):
    """
    Drum and Bass remix:
      1. Detect source BPM → compute drums_tempo_factor to hit ~170 BPM
         (user drums_tempo_factor acts as a ±multiplier on top, default 1.0)
      2. Drums: stretched to target DnB grid
      3. Bass: low-shelf boost, stays at source tempo (sub locks the groove)
      4. Vocals/other: pitched + tempo-matched to drums
      5. Weighted re-mix: drums loud, bass solid, vocals back, other back
      6. LUFS normalise to -14 LUFS
    """

    @property
    def engine_id(self) -> str:
        return "drum_and_bass"

    @property
    def display_name(self) -> str:
        return "Drum and Bass"

    def get_default_params(self) -> RemixParams:
        return RemixParams(
            tempo_factor=1.0,          # applied to vocals/other (1.0 = follow drums)
            pitch_shift_semi=2.0,
            bass_boost_db=6.0,
            drums_tempo_factor=1.0,    # now a scale on auto-detected ratio (1.0 = hit 170 BPM)
        )

    def process(self, stems: StemPaths, params: RemixParams,
                output_path: Path) -> Path:
        output_path.parent.mkdir(parents=True, exist_ok=True)
        sr = None

        def load(path):
            nonlocal sr
            a, s = librosa.load(str(path), sr=None, mono=False)
            sr = s
            return np.stack([a, a]) if a.ndim == 1 else a

        drums_raw = load(stems.drums)
        bass      = load(stems.bass)
        vocals    = load(stems.vocals)
        other     = load(stems.other)

        # ── 1. Detect source BPM from drums stem (cleaner signal than mix)
        source_bpm = detect_bpm(drums_raw, sr)
        auto_factor = _DnB_TARGET_BPM / source_bpm        # e.g. 170/90 ≈ 1.89
        drum_factor = auto_factor * params.drums_tempo_factor   # user can nudge

        # Vocals/other follow drums tempo; params.tempo_factor can add offset
        vocal_factor = drum_factor * params.tempo_factor if params.tempo_factor != 1.0 \
                       else drum_factor

        # ── 2. Stretch drums to DnB grid
        drums = time_stretch(drums_raw, drum_factor)

        # ── 3. Bass: boost low shelf, preserve original tempo (sub stays locked)
        bass = self._bass_boost(bass, sr, params.bass_boost_db)

        # ── 4. Vocals + other: pitch up, match drum tempo
        vocals = pitch_shift(time_stretch(vocals, vocal_factor), sr, params.pitch_shift_semi)
        other  = time_stretch(other, vocal_factor)

        # ── 5. Weighted mix — drums & bass prominent
        min_len = min(a.shape[1] for a in [drums, bass, vocals, other])
        audio = (drums[:, :min_len]  * 1.2 +
                 bass[:, :min_len]   * 1.0 +
                 vocals[:, :min_len] * 0.7 +
                 other[:, :min_len]  * 0.5) / 3.4

        # ── 6. LUFS normalise
        audio = normalize_lufs(audio.astype(np.float32), sr, target_lufs=-14.0)

        sf.write(str(output_path), audio.T, sr)
        return output_path

    def _bass_boost(self, audio: np.ndarray, sr: int, boost_db: float) -> np.ndarray:
        gain = 10 ** (boost_db / 20.0)
        nyq = sr / 2.0
        sos_lp = butter(2, 250.0 / nyq, btype='low',  output='sos')
        sos_hp = butter(2, 250.0 / nyq, btype='high', output='sos')
        return (sosfilt(sos_lp, audio) * gain + sosfilt(sos_hp, audio)).astype(np.float32)
