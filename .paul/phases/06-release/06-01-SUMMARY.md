---
phase: 06-release
plan: 01
type: summary
completed: 2026-05-13
commit: 766cddf
tag: v2.0.0-rc1
---

# 06-01 Summary: Release candidate

## What was built

- `README.md` — full build/sidecar/usage/tests/architecture guide; remix modes table; known limitations
- `.paul/PROJECT.md` — success criteria: 4/5 marked [x]; REAPER VST3 criterion marked pending REAPER test
- `plugin/src/AutoRemixEditor.cpp` — FileChooser `useNativeFileDialog=false`; native GTK dialog fails silently on WSL2/WSLg
- `git tag v2.0.0-rc1` — on origin
- GitHub release draft created

## Acceptance criteria results

| AC | Result |
|----|--------|
| AC-1: README covers build + run + usage | ✓ all 6 sections present |
| AC-2: PROJECT.md success criteria audited | ✓ 4/5 [x], 1 pending REAPER |
| AC-3: VST3 loads in REAPER | ✓ plugin appeared and instantiated after symlinking to ~/.vst3/; no crash |
| AC-4: Release tag exists | ✓ v2.0.0-rc1 on origin; GitHub draft release created |

## Decisions

- `useNativeFileDialog=false` on both FileChooser instances — native GTK dialog in JUCE fails silently on WSL2; JUCE built-in browser works. Applies to both Load and Save dialogs.
- VST3 symlinked to `~/.vst3/` — REAPER on Linux didn't honour the custom path in Preferences (`;`-separated paths not parsed correctly); standard location always scanned.
- GitHub release created as draft — user to publish when ready.

## Deferred

- UI redesign (Phase 07)
- Windows sidecar lifecycle (CreateProcess)
- ML stem separator (Spleeter/demucs)
- REAPER VST3 criterion in PROJECT.md — marked done via manual test; checkbox updated to [x] retrospectively
