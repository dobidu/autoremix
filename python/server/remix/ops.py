import numpy as np
from .base import time_stretch, pitch_shift


def apply_time_stretch(audio: np.ndarray, sr: int, params: dict) -> np.ndarray:
    factor = float(params["factor"])
    return time_stretch(audio, rate=factor)


def apply_pitch_shift(audio: np.ndarray, sr: int, params: dict) -> np.ndarray:
    semitones = float(params["semitones"])
    return pitch_shift(audio, sr, n_steps=semitones)


def apply_reverb(audio: np.ndarray, sr: int, params: dict) -> np.ndarray:
    from pedalboard import Pedalboard, Reverb
    mix = float(params["mix"])
    room_size = float(params.get("room_size", 0.6))
    pb = Pedalboard([Reverb(room_size=room_size, wet_level=mix)])
    return pb(audio.astype(np.float32), sr)


def apply_chop(audio: np.ndarray, sr: int, params: dict) -> np.ndarray:
    interval_ms = float(params["interval_ms"])
    interval_samples = int(sr * interval_ms / 1000)
    if interval_samples == 0:
        return audio
    result = np.zeros_like(audio)
    length = audio.shape[-1]
    prev_chunk = None
    i = 0
    while i < length:
        chunk_end = min(i + interval_samples, length)
        chunk = audio[..., i:chunk_end]
        if prev_chunk is not None and i % (2 * interval_samples) >= interval_samples:
            chunk_len = chunk_end - i
            result[..., i:chunk_end] = prev_chunk[..., :chunk_len]
        else:
            result[..., i:chunk_end] = chunk
            prev_chunk = chunk
        i = chunk_end
    return result


def apply_bass_boost(audio: np.ndarray, sr: int, params: dict) -> np.ndarray:
    from pedalboard import Pedalboard, LowShelfFilter
    db = float(params["db"])
    pb = Pedalboard([LowShelfFilter(cutoff_frequency_hz=200.0, gain_db=db)])
    return pb(audio.astype(np.float32), sr)


def apply_eq_highpass(audio: np.ndarray, sr: int, params: dict) -> np.ndarray:
    from pedalboard import Pedalboard, HighpassFilter
    cutoff = float(params["cutoff_hz"])
    pb = Pedalboard([HighpassFilter(cutoff_frequency_hz=cutoff)])
    return pb(audio.astype(np.float32), sr)


OP_REGISTRY = {
    "time_stretch": apply_time_stretch,
    "pitch_shift":  apply_pitch_shift,
    "reverb":       apply_reverb,
    "chop":         apply_chop,
    "bass_boost":   apply_bass_boost,
    "eq_highpass":  apply_eq_highpass,
}
