from pathlib import Path
import numpy as np
import soundfile as sf
import librosa
from scipy.signal import butter, sosfilt
from .base import IRemixEngine, RemixParams, time_stretch, pitch_shift
from ..separators.base import StemPaths

class DrumAndBassEngine(IRemixEngine):
    """
    Drum and Bass remix:
    - Drums: doubled tempo (2x), tight
    - Bass: preserved original tempo but boosted (+6 dB, low-shelf)
    - Vocals/other: slightly pitched up (+2 semi), tempo matched to drum grid
    """

    @property
    def engine_id(self) -> str:
        return "drum_and_bass"

    @property
    def display_name(self) -> str:
        return "Drum and Bass"

    def get_default_params(self) -> RemixParams:
        return RemixParams(
            tempo_factor=1.4,
            pitch_shift_semi=2.0,
            bass_boost_db=6.0,
            drums_tempo_factor=2.0,
        )

    def process(self, stems: StemPaths, params: RemixParams, output_path: Path) -> Path:
        output_path.parent.mkdir(parents=True, exist_ok=True)
        sr = None

        def load(path):
            nonlocal sr
            a, s = librosa.load(str(path), sr=None, mono=False)
            sr = s
            return np.stack([a, a]) if a.ndim == 1 else a

        vocals = load(stems.vocals)
        drums  = load(stems.drums)
        bass   = load(stems.bass)
        other  = load(stems.other)

        # Process drums: double tempo
        drums = time_stretch(drums, params.drums_tempo_factor)

        # Process bass: boost + keep tempo
        bass = self._bass_boost(bass, sr, params.bass_boost_db)

        # Vocals + other: pitch up, tempo adjust
        vocals = pitch_shift(time_stretch(vocals, params.tempo_factor), sr, params.pitch_shift_semi)
        other  = time_stretch(other, params.tempo_factor)

        # Align lengths
        min_len = min(arr.shape[1] for arr in [vocals, drums, bass, other])
        audio = (drums[:, :min_len] * 1.2 +
                 bass[:, :min_len]   * 1.0 +
                 vocals[:, :min_len] * 0.8 +
                 other[:, :min_len]  * 0.6) / 3.6

        # Normalize
        peak = np.max(np.abs(audio))
        if peak > 0: audio = audio / peak * 0.9

        sf.write(str(output_path), audio.T, sr)
        return output_path

    def _bass_boost(self, audio: np.ndarray, sr: int, boost_db: float) -> np.ndarray:
        gain = 10 ** (boost_db / 20)
        nyq = sr / 2
        sos = butter(2, 250 / nyq, btype="low", output="sos")
        boosted = sosfilt(sos, audio) * gain
        sos_hp = butter(2, 250 / nyq, btype="high", output="sos")
        rest = sosfilt(sos_hp, audio)
        return boosted + rest
