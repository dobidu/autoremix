# PAUL State

## Current Position

Milestone: v6 Interactive Mixing — COMPLETE ✅
Phase: 13 (Interactive Mixing) — ALL 3 PLANS COMPLETE
Plan: 13-03 complete
Status: Phase 13 complete — ready to merge v2 → main
Last activity: 2026-05-16 — Phase 13 transition complete

Progress:
- Milestone: v6 Interactive Mixing [██████████] 100%
- Phase 13: [██████████] 100% (3/3 plans)

## Loop Position

```
PLAN ──▶ APPLY ──▶ UNIFY
  ✓        ✓        ✓     [Phase 13 complete — milestone done]
```

## Session Continuity

Last session: 2026-05-16
Stopped at: Phase 13 UNIFY + transition complete
Next action: merge v2 → main (git checkout main && git merge v2 --no-ff)

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
