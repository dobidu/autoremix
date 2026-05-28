# PAUL State

## Current Position

Milestone: v12 (v4-native) — IN PROGRESS
Branch: native
Phase: 27 — Wire-up + release v4.0.0 — 27-02 unified
Status: 27-04 PLAN ✓. Next: 27-04 APPLY (README rewrite + version 4.0.0 + tag v4.0.0).
Last activity: 2026-05-28

Progress:
- v3 main: FROZEN at v3.2.0
- Milestone v12 (v4-native): [███████░░░] 69% (9 of 13 sub-plans; Phase 27 remains: 4 plans)

Progress:
- v3 main: FROZEN at v3.2.0
- Milestone v12 (v4-native): [██████░░░░] 62% (8 of 13 sub-plans; 26-02 + Phase 27 remain)

## Loop Position

```
PLAN ──▶ APPLY ──▶ UNIFY
  ✓        ✓        ✓     [18-01 complete] Font + theme foundation
  ✓        ✓        ✓     [18-02 complete] ScreenContext + ScreenBase + ScreenEmpty
  ✓        ✓        ✓     [18-03 complete] ScreenSeparating + ScreenStemsReady
  ✓        ✓        ✓     [18-04 complete] ScreenModeParams
  ✓        ✓        ✓     [18-05 complete] ScreenRender + editor migration
  ✓        ✓        ✓     [19-01 complete] Stem drag-to-DAW
  ✓        ✓        ✓     [20-01 complete] Audio playback (stems + original/remix preview)
  ✓        ✓        ✓     [21-01 complete] AutoMashup sidecar (MashupEngine + endpoint)
  ✓        ✓        ✓     [21-02 complete] C++ glue (MashupParams/Result + AudioBridge::mashup + ScreenContext fields)
  ✓        ✓        ✓     [21-03 complete] ScreenMashup UI (widgets, layout, run_mashup wiring)
  ✓        ✓        ✓     [21-04 complete] Entry-point flow + redesign (8-stem 2-col mixer, vibrant colors, render progress)
  ✓        ✓        ✓     [21-05 complete] Mashup templates (8 built-ins + 5 feel knobs + Advanced ▾)
  ✓        ✓        ✓     [21-06 complete] Docs + release v3.1.0 — all green
  ✓        ✓        ✓     [22-01 complete] Track B separation uses ScreenSeparating (mashup-aware)
  ✓        ✓        ✓     [22-02 complete] ScreenRender mashup-aware Done state (MASHUP DONE + 3 play btns + New Mashup)
  ✓        ✓        ✓     [22-03 complete] Tooltips + slider unit suffixes + SIDECAR label
  ✓        ✓        ✓     [22-04 complete] Drop-zone orientation copy (headline + subtitle + hint)
  ✓        ✓        ✓     [22-05 complete] Final polish: copy + 3 waveforms + reveal + status

═══ v4 NATIVE REWRITE (branch: native) ═══
  ✓        ✓        ✓     [23-01 complete] RubberBand FetchContent + TimePitchStretcher wrapper
  ✓        ✓        ✓     [23-02 complete] NativeAnalysis.h: BPM, beats, onsets, key, semitone_delta, LUFS, energy gates
  ✓        ✓        ✓     [23-03 complete] NativeAlgorithmicSeparator.h: 4-band IIR split (closes Phase 23)
  ✓        ✓        ✓     [24-01 complete] Native remix engines: ChoppedAndScrewed + SlowedReverb + DrumAndBass
  ✓        ✓        ✓     [24-02 complete] EffectChainEngine + 11 ops (closes Phase 24)
  ✓        ✓        ✓     [25-01 complete] NativeMashupEngine.h (port of v3 mashup.py)
  ✓        ✓        ✓     [25-02 complete] NativePresetTypes + NativePresetLoaders (binary + user dir, closes Phase 25)
  ✓        ✓        ✓     [26-01 complete] Demucs → ONNX export tooling (352.9 MB, opset 18, parity PASS, DOD chosen)
  ✓        ✓        ✓     [26-02 complete] ORT FetchContent + ModelDownloader + NativeDemucsSeparator (closes Phase 26)
  ✓        ✓        ✓     [27-01 complete] Rip 7 AudioBridge call sites; wire screens to native + DOD UI; fix TimePitchStretcher offline hang
  ✓        ✓        ✓     [27-02 complete] Delete AudioBridge; scrub PluginProcessor; CI + release.yml v4-ready (4b40bfc)
  ✓        ✓        ✓     [27-03 complete] Full regression test — 0 regressions, v4 cleared for release
```

## Session Continuity

Last session: 2026-05-28
Stopped at: 27-04 planned. PLAN at .paul/phases/27-wireup-release/27-04-PLAN.md
Next action: /paul:apply 27-04 — README rewrite, version 4.0.0, commit, tag v4.0.0
Resume file: .paul/phases/27-wireup-release/27-04-PLAN.md

## Untracked work (2026-05-16 hotfix session)
- fix: waveform display (setBufferedToImage removed, callAsync repaint)
- fix: MP3 waveform via JUCE_USE_MP3AUDIOFORMAT=1
- fix: sidecar auto-start used wrong python invocation (direct path → -m module)
- fix: status bar UTF-8 garbage (…/— → .../-)
- feat: base.py — normalize_lufs, detect_bpm, beat_aligned_ms, apply_highpass, apply_high_shelf
- feat: ChoppedAndScrewed — BPM detection, beat-aligned chop, HP after pitch-down
- feat: SlowedReverb — presence boost scales with slowdown, LUFS out
- feat: DrumAndBass — auto-targets 170 BPM from source BPM, LUFS out
- add: pyloudnorm>=0.1.1 to requirements

## Decisions
- Python sidecar port: 17432
- Temp dir: /tmp/autoremix/
- Stem format: WAV 44100 Hz stereo, final format tracks input
- IPC: HTTP+JSON (not gRPC)
- Default separator: algorithmic
- Default engine: ChoppedAndScrewedEngine
- spleeter → demucs (tensorflow/cp312 incompatibility; see libs/demucs.md)
- pyrubberband pinned >=0.4.0 (imp removed in Python 3.12)
- JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR requires explicit default ctor
- AudioBridge timeout 30s (ML separation on CPU can exceed 10s)
- Python server ignores output_dir from request; C++ uses paths from response
- pyrubberband dropped (requires rubberband CLI binary); replaced with librosa helpers
- librosa time_stretch/pitch_shift: stereo requires per-channel processing
- CMAKE_POSITION_INDEPENDENT_CODE ON required for cpr to link into VST3 .so
- AudioBridge as processor member (not editor) — editor accesses via getBridge()
- std::thread::detach + MessageManager::callAsync for background→UI updates
- analysis.py functions pure: accept (audio, sr, **kwargs), return new arrays, no mutation
- Musical chop injection only for effect-chain presets (non-empty effects)
- Effect-chain presets use engine: "effect_chain" string (RemixPreset.engine is str not Optional)
- chop_mode appended to effects (not prepended) — existing ops run first
- Demucs ONNX shipping: download. Reason: 352.9 MB file too large for 3× format bundling; standard ML pattern; allows model updates without plugin re-release.
  - Cache dir: `juce::File::userApplicationDataDirectory/autoremix/models/htdemucs.onnx`
  - Hosting URL (placeholder, pinned in 26-02): `https://github.com/dobidu/autoremix/releases/download/v4.0.0-models/htdemucs.onnx`
  - SHA256: TBD (compute at release time and pin into plugin code in 26-02)
- htdemucs ONNX export: torch.stft on complex tensors + nn.MultiheadAttention fused op both blocked the bare export. Fix is offline monkey-patch in tools/export_demucs_onnx.py — conv1d/conv_transpose1d DFT replacement + manual MHA primitive-ops. Patches verified by numerical parity (max abs diff < 1e-3 vs unpatched).

## Blockers
none

## Deferred
- Real-time processing
- ML remix engines
- LV2 plugin format
- Spleeter/demucs ML separator (Phase 02.1 or 05)
- C++ AlgorithmicSeparator real impl (low priority; HTTP path works)
- chop_mode selector has no effect on legacy presets (empty effects) — no UI warning shown

## Decisions
- Python sidecar port: 17432
- Temp dir: /tmp/autoremix/
- Stem format: WAV 44100 Hz stereo, final format tracks input
- IPC: HTTP+JSON (not gRPC)
- Default separator: algorithmic
- Default engine: ChoppedAndScrewedEngine
- spleeter → demucs (tensorflow/cp312 incompatibility; see libs/demucs.md)
- pyrubberband pinned >=0.4.0 (imp removed in Python 3.12)
- JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR requires explicit default ctor
- AudioBridge timeout 30s (ML separation on CPU can exceed 10s)
- Python server ignores output_dir from request; C++ uses paths from response
- pyrubberband dropped (requires rubberband CLI binary); replaced with librosa helpers
- librosa time_stretch/pitch_shift: stereo requires per-channel processing
- CMAKE_POSITION_INDEPENDENT_CODE ON required for cpr to link into VST3 .so
- AudioBridge as processor member (not editor) — editor accesses via getBridge()
- std::thread::detach + MessageManager::callAsync for background→UI updates

## Blockers
none

## Deferred
- Real-time processing
- ML remix engines
- LV2 plugin format
- Spleeter/demucs ML separator (Phase 02.1 or 05)
- C++ AlgorithmicSeparator real impl (low priority; HTTP path works)
