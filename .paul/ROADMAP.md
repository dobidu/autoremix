# AutoRemix v2 — Roadmap

## Phase 00 — Setup & Scaffolding
Clone repo, submodules, Python env, verify build, install tooling.
Deliverable: green build + server health check passing.

## Phase 01 — Interfaces & Types
Define IStemSeparator, IRemixEngine, SeparatorRegistry, RemixRegistry, PluginTypes.
Deliverable: header-only interfaces compile cleanly; stubs pass.

## Phase 02 — Stem Separation
- AlgorithmicSeparator (stub: splits by frequency band, no ML)
- SpleeterSeparator (calls Python /api/v1/separate)
- Python: spleeter_sep.py + FastAPI endpoint
Deliverable: load WAV → 4 stem WAVs in /tmp/autoremix/

## Phase 03 — Remix Engines
- ChoppedAndScrewedEngine (pyrubberband + chopper)
- SlowedReverbEngine (pyrubberband + pedalboard reverb)
- DrumAndBassEngine (drums 2x tempo + bass EQ)
- AudioBridge calls /api/v1/remix
Deliverable: each engine produces valid WAV output from test stems

## Phase 04 — Plugin UI
- File chooser (juce::FileChooser)
- Stem waveform thumbnails (juce::AudioThumbnail)
- Mode selector (juce::ComboBox)
- Params panel (knobs for tempo/pitch/reverb)
- Process button + progress bar
Deliverable: plugin loads in REAPER, UI functional

## Phase 05 — Integration & Testing
- End-to-end test with real audio
- Sidecar lifecycle (plugin starts/kills server process)
- Error handling (server down, file too large, unsupported format)
- CI: GitHub Actions (build + pytest)
Deliverable: demo video, release-ready build
