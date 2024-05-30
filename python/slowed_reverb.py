import librosa
import numpy as np
import soundfile as sf

def detect_bpm(y, sr):
    tempo, _ = librosa.beat.beat_track(y=y, sr=sr)
    return tempo

def change_bpm(y, bpm, new_bpm):
    rate = new_bpm / bpm
    print(f"Original BPM: {bpm}, New BPM: {new_bpm}, Rate: {rate}")
    y_stretched = librosa.effects.time_stretch(y, rate=rate)
    return y_stretched

def change_pitch(y, sr, n_steps):
    print(f"Changing pitch by {n_steps} semitones")
    return librosa.effects.pitch_shift(y, sr=sr, n_steps=n_steps)

def add_reverb(y):
    print("Adding reverb...")
    y_with_reverb = librosa.effects.preemphasis(y)
    return y_with_reverb

def merge_audio(original_audio, reverb_audio, reverb_factor):
    print("Merging audio with reverb...")
    merged_audio = original_audio + (reverb_factor * reverb_audio)
    return merged_audio

def process_audio(input_file, output_file_no_reverb, output_file_with_reverb, bpm_change=-20, pitch_change=-3, reverb_factor=3.0):
    # Load the audio file
    print(f"Loading audio file: {input_file}")
    y, sr = librosa.load(input_file, sr=None)

    # Detect BPM
    bpm = detect_bpm(y, sr)
    print(f"Detected BPM: {bpm} (type: {type(bpm)})")

    # Ensure bpm is a scalar
    if isinstance(bpm, np.ndarray):
        bpm = float(bpm[0])
    else:
        bpm = float(bpm)
    
    new_bpm = bpm + bpm_change

    # Change BPM
    y_stretched = change_bpm(y, bpm, new_bpm)

    # Change pitch
    y_shifted = change_pitch(y_stretched, sr, pitch_change)

    # Add reverb
    y_with_reverb = add_reverb(y_shifted)

    # Merge audio with reverb
    merged_audio = merge_audio(y_shifted, y_with_reverb, reverb_factor)

    # Save the modified audio with reverb
    print(f"Saving output audio file with reverb: {output_file_with_reverb}")
    sf.write(output_file_with_reverb, merged_audio, sr)

    # Save the modified audio without reverb
    print(f"Saving output audio file without reverb: {output_file_no_reverb}")
    sf.write(output_file_no_reverb, y_shifted, sr)

if __name__ == "__main__":
    input_file = "behind_blue_eyes.mp3"
    output_file_no_reverb = "behind_blue_eyes_slowed.mp3"
    output_file_with_reverb = "behind_blue_eyes_slowed_reverb.mp3"
    process_audio(input_file, output_file_no_reverb, output_file_with_reverb)