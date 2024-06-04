import librosa
import soundfile as sf
import numpy as np


def detect_bpm(y, sr):
    tempo, _ = librosa.beat.beat_track(y=y, sr=sr)
    return tempo

#Função que altera o BPM da música
def change_bpm(y, bpm, new_bpm):
    rate = new_bpm / bpm
    print(f"Original BPM: {bpm}, New BPM: {new_bpm}, Rate: {rate}")
    y_shortened = librosa.effects.time_stretch(y, rate=rate)
    return y_shortened

#Função que altera o pitch(tom) da música
def change_audio(y, sr, steps):
    print(f"Change pitch to look like Alvin")
    return librosa.effects.pitch_shift(y, sr= sr, n_steps= steps)


#Função que faz com que a música pareça com alvin e os esquilos
def alvin_audio(input_file, output_file, pitch_shift = 5, bpm_shift = 20):
    # Load the audio file
    print(f"Loading audio file: {input_file}")
    y, sr = librosa.load(input_file, sr=None)

    # Detect BPM
    bpm = detect_bpm(y, sr)
    print(f"Detected BPM: {bpm} (type: {type(bpm)})")

    if isinstance(bpm, np.ndarray):
        bpm = float(bpm[0])
    else:
        bpm = float(bpm)

    new_bpm = bpm + bpm_shift

    y_shortned = change_bpm(y, bpm, new_bpm)

    y_alvined = change_audio(y_shortned, sr, pitch_shift)

    sf.write(output_file, y_alvined, sr)
    print(f"Processed audio saved as {output_file}")



if __name__ == "__main__":
    input_file = "input.mp3"  # Caminho para o arquivo de entrada MP3
    output_file = "audio_alvined.mp3"  # Caminho para o arquivo de saída MP3
    pitch_shift = 8  # Número de semitons para mudar a afinação
    bpm_shift = 30  # Mudança no BPM

    alvin_audio(input_file, output_file, pitch_shift, bpm_shift)