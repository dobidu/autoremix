# PAUL State

## Current Position

Milestone: v2 Remix Preset Format
Phase: 09 of 10 (Remix Presets) — Executing
Plan: 09-01 complete
Status: 09-01 loop closed
Last activity: 2026-05-13 — 09-01 PLAN ✓ APPLY ✓ UNIFY ✓

Progress:
- Milestone: [██░░░░░░░░] 20%
- Phase 09: [██░░░░░░░░] 20%

## Loop Position

```
PLAN ──▶ APPLY ──▶ UNIFY
  ✓        ✓        ✓     [09-01 closed]
```

## Session Continuity

Last session: 2026-05-13
Stopped at: 09-01 unified
Next action: /paul:plan for 09-02 (PresetLoader + GET /api/v1/presets + remix uses preset params)
Resume file: .paul/phases/09-remix-presets/09-02-PLAN.md

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
