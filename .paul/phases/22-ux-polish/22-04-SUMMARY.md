---
phase: 22-ux-polish
plan: 04
subsystem: plugin-ui
tags: [juce, ux, copy, onboarding]

provides:
  - First-run orientation copy on empty drop zone (headline + subtitle + hint)

key-files:
  modified:
    - plugin/include/ScreenEmpty.h

key-decisions:
  - Single screen, 1-file quick-fix — copy + minor layout shift only.
  - Subtitle uses middle dot (·) U+00B7 as separator — safe in any font, no em-dash risk.

duration: ~5min
started: 2026-05-19
completed: 2026-05-19
---

# Phase 22 Plan 04 Summary

**Empty drop zone now greets new users with a 3-line orientation: bold headline, one-line app intro, action hint.**

## ACs

| AC | Status |
|----|--------|
| AC-1: 3-line drop-zone copy | Pass (user approved live) |
| AC-2: Build green | Pass |

## Changes

```
plugin/include/ScreenEmpty.h  ~+15 / -8 lines
```

Headline: "Drop a song here"
Subtitle: "Separate stems · Remix · Mashup two tracks"
Hint:     "or click Load File below"

## Next

`/paul:plan 22-05` — minor copy/visual (Anchor → Match A's key, Custom → Free mix, em-dash, render time hint, reveal-in-folder).
