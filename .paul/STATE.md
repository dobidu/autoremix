# PAUL State

## Current Position

Milestone: v3 Effect Chain DSL ✅ COMPLETE
Phase: 10 of 10 — ALL PLANS COMPLETE
Plan: 10-03 complete
Status: checkpoint — merge v2 → main, tag v2.2.0
Last activity: 2026-05-14 — 10-03 PLAN ✓ APPLY ✓

Progress:
- Milestone: v3 Effect Chain DSL [██████████] 100%
- Phase 10: [██████████] 100%

## Loop Position

```
PLAN ──▶ APPLY ──▶ UNIFY
  ✓        ✓        ✓     [10-03 closed]
```

## Session Continuity

Last session: 2026-05-14
Stopped at: Phase 10 closed — merged v2→main, tagged v2.2.0, pushed
Next action: start new milestone or deferred work (see Deferred below)

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
