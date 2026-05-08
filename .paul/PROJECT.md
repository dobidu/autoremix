# AutoRemix v2 — Project Context

## Vision
Audio plugin for automatic creative remixing. Load a track, choose a remix style,
get a processed version. Both stem separation and remix are pluggable — algorithmic
and ML backends coexist behind stable interfaces.

## Stack
- JUCE 7+ (C++20): plugin host (VST3/AU/LV2 via juce_audio_plugin_client)
- libcpr: HTTP client for plugin→Python IPC
- nlohmann/json: JSON serialization
- FastAPI + uvicorn: Python sidecar
- Spleeter (Deezer): ML stem separation (2/4/5 stems)
- librosa + soundfile: audio processing in Python
- rubberband (via pyrubberband): time-stretch and pitch-shift

## MVP Remix Modes
1. Chopped & Screwed: slow tempo (0.7x), pitch down (-4 semi), chop every N bars
2. Slowed Reverb: slow tempo (0.75x), heavy reverb (IR or algorithmic), slight pitch down
3. Drum and Bass: double drums tempo, bass boost, high-pass rest, re-sequence

## Non-goals (v2)
- Real-time processing (offline batch only in MVP)
- Cloud processing
- GUI animation
- MIDI output

## Success Criteria
- [ ] Load a WAV/AIFF, press "Separate" → Spleeter runs → 4 stems in temp dir
- [ ] Select "Chopped & Screwed" → press "Remix" → output WAV generated
- [ ] Plugin loads in REAPER without crash (VST3)
- [ ] Python sidecar starts/stops cleanly from plugin lifecycle
- [ ] IStemSeparator implementors can be swapped via config without recompile

## Constraints
- GPL-3.0 compatible deps only (Spleeter=MIT, librosa=ISC, JUCE=GPL ok)
- No JUCE_MODAL_LOOPS in audio thread (already defined in CMakeLists)
- Python sidecar ≤ 500ms startup time after model load
