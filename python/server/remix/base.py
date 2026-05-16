from abc import ABC, abstractmethod
from dataclasses import dataclass
from pathlib import Path

import numpy as np
import librosa
from scipy.signal import butter, sosfilt

from ..separators.base import StemPaths


# ---------------------------------------------------------------------------
# Loudness / normalization
# ---------------------------------------------------------------------------

def normalize_peak(audio: np.ndarray, target_db: float = -1.0) -> np.ndarray:
    peak = np.max(np.abs(audio))
    if peak < 1e-6:
        return audio
    return audio * (10 ** (target_db / 20.0) / peak)


def normalize_lufs(audio: np.ndarray, sr: int,
                   target_lufs: float = -14.0) -> np.ndarray:
    """EBU R128 integrated-loudness normalization. Falls back to peak if
    pyloudnorm is absent or audio is too short to measure."""
    try:
        import pyloudnorm as pyln
        # pyloudnorm expects (samples, channels) float64
        data = (audio.T if audio.ndim == 2 else audio[:, None]).astype(np.float64)
        meter = pyln.Meter(sr)
        loudness = meter.integrated_loudness(data)
        if not np.isfinite(loudness):
            return normalize_peak(audio, target_db=-1.0)
        normalized = pyln.normalize.loudness(data, loudness, target_lufs)
        result = normalized.T if audio.ndim == 2 else normalized[:, 0]
        result = result.astype(np.float32)
        # Safety: pull back if LUFS normalization pushed peaks past ±1
        peak = np.max(np.abs(result))
        if peak > 0.99:
            result = result * (0.99 / peak)
        return result
    except Exception:
        return normalize_peak(audio, target_db=-1.0)


# ---------------------------------------------------------------------------
# BPM / musical timing
# ---------------------------------------------------------------------------

def detect_bpm(audio: np.ndarray, sr: int) -> float:
    """Estimate tempo from audio. Returns BPM (float)."""
    mono = audio.mean(axis=0).astype(np.float32) if audio.ndim == 2 else audio.astype(np.float32)
    tempo, _ = librosa.beat.beat_track(y=mono, sr=sr)
    return max(float(np.asarray(tempo).item()), 60.0)


def beat_aligned_ms(interval_ms: float, bpm: float, min_beats: int = 1) -> float:
    """Snap interval_ms to the nearest whole-beat multiple at the given BPM."""
    beat_ms = 60_000.0 / bpm
    n_beats = max(min_beats, round(interval_ms / beat_ms))
    return n_beats * beat_ms


# ---------------------------------------------------------------------------
# Spectral shaping
# ---------------------------------------------------------------------------

def apply_highpass(audio: np.ndarray, sr: int,
                   cutoff_hz: float = 80.0, order: int = 2) -> np.ndarray:
    """Remove sub-bass rumble — useful after pitch-down to clear mud."""
    nyq = sr / 2.0
    sos = butter(order, min(cutoff_hz / nyq, 0.99), btype='high', output='sos')
    return sosfilt(sos, audio).astype(np.float32)


def apply_high_shelf(audio: np.ndarray, sr: int,
                     gain_db: float = 2.0, freq_hz: float = 5000.0) -> np.ndarray:
    """Restore presence/air lost during time-stretch. Additive high-shelf."""
    if abs(gain_db) < 0.1:
        return audio
    nyq = sr / 2.0
    sos = butter(1, min(freq_hz / nyq, 0.99), btype='high', output='sos')
    hp = sosfilt(sos, audio)
    return (audio + (10 ** (gain_db / 20.0) - 1.0) * hp).astype(np.float32)


def apply_lowpass(audio: np.ndarray, sr: int,
                  cutoff_hz: float = 18000.0, order: int = 2) -> np.ndarray:
    nyq = sr / 2.0
    sos = butter(order, min(cutoff_hz / nyq, 0.99), btype='low', output='sos')
    return sosfilt(sos, audio).astype(np.float32)


# ---------------------------------------------------------------------------
# Time / pitch helpers
# ---------------------------------------------------------------------------

def time_stretch(audio: np.ndarray, rate: float) -> np.ndarray:
    """Stereo-safe librosa time stretch. rate < 1 = slower."""
    if audio.ndim == 1:
        return librosa.effects.time_stretch(audio, rate=rate)
    return np.stack([librosa.effects.time_stretch(ch, rate=rate) for ch in audio])


def pitch_shift(audio: np.ndarray, sr: int, n_steps: float) -> np.ndarray:
    """Stereo-safe librosa pitch shift."""
    if audio.ndim == 1:
        return librosa.effects.pitch_shift(audio, sr=sr, n_steps=n_steps)
    return np.stack([librosa.effects.pitch_shift(ch, sr=sr, n_steps=n_steps) for ch in audio])


# ---------------------------------------------------------------------------
# Data model & interface
# ---------------------------------------------------------------------------

@dataclass
class RemixParams:
    tempo_factor: float = 1.0
    pitch_shift_semi: float = 0.0
    reverb_mix: float = 0.0
    chop_interval_ms: float = 0.0
    bass_boost_db: float = 0.0
    drums_tempo_factor: float = 1.0


class IRemixEngine(ABC):

    @property
    @abstractmethod
    def engine_id(self) -> str: ...

    @property
    @abstractmethod
    def display_name(self) -> str: ...

    @abstractmethod
    def get_default_params(self) -> RemixParams: ...

    @abstractmethod
    def process(self, stems: StemPaths, params: RemixParams,
                output_path: Path) -> Path: ...
