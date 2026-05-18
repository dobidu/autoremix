"""MashupEngine — combine two tracks via per-stem gain pairs.

Track A is the anchor (target BPM and target key default to A's analysis).
Track B is time-stretched and pitch-shifted to match A's tempo + key.

Output = sum over all 8 stems (4 from A + 4 from B), each multiplied by its
user-specified gain (0.0 = silent, 1.0 = unity, 2.0 = +6dB), then LUFS-normalized.
"""

from pathlib import Path
from typing import Optional

import librosa
import numpy as np
import soundfile as sf

from ..separators.base import IStemSeparator, StemPaths
from .analysis import detect_key, semitone_delta
from .base import detect_bpm, normalize_lufs, pitch_shift, time_stretch


_STEM_NAMES = ("vocals", "drums", "bass", "other")


def _apply_highpass(audio: np.ndarray, sr: int, cutoff_hz: float) -> np.ndarray:
    """Pedalboard high-pass on a (channels, samples) array."""
    from pedalboard import HighpassFilter, Pedalboard
    pb = Pedalboard([HighpassFilter(cutoff_frequency_hz=cutoff_hz)])
    # pedalboard expects (samples, channels) float32
    return pb(audio.astype(np.float32).T, sr).T


def _apply_master_reverb(audio: np.ndarray, sr: int,
                         mix: float, room: float) -> np.ndarray:
    """Pedalboard Reverb on a (channels, samples) array. Wet adds a tail."""
    from pedalboard import Pedalboard, Reverb
    pb = Pedalboard([Reverb(room_size=room, wet_level=mix)])
    return pb(audio.astype(np.float32).T, sr).T


class MashupEngine:
    def __init__(self, stretch_min: float = 0.5, stretch_max: float = 2.0):
        self.stretch_min = stretch_min
        self.stretch_max = stretch_max

    def process(
        self,
        file_a: Path,
        file_b: Path,
        separator: IStemSeparator,
        stem_gains_a: dict,
        stem_gains_b: dict,
        target_bpm: Optional[float],
        target_key: Optional[str],
        out_dir_a: Path,
        out_dir_b: Path,
        output_path: Path,
        bpm_modifier: float = 1.0,
        master_pitch_offset_semi: float = 0.0,
        master_reverb_mix: float = 0.0,
        master_reverb_room: float = 0.5,
        highpass_b_hz: float = 0.0,
    ) -> dict:
        stems_a = separator.separate(file_a, out_dir_a)
        stems_b = separator.separate(file_b, out_dir_b)

        sr = sf.info(str(stems_a.vocals)).samplerate
        sr_b = sf.info(str(stems_b.vocals)).samplerate
        if sr != sr_b:
            raise ValueError(
                f"Sample rate mismatch between separated stems: A={sr}, B={sr_b}"
            )

        y_a, sr_a_load = librosa.load(str(file_a), sr=None, mono=True, duration=60.0)
        y_b, sr_b_load = librosa.load(str(file_b), sr=None, mono=True, duration=60.0)
        bpm_a = detect_bpm(y_a, sr_a_load)
        bpm_b = detect_bpm(y_b, sr_b_load)
        key_a = detect_key(y_a, sr_a_load)
        key_b = detect_key(y_b, sr_b_load)

        bpm_target = float(target_bpm) if target_bpm else float(bpm_a)
        bpm_target *= float(bpm_modifier)
        key_target = target_key if target_key else key_a

        stretch = bpm_target / bpm_b
        stretch = max(self.stretch_min, min(self.stretch_max, stretch))
        # Track A also needs the bpm_modifier (since target may differ from A's bpm).
        stretch_a = bpm_target / float(bpm_a)
        stretch_a = max(self.stretch_min, min(self.stretch_max, stretch_a))
        delta_b = semitone_delta(key_b, key_target)

        all_processed: list[np.ndarray] = []
        for name in _STEM_NAMES:
            # Track A — stretched if bpm_modifier shifts tempo from A's native
            audio_a, _ = sf.read(str(getattr(stems_a, name)), dtype="float32", always_2d=True)
            audio_a = audio_a.T
            if abs(stretch_a - 1.0) > 1e-3:
                audio_a = time_stretch(audio_a, rate=stretch_a)
            gain_a = float(stem_gains_a.get(name, 1.0))
            if gain_a != 0.0:
                all_processed.append(audio_a * gain_a)

            # Track B — stretched + pitched to match A's target
            audio_b, _ = sf.read(str(getattr(stems_b, name)), dtype="float32", always_2d=True)
            audio_b = audio_b.T
            if highpass_b_hz > 0.0:
                audio_b = _apply_highpass(audio_b, sr, float(highpass_b_hz))
            if abs(stretch - 1.0) > 1e-3:
                audio_b = time_stretch(audio_b, rate=stretch)
            if delta_b != 0:
                audio_b = pitch_shift(audio_b, sr, n_steps=float(delta_b))
            gain_b = float(stem_gains_b.get(name, 1.0))
            if gain_b != 0.0:
                all_processed.append(audio_b * gain_b)

        if not all_processed:
            # All gains zero — write silence at min length
            min_len = 1
            mixed = np.zeros((2, min_len), dtype=np.float32)
        else:
            min_len = min(a.shape[-1] for a in all_processed)
            trimmed = [a[..., :min_len] for a in all_processed]
            mixed = np.stack(trimmed).sum(axis=0).astype(np.float32)

            # Master pitch offset (applied to the summed mix)
            if abs(master_pitch_offset_semi) > 1e-3:
                mixed = pitch_shift(mixed, sr, n_steps=float(master_pitch_offset_semi))

            # Master reverb (Pedalboard wet level)
            if master_reverb_mix > 0.0:
                mixed = _apply_master_reverb(mixed, sr,
                                             float(master_reverb_mix),
                                             float(master_reverb_room))
                min_len = mixed.shape[-1]

            mixed = normalize_lufs(mixed, sr)

        output_path.parent.mkdir(parents=True, exist_ok=True)
        sf.write(str(output_path), mixed.T, sr)

        return {
            "target_bpm": float(bpm_target),
            "target_key": key_target,
            "length_sec": float(min_len) / float(sr),
            "source_bpm_a": float(bpm_a),
            "source_bpm_b": float(bpm_b),
            "source_key_a": key_a,
            "source_key_b": key_b,
            "stretch_factor_b": float(stretch),
            "semitone_shift_b": int(delta_b),
        }
