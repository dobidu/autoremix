import numpy as np
from .base import time_stretch, pitch_shift
from .analysis import detect_beats, detect_onsets, detect_bars, detect_energy_gates, detect_structure


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


def _chop_at_boundaries(audio: np.ndarray, boundaries: np.ndarray, repeat: int) -> np.ndarray:
    if len(boundaries) == 0:
        return audio
    length = audio.shape[-1]
    edges = np.unique(np.concatenate([[0], boundaries, [length]]))
    edges = edges[(edges >= 0) & (edges <= length)]
    result_chunks = []
    for i in range(len(edges) - 1):
        chunk = audio[..., edges[i]:edges[i + 1]]
        if chunk.shape[-1] == 0:
            continue
        for _ in range(max(1, repeat)):
            result_chunks.append(chunk)
    if not result_chunks:
        return audio
    out = np.concatenate(result_chunks, axis=-1)
    if out.shape[-1] >= length:
        return out[..., :length]
    pad_shape = list(audio.shape)
    pad_shape[-1] = length - out.shape[-1]
    return np.concatenate([out, np.zeros(pad_shape, dtype=audio.dtype)], axis=-1)


def apply_chop_beats(audio: np.ndarray, sr: int, params: dict) -> np.ndarray:
    repeat   = int(params.get("repeat", 2))
    division = float(params.get("division", 1.0))
    offset   = int(params.get("offset_beats", 0))

    beat_times = detect_beats(audio, sr)
    if division != 1.0 and division > 0:
        intervals = np.diff(beat_times)
        steps = max(1, int(round(1.0 / division)))
        extra = []
        for i, interval in enumerate(intervals):
            for s in range(1, steps):
                extra.append(beat_times[i] + s * interval / steps)
        beat_times = np.sort(np.concatenate([beat_times, extra]))

    if offset < len(beat_times):
        beat_times = beat_times[offset:]

    boundaries = np.clip((beat_times * sr).astype(int), 0, audio.shape[-1])
    return _chop_at_boundaries(audio, boundaries, repeat)


def apply_chop_onsets(audio: np.ndarray, sr: int, params: dict) -> np.ndarray:
    repeat     = int(params.get("repeat", 2))
    min_gap_ms = float(params.get("min_gap_ms", 80.0))
    threshold  = float(params.get("threshold", 0.3))

    onset_times = detect_onsets(audio, sr, min_gap_ms=min_gap_ms, threshold=threshold)
    boundaries  = np.clip((onset_times * sr).astype(int), 0, audio.shape[-1])
    return _chop_at_boundaries(audio, boundaries, repeat)


def apply_chop_bars(audio: np.ndarray, sr: int, params: dict) -> np.ndarray:
    beats_per_bar = int(params.get("beats_per_bar", 4))
    repeat        = int(params.get("repeat", 2))

    bar_times  = detect_bars(audio, sr, beats_per_bar=beats_per_bar)
    boundaries = np.clip((bar_times * sr).astype(int), 0, audio.shape[-1])
    return _chop_at_boundaries(audio, boundaries, repeat)


def apply_gate_energy(audio: np.ndarray, sr: int, params: dict) -> np.ndarray:
    threshold_db = float(params.get("threshold_db", -20.0))
    hold_ms      = float(params.get("hold_ms", 50.0))

    mask   = detect_energy_gates(audio, sr, threshold_db=threshold_db, hold_ms=hold_ms)
    result = audio.copy()
    result[..., ~mask] = 0.0
    return result


def apply_structural_cut(audio: np.ndarray, sr: int, params: dict) -> np.ndarray:
    n_segments = int(params.get("n_segments", 8))
    mode       = str(params.get("mode", "repeat_first"))

    segments = detect_structure(audio, sr, n_segments=n_segments)
    chunks = []
    for start_s, end_s in segments:
        s_idx = int(start_s * sr)
        e_idx = min(int(end_s * sr), audio.shape[-1])
        chunk = audio[..., s_idx:e_idx]
        if chunk.shape[-1] > 0:
            chunks.append(chunk)

    if not chunks:
        return audio

    if mode == "reverse":
        chunks = chunks[::-1]
    elif mode == "shuffle":
        rng = np.random.default_rng(seed=42)
        idx = rng.permutation(len(chunks))
        chunks = [chunks[i] for i in idx]
    elif mode == "repeat_first":
        first = chunks[0]
        interleaved = [first]
        for c in chunks[1:]:
            interleaved.append(c)
            interleaved.append(first)
        chunks = interleaved

    return np.concatenate(chunks, axis=-1)


OP_REGISTRY = {
    "time_stretch":   apply_time_stretch,
    "pitch_shift":    apply_pitch_shift,
    "reverb":         apply_reverb,
    "chop":           apply_chop,
    "bass_boost":     apply_bass_boost,
    "eq_highpass":    apply_eq_highpass,
    "chop_beats":     apply_chop_beats,
    "chop_onsets":    apply_chop_onsets,
    "chop_bars":      apply_chop_bars,
    "gate_energy":    apply_gate_energy,
    "structural_cut": apply_structural_cut,
}
