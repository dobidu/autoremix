# Summary: Phase 04, Plan 01 — Plugin UI

## What Was Done

### AutoRemixAudioProcessor — AudioBridge member
- `autoremix::AudioBridge bridge_{"http://127.0.0.1", 17432}` added as private member
- `getBridge()` public getter exposes it to editor
- Editor calls `bridge_.isServerAlive()`, `separateStems()`, `applyRemix()` via getter

### AutoRemixEditor — ComboBox + launchAsync FileChooser
- `remix_selector` populated with 3 items (IDs 1-3):
  "Chopped & Screwed" → chopped_screwed
  "Slowed + Reverb"   → slowed_reverb
  "Drum and Bass"     → drum_and_bass
- `loadFile()` replaced synchronous `browseForFileToOpen()` with `launchAsync`
  (synchronous modal not safe in plugin context; also fixes Linux path separator bug)
- `chooser_` stored as `std::unique_ptr<juce::FileChooser>` member (required — launchAsync
  fires callback asynchronously; chooser must outlive the call)
- `file_path` renamed to `file_path_`; `output_path_` added for save flow

### onClick_Play() — background thread + callAsync UI updates
- Guards: empty file_path → message; sidecar down → message
- Engine config table (static array) maps ComboBox ID → engine_id + RemixParams defaults
- Spawns `std::thread(...).detach()` for blocking HTTP calls (separateStems 30s, applyRemix 60s)
- UI updates via `juce::MessageManager::callAsync` — safe cross-thread UI mutation
- Progress: debug_text shows "Separating..." → "Remixing..." → "Done: <path>" or error
- save_btn enabled only after successful remix

### onClick_Save() — launchAsync copy
- Opens FileChooser (save mode, async)
- `juce::File::copyFileTo()` on confirm

### CMakeLists.txt — `-fPIC` fix
- Added `set(CMAKE_POSITION_INDEPENDENT_CODE ON)` before `FetchContent_MakeAvailable(cpr)`
- Root cause: cpr static lib compiled without PIC → linker failure when embedding into
  VST3 shared object (.so). Was latent until AudioBridge became a processor member
  (previously AudioBridge object was dead-stripped from VST3 link)

## Verification Results

- [x] AC-1: ComboBox shows 3 engines; processor has bridge_; FileChooser async
- [x] AC-2: Play button → background thread → separate → remix → debug_text status
- [x] AC-3: cmake --build exits 0 — 223/223 targets (VST3 + Standalone)

## Decisions Made

- `std::thread::detach` for background work (sufficient for MVP; cancel/join deferred Phase 05)
- Save disabled until remix completes (prevents saving stale output_path_)
- `CMAKE_POSITION_INDEPENDENT_CODE ON` at project scope (affects all FetchContent deps —
  correct for a project that builds both static and shared targets)

## Files Modified

- `plugin/include/PluginProcessor.h` — bridge_ member + getBridge() getter
- `plugin/include/PluginEditor.h` — chooser_, file_path_, output_path_ members
- `plugin/src/AutoRemixEditor.cpp` — full rewrite of UI wiring
- `CMakeLists.txt` — CMAKE_POSITION_INDEPENDENT_CODE ON
