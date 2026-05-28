---
phase: 27-wireup-release
plan: 04
subsystem: docs + release
tags: [readme, version-bump, git-tag, release]

requires:
  - phase: 27-03
    provides: Full regression pass confirmed; v4 native plugin cleared for release

provides:
  - README.md rewritten for v4 (no external runtime, native arch, 3-step Quick Start)
  - CMakeLists.txt: project VERSION 4.0.0
  - Commit 2412877 feat(27-04)
  - Annotated tag v4.0.0 pushed to origin
  - GitHub Release workflow triggered (3 platform zips)
  - Phase 27 complete — v4.0.0 shipped

affects: []

tech-stack:
  added: []
  patterns:
    - "grep -i python README.md → EXIT 1 (zero matches) is the release-readiness signal"

key-files:
  modified:
    - README.md
    - CMakeLists.txt
    - .paul/STATE.md
    - .paul/phases/27-wireup-release/27-03-PLAN.md
    - .paul/phases/27-wireup-release/27-03-SUMMARY.md
    - .paul/phases/27-wireup-release/27-04-PLAN.md

key-decisions:
  - "Removed word 'Python' from tagline and architecture text (not just install instructions) to make grep-based AC truly zero-match"
  - "Version bumped to 4.0.0 (was 2.4.0 in project() — JUCE auto-picks this up for plugin version)"

duration: ~25 min
started: 2026-05-28T15:00:00Z
completed: 2026-05-28T15:25:00Z
---

# Phase 27 Plan 04: README v4 + Version Bump + Release Summary

**README rewritten for v4 (zero sidecar references). VERSION 4.0.0. Tag v4.0.0 pushed. GitHub Release workflow triggered. Phase 27 and v12 milestone complete.**

## Performance

| Metric | Value |
|--------|-------|
| Duration | ~25 min |
| Completed | 2026-05-28 |
| Tasks | 3 of 3 PASS |
| Files modified | 2 (README.md, CMakeLists.txt) |
| Commit | `2412877` feat(27-04) |
| Tag | `v4.0.0` (annotated, pushed) |

## Acceptance Criteria Results

| Criterion | Status | Notes |
|-----------|--------|-------|
| AC-1: README zero Python/sidecar refs | PASS | grep EXIT 1 — zero matches |
| AC-2: Correct v4 install flow | PASS | 3-step Quick Start; Demucs DOD section |
| AC-3: Native architecture diagram | PASS | ASCII diagram with all native components |
| AC-4: Version 4.0.0 in CMakeLists.txt | PASS | `project(AutoRemix VERSION 4.0.0)` |
| AC-5: Commit + tag | PASS | 2412877 + v4.0.0 pushed to origin |

## README Changes Summary

| Section | Change |
|---------|--------|
| Tagline | "no Python required" → "no external runtime required" |
| Quick Start | 3 steps: clone → cmake → run (sidecar steps removed) |
| Prerequisites | Python 3.12, uv removed from all 3 platforms |
| Separator description | "uv pip install demucs" → native DOD description |
| Step 2 description | "Python sidecar" → "native separator" |
| Architecture | Full replacement: native C++ diagram, no HTTP IPC box |
| Python Sidecar section | Deleted entirely |
| Running Tests section | Deleted entirely |
| Custom Presets | "restart the sidecar" → "loaded at startup" |
| Known Limitations | Remove AudioBridge timeout; update Demucs entry |

## Deviations from Plan

### Minor: "Python" word in tagline required two-pass fix
- Initial write: "no Python required" in tagline + "No Python, no HTTP IPC" in architecture
- Grep found 2 hits (both benign but violated strict AC-1)
- Fix: "no external runtime required" + "No HTTP IPC, no child processes"
- Net effect: AC-1 now zero-match; phrasing is actually cleaner

## Issues Encountered

None.

## Release Status

```
git push origin native     → OK (2 commits ahead: 4b40bfc, 2412877)
git push origin v4.0.0     → OK ([new tag] v4.0.0 -> v4.0.0)
```

GitHub Actions release.yml triggered on v4.0.0 tag:
- `build-linux`  → ubuntu-22.04 → AutoRemix-linux-v4.0.0.zip
- `build-macos`  → macos-14     → AutoRemix-macos-v4.0.0.zip
- `build-windows` → windows-latest → AutoRemix-windows-v4.0.0.zip
- `publish-release` → GitHub Release with all 3 zips + v4 release notes

---
*Phase: 27-wireup-release, Plan: 04*
*Completed: 2026-05-28*
