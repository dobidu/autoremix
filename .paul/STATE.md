# PAUL State

## Current Position

Milestone: v9 — IN PROGRESS
Phase: 20 — COMPLETE
Status: In-plugin audio playback delivered (stems + original/remix preview)
Last activity: 2026-05-16

Progress:
- Milestone: v9 [██████████] 100%

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
```

## Session Continuity

Last session: 2026-05-16
Stopped at: Phase 20 complete; build clean
Next action: commit Phase 20; user testing

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
