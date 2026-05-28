---
phase: 27-wireup-release
plan: 02
subsystem: infra
tags: [cmake, ci, github-actions, cleanup, juce, cpr]

requires:
  - phase: 27-01
    provides: All AudioBridge call sites replaced; AudioBridge now dead code in plugin

provides:
  - AudioBridge.cpp/h deleted from repo
  - bridge_ member + getBridge() removed from PluginProcessor
  - Dead compile definitions removed (AUTOREMIX_SIDECAR_PORT, AUTOREMIX_SIDECAR_HOST)
  - Stale AudioBridge comments cleaned from PluginTypes.h
  - CI rewritten: C++ Release build smoke on push (no Python pytest)
  - Release workflow: sidecar stripped from all 3 platform zips; v4 release notes
  - feat(27-01-02) commit: 4b40bfc

affects: [27-03-regression, 27-04-release]

tech-stack:
  added: []
  patterns:
    - "cpr::cpr stays: ModelDownloader uses it for DOD — not AudioBridge-only"
    - "CI triggers on native branch (not just main)"

key-files:
  deleted:
    - plugin/src/AudioBridge.cpp
    - plugin/include/AudioBridge.h
  modified:
    - plugin/include/PluginProcessor.h
    - plugin/include/PluginTypes.h
    - plugin/CMakeLists.txt
    - .github/workflows/ci.yml
    - .github/workflows/release.yml

key-decisions:
  - "cpr::cpr retained: ModelDownloader.h uses cpr::Download for streaming htdemucs DOD — assumption in plan was wrong"
  - "AUTOREMIX_SIDECAR_PORT + AUTOREMIX_SIDECAR_HOST compile defs removed (dead sidecar config)"
  - "CI now triggers on native branch (was main-only)"

patterns-established:
  - "Release zips contain only plugin binaries + README + LICENSE — no runtime scripts"

duration: ~30min
started: 2026-05-28T13:30:00Z
completed: 2026-05-28T14:00:00Z
---

# Phase 27 Plan 02: AudioBridge Deletion + CI/Release Cleanup Summary

**AudioBridge.cpp/h deleted; PluginProcessor scrubbed; CI rewritten for C++ smoke build; release packaging stripped of sidecar; all 27-01+27-02 changes committed in 4b40bfc.**

## Performance

| Metric | Value |
|--------|-------|
| Duration | ~30 min |
| Completed | 2026-05-28 |
| Tasks | 2 completed |
| Files deleted | 3 (AudioBridge.cpp, AudioBridge.h, SidecarHealthDot.h via 27-01 commit) |
| Files modified | 5 |
| Commit | `4b40bfc` feat(27-01-02) |

## Acceptance Criteria Results

| Criterion | Status | Notes |
|-----------|--------|-------|
| AC-1: AudioBridge removed from plugin build | Pass (partial) | Files deleted, PluginProcessor clean, build green. cpr::cpr retained (see deviation) |
| AC-2: CI no longer references sidecar Python | Pass | 0 pytest/uv/sidecar hits in ci.yml; C++ Release build added |
| AC-3: Release workflow packages v4 artifacts only | Pass | 0 sidecar/launch.bat hits; release notes rewritten for single-binary v4 |
| AC-4: All 27-01+27-02 work committed | Pass | commit 4b40bfc, git status clean |

## Accomplishments

- `AudioBridge.cpp` and `AudioBridge.h` deleted — the v3 HTTP IPC layer is gone from the codebase
- `PluginProcessor.h` clean: no `bridge_` member, no `getBridge()`, no `#include "AudioBridge.h"`
- Dead `AUTOREMIX_SIDECAR_PORT=17432` + `AUTOREMIX_SIDECAR_HOST` compile defs removed
- CI now builds C++ Release on every push to `native` or `main` — catches regressions before they accumulate
- Release zips no longer bundle `sidecar/` folder or `launch.bat`/`launch.sh` — v4 is truly single-binary

## Task Commits

Both tasks bundled with 27-01 into a single atomic commit:

| Task | Commit | Description |
|------|--------|-------------|
| T1: Delete AudioBridge + clean build system | `4b40bfc` | Files deleted, CMakeLists + PluginProcessor cleaned |
| T2: CI + release workflow + commit | `4b40bfc` | ci.yml + release.yml rewritten; working tree clean |

## Files Created/Modified

| File | Change | Purpose |
|------|--------|---------|
| `plugin/src/AudioBridge.cpp` | Deleted | HTTP sidecar IPC — dead post-27-01 |
| `plugin/include/AudioBridge.h` | Deleted | AudioBridge class declaration |
| `plugin/include/PluginProcessor.h` | Modified | Remove bridge_ member, getBridge(), #include AudioBridge.h |
| `plugin/include/PluginTypes.h` | Modified | Remove stale "returned by AudioBridge::..." comments |
| `plugin/CMakeLists.txt` | Modified | Remove src/AudioBridge.cpp from sources; remove SIDECAR_PORT/HOST defs |
| `.github/workflows/ci.yml` | Modified | Replace Python pytest with C++ Release build smoke |
| `.github/workflows/release.yml` | Modified | Strip sidecar packaging; rewrite release notes for v4 |

## Decisions Made

| Decision | Rationale | Impact |
|----------|-----------|--------|
| Retain `cpr::cpr` dep | Plan incorrectly assumed cpr was AudioBridge-only; `ModelDownloader.h` uses `cpr::Download` for streaming htdemucs DOD | cpr still in link; full cpr removal deferred until ModelDownloader is rewritten (post-v4.0.0 or 27-03 if time allows) |
| Remove AUTOREMIX_SIDECAR_PORT/HOST defs | Dead config — no code references these after 27-01 | Clean CMakeLists; no functional impact |
| CI triggers on `native` + `main` | v4 work is on `native` branch; CI was only watching `main` | Regressions caught on native branch going forward |

## Deviations from Plan

### Summary

| Type | Count | Impact |
|------|-------|--------|
| Wrong assumption | 1 | cpr::cpr retained; noted for 27-03/post-release cleanup |
| Scope addition | 1 | PluginTypes.h stale comments cleaned (minor, no risk) |

**Total impact:** Minor — build is clean, AudioBridge is gone, cpr stays for legitimate reason.

### Auto-fixed Issues

**1. cpr::cpr is still used by ModelDownloader**
- Found during: Task 1 (build failed after cpr removal)
- Issue: Plan said cpr was "AudioBridge-only" — actually `ModelDownloader.h:25` includes `<cpr/cpr.h>` and uses `cpr::Download()` for the streaming DOD
- Fix: Restored `cpr::cpr` to `target_link_libraries`; documented deviation
- Files: `plugin/CMakeLists.txt`

**2. Stale `AudioBridge::` references in PluginTypes.h**
- Found during: Task 1 static grep (`grep -r "AudioBridge" plugin/`)
- Issue: 4 comments said "returned by AudioBridge::getX()" — accurate in v3, wrong in v4
- Fix: Stripped to just `// --- Preset metadata ---` etc.
- Files: `plugin/include/PluginTypes.h`

### Deferred Items

- **cpr removal**: Replace `cpr::Download` in `ModelDownloader.h` with JUCE's `juce::WebInputStream` or raw libcurl. Blocked on Linux: `JUCE_USE_CURL=0` disables JUCE HTTP on Linux, so direct replacement needs either `JUCE_USE_CURL=1` or a raw curl call. Low urgency — cpr is a clean dep, not a problem.

## Issues Encountered

| Issue | Resolution |
|-------|------------|
| Release build failed after cpr removal | ModelDownloader.h still includes cpr/cpr.h. Restored cpr::cpr to link; documented as deviation |

## Next Phase Readiness

**Ready:**
- `sidecar/` directory still on disk but no build or CI references it
- `plugin/` is clean: zero AudioBridge mentions in any source file
- CI gates the `native` branch on C++ Release build
- Release workflow produces v4-correct artifacts

**Concerns:**
- `cpr::cpr` still linked — users need libcurl on Linux to run the plugin (for model download). Already was the case; no regression.
- 27-03 regression test: need to verify all 6 screens + remix + mashup still work after the combined 27-01+27-02 changes. The Release build passes but no UI flow has been re-verified since the commit.

**Blockers:**
- None

---
*Phase: 27-wireup-release, Plan: 02*
*Completed: 2026-05-28*
