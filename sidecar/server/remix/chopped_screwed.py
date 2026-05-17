from pathlib import Path

import numpy as np
import soundfile as sf
import librosa

from .base import (IRemixEngine, RemixParams,
                   time_stretch, pitch_shift,
                   detect_bpm, beat_aligned_ms,
                   apply_highpass, normalize_lufs)
from ..separators.base import StemPaths


class ChoppedAndScrewedEngine(IRemixEngine):
    """
    Houston-style Chopped & Screwed:
      1. Detect source BPM → use it to align chop to beat grid
      2. Slow down (tempo_factor × source BPM)
      3. Pitch shift down
      4. High-pass at 60 Hz — pitch-down thickens the sub; trim the mud
      5. Periodic beat-aligned stutter chop
      6. Light reverb tail
      7. LUFS normalise to -14 LUFS
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
            reverb_mix=0.10,
            chop_interval_ms=500.0,   # snapped to nearest beat at runtime
        )

    def process(self, stems: StemPaths, params: RemixParams,
                output_path: Path) -> Path:
        output_path.parent.mkdir(parents=True, exist_ok=True)

        audio, sr = self._mix_stems(stems)

        # ── 1. Detect BPM on the dry mix before any processing
        source_bpm = detect_bpm(audio, sr)

        # ── 2. Time-stretch
        audio = time_stretch(audio, params.tempo_factor)

        # ── 3. Pitch shift down
        audio = pitch_shift(audio, sr, params.pitch_shift_semi)

        # ── 4. High-pass: pitch-down accumulates sub-mud below ~80 Hz
        if params.pitch_shift_semi < -1.0:
            cutoff = 60.0 + abs(params.pitch_shift_semi) * 4.0   # scale with depth
            audio = apply_highpass(audio, sr, cutoff_hz=cutoff)

        # ── 5. Beat-aligned chop
        if params.chop_interval_ms > 0:
            # Snap the user's ms value to the nearest beat (post-slowdown BPM)
            slowed_bpm = source_bpm * params.tempo_factor
            aligned_ms = beat_aligned_ms(params.chop_interval_ms, slowed_bpm)
            audio = self._chop(audio, sr, aligned_ms, stutter_every=4)

        # ── 6. Light reverb
        if params.reverb_mix > 0:
            audio = self._apply_reverb(audio, sr, params.reverb_mix)

        # ── 7. LUFS normalise
        audio = normalize_lufs(audio, sr, target_lufs=-14.0)

        sf.write(str(output_path), audio.T if audio.ndim > 1 else audio, sr)
        return output_path

    # ------------------------------------------------------------------

    def _mix_stems(self, stems: StemPaths):
        arrays, sr = [], None
        for path in [stems.vocals, stems.drums, stems.bass, stems.other]:
            a, s = librosa.load(str(path), sr=None, mono=False)
            if a.ndim == 1:
                a = np.stack([a, a])
            arrays.append(a)
            sr = s
        min_len = min(a.shape[1] for a in arrays)
        mixed = sum(a[:, :min_len] for a in arrays) / len(arrays)
        return mixed.astype(np.float32), sr

    def _chop(self, audio: np.ndarray, sr: int, interval_ms: float,
              stutter_every: int = 4) -> np.ndarray:
        """Every stutter_every chunks, repeat that chunk once (DJ spin-back feel)."""
        chunk_samples = int(sr * interval_ms / 1000)
        if chunk_samples < 1:
            return audio
        length = audio.shape[-1]
        chunks, n, i = [], 0, 0
        while i < length:
            end = min(i + chunk_samples, length)
            chunk = audio[..., i:end]
            chunks.append(chunk)
            if (n + 1) % stutter_every == 0:
                chunks.append(chunk)   # stutter repeat
            i, n = end, n + 1
        out = np.concatenate(chunks, axis=-1)
        return out[..., :length]

    def _apply_reverb(self, audio: np.ndarray, sr: int, mix: float) -> np.ndarray:
        try:
            from pedalboard import Pedalboard, Reverb
            pb = Pedalboard([Reverb(room_size=0.55, wet_level=mix,
                                    dry_level=1.0 - mix)])
            return pb(audio.astype(np.float32), sr)
        except ImportError:
            return audio
