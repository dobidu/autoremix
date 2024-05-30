from pydub import AudioSegment
import numpy as np
import random
import librosa

def detect_bpm(input_file):
    # Load the file with librosa to detect BPM
    y, sr = librosa.load(input_file, sr=None)
    tempo, _ = librosa.beat.beat_track(y=y, sr=sr)
    return tempo.item()  # Ensure tempo is a scalar by extracting the single element from the array

def normalize_audio(audio_segment):
    return audio_segment.normalize()

def chop_and_screw(input_file, output_file, pitch_shift=-2, speed_factor=0.8, bpm=None, chop_variance=0.5, skip_factor=0.5):

    # Load audio file
    track = AudioSegment.from_file(input_file)

    # Normalize the input audio
    track = normalize_audio(track)

    # If bpm is not provided, detect it
    if bpm is None:
        bpm = detect_bpm(input_file)

    # Calculate chop sizes based on BPM
    beat_duration = 60000 / bpm  # Duration of a beat in milliseconds
    min_chop_size = int(beat_duration * (1 - chop_variance))  # Minimum chop size based on variance
    max_chop_size = int(beat_duration * (1 + chop_variance))  # Maximum chop size based on variance

    # Print parameters
    print("Pitch shift:", pitch_shift)
    print("Speed factor:", speed_factor)
    print("BPM:", bpm)
    print("Chop variance:", chop_variance)
    print("Skip factor:", skip_factor)

    # Convert track to numpy array
    samples = np.array(track.get_array_of_samples(), dtype=np.float32) / (2 ** 15)
    sr = track.frame_rate

    # Apply pitch shift using librosa
    samples = librosa.effects.pitch_shift(samples, sr=sr, n_steps=pitch_shift)
    # Apply time stretch using librosa
    # samples = librosa.effects.time_stretch(samples, speed_factor)
    samples = librosa.effects.time_stretch(samples, rate=speed_factor)

    # Convert back to int16 for pydub
    samples = (samples * (2 ** 15)).astype(np.int16)

    track = AudioSegment(
        samples.tobytes(),
        frame_rate=sr,
        sample_width=samples.dtype.itemsize,
        channels=track.channels
    )

    # Chop the track
    chopped_track = AudioSegment.silent(duration=0)
    position = 0
    fade_duration = 10  # Duration of the fade-in and fade-out in milliseconds

    while position < len(track):
        # Determine the size of the chop
        chop_size = random.randint(min_chop_size, max_chop_size)
        chop = track[position:position + chop_size]

        # Apply fade-in and fade-out to smooth the transitions
        chop = chop.fade_in(fade_duration).fade_out(fade_duration)

        # Append chop to the new track
        chopped_track += chop

        # Advance position by chop size and skip some random length based on skip_factor
        position += chop_size + int(chop_size * skip_factor)

    # Normalize the output audio to avoid clipping
    chopped_track = normalize_audio(chopped_track)

    # Export the chopped and screwed track
    chopped_track.export(output_file, format="mp3")

    print("Chopped and screwed track saved to: ", output_file)


# Parameters
'''
params = {
    "pitch_shift": 1,         # Pitch shift in semitones (positive values to pitch up)
    "speed_factor": 1.2,      # Factor to speed up the speed (1.2 means 120% of original speed)
    "chop_variance": 0.3,     # Variance for chop size (0.3 means chop sizes range from 70% to 130% of a beat)
    "skip_factor": 0.3        # Factor to skip parts of the track after each chop (0.3 means skip 30% of the size of each chop)
}
'''

params = {
    "pitch_shift": -2,        # Pitch shift in semitones (negative values to pitch down)
    "speed_factor": 0.8,      # Factor to slow down the speed (0.8 means 80% of original speed)
    "chop_variance": 0.5,     # Variance for chop size (0.5 means chop sizes range from half to one and a half beats)
    "skip_factor": 0.5        # Factor to skip parts of the track after each chop (0.5 means skip half the size of each chop)
}

# Input and output file paths
input_file = "track.mp3"
output_file = "track_chopped_and_screwed.mp3"

# Run the chop and screw process
chop_and_screw(input_file, output_file, **params)
