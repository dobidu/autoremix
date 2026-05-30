---
phase: 31-structural-analysis
plan: 01
type: summary
status: complete
commit: 0076e9d
date: 2026-05-29
---

# Summary: 31-01 — SongStructure + energy map + phrase grid

## What was built

`NativeAnalysis.h` extended with:

- **`SongStructure` struct**: `bpm`, `beat_times`, `bar_times`, `phrase_times`,
  `energy_per_bar`, `energy_peaks`, `key`, `brightness_per_bar` (empty placeholder),
  `vocal_presence_per_bar` (empty placeholder), `section_boundaries` (empty placeholder),
  `duration_seconds`, `num_bars`. Helpers: `bar_index_at()`, `bar_time_at()`.
- **`energy_per_bar()`**: RMS per bar window normalized to [0, 1]
- **`energy_peaks()`**: local maxima with `min_sep_bars` distance enforcement
- **`phrase_times()`**: every N bars (default 4)
- **`analyze_structure()`**: partial entry point — populates rhythm + energy fields;
  `brightness_per_bar`, `vocal_presence_per_bar`, `section_boundaries` left empty (31-02)

## AC results
- AC-1: PASS — all rhythm + energy fields populated from analyze_structure()
- AC-2: PASS — energy_peaks() detects local maxima with min 4-bar separation
- AC-3: PASS — phrase_times() aligns to bar boundaries at phrase_bars stride
