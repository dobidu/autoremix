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
- librosa + soundfile + pedalboard: audio processing in Python
- Note: Spleeter deferred (TF/cp312 incompatibility); pyrubberband dropped (requires CLI binary); algorithmic FFT separator used in MVP

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
- [x] Load a WAV/AIFF, press "Separate" → 4 stems in temp dir (algorithmic FFT separator; Spleeter deferred — TF/cp312)
- [x] Select "Chopped & Screwed" → press "Remix" → output WAV generated (all 3 engines verified Phase 03 + pytest)
- [ ] Plugin loads in REAPER without crash (VST3) — Deferred: pending manual REAPER smoke test
- [x] Python sidecar starts/stops cleanly from plugin lifecycle (AudioBridge::startSidecar/stopSidecar, Phase 05-02)
- [x] IStemSeparator implementors can be swapped via config without recompile (SeparatorRegistry + RemixRegistry, Phase 01)
- [x] Stem mix sliders (Vocals/Drums/Bass/Other 0–2×) send pre-weighting to sidecar — Phase 13-01
- [x] In-plugin audio preview via AudioTransportSource plays remixed WAV through plugin output — Phase 13-02
- [x] Save-as-preset writes custom preset JSON to user dir; style_combo_ refreshes live — Phase 13-03
- [x] Musical chop modes (beat/onset/bar/energy/structural) selectable from UI; ops inject into effect chains — Phase 14
- [x] Pairwise mashup: load 2 files, two-column 8-stem mixer with per-stem volume, 8 built-in templates, 5 feel knobs (tempo mod, master pitch, reverb mix + room, HPF B); auto BPM + key alignment — Phase 21
- [x] Native preset loading: 17 JSONs (9 remix + 8 mashup) embedded in plugin binary via `juce_add_binary_data`; user JSONs read from `~/.config/autoremix/{modes,mashup}` (or platform equivalent) override built-ins — Phase 25

## Constraints
- GPL-3.0 compatible deps only (Spleeter=MIT, librosa=ISC, JUCE=GPL ok)
- No JUCE_MODAL_LOOPS in audio thread (already defined in CMakeLists)
- Python sidecar ≤ 500ms startup time after model load

## Phase 13 Key Decisions
- Stem pre-weighting in Python before engine dispatch (universal; no engine changes needed)
- AudioTransportSource in processor (not editor); processBlock routes transport audio to output
- AlertWindow runModalLoop() safe: JUCE_MODAL_LOOPS_PERMITTED=1 in CMakeLists
- POST /api/v1/presets updates module-level _presets in-place (MVP-safe, no lock needed)

## Phase 14 Key Decisions
- analysis.py functions pure (no mutation): accept (audio, sr, **kwargs), return new arrays
- Musical chop injection only for effect-chain presets (non-empty effects) — legacy engine path unchanged
- copy.copy(preset) before injection prevents _presets dict mutation (shallow copy + new list)
- Effect-chain presets use `"engine": "effect_chain"` string — RemixPreset.engine is str, not Optional[str]
- chop_mode appended (not prepended) to effects — existing ops run first, chop on post-processed audio
- OP_REGISTRY grows 6 → 11 entries; _chop_at_boundaries private helper shared by 3 boundary-based ops

## Phase 25 Key Decisions (v4 native)
- Built-in presets embedded in plugin binary via `juce_add_binary_data(AutoRemixPresetsData)` — no filesystem dependency for shipped defaults; 17 JSONs (9 remix + 8 mashup) compiled into the .a/.so/.vst3
- Routing built-ins by suffix `_json` + allow-list of 8 known mashup resource ids (BinaryData name mangling: `drum_swap.json` → `drum_swap_json`)
- User dir via `juce::File::userApplicationDataDirectory` → cross-platform (Linux ~/.config, macOS Application Support, Windows %APPDATA%). Slight macOS divergence from v3 (which used ~/.config everywhere)
- nlohmann/json `j.value(key, default)` for permissive parsing — unknown keys ignored, malformed JSONs logged via juce::Logger + skipped (no throw across the boundary)
- De-dup by id, last-wins: built-ins load first, user dir appended, final vector contains the latest entry per id
- Forward-declare `BinaryData::namedResourceList[]` / `namedResourceListSize` / `getNamedResource` rather than `#include "BinaryData.h"` — multiple juce_add_binary_data targets share the same `BinaryData` namespace; forward decls keep header decoupled from include ordering
- MashupEngine ported header-only: in-memory `(NativeStems, NativeStems, sr, MashupParams) → MashupResult` (no file I/O at engine layer; caller owns WAV writing)
- RubberBand semantics: `setTimeRatio(target_bpm / source_bpm)` matches v3 librosa `rate>1 = output longer = slowed`; per-stem fresh stretcher to avoid state accumulation

---
*Last updated: 2026-05-20 after Phase 25 (native mashup + preset loaders)*
