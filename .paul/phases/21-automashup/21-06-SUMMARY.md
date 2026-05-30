---
phase: 21-automashup
plan: 06
subsystem: docs-release
tags: [docs, readme, claude-md, release, version-bump, github-actions]

requires:
  - phase: 21-automashup
    provides: working mashup feature end-to-end (21-01..21-05)
provides:
  - README Step 6 Mashup tutorial section
  - CLAUDE.md refreshed architecture + key types + IPC list
  - PROJECT.md success criterion for mashup
  - Sidecar version 0.4.0 → 0.5.0
  - ROADMAP Phase 21 marked COMPLETE
  - git tag v3.1.0 pushed → GitHub Release with 3 platform zips
  - CI fix: install requirements-dev.txt so pytest is available
affects: [next phase planning]

tech-stack:
  added: []
  patterns: []

key-files:
  created:
    - .paul/phases/21-automashup/21-06-PLAN.md
    - .paul/phases/21-automashup/21-06-SUMMARY.md
  modified:
    - README.md
    - CLAUDE.md
    - .paul/PROJECT.md
    - .paul/ROADMAP.md
    - .paul/STATE.md
    - sidecar/server/main.py
    - sidecar/server/models.py
    - .github/workflows/ci.yml

key-decisions:
  - "README Step 6 uses an ASCII mockup of ScreenMashup with the TEMPLATE combo, two-column 8-stem mixer, and target controls — matches the style of the other 5 steps."
  - "CLAUDE.md got a cleanup beyond mashup: python/ → sidecar/ path, removed stale Spleeter mention, listed all 7 IPC endpoints."
  - "Sidecar version bumped in two places: FastAPI app constructor + HealthResponse default (both stale at 0.4.0)."
  - "CI required a fix: requirements.txt has no pytest; requirements-dev.txt does. Added a second uv pip install line in ci.yml."

patterns-established:
  - "When bumping sidecar version, check BOTH FastAPI() constructor AND HealthResponse Pydantic default — they had drifted in the past."

duration: ~20min (incl. CI fix)
started: 2026-05-18
completed: 2026-05-18
---

# Phase 21 Plan 06: Docs + Release Summary

**Phase 21 closed. README has a full mashup tutorial, CLAUDE.md + PROJECT.md reflect the new feature, sidecar version bumped to 0.5.0, ROADMAP Phase 21 ✅ COMPLETE, v3.1.0 tagged + pushed, GitHub Release published with Linux/macOS/Windows binaries, CI fixed and green.**

## Performance

| Metric | Value |
|--------|-------|
| Duration | ~20min (incl. CI fix) |
| Tasks | 4/4 + checkpoint approved |
| Files modified | 7 |
| Files created | 2 (this SUMMARY + PLAN) |
| Commits | 3 (docs + release, CI fix, this) |
| Tags | v3.1.0 pushed |
| CI | green after fix |
| Release | live at github.com/dobidu/autoremix/releases/tag/v3.1.0 |

## Acceptance Criteria Results

| Criterion | Status | Notes |
|-----------|--------|-------|
| AC-1: README mashup section | Pass | Step 6 with ASCII, templates table, feel-knobs table |
| AC-2: CLAUDE.md mashup architecture | Pass | mashup_presets/ + ScreenMashup + MashupEngine listed |
| AC-3: PROJECT.md success criterion | Pass | new [x] entry under Phase 14 |
| AC-4: Sidecar v0.5.0 | Pass | both FastAPI constructor + HealthResponse default |
| AC-5: ROADMAP Phase 21 closed | Pass | ✅ COMPLETE (2026-05-18); all 6 sub-plans ticked |
| AC-6: v3.1.0 tag + release | Pass | tag pushed; GitHub Actions Release workflow green |

## Accomplishments

- Phase 21 closed cleanly with full documentation pass.
- Three platform binaries available to non-builders via the GitHub Release.
- CLAUDE.md refresh fixed unrelated technical drift (python/ → sidecar/ path) as a side benefit.
- CI bug surfaced + fixed before users could hit it: requirements-dev.txt now installed in CI.
- All sub-plans of Phase 21 visible in STATE loop position with ✓ ✓ ✓.

## Deviations from Plan

- One extra commit beyond the plan: `fix(ci): install requirements-dev.txt`. Plan assumed CI would pass on the docs commit — it didn't, because the test job lacked pytest. Diagnosed + fixed in <5min.
- CLAUDE.md refresh exceeded plan scope slightly (cleanup of stale paths + Spleeter mention). Caveman judgment: docs that are getting touched anyway should be left clean.

## Open Items / Deferred

- **Screenshots**: docs/screenshots/ still has only `.gitkeep`. Plan flagged it as a manual task for the user — capture and commit when convenient.
- **CHANGELOG.md**: still not maintained; release notes come from softprops/action-gh-release's tag-based body template. Acceptable per plan's scope limits.

## Files Changed

```
README.md                              +95 lines (Step 6 section + 6-screen architecture)
CLAUDE.md                              +18 lines (refresh + mashup additions)
.paul/PROJECT.md                        +2 lines (mashup criterion + timestamp)
.paul/ROADMAP.md                       changed (Phase 21 status block rewritten)
.paul/STATE.md                          +1 line (21-06 row)
sidecar/server/main.py                  +0/-0 (1 char: 4 → 5)
sidecar/server/models.py                +0/-0 (1 char: 4 → 5)
.github/workflows/ci.yml                +1 line (uv pip install requirements-dev.txt)
```

## Verification

- `grep -c "Mashup\|mashup" README.md` → 25+ matches
- `grep -c "mashup\|Mashup" CLAUDE.md` → 5+ matches
- `grep "version=\"0.5.0\"" sidecar/server/main.py` → matches
- `git tag -l v3.1.0` → v3.1.0
- `git ls-remote --tags origin v3.1.0` → matches
- GitHub Actions Release workflow: all platforms green (per user report)
- CI workflow: green (after `5f33dd6`)

## Phase 21 Wrap-Up

Phase 21 — AutoMashup — shipped end-to-end across 6 sub-plans:

| Sub-plan | Deliverable |
|----------|-------------|
| 21-01 | Sidecar: MashupEngine + endpoint + semitone_delta + 7 tests |
| 21-02 | C++ glue: MashupParams/Result + AudioBridge::mashup + ScreenContext |
| 21-03 | ScreenMashup UI: two-column 8-stem mixer |
| 21-04 | Entry point: [Mashup >] button + 2nd-file flow + redesign |
| 21-05 | Mashup templates: 8 built-ins + 5 feel knobs + Advanced ▾ |
| 21-06 | Docs + release v3.1.0 |

Test count: 54 → 65 (+11 mashup-related)
Release: v3.0.2 → v3.1.0
LOC delta: ~3300 added across plugin + sidecar + tests + docs

## Next

No outstanding Phase 21 work. Open to start Phase 22 with new feature
ideas, polish work, or pause for user testing on the v3.1.0 release.
