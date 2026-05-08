from pathlib import Path
import numpy as np
import soundfile as sf
import librosa
import pyrubberband as pyrb
from .base import IRemixEngine, RemixParams
from ..separators.base import StemPaths

class ChoppedAndScrewedEngine(IRemixEngine):
    """
    Classic Houston Chopped & Screwed:
    - All stems slowed (tempo_factor ≈ 0.70)
    - Pitch shifted down (pitch_shift_semi ≈ -4.0)
    - Periodic chops (silence + repeat of previous chunk)
    """

    @property
    def engine_id(self) -> str:
        return "chopped_screwed"

    @property
    def display_name(self) -> str:
        return "Chopped & Screwed"

    def get_default_params(self) -> RemixParams:
        return RemixParams(
            tempo_factor=0.70,
            pitch_shift_semi=-4.0,
            reverb_mix=0.05,
            chop_interval_ms=2000.0,
        )

    def process(self, stems: StemPaths, params: RemixParams, output_path: Path) -> Path:
        output_path.parent.mkdir(parents=True, exist_ok=True)

        # Load and mix all stems
        mixed = self._mix_stems(stems)
        audio, sr = mixed

        # 1. Time-stretch (slow down)
        audio = pyrb.time_stretch(audio, sr, params.tempo_factor)

        # 2. Pitch shift
        audio = pyrb.pitch_shift(audio, sr, params.pitch_shift_semi)

        # 3. Chop if requested
        if params.chop_interval_ms > 0:
            audio = self._chop(audio, sr, params.chop_interval_ms)

        # 4. Light reverb via pedalboard
        if params.reverb_mix > 0:
            audio = self._apply_reverb(audio, sr, params.reverb_mix)

        sf.write(str(output_path), audio.T if audio.ndim > 1 else audio, sr)
        return output_path

    def _mix_stems(self, stems: StemPaths):
        arrays = []
        sr = None
        for path in [stems.vocals, stems.drums, stems.bass, stems.other]:
            a, s = librosa.load(str(path), sr=None, mono=False)
            if a.ndim == 1:
                a = np.stack([a, a])
            arrays.append(a)
            sr = s
        min_len = min(a.shape[1] for a in arrays)
        mixed = sum(a[:, :min_len] for a in arrays) / len(arrays)
        return mixed, sr

    def _chop(self, audio: np.ndarray, sr: int, interval_ms: float) -> np.ndarray:
        interval_samples = int(sr * interval_ms / 1000)
        result = np.zeros_like(audio)
        i = 0
        prev_chunk = None
        while i < audio.shape[-1]:
            chunk_end = min(i + interval_samples, audio.shape[-1])
            chunk = audio[..., i:chunk_end]
            if prev_chunk is not None and i % (2 * interval_samples) >= interval_samples:
                # Repeat previous chunk (the "chop")
                chunk_len = chunk_end - i
                result[..., i:chunk_end] = prev_chunk[..., :chunk_len]
            else:
                result[..., i:chunk_end] = chunk
                prev_chunk = chunk
            i = chunk_end
        return result

    def _apply_reverb(self, audio: np.ndarray, sr: int, mix: float) -> np.ndarray:
        try:
            from pedalboard import Pedalboard, Reverb
            pb = Pedalboard([Reverb(room_size=0.6, wet_level=mix)])
            return pb(audio.astype(np.float32), sr)
        except ImportError:
            return audio  # fallback: no reverb
