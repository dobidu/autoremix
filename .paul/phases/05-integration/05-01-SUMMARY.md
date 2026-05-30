---
phase: 05-integration
plan: 01
type: summary
completed: 2026-05-09
commit: cb72a17
---

# 05-01 Summary: pytest suite + error hardening

## What was built

- `python/tests/conftest.py` — session-scoped synthetic WAV fixture (3s stereo 44100Hz, deterministic seed), function-scoped httpx ASGITransport client
- `python/tests/test_health.py` — verifies health endpoint status, engine count, separator count
- `python/tests/test_separate.py` — algorithmic separation produces 4 non-empty WAV stems; missing file → 400; unknown separator → 400
- `python/tests/test_remix.py` — parametrized over all 3 engine IDs (chopped_screwed, slowed_reverb, drum_and_bass); stems fixture shared across parametrize; unknown engine → 400
- `python/pytest.ini` — asyncio_mode=auto so all async tests run without decorator
- `python/server/main.py` — added `except HTTPException: raise` before broad `except Exception` in both `/separate` and `/remix` handlers; prevents HTTPException being swallowed and returning wrong status code

## Acceptance criteria results

| AC | Result |
|----|--------|
| AC-1: pytest passes green | ✓ 8/8 passed |
| AC-2: health endpoint | ✓ status=ok, 3 engines, 1 separator |
| AC-3: separation produces 4 WAVs | ✓ all 4 stems exist, size > 0 |
| AC-4: all 3 engines produce WAV | ✓ chopped_screwed, slowed_reverb, drum_and_bass |
| AC-5: error paths return 4xx | ✓ missing file → 400, unknown ids → 400 |

## Decisions

- Used `scope="session"` for test_wav (one WAV generation per run) and function scope for client + stems (simpler, avoids scope mismatch with async fixtures)
- librosa DeprecationWarnings from audioread/rawread are upstream noise — not suppressed (they resolve when audioread drops Python 3.12 deprecated stdlib)

## Deferred

- requirements-dev.txt not created (pytest deps already in requirements.txt)
- CI yaml (Phase 05-02)
