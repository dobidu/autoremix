# AutoRemix

> JUCE VST3/AU/Standalone plugin + Python FastAPI sidecar for creative stem-based audio remixing.

Load any audio file, separate it into stems, tweak per-stem levels and remix parameters, and get a processed output — all inside the plugin. No DAW required for the standalone mode.

Stem separation and remix engines are **pluggable**: new backends register via `SeparatorRegistry` / `RemixRegistry` without touching existing code.

---

## Screenshots

| Screen | Description |
|--------|-------------|
| ![Drop Zone](docs/screenshots/01-empty.png) | **Step 1 — Drop Zone**: drag a file or browse |
| ![Separating](docs/screenshots/02-separating.png) | **Step 2 — Separating**: 4-stem extraction in progress |
| ![Stems Ready](docs/screenshots/03-stems-ready.png) | **Step 3 — Stems Ready**: per-stem playback, mix, and drag-to-DAW |
| ![Mode Params](docs/screenshots/04-mode-params.png) | **Step 4 — Mode & Parameters**: choose style, tune remix settings |
| ![Render](docs/screenshots/05-render.png) | **Step 5 — Render**: dual waveform comparison, play original vs remix |

> **Note:** Screenshots not yet captured. Run the standalone app and record your own.

---

## Quick Start

```bash
# 1. Clone
git clone --recurse-submodules https://github.com/dobidu/autoremix.git
cd autoremix

# 2. Build plugin (Linux / macOS)
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel

# 3. Start Python sidecar
cd python
uv venv && source .venv/bin/activate
uv pip install -r requirements.txt
python -m server.main &

# 4. Launch standalone plugin
cd ..
AUTOREMIX_SERVER_PATH=$(pwd)/python \
  ./build/plugin/AutoRemix_artefacts/Release/Standalone/AutoRemix
```

---

## Complete Tutorial

### Prerequisites

- Python sidecar running on port 17432 (see [Python Sidecar](#python-sidecar))
- Plugin launched (standalone or loaded in DAW as VST3/AU)
- Audio file ready (WAV, AIFF, FLAC, MP3, OGG, M4A)

### The Interface

The plugin window is **960 × 600 px**. It has three persistent zones and a central area that changes per screen:

```
┌─────────────────────────────────────────────────────────────────────────────────────┐
│  AutoRemix   [Algorithmic FFT ▼]                                                  ● │  ← header
├─────────────────────────────────────────────────────────────────────────────────────┤
│                                                                                     │
│                         [ current screen content ]                                  │
│                                                                                     │
├─────────────────────────────────────────────────────────────────────────────────────┤
│  Ready                                                                              │  ← status bar
└─────────────────────────────────────────────────────────────────────────────────────┘
```

- **Separator combo** (header): choose stem separator before processing
- **Health dot** (top-right): green = sidecar connected, amber = checking, red = unreachable
- **Status bar** (footer): shows current operation and messages

---

### Step 1 — Load a File

**Screen: Empty (Drop Zone)**

```
┌─────────────────────────────────────────────────────────────────────────────────────┐
│  AutoRemix   [Algorithmic FFT ▼]                                                  ● │
├─────────────────────────────────────────────────────────────────────────────────────┤
│                                                                                     │
│                                                                                     │
│                        Drop an audio file here                                      │
│                             — or —                                                  │
│                           [ Browse... ]                                             │
│                                                                                     │
│                                                                                     │
├─────────────────────────────────────────────────────────────────────────────────────┤
│  Ready                                                                              │
└─────────────────────────────────────────────────────────────────────────────────────┘
```

> `[SCREENSHOT: 01-empty.png — drop zone with drag-drop hint and Browse button]`

**How to load a file:**

- **Drag and drop**: drag any supported audio file onto the plugin window
- **Browse**: click the Browse button to open a file picker

Supported formats: `.wav`, `.aif`, `.aiff`, `.mp3`, `.flac`, `.ogg`, `.m4a`

After loading, the plugin analyses the file (BPM detection, key detection) and shows a waveform preview:

```
┌─────────────────────────────────────────────────────────────────────────────────────┐
│  AutoRemix   [Algorithmic FFT ▼]                                                  ● │
├─────────────────────────────────────────────────────────────────────────────────────┤
│                                                                                     │
│   ▒▒▒▓▓▒░░▒▒▓▒░▒▒▓▓░▒▒▒▒▓▓▒░░▒▒▓▒░▒▒▓▓░▒▒▒▒▓▓▒░░▒▒▓▒░▒▒▓▓░                       │
│                                                                                     │
│   detected: 128 BPM  ·  A minor                                                    │
│                                                                                     │
│                   [ Separate Stems ]       [ Change File ]                          │
│                                                                                     │
├─────────────────────────────────────────────────────────────────────────────────────┤
│  Analyzed: 128.0 BPM, A minor                                                       │
└─────────────────────────────────────────────────────────────────────────────────────┘
```

> `[SCREENSHOT: 01b-empty-loaded.png — waveform visible, BPM and key displayed, Separate Stems button]`

**Before separating**, choose your stem separator in the header combo:
- **Algorithmic FFT** — fast, always available, rough stems (band-split)
- **Demucs (ML)** — slow on CPU (~1-5 min), professional-quality isolation (requires `pip install demucs`)

Click **Separate Stems** to proceed.

---

### Step 2 — Stem Separation

**Screen: Separating**

```
┌─────────────────────────────────────────────────────────────────────────────────────┐
│  AutoRemix   [Algorithmic FFT ▼]                                                  ● │
├─────────────────────────────────────────────────────────────────────────────────────┤
│                          SEPARATING                                                 │
│                                                                                     │
│   Vocals  ████████████████████░░░░░░░░░░░░░░░░░░░   done                          │
│   Drums   ████████████████████████████░░░░░░░░░░░░   done                          │
│   Bass    ██████████░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░   processing...                │
│   Other   ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░   waiting                     │
│                                                                                     │
│                         12 s                                                        │
├─────────────────────────────────────────────────────────────────────────────────────┤
│  Separating stems...                                                                │
└─────────────────────────────────────────────────────────────────────────────────────┘
```

> `[SCREENSHOT: 02-separating.png — 4 stem progress rows with elapsed time counter]`

The plugin sends the audio to the Python sidecar, which runs the selected separator and writes 4 stem WAV files to a temp directory. Progress is shown per-stem with an elapsed timer. This screen navigates automatically to Stems Ready when complete.

---

### Step 3 — Stems Ready

**Screen: Stems Ready**

```
┌─────────────────────────────────────────────────────────────────────────────────────┐
│  AutoRemix   [Algorithmic FFT ▼]                                                  ● │
├─────────────────────────────────────────────────────────────────────────────────────┤
│  STEMS                                                                              │
│                                                                                     │
│  ● Vocals  ▒▒▓▓▒░▒░░░▒▒▒▓▒▒▒░░▒▒▒▒▒▓▓▒░░▒▒░   ▶  M  S  ──────●────── 1.0        │
│  ● Drums   ▒░▒░▒░▒░▒░▒░▒░▒░▒░▒░▒░▒░▒░▒░▒░▒░   ▶  M  S  ──────●────── 1.0        │
│  ● Bass    ▓▓░░░░▓▓░░░░▓▓░░░░▓▓░░░░▓▓░░░░▓▓   ▶  M  S  ──────●────── 1.0        │
│  ● Other   ░▒▓▒░▓▒░▓▒░▓▒░░▒░░░▒░░▒▒░░▒░░░▒░   ▶  M  S  ──────●────── 1.0        │
│                                                                                     │
│  [ < Back ]                                              [ Choose Style > ]         │
├─────────────────────────────────────────────────────────────────────────────────────┤
│  Stems ready. Play, adjust mix, then choose a remix style.                          │
└─────────────────────────────────────────────────────────────────────────────────────┘
```

> `[SCREENSHOT: 03-stems-ready.png — 4 stem rows with waveforms, play/mute/solo controls, gain sliders]`

**Controls per stem row:**

| Control | Action |
|---------|--------|
| `▶` / `■` | Play / stop this stem. Multiple stems can play simultaneously. A white cursor moves over the waveform while playing. |
| `M` (Mute) | Silence this stem in the remix output |
| `S` (Solo) | Mute all other stems |
| Slider (0–2×) | Per-stem gain applied to the remix output (0 = silent, 1 = unity, 2 = double) |

**Drag to DAW**: drag any stem row out of the plugin window to export the raw stem WAV file directly to a DAW track or file manager.

When satisfied with the stem balance, click **Choose Style →** to proceed.

---

### Step 4 — Mode & Parameters

**Screen: Mode Params**

```
┌─────────────────────────────────────────────────────────────────────────────────────┐
│  AutoRemix   [Algorithmic FFT ▼]                                                  ● │
├─────────────────────────────────────────────────────────────────────────────────────┤
│  STYLE                                                                              │
│  [ Chop & Screw ▼──────────────────────────────────────── ] [ Save as Preset ]     │
│                                                                                     │
│  PARAMETERS                                                                         │
│  Target BPM   ──────────●─────────────  89.6  BPM                                  │
│  Pitch Shift  ──●───────────────────── −4.0   semi                                 │
│  Reverb Mix   ──────●───────────────── 0.05                                        │
│  Chop Mode    [ Beat-Aligned ▼]                                                     │
│  Chop Beats   ────────●────────────────  2.0  beats                                │
│                                                                                     │
│  [ < Back ]                                                  [ Remix → ]            │
├─────────────────────────────────────────────────────────────────────────────────────┤
│  Detected: 128.0 BPM → target 89.6 BPM                                             │
└─────────────────────────────────────────────────────────────────────────────────────┘
```

> `[SCREENSHOT: 04-mode-params.png — preset combo, parameter sliders, chop mode combo, Save as Preset button]`

**Style preset combo** — choose from 9 built-in styles or any user presets:

| Preset | Engine | Character |
|--------|--------|-----------|
| Chop & Screw | chopped_screwed | 0.7× tempo, −4 semi pitch |
| Slowed Reverb | slowed_reverb | 0.75× tempo, heavy reverb |
| Drum & Bass | drum_and_bass | 2× drums, bass boost |
| Trap Stutter | effect chain | beat-chop vocals, onset drums |
| Onset Drill | effect chain | onset vocal chop, energy gate |
| Structural Loop | effect chain | structural reorder + reverb |
| Phonk | effect chain | 0.88×, −3 semi, bass boost |
| Jersey Club | effect chain | 1.25× drums, tight vocal chops |
| Nightcore | effect chain | 1.30×, +4 semi |

**Parameters:**

| Parameter | Range | Description |
|-----------|-------|-------------|
| Target BPM | 40–200 | Output tempo. Defaults from preset. Detected source BPM shown in status bar. |
| Pitch Shift | −24 to +24 semi | Pitch shift in semitones |
| Reverb Mix | 0–1 | Wet/dry reverb blend |
| Chop Mode | 6 options | How effect-chain presets cut the audio (see table below) |
| Chop Beats | 0.25–16 | Chop interval in beats (active for Fixed and Beat-Aligned modes) |

**Chop Mode options** (effect-chain presets only):

| Mode | Description |
|------|-------------|
| Fixed (ms) | Fixed-interval chop in milliseconds |
| Beat-Aligned | Cuts at beat positions detected by librosa |
| Onset-Triggered | Cuts at transient onsets (drum hits, note attacks) |
| Bar-Locked | Cuts every N beats (default 4 = one bar) |
| Energy Gate | Silences low-energy regions below threshold |
| Structural | Reorders structural segments (verse/chorus-level) |

**Save as Preset**: enter a name → saves to `~/.config/autoremix/modes/` (Linux/macOS) or `%APPDATA%\autoremix\modes\` (Windows). Appears in the combo immediately.

Click **Remix →** to start rendering.

---

### Step 5 — Render & Compare

**Screen: Render — in progress**

```
┌─────────────────────────────────────────────────────────────────────────────────────┐
│  AutoRemix   [Algorithmic FFT ▼]                                                  ● │
├─────────────────────────────────────────────────────────────────────────────────────┤
│                           RENDERING...                                              │
│                              42 s                                                   │
│  ████████████████████████████████░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░           │
│                                                                                     │
│                           [ Cancel ]                                                │
├─────────────────────────────────────────────────────────────────────────────────────┤
│  Remixing...                                                                        │
└─────────────────────────────────────────────────────────────────────────────────────┘
```

> `[SCREENSHOT: 05a-rendering.png — progress bar and elapsed timer while remix runs]`

The plugin sends stems + parameters to the sidecar for processing. A Cancel button aborts and returns to Mode Params.

**Screen: Render — done**

```
┌─────────────────────────────────────────────────────────────────────────────────────┐
│  AutoRemix   [Algorithmic FFT ▼]                                                  ● │
├─────────────────────────────────────────────────────────────────────────────────────┤
│  DONE                                                                               │
│                                                                                     │
│  ORIGINAL ─────────────────────────────────────────────────────────────────────    │
│  ▒▒▓▓▒░▒░░░▒▒▒▓▒▒▒░░▒▒▒▒▒▓▓▒░░▒▒░▒░░░▒▒▒▓▒▒▒░░▒▒▒▒▒▓▓▒░░▒▒▒▒▓▓▒░▒░░░▒▒▒          │
│                                                                                     │
│  REMIX ──────────────────────────────────────────────────────────────────────────  │
│  ▓▒░▒░░░▒▒▒▓▒▒▒░░▒▒▒▒▒▓▓▒░░▒▒░▒░░░▒▒░▒▒▒▒░▒▒▒▓▒▒░░▒▒▒▒▒▒░▒░░░▒▒▒▒▓▓▒░▒░░░▒▒     │
│                                                                                     │
│  [ New File ]  [ ▶ Original ]  [ ▶ Remix ]  [ Save ]          [ New Remix ]        │
├─────────────────────────────────────────────────────────────────────────────────────┤
│  Done. Output: /tmp/autoremix/output/track_remix.wav                                │
└─────────────────────────────────────────────────────────────────────────────────────┘
```

> `[SCREENSHOT: 05b-done.png — DONE state with two stacked waveforms and action buttons]`

**Comparing Original vs Remix:**

- **▶ Original** — plays the source file through the plugin. A white cursor moves across the Original waveform while playing. Click again to stop.
- **▶ Remix** — plays the rendered remix. A white cursor moves across the Remix waveform. Switching between Original/Remix stops the other automatically.

**Other actions:**

| Button | Action |
|--------|--------|
| Save | Save remix WAV to a location you choose |
| New Remix | Return to Mode Params and adjust settings |
| New File | Start over with a new audio file |

---

## Requirements

### Linux
- CMake ≥ 3.22, Ninja
- GCC 12+ or Clang 14+ (C++20)
- Python 3.12, [uv](https://github.com/astral-sh/uv)
- `libasound2-dev libfreetype6-dev libfontconfig1-dev libx11-dev libxinerama-dev libxcursor-dev libxrandr-dev libgl1-mesa-dev libcurl4-openssl-dev libgtk-3-dev`

### macOS
- Xcode 14+ (Command Line Tools sufficient)
- CMake ≥ 3.22, Ninja (`brew install cmake ninja`)
- Python 3.12, [uv](https://github.com/astral-sh/uv)

### Windows
- Visual Studio 2022 with C++ workload
- CMake ≥ 3.22 (included with VS or from cmake.org)
- Python 3.12, [uv](https://pypi.org/project/uv/)
- No extra system libs required (WinHTTP used automatically)

---

## Build

### Linux / macOS

```bash
git clone --recurse-submodules https://github.com/dobidu/autoremix.git
cd autoremix
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

**Artifacts:**
```
build/plugin/AutoRemix_artefacts/Release/Standalone/AutoRemix          # Linux
build/plugin/AutoRemix_artefacts/Release/Standalone/AutoRemix.app      # macOS
build/plugin/AutoRemix_artefacts/Release/VST3/AutoRemix.vst3/
build/plugin/AutoRemix_artefacts/Release/AU/AutoRemix.component/       # macOS only
```

### Windows

```bat
git clone --recurse-submodules https://github.com/dobidu/autoremix.git
cd autoremix
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
```

**Artifacts:**
```
build\plugin\AutoRemix_artefacts\Release\Standalone\AutoRemix.exe
build\plugin\AutoRemix_artefacts\Release\VST3\AutoRemix.vst3\
```

---

## Python Sidecar

The plugin communicates with a local Python FastAPI server on port **17432**.

```bash
cd python
uv venv
source .venv/bin/activate     # Linux / macOS
# .venv\Scripts\activate      # Windows

uv pip install -r requirements.txt
python -m server.main         # starts on http://127.0.0.1:17432
```

**Auto-start**: set `AUTOREMIX_SERVER_PATH` to the `python/` directory and the plugin will fork the sidecar automatically on launch and shut it down on exit:

```bash
# Linux / macOS
export AUTOREMIX_SERVER_PATH=/path/to/autoremix/python
./build/plugin/AutoRemix_artefacts/Release/Standalone/AutoRemix

# Windows
set AUTOREMIX_SERVER_PATH=C:\path\to\autoremix\python
build\plugin\AutoRemix_artefacts\Release\Standalone\AutoRemix.exe
```

**Optional — Demucs ML separator** (high-quality stem isolation, ~2 GB download):

```bash
pip install demucs
```

Without demucs, the algorithmic FFT separator is always available. With demucs installed, `DemucsSeparator` appears automatically in the sidecar's `/health` response and in the plugin's separator combo.

---

## Stem Separators

| ID | Method | Quality | Speed |
|----|--------|---------|-------|
| `algorithmic` | FFT band-split | Low (leaky, bleeding) | Instant |
| `demucs` | Demucs `htdemucs` ML | High (clean isolation) | ~1–5 min CPU / ~30 s GPU |

---

## Remix Engines & Presets

### Built-in Engine Presets

| Preset | Engine | Character |
|--------|--------|-----------|
| Chop & Screw | chopped_screwed | 0.7× tempo, −4 semi pitch, chop every 2 s |
| Slowed Reverb | slowed_reverb | 0.75× tempo, algorithmic reverb |
| Drum & Bass | drum_and_bass | 2× drums, bass boost +6 dB |

### Built-in Effect-Chain Presets

| Preset | Style | Key ops |
|--------|-------|---------|
| Trap Stutter | Trap | beat-chop vocals ×3, onset drums ×2, +8 dB bass |
| Onset Drill | Drill | onset vocal chop, energy gate other, beat bass |
| Structural Loop | Experimental | structural reorder + reverb + 0.85× stretch |
| Phonk | Phonk | 0.88×, −3 semi, +8 dB bass, dark reverb |
| Jersey Club | Jersey Club | 1.25× drums/bass, tight onset vocal chops ×3 |
| Nightcore | Nightcore | 1.30× all, +4 semi (vocals + other only) |

---

## Custom Presets

Presets are JSON files. Drop them in `~/.config/autoremix/modes/` (Linux/macOS) or `%APPDATA%\autoremix\modes\` (Windows), then restart the sidecar.

**Engine-based preset (v1.0):**
```json
{
  "id": "my_style",
  "version": "1.0",
  "name": "My Style",
  "engine": "chopped_screwed",
  "params": {
    "tempo_factor": 0.80, "pitch_shift_semi": -2.0,
    "reverb_mix": 0.20, "chop_interval_ms": 1500.0,
    "bass_boost_db": 0.0, "drums_tempo_factor": 1.0
  },
  "stem_mix": { "vocals": 1.0, "drums": 1.0, "bass": 1.0, "other": 1.0 },
  "effects": []
}
```

**Effect-chain preset (v2.0):**
```json
{
  "id": "my_chain",
  "version": "2.0",
  "name": "My Chain",
  "params": { "tempo_factor": 0.75, "pitch_shift_semi": -2.0, "reverb_mix": 0.1,
              "chop_interval_ms": 0.0, "bass_boost_db": 0.0, "drums_tempo_factor": 1.0 },
  "stem_mix": { "vocals": 1.0, "drums": 1.0, "bass": 1.2, "other": 0.8 },
  "effects": [
    { "op": "time_stretch", "stems": "all",    "params": { "factor": 0.75 } },
    { "op": "reverb",       "stems": "all",    "params": { "mix": 0.20, "room_size": 0.7 } },
    { "op": "bass_boost",   "stems": ["bass"], "params": { "db": 6.0 } }
  ]
}
```

**Available effect ops:**

| Op | Key param(s) | Description |
|----|-------------|-------------|
| `time_stretch` | `factor` (0.1–4.0) | Tempo scaling (<1 = slower) |
| `pitch_shift` | `semitones` (−24–24) | Pitch change |
| `reverb` | `mix`, `room_size` | Algorithmic reverb |
| `chop` | `interval_ms` | Fixed-interval chop |
| `bass_boost` | `db` | Low-shelf boost/cut |
| `eq_highpass` | `cutoff_hz` | High-pass filter |
| `chop_beats` | `division`, `repeat` | Beat-aligned chop |
| `chop_onsets` | `min_gap_ms`, `threshold`, `repeat` | Onset-triggered chop |
| `chop_bars` | `beats_per_bar`, `repeat` | Bar-aligned chop |
| `gate_energy` | `threshold_db`, `hold_ms` | Energy gate |
| `structural_cut` | `n_segments`, `mode` | Structural segment reorder |

`stems` accepts `"all"`, a single name, or a list: `["bass", "drums"]`.

---

## Running Tests

```bash
cd python
source .venv/bin/activate
python -m pytest tests/ -v
```

54 tests: health, separation (algorithmic + demucs), 3 engines, 11 effect ops, 5 musical analysis functions, chain interpreter, preset loading, error paths.

---

## Architecture

```
┌─────────────────────────────────┐     HTTP/JSON       ┌──────────────────────────────┐
│  JUCE Plugin (C++20)            │ ◄─────────────────► │  Python Sidecar (FastAPI)    │
│                                 │   port 17432        │                              │
│  PluginProcessor                │                     │  POST /api/v1/separate       │
│  ├─ AudioBridge (HTTP IPC)      │                     │  POST /api/v1/remix          │
│  ├─ MixerAudioSource            │                     │  GET  /api/v1/health         │
│  │   ├─ StemPlayer[4]           │                     │  GET  /api/v1/presets        │
│  │   └─ preview transport       │                     │  GET  /api/v1/analyze        │
│  └─ SidecarHealthDot            │                     │  POST /api/v1/presets        │
│                                 │                     │                              │
│  PluginEditor (5-screen flow)   │                     │  AlgorithmicSeparator        │
│  ├─ ScreenEmpty                 │                     │  DemucsSeparator             │
│  ├─ ScreenSeparating            │                     │  ChoppedAndScrewedEngine     │
│  ├─ ScreenStemsReady            │                     │  SlowedReverbEngine          │
│  ├─ ScreenModeParams            │                     │  DrumAndBassEngine           │
│  └─ ScreenRender                │                     │  EffectChainEngine (DSL)     │
└─────────────────────────────────┘                     └──────────────────────────────┘
```

**Sidecar lifecycle:**
- Auto-started via `fork()` + `execl("python3", ...)` (Linux/macOS) or `CreateProcess` (Windows) when `AUTOREMIX_SERVER_PATH` is set
- Auto-stopped on plugin unload via `SIGTERM` / `TerminateProcess`
- All ML/IO runs in `asyncio.to_thread()` — health endpoint always responsive

**Pluggability:** implement `IStemSeparator` or `IRemixEngine`, register in the respective registry — no changes to existing code.

---

## Known Limitations

- Algorithmic FFT separator: rough stems (band-split only). Use demucs for production.
- No real-time processing — offline batch only.
- Demucs CPU inference: ~1–5 min per track. GPU requires CUDA-enabled torch.
- AudioBridge separation timeout: 300 s.
- AU format (macOS): not code-signed — requires user to manually allow in System Settings > Privacy & Security on macOS 13+.

---

## License

GPL-3.0. See [LICENSE](LICENSE).
