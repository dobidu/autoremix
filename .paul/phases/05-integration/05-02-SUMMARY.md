---
phase: 05-integration
plan: 02
type: summary
completed: 2026-05-09
commit: 4ea887b
---

# 05-02 Summary: CI + sidecar lifecycle

## What was built

- `.github/workflows/ci.yml` — GitHub Actions job `python-tests` triggers on push/PR to v2/main; installs uv, creates venv, installs requirements.txt, runs `.venv/bin/python -m pytest tests/ -v`
- `plugin/src/AudioBridge.cpp` — `startSidecar`: fork/exec python3 with script path, polls `isServerAlive()` every 250ms up to 10s, SIGKILL + reset on timeout. `stopSidecar`: SIGTERM, 8-poll waitpid loop (2s), SIGKILL fallback, resets `sidecar_pid_`
- `plugin/src/AutoRemixProcessor.cpp` — constructor reads `AUTOREMIX_SERVER_PATH` env var, calls `bridge_.startSidecar(path)` if set; destructor calls `bridge_.stopSidecar()`

## Acceptance criteria results

| AC | Result |
|----|--------|
| AC-1: CI runs pytest on push | ✓ yaml valid, triggers on v2/main push+PR |
| AC-2: startSidecar launches process | ✓ fork/exec + liveness poll implemented |
| AC-3: stopSidecar terminates cleanly | ✓ SIGTERM→SIGKILL, waitpid, pid reset |
| AC-4: PluginProcessor auto-starts sidecar | ✓ env var wired in constructor/destructor |

## Decisions

- Used `execl` then `execlp` fallback: covers `/usr/bin/python3` on Ubuntu CI and PATH-based python3 on macOS/other
- No Windows support (CreateProcess) — Linux/macOS only for MVP per scope limits
- No C++ build job in CI — JUCE + cmake + cpr fetch takes ~15 min uncached; pytest is the meaningful gate

## Deferred

- Windows sidecar lifecycle (CreateProcess/TerminateProcess)
- Bundled server path discovery (AUTOREMIX_SERVER_PATH env var is sufficient for MVP)
- C++ CI job (cmake + JUCE, too heavy without caching strategy)
