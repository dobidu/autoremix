# PAUL State

## Loop Position
PHASE: 00-setup
STATUS: NOT_STARTED
PLAN: none

## Session State
Last action: project initialized
Next action: /paul:plan 00-setup

## Decisions
- Python sidecar port: 17432 (avoid conflicts)
- Temp dir: /tmp/autoremix/
- Stem format: WAV 44100 Hz stereo (intermediário), final format tracks input
- IPC: HTTP+JSON (not gRPC) to keep it simple for MVP
- Default separator: SpleeterSeparator (4stems model)
- Default engine: ChoppedAndScrewedEngine

## Blockers
none

## Deferred
- Real-time processing
- ML remix engines
- LV2 plugin format
