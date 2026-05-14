# AutoRemix v2 — Roadmap

## Phase 00 — Setup & Scaffolding ✅ COMPLETE (2026-05-08)
Clone repo, submodules, Python env, verify build, install tooling.
Deliverable: green build + server health check passing.

## Phase 01 — Interfaces & Types ✅ COMPLETE (2026-05-08)
Define IStemSeparator, IRemixEngine, SeparatorRegistry, RemixRegistry, PluginTypes.
Deliverable: header-only interfaces compile cleanly; stubs pass.

## Phase 02 — Stem Separation ✅ COMPLETE (2026-05-08)
- AlgorithmicSeparator (FFT band-split, working end-to-end via HTTP)
- AudioBridge::separateStems() implemented (cpr::Post + nlohmann/json)
- SpleeterSeparator: blocked by TF/cp312; replaced by demucs (Phase 02.1 or 05)
Deliverable: load WAV → 4 stem WAVs in /tmp/autoremix/ ✓ (algorithmic path)

## Phase 03 — Remix Engines ✅ COMPLETE (2026-05-08)
- ChoppedAndScrewedEngine (librosa time_stretch/pitch_shift + chopper)
- SlowedReverbEngine (librosa + pedalboard reverb)
- DrumAndBassEngine (drums 2x tempo + bass EQ)
- AudioBridge::applyRemix() calls /api/v1/remix
- pyrubberband → librosa (no rubberband CLI needed)
Deliverable: all 3 engines produce valid WAV from test stems ✓

## Phase 04 — Plugin UI ✅ COMPLETE (2026-05-09)
- FileChooser (launchAsync), ComboBox (3 engines), Play/Save buttons
- Background thread (std::thread::detach) + callAsync UI updates
- AudioBridge member on processor; editor calls via getBridge()
- CMAKE_POSITION_INDEPENDENT_CODE fix for VST3 .so link
Deliverable: Standalone functional — load WAV → separate → remix → save ✓

## Phase 06 — Release ✅ COMPLETE (2026-05-13)
- README.md full build/setup/usage guide
- PROJECT.md success criteria: 4/5 [x] (REAPER VST3 verified manually)
- FileChooser useNativeFileDialog=false (WSL2 GTK fix)
- git tag v2.0.0-rc1 + GitHub release draft
Deliverable: tagged release candidate, VST3 verified in REAPER ✓

## Phase 07 — UI Redesign ✅ COMPLETE (2026-05-13)
- Status label + ProgressBar (removed debug_text/stop_btn)
- Dark theme via AutoRemixLookAndFeel (Dracula palette)
- Waveform display via AudioThumbnail (480×340 window)

## Phase 08 — Pro UI ✅ COMPLETE (2026-05-13)
Research-first heavy redesign toward pro audio plugin aesthetics.
- 08-01: Design spec + LookAndFeel foundation (color tokens, typography system)
- 08-02: Layout overhaul (600×400 window, header bar, zone layout)
- 08-03: StyleTabBar replacing ComboBox + RemixParams sliders
- 08-04: WaveformDisplay overhaul + SidecarHealthDot
- 08-05: Inter font embedding via BinaryData + getTypefaceForFont override

## Phase 09 — Remix Preset Format ✅ COMPLETE (2026-05-14)

- 09-01: 3 built-in preset JSON files + JSON Schema (chopped_screwed, slowed_reverb, drum_and_bass)
- 09-02: PresetLoader + GET /api/v1/presets endpoint + Pydantic models
- 09-03: PresetInfo struct + AudioBridge::getPresets() C++ bridge
- 09-04: StyleTabBar dynamic labels + editor fully data-driven from sidecar presets
Deliverable: preset system end-to-end — JSON → sidecar → C++ → UI ✓

Goal: replace hardcoded engines with a data-driven preset system. Users define custom
remix modes as JSON files; built-in modes become presets too. Foundation for Option B
(full effect-chain DSL) in a future phase.

### Architecture

**Preset JSON schema** (`python/server/presets/*.json` + `~/.config/autoremix/modes/*.json`):
```json
{
  "id": "chopped_screwed",
  "version": "1.0",
  "name": "Chop & Screw",
  "description": "Houston-style slowed and chopped",
  "author": "AutoRemix",
  "tags": ["hip-hop", "southern"],
  "engine": "chopped_screwed",
  "params": {
    "tempo": 0.70, "pitch": -4.0, "reverb": 0.05, "chop_ms": 2000.0,
    "bass_boost_db": 0.0, "drums_tempo_factor": 1.0
  },
  "stem_mix": { "vocals": 1.0, "drums": 1.0, "bass": 1.0, "other": 1.0 },
  "effects": []
}
```
- `engine`: maps to an existing registered C++ / Python engine (Option A)
- `effects`: empty now; Option B pipeline hook for future phase
- Built-in presets: bundled in `python/server/presets/` (3 files replacing hardcoded defaults)
- User presets: `~/.config/autoremix/modes/*.json` (XDG; loaded at sidecar startup)

**Python sidecar additions:**
- `RemixPreset` Pydantic model
- `PresetLoader`: scans both dirs, merges, de-dupes by id (user overrides built-in)
- `GET /api/v1/presets` → `[{id, name, params, stem_mix}]`
- `POST /api/v1/remix` uses preset params (no longer hardcoded per engine)

**C++ additions:**
- `PresetInfo` struct: `{id, name, default_params}`
- `AudioBridge::getPresets()` → calls `/api/v1/presets`
- StyleTabBar populated dynamically from preset list (tab count = preset count)
- `loadEngineDefaults` replaced by per-preset param map fetched from sidecar

### Sub-plans
- 09-01: Format spec + 3 built-in preset JSON files
- 09-02: Python sidecar — PresetLoader + `GET /api/v1/presets` + remix uses preset params
- 09-03: C++ — `AudioBridge::getPresets()` + `PresetInfo` struct + RemixParams preset_id
- 09-04: Dynamic UI — StyleTabBar populated from presets; loadEngineDefaults data-driven
- 09-05: Release prep — merge v2→main, version bump 2.1.0, README update, git tag

## Phase 10 — Effect Chain DSL ✅ COMPLETE (2026-05-14)

Option B implemented: `effects` array drives a declarative pipeline interpreter.
Each effect step specifies which stems, which operation, and parameters. Sidecar
executes the chain instead of delegating to a registered engine.

- 10-01: Design spec + schema v2.0 (engine field optional; effects items fully typed with 6-op enum)
- 10-02: `ops.py` (6 op functions + OP_REGISTRY), `EffectChainEngine`, dispatch routing in main.py
- 10-03: pytest coverage (26 tests total), version bump 2.2.0, release

Deliverable: custom preset JSON with non-empty `effects` → chain interpreter executes → valid WAV ✓

## Phase 05 — Integration & Testing ✅ COMPLETE (2026-05-09)
- pytest suite: 8 tests, health + separate + 3 remix engines + error paths
- Error hardening: HTTPException re-raise before broad except
- CI: GitHub Actions pytest job on push/PR to v2/main
- AudioBridge startSidecar/stopSidecar POSIX fork/exec + SIGTERM lifecycle
- PluginProcessor auto-starts sidecar from AUTOREMIX_SERVER_PATH env var
Deliverable: pytest green, CI yaml, sidecar lifecycle ✓
