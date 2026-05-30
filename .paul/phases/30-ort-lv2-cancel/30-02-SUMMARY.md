---
phase: 30-ort-lv2-cancel
plan: 02
type: summary
status: complete
commit: ecd0d6d
date: 2026-05-29
---

# Summary: 30-02 — LV2 format + CI packaging

## What was built

- `plugin/CMakeLists.txt`: `FORMATS VST3 AU LV2 Standalone`
- `LV2URI "https://github.com/dobidu/autoremix"` added — JUCE requires this
  (parameter is `LV2URI` not `LV2_URI`; `LV2_URI` was silently ignored)
- `.github/workflows/release.yml`: `AutoRemix.lv2` bundle included in
  Linux (CPU + GPU) and macOS (CPU + GPU) package steps — 4 additions

## Decisions
- LV2URI = `https://github.com/dobidu/autoremix` (matches repo canonical URL)
- LV2 not included in Windows artifacts (LV2 not used on Windows DAWs)
- Discovery: JUCE param name is `LV2URI` not `LV2_URI` — the underscore version
  was silently ignored, producing the JUCE default `/plugins/AutoRemix` which
  failed the `startsWithValidScheme` static assertion

## Acceptance criteria results
| AC | Result |
|----|--------|
| AC-1: LV2 bundle produced | PASS — `AutoRemix.lv2/` in build output (310 targets) |
| AC-2: LV2 in release artifacts | PASS — 4 package steps updated |
| AC-3: VST3/AU/Standalone unaffected | PASS |
