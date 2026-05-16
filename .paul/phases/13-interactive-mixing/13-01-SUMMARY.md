---
phase: 13-interactive-mixing
plan: 01
type: summary
status: complete
date: 2026-05-15
commit: 2e5cc30
---

# 13-01 SUMMARY — Stem Mix Sliders

## What Was Built

Four stem-mix gain sliders (Vocals / Drums / Bass / Other, range 0.0–2.0) in the
controls zone right column. Weights flow UI → C++ RemixParams → AudioBridge JSON body
→ Python `_apply_stem_weights()` pre-weighting before any engine runs.

## Acceptance Criteria Results

| AC | Result |
|----|--------|
| AC-1: Stem weights sent UI → sidecar | ✓ `applyRemix()` sends `stem_mix_override` dict |
| AC-2: Sidecar pre-weights before engine | ✓ `_apply_stem_weights()` in main.py; no engine/chain_interpreter changes |
| AC-3: Two-column layout, no overlap | ✓ Left col x=96 w=220 (params), right col x=328 w=256 (stems), file_lbl own row |

## Files Modified

| File | Change |
|------|--------|
| `plugin/include/PluginTypes.h` | +4 gain floats in RemixParams; +SeparatorInfo struct |
| `plugin/src/AudioBridge.cpp` | `stem_mix_override` in applyRemix JSON; `getAvailableSeparators()` |
| `plugin/include/PluginEditor.h` | 4 stem slider+label members; separator_combo_ |
| `plugin/src/AutoRemixEditor.cpp` | Two-column layout; stem sliders wired; gains in onClick_Play() |
| `python/server/models.py` | `stem_mix_override: Optional[dict[str, float]] = None` in RemixRequest |
| `python/server/main.py` | `_apply_stem_weights()` helper; applied in remix() route |

## Decisions Made

- Pre-weighting (scale stem WAV files before dispatch) chosen over per-engine changes — universal, zero engine code modification
- `stem_mix_override` always sent (even at 1.0); Python side: scale is no-op → harmless
- CYAN label colour distinguishes stem mix column from remix params (COMMENT colour)
- File label given dedicated thin row (y=203, h=14) to prevent overlap with slider rows starting y=218

## Deferred

Nothing deferred from this plan.

## Verification

- Build: clean, zero errors
- Tests: 27/27 pass (demucs skipped)
- `grep "stem_mix_override" python/server/main.py` → helper + route usage ✓
- `grep "stem_mix_override" plugin/src/AudioBridge.cpp` → JSON body entry ✓
- `grep "vocals_gain\|drums_gain" plugin/include/PluginTypes.h` → fields ✓
- `grep "/tmp/autoremix" plugin/src/AutoRemixEditor.cpp` → empty ✓
