# PAUL State

## Current Position

Milestone: v2 UI Polish
Phase: 08 of 9 (Pro UI) — Executing
Plan: 08-04 complete
Status: 08-04 loop closed — awaiting human verify
Last activity: 2026-05-13 — 08-04 PLAN ✓ APPLY ✓ UNIFY ✓

Progress:
- Milestone: [█████████░] 90%
- Phase 08: [██████████] 85%

## Loop Position

```
PLAN ──▶ APPLY ──▶ UNIFY
  ✓        ✓        ✓     [08-04 closed]
```

## Session Continuity

Last session: 2026-05-13
Stopped at: 08-04 unified
Next action: /paul:plan for 08-05 (Inter font + polish pass)
Resume file: .paul/phases/08-pro-ui/08-05-PLAN.md

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
