# AutoRemix v2 — Roadmap

---

## v5.0 — Creative DJ Suite — In progress

Transform AutoRemix into a sophisticated DJ/producer tool. Templates become the
unified mechanism for every feature — scratch, structure, samples, effects, and DJ
techniques are all configurable blocks in a template JSON. Closes with a complete
UI overhaul targeting a visually stunning, timeline-based, DAW-quality interface.

### Phase 31 — Advanced Structural Analysis ✅ COMPLETE (2026-05-29)
Foundation for all structure-aware features: SongStructure data type, energy map,
phrase grid, spectral centroid, vocal presence, and section boundary detection.

- **31-01**: `SongStructure` struct + energy map + phrase grid
- **31-02**: Spectral centroid per bar + vocal presence + section boundaries
- **31-03**: Template schema v2 — `StructureConfig` in `NativePresetTypes.h`
- **31-04**: `NativeStructuredRemixEngine.h` skeleton — analyze + arrange + assemble

### Phase 32 — Effects Expansion (distortion, delays, creative reverbs, filter sweep, vinyl, sidechain)
### Phase 33 — Sample System (library, auto-align, auto-positioning, one-shots/loops)
### Phase 34 — Auto Scratch Engine
### Phase 35 — Structured Remix Engine (drop engineering, build-up gen, layer scheduling)
### Phase 36 — Structured Mashup Engine (cross-track phrase alignment, EQ swap, harmonic mixing)
### Phase 37 — DJ Techniques (loop roll, hot cues, beat juggling, tempo/pitch automation)
### Phase 38 — Total UI Overhaul (timeline, MPC pads, stem tracks, new aesthetic)

---

## v6.0 — Setlist Mode

Transform AutoRemix into a full DJ set builder. User assembles a playlist of remix
entries (remix / mashup / bypass / section), configures transitions and auto-sync,
and exports a complete mixed set as a single WAV.

**Setlist mode** is a peer top-level mode (persistent `SETLIST` pill in header),
completely separate from the standard single-track workflow. `SetlistConfig` persists
in `ScreenContext` across all navigation cycles.

### Entry types
`remix` · `mashup` · `bypass` · `section` (highlights from a single track)

### Transition library (10 types)
`fade` · `cut` · `echo_out` · `loop_bridge` · `scatter` · `filter_sweep` ·
`stutter_out` · `vinyl_stop` · `drop_transition` · `harmonic_fade`

### Buffering strategy (research-validated)
**Temp-file assembly + fixed-lookahead pipeline.** Every completed entry is flushed
to a temp WAV. `SetlistRenderer` uses `juce::ThreadPool(2)` — Separation Worker
pipelined against Remix Worker (never two ONNX sessions simultaneously). Final
assembly reads only the overlap window (≤10s PCM/boundary). Peak temp disk: N × ~50MB.
Lazy evaluation and priority queue deferred (incompatible with Demucs sequential chunks).

### Setlist structures
`full_tracks` · `highlights` · `energy_curve` · `frankenstein` (micro-slice)

### Phase 39 — Setlist Core Types + Renderer (distinct from standard workflow)

- **39-01**: `SetlistEntry` + `SetlistConfig` + `TransitionConfig` in `PluginTypes.h`.
  Per-entry: `entry_type`, `source_path_a/b`, `separator_id` (per-entry, not global),
  `preset_id`, `section_start/end_bar`, `transition_to_next`, mutable pipeline fields
  (`status` as `std::atomic<uint8_t>`, `FileAnalysis`, `StemPaths`, `rendered_path`).
  Add `SetlistConfig setlist;` to `ScreenContext` + `open_setlist_screen` callback.
- **39-02**: `SetlistRenderer`. `juce::ThreadPool(2)`, bounded queue (cap 1) between
  workers, per-entry cancel tokens, `assemble()` with overlap-window crossfade.
  Temp dir `/tmp/autoremix/setlist/<session_id>/`. Cleanup on cancel and destroy.
- **39-03**: Transition library — 10 transition types as composable audio ops using
  `NativeEffectOps` infrastructure. Min overlap: 4-8s per boundary.
- **39-04**: `SetlistConfig` JSON serialization. Presets by ID (missing ID → default +
  warning, no hard abort). `schema_version` for forward compat. `status` not serialized.

### Phase 40 — Smart Ordering + Sync

- **40-01**: BPM autosync. Per-entry `resolved_bpm`; chain anchors to `entries[0]`.
  Crossfade-aware `overlap_bars`. Bypass entries not stretched.
- **40-02**: Harmonic ordering. 24-position Camelot lookup table. Greedy nearest-neighbor
  sort. `camelot_warnings` vector. `harmonic_fade` auto-selected for low-compatibility.
- **40-03**: Energy-curve + highlights structures using `SongStructure::energy_per_bar`
  (Phase 31 dependency). `frankenstein`: 4-8 bar micro-slice interleave. Degrades
  gracefully without Phase 31.

### Phase 41 — Setlist UI (peer top-level screen; deep design research pass first)

**Architecture decisions from research:**
Rekordbox/Ableton model: `ScreenId::Setlist` as peer top-level, not a sub-flow.
48px entry rows, `juce::Viewport` scroll. `AudioThumbnailCache` capped at 10; entries
11+ pre-rendered to `juce::Image` on worker. Energy curve at 1.5px stroke (1px aliases
on dark bg). Per-entry editor reuses `ScreenModeParams`/`ScreenMashup` via
`ctx_.setlist_edit_idx`. Drag-and-drop via custom `Component` (not `juce::ListBox`).
`SetlistTimelineStrip` as reusable `Component` for Phase 38 unification.

**Phase 41 opens with a deep design research pass** (10 open questions documented)
before any code: JUCE drag-and-drop row reorder pattern, Camelot mapping from
`detect_key()` output, `juce::Path` aliasing verification, per-entry vs global export,
integration path with Phase 38 DAW timeline.

- **41-01**: `ScreenSetlist` layout. Header pill + 48px scrollable rows + 80px mini
  timeline strip. Per row: drag handle, index, filename (truncated ~200px), type badge,
  preset, BPM, Camelot key chip (hue-coded), duration, status chip, delete.
  24px gutter rows with transition icon + Camelot compatibility dot.
- **41-02**: Drag-and-drop reorder. Custom `Component`, 2px `ACCENT` insertion line.
  Multi-file drop zone at list bottom. Multi-file `filesDropped()` → auto-populate.
- **41-03**: Transition picker overlay (10 icons, 48×48px). Hover tooltip: 200×60px
  envelope sketch (visual only). Camelot compatibility dot (green/amber).
- **41-04**: Per-entry editor flow. `ctx_.setlist_edit_idx` wires existing screens.
- **41-05**: Render pipeline wiring. `SetlistRenderer` instantiation, per-entry callbacks
  → `callAsync` → status chips + timeline strip fill animation.
- **41-06**: WAV export + temp cleanup. Save-as (WAV v6.0; MP3 deferred).

---

## v4.3 — ORT upgrade + LV2 + cancellation ✅ COMPLETE (2026-05-29)

Maintenance + UX release: ORT 1.17→1.21 + SHA256 pinning, LV2 format, Demucs cancel.

### Phase 30 — ORT upgrade + LV2 + cancellation ✅ COMPLETE (2026-05-29)

- **30-01**: ORT 1.17→1.21 + SHA256 pinning (all 5 packages) ✅
- **30-02**: LV2 format (Ardour/Bitwig) + CI packaging ✅
- **30-03**: Demucs cancel + 5 thread-safety fixes ✅

---

## v4.2 — macOS CoreML EP ✅ COMPLETE (2026-05-29)

Extend GPU acceleration to Apple Silicon (M1/M2/M3) via CoreML EP in ONNX Runtime.
Same `-DAUTOREMIX_GPU=ON` flag; macOS builds get `AUTOREMIX_COREML=1` compile definition.

**Target**: macOS 12+ on Apple Silicon (CoreML Neural Engine) + Intel fallback to CPU EP.

### Phase 29 — macOS CoreML EP ✅ COMPLETE (2026-05-29)

- **29-01**: CMake CoreML define + NativeDemucsSeparator CoreML path + CI build-macos-gpu + v4.2.0 ✅

---

## v4.1 — GPU Demucs ✅ COMPLETE (2026-05-29)

Accelerate ML stem separation via GPU execution providers in ONNX Runtime.
Controlled by `-DAUTOREMIX_GPU=ON` CMake flag. CPU path unchanged (default).

**Targets**:
- Linux: CUDA EP (onnxruntime-linux-x64-gpu, requires CUDA 11.8+) ✅
- Windows: DirectML EP (Microsoft.ML.OnnxRuntime.DirectML, DX12 GPU) ✅
- macOS: CoreML EP deferred to v4.2 (Phase 29)

**Shipping**: separate GPU artifacts (`AutoRemix-linux-gpu-v4.1.0.zip` etc.)
alongside CPU builds. GPU binary falls back to CPU EP transparently.
Model status dot shows **blue** when GPU EP is active.

### Phase 28 — GPU Demucs ✅ COMPLETE (2026-05-29)

- **28-01**: CMake GPU ORT FetchContent + NativeDemucsSeparator EP selection ✅
- **28-02**: CI GPU build matrix + ModelStatusDot GPU indicator + README + release v4.1.0 ✅

---

## v3.x main branch — FROZEN (2026-05-20)

v3.2.0 is the last v3 release. Bugfixes are accepted into a v4 native branch
and backported only if critical. New features land in v4.

---

## v4.0 — Native rewrite (branch: `native`)

Eliminate the Python sidecar. All audio processing native C++ inside the
plugin/standalone. Single-binary install. No HTTP IPC. No external runtime.

**Why**: Python sidecar = install friction (uv venv, deps, ~2 GB demucs model)
+ runtime fragility (port conflicts, stale processes, env-var paths) + DAW
hostility (extra child process inside the host).

**Native replacements**:
- RubberBand (GPL) — time-stretch + pitch-shift
- aubio (MIT) — BPM, beats, onsets
- juce::dsp — FFT, IIR filters, Reverb
- Port Krumhansl-Schmuckler key detection (~80 LOC, was in Python)
- Port EBU R128 LUFS (~150 LOC)
- ONNX Runtime (MIT) + demucs ONNX export — ML stem separation
- nlohmann/json — preset JSON loading (already in plugin)

### Phase 23 — Native foundation ✅ COMPLETE (2026-05-20)

- **23-01**: RubberBand FetchContent + `TimePitchStretcher` wrapper ✅
- **23-02**: `NativeAnalysis.h` — BPM, beats, onsets, bars, key, semitone_delta, LUFS, energy gates (juce::dsp::FFT only; aubio dropped) ✅
- **23-03**: `NativeAlgorithmicSeparator.h` — 4-band IIR cascade ✅

### Phase 24 — Native remix engines ✅ COMPLETE (2026-05-20)

- **24-01**: `NativeRemixEngines.h` — ChoppedAndScrewed + SlowedReverb + DrumAndBass ✅
- **24-02**: `NativeEffectOps.h` (11 ops) + `NativeEffectChainEngine.h` (interpreter) ✅

### Phase 25 — Native mashup + presets ✅ COMPLETE (2026-05-20)

- **25-01**: `NativeMashupEngine.h` — full port of v3 `mashup.py` (BPM/key alignment, 5 feel knobs, per-stem RubberBand) ✅
- **25-02**: `NativePresetTypes.h` + `NativePresetLoaders.h` — 17 JSONs (9 remix + 8 mashup) embedded via `juce_add_binary_data(AutoRemixPresetsData)`; user JSONs from `~/.config/autoremix/{modes,mashup}` override built-ins on id collision ✅

### Phase 26 — Native Demucs (ONNX) ✅ COMPLETE (2026-05-22)

- **26-01**: `tools/export_demucs_onnx.py` — offline torch.onnx.export with 3 classes of monkey-patches (conv-DFT, HTDemucs real-tensor ops, manual MHA) + numerical parity check. Produces `models/htdemucs.onnx` (352.9 MB, opset 18). Distribution: download-on-demand. ✅
- **26-02**: ONNX Runtime 1.17.0 via FetchContent + `plugin/include/dsp/ModelDownloader.h` (DOD + SHA256 verification + retry × 3) + `plugin/include/dsp/NativeDemucsSeparator.h` (chunked Ort::Session inference with 25 % raised-cosine crossfade). Header-only, smoke includes only — screen wiring lands in 27-01. ✅

### Phase 27 — Wire-up + release v4.0.0

- **27-01**: Remove `AudioBridge` HTTP layer; replace with direct C++ calls from screens
- **27-02**: Remove `sidecar/` from build packaging + CI
- **27-03**: Full regression test on all 6 screens + remix + mashup flows
- **27-04**: README rewrite (no Python install section) + version bump 3.2.0 → 4.0.0 + tag v4.0.0

Deliverable: v4.0.0 ships as single-binary VST3/AU/Standalone. Zero Python.
All v3.2 features preserved.

---

## Phase 22 — UX Polish ✅ COMPLETE (2026-05-19)

Post-21 polish pass addressing friction points found during user testing
of the mashup flow + general UX gaps.

- **22-01**: Track B progress screen — route through `ScreenSeparating` in mashup mode ✅
- **22-02**: ScreenRender mashup-aware Done state — "MASHUP DONE" header, ▶ Track A / B / Mashup, "New Mashup" → ScreenMashup ✅
- **22-03**: Tooltips + labels + slider units — health dot, M/S, separator combo, ×/BPM/Hz/semi suffixes, SIDECAR label ✅
- **22-04**: First-time orientation — drop-zone headline + subtitle + hint ✅
- **22-05**: Final polish — Free mix / Match A's key / · / time hint / 3-waveform Done state with distinct colors / reveal-in-folder / status messages ✅

Deliverable: mashup flow polished end-to-end; remix flow unchanged ✓

---

## Phase 21 — AutoMashup ✅ COMPLETE (2026-05-18)

Pairwise mashup: combine two tracks into a coherent remix. Two-column
8-stem mixer with per-stem volume sliders. 8 built-in templates + 5 feel
knobs (tempo mod, master pitch, reverb mix + room, HPF on B). Sidecar
auto-aligns BPM and key via time-stretch + pitch-shift.

### Sub-plans

- **21-01**: Sidecar — `MashupEngine` + `POST /api/v1/mashup` + pytest coverage ✅
- **21-02**: C++ glue — `MashupParams`, `AudioBridge::mashup()`, `ScreenContext` fields ✅
- **21-03**: `ScreenMashup` UI — two-column 8-stem mixer with per-stem volume sliders ✅
- **21-04**: Entry-point flow — "Mashup >" button (teal) on `ScreenStemsReady`; 2nd-file load + 2nd separation ✅
- **21-05**: Mashup templates — 8 built-in JSON templates + 5 feel knobs (bpm_modifier, master_pitch_offset_semi, master_reverb_mix, master_reverb_room, highpass_b_hz) + Advanced ▾ disclosure ✅
- **21-06**: Release — README mashup tutorial + CLAUDE/PROJECT updates + sidecar v0.5.0 + git tag v3.1.0 ✅

Deliverable: load two files → pick stems from each (or apply a template) → render unified mashup with aligned tempo + key ✓

Deliverable: load two files → pick stems from each → render unified mashup with aligned tempo + key.

---

## Phase 20 — Audio Playback ✅ COMPLETE (2026-05-16)

In-plugin playback replacing OS media player:

- **20-01**: Per-stem ▶/■ toggle buttons in ScreenStemsReady (4 simultaneous via MixerAudioSource); ▶ Original / ▶ Remix exclusive play in ScreenRender Done state; 100ms Timer polling for EOF sync; onExit() stops all playback

Deliverable: no OS player launched anywhere; stems and remix auditioned entirely inside the plugin.

---

## Phase 19 — Stem Drag-to-DAW ✅ COMPLETE (2026-05-16)

Drag any stem row in ScreenStemsReady to a DAW track or file manager to export the temp WAV.

- **19-01**: mouseDown/mouseDrag on ScreenStemsReady; 8px threshold; performExternalDragDropOfFiles; guards for validity + file existence

Deliverable: stems draggable to DAW when running as VST3; copy semantics (canMoveFiles=false).

---

## Phase 18 — UX Redesign ✅ COMPLETE (2026-05-16)

Full UI overhaul: 960×600, burnt-orange brutalist theme, screen-based navigation.

- **18-01**: Fonts (Space Grotesk + JetBrains Mono) + new LookAndFeel palette (ACCENT=#D4652A, RADIUS=0)
- **18-02**: ScreenContext shared state + ScreenBase + ScreenEmpty (drag-drop, waveform, analysis)
- **18-03**: ScreenSeparating (4-stem status + timer) + ScreenStemsReady (per-stem M/S/gain/waveform)
- **18-04**: ScreenModeParams (preset combo, BPM/pitch/reverb/chop sliders)
- **18-05**: ScreenRender (Rendering/Done/Error state machine) + editor legacy migration

Deliverable: 5-screen flow (Empty→Separating→StemsReady→ModeParams→Render) fully wired; 4/4 build targets link.

---

## Phase 17 — More Remix Styles ✅ COMPLETE (2026-05-16)

Three new built-in effect-chain presets expanding creative options.

- **17-01**: `phonk.json`, `jersey_club.json`, `nightcore.json` — JSON only, zero new Python
- **17-02**: 4 new pytest tests + version bump v2.6.0 + README update

Deliverable: 9 total presets; Phonk/Jersey Club/Nightcore selectable in UI after sidecar restart; 54 tests green.

---

## Phase 16 — Performance + UX Polish ✅ COMPLETE (2026-05-16)

Reduce friction in the core load→remix workflow.

- **16-01**: Drag & drop file load onto waveform zone + elapsed-time counter during separation
- **16-02**: Cancel button during remix + sidecar health auto-poll (health dot updates every 5s)

Deliverable: load files by dragging; see time elapsed during slow separation; cancel stuck remixes; health dot reflects live sidecar state.

---

## Phase 15 — Musical UI Parameters ✅ COMPLETE (2026-05-16)

Replace raw multiplier sliders with musically meaningful controls. Engine BPM
detection already exists in base.py; expose it end-to-end to the user.

- **15-01**: `GET /api/v1/analyze` endpoint + `AudioBridge::analyzeFile()` + `FileAnalysis` struct
- **15-02**: Target BPM slider (40–200 BPM), detected BPM/key labels in UI, chop slider in beats

Deliverable: after loading a file, user sees detected BPM + key; tempo slider shows
target BPM (not a raw ratio); chop slider shows beats (not raw ms).

---

## Phase 14 — Musical Chop Intelligence ✅ COMPLETE (2026-05-16)

Replaces fixed-interval chopping with musically-intelligent remixing. Four new chop modes
(beat, onset, bar, energy) + structural segmentation enable preset authors to align cuts to
the music's own rhythm and form rather than an arbitrary millisecond grid.

- **14-01**: `analysis.py` — 5 librosa detection functions + 11 pytest tests
- **14-02**: 5 new ops in `ops.py` (chop_beats, chop_onsets, chop_bars, gate_energy, structural_cut) + 12 pytest tests; OP_REGISTRY 6→11
- **14-03**: 3 new built-in presets (trap_stutter, onset_drill, structural_loop); PresetLoader 3→6 presets
- **14-04**: chop_mode ComboBox (6 modes) in editor; field flows RemixParams→AudioBridge→sidecar→EffectChainEngine injection

Deliverable: beat/onset/bar/energy/structural chop modes selectable from UI; injected into effect-chain presets at remix time; 23 tests green ✓

## Phase 13 — Interactive Mixing ✅ COMPLETE (2026-05-16)

Three sub-plans delivering real creative control over remix output:

- **13-01**: Stem mix sliders (Vocals/Drums/Bass/Other 0–2×) — Python pre-weighting + C++ gains + UI two-column layout
- **13-02**: In-plugin audio preview — AudioTransportSource plays remixed output without export
- **13-03**: Preset editor UI — "Save" button + AlertWindow dialog + POST /api/v1/presets + style_combo_ refresh

Deliverable: users control stem balance live, preview result instantly, save custom presets ✓

## Phase 12 — Separator UI + Cross-Platform Temp Dir ✅ COMPLETE (2026-05-15)

- `SeparatorInfo` type + `AudioBridge::getAvailableSeparators()` (parses `/api/v1/health`)
- `separator_combo_` in plugin header — user selects Demucs (ML) or Algorithmic FFT at runtime
- `onClick_Play` passes selected `sep_id` — no more hardcoded `"algorithmic"`
- `/tmp/autoremix` replaced with `juce::File::tempDirectory` — cross-platform (Windows fix)
- `StyleTabBar.h` deleted; version v2.4.0

Deliverable: separator selection end-to-end from UI → bridge → sidecar ✓

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

## Phase 11 — Demucs ML Separator ✅ COMPLETE (2026-05-15)

Replaced FFT band-split with Meta's Demucs (htdemucs model) for professional-quality
stem separation. Optional dep: `DemucsSeparator.is_available()=False` falls back to
algorithmic when torch/demucs not installed. AudioBridge separation timeout 30s → 300s.

- 11-01: `DemucsSeparator` + registry registration + 300s AudioBridge timeout
- 11-02: `pip install demucs`, pytest coverage (`pytest.importorskip` for CI), v2.3.0 release

Deliverable: `POST /api/v1/separate` with `separator_id=demucs` → 4 clean ML stems ✓

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
