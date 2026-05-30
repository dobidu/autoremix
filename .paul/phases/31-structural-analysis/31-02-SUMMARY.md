---
phase: 31-structural-analysis
plan: 02
type: summary
status: complete
commit: aeb0e9c
date: 2026-05-29
---

# Summary: 31-02 — Spectral centroid + vocal presence + section boundaries

## What was built

`NativeAnalysis.h` further extended:

- **`spectral_centroid_per_bar()`**: FFT centroid per bar window (frequency center of mass),
  averaged across STFT frames, normalized to [0, 1]. Inline IIR lambda avoids dependency
  on NativeRemixEngines.h (detail::apply_iir_filter not in scope).
- **`vocal_presence_per_bar()`**: IIR bandpass (1-4kHz HPF + LPF) ratio heuristic per bar.
  Ratio of band RMS to full RMS, normalized to [0, 1]. Short bars (<512 samples) → 0.5.
- **`section_boundaries()`**: simplified novelty function. |Δenergy| + |Δbrightness| per bar,
  3-point moving average smoothing, threshold = mean × 1.2, min_sep = 4 bars peak picking.
  No self-similarity matrix (O(N²) deferred).
- **`analyze_structure()`**: completed — all 10 fields now populated.

## Bugs fixed during implementation
- Redeclaration of `N` variable: removed stale `const int N = mono_buf.getNumSamples()` line
- `detail::apply_iir_filter` not in scope: replaced with inline lambda applying IIR per channel

## AC results
- AC-1: PASS — brightness_per_bar tracks spectral centroid, normalized
- AC-2: PASS — vocal_presence_per_bar detects vocal vs instrumental ratio heuristic
- AC-3: PASS — section_boundaries produces 2-12 boundaries on typical songs
