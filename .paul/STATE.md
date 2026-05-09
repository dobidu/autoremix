# PAUL State

## Current Position

Milestone: v2 MVP
Phase: 03 of 6 (Remix Engines) — COMPLETE
Plan: 03-01 unified
Status: Ready for Phase 04
Last activity: 2026-05-08 — Phase 03 loop closed

Progress:
- Milestone: [████░░░░░░] 65%
- Phase 03: [██████████] 100% ✓

## Loop Position

```
PLAN ──▶ APPLY ──▶ UNIFY
  ✓        ✓        ✓     [Phase 03 complete]
```

## Session Continuity

Last session: 2026-05-08
Stopped at: Phase 03 complete — all 3 engines produce valid WAV
Next action: /paul:plan (Phase 04 — Plugin UI)
Resume file: .paul/ROADMAP.md

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

## Blockers
none

## Deferred
- Real-time processing
- ML remix engines
- LV2 plugin format
- Spleeter/demucs ML separator (Phase 02.1 or 05)
- C++ AlgorithmicSeparator real impl (low priority; HTTP path works)
