import numpy as np
import librosa

_NOTE_NAMES = ['C', 'C#', 'D', 'Eb', 'E', 'F', 'F#', 'G', 'Ab', 'A', 'Bb', 'B']

# Krumhansl-Schmuckler key profiles
_MAJOR_PROFILE = np.array([6.35, 2.23, 3.48, 2.33, 4.38, 4.09,
                             2.52, 5.19, 2.39, 3.66, 2.29, 2.88])
_MINOR_PROFILE = np.array([6.33, 2.68, 3.52, 5.38, 2.60, 3.53,
                             2.54, 4.75, 3.98, 2.69, 3.34, 3.17])


def detect_key(audio: np.ndarray, sr: int) -> str:
    """Detect dominant musical key using Krumhansl chroma profiles.
    Returns note name + "" for major or "m" for minor, e.g. "C", "Am", "F#"."""
    mono = _to_mono(audio).astype(np.float32)
    chroma = librosa.feature.chroma_cqt(y=mono, sr=sr)
    chroma_mean = chroma.mean(axis=1)  # shape (12,)

    best_corr = -np.inf
    best_key = "C"
    for root in range(12):
        shifted_major = np.roll(_MAJOR_PROFILE, root)
        shifted_minor = np.roll(_MINOR_PROFILE, root)
        corr_major = float(np.corrcoef(chroma_mean, shifted_major)[0, 1])
        corr_minor = float(np.corrcoef(chroma_mean, shifted_minor)[0, 1])
        if corr_major > best_corr:
            best_corr = corr_major
            best_key = _NOTE_NAMES[root]
        if corr_minor > best_corr:
            best_corr = corr_minor
            best_key = _NOTE_NAMES[root] + "m"
    return best_key


def _to_mono(audio: np.ndarray) -> np.ndarray:
    if audio.ndim == 1:
        return audio
    return audio.mean(axis=0)


def detect_beats(audio: np.ndarray, sr: int) -> np.ndarray:
    """Return sorted beat timestamps in seconds."""
    mono = _to_mono(audio)
    if mono.shape[0] < sr:
        return np.array([0.0])
    _, beat_frames = librosa.beat.beat_track(y=mono, sr=sr)
    if len(beat_frames) == 0:
        return np.array([0.0])
    return np.sort(librosa.frames_to_time(beat_frames, sr=sr).astype(float))


def detect_onsets(
    audio: np.ndarray,
    sr: int,
    min_gap_ms: float = 80.0,
    threshold: float = 0.3,
) -> np.ndarray:
    """Return onset timestamps in seconds, filtered by minimum gap."""
    mono = _to_mono(audio)
    onset_frames = librosa.onset.onset_detect(
        y=mono, sr=sr, delta=threshold, units="frames"
    )
    if len(onset_frames) == 0:
        return np.array([0.0])
    times = librosa.frames_to_time(onset_frames, sr=sr).astype(float)
    min_gap_s = min_gap_ms / 1000.0
    filtered = [times[0]]
    for t in times[1:]:
        if t - filtered[-1] >= min_gap_s:
            filtered.append(t)
    return np.array(filtered)


def detect_bars(
    audio: np.ndarray,
    sr: int,
    beats_per_bar: int = 4,
) -> np.ndarray:
    """Return bar-boundary timestamps (every beats_per_bar-th beat)."""
    beat_times = detect_beats(audio, sr)
    return beat_times[::beats_per_bar]


def detect_energy_gates(
    audio: np.ndarray,
    sr: int,
    threshold_db: float = -20.0,
    hold_ms: float = 50.0,
) -> np.ndarray:
    """Return sample-aligned boolean mask: True where audio exceeds threshold_db."""
    mono = _to_mono(audio)
    hop_length = 512
    rms = librosa.feature.rms(y=mono, hop_length=hop_length)[0]
    rms_db = 20.0 * np.log10(rms + 1e-9)
    frame_mask = rms_db >= threshold_db

    # apply hold
    hold_frames = max(1, int(hold_ms / 1000.0 * sr / hop_length))
    held = frame_mask.copy()
    for i in range(len(frame_mask)):
        if frame_mask[i]:
            held[i : i + hold_frames] = True

    # upsample frames → samples
    mask_samples = np.repeat(held, hop_length)
    n_samples = audio.shape[-1]
    if len(mask_samples) >= n_samples:
        return mask_samples[:n_samples]
    # pad with last value if short
    pad = np.full(n_samples - len(mask_samples), held[-1] if len(held) else False)
    return np.concatenate([mask_samples, pad])


def detect_structure(
    audio: np.ndarray,
    sr: int,
    n_segments: int = 8,
) -> list[tuple[float, float]]:
    """Return list of (start_sec, end_sec) structural segments covering full duration."""
    mono = _to_mono(audio)
    duration = mono.shape[0] / sr

    if n_segments < 2 or duration < 1.0:
        return [(0.0, duration)]

    mfcc = librosa.feature.mfcc(y=mono, sr=sr, n_mfcc=13)
    n_frames = mfcc.shape[1]
    k = min(n_segments, n_frames - 1)
    if k < 2:
        return [(0.0, duration)]

    boundary_frames = librosa.segment.agglomerative(mfcc, k)
    # include start and end
    all_frames = np.unique(np.concatenate([[0], boundary_frames, [n_frames]]))
    all_times = librosa.frames_to_time(all_frames, sr=sr)
    all_times = np.clip(all_times, 0.0, duration)
    all_times[-1] = duration  # ensure exact end

    segments = [(float(all_times[i]), float(all_times[i + 1]))
                for i in range(len(all_times) - 1)]
    return segments
