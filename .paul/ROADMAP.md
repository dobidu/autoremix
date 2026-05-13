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

## Phase 07 — UI Redesign (planned)
- Waveform display
- Progress indicator during separation/remix
- Remove debug text area from production UI
- General visual polish

## Phase 05 — Integration & Testing ✅ COMPLETE (2026-05-09)
- pytest suite: 8 tests, health + separate + 3 remix engines + error paths
- Error hardening: HTTPException re-raise before broad except
- CI: GitHub Actions pytest job on push/PR to v2/main
- AudioBridge startSidecar/stopSidecar POSIX fork/exec + SIGTERM lifecycle
- PluginProcessor auto-starts sidecar from AUTOREMIX_SERVER_PATH env var
Deliverable: pytest green, CI yaml, sidecar lifecycle ✓
