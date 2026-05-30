---
phase: 18-ux-redesign
plan: 04
status: complete
date: 2026-05-16
---

# 18-04 Summary — ScreenModeParams

## What was built
- `plugin/include/ScreenModeParams.h` (new file):
  - Preset ComboBox populated from ctx_.presets in onEnter()
  - Source BPM read-only label (ctx_.detected_bpm)
  - Target BPM slider (40–200, 0.5 step) → ctx_.target_bpm
  - Pitch slider (-12 to +12 semitones, 0.5 step) → ctx_.pitch_semi
  - Reverb slider (0–1, 0.01 step) → ctx_.reverb_mix
  - Chop Mode ComboBox (Fixed/Beat/Onset/Bar/Energy/Structural) → ctx_.chop_mode_idx
  - Chop Beats slider (0.25–8, 0.25 step) → ctx_.chop_beats
  - onPresetChanged(): syncs sliders from preset.default_params (tempo_factor × detected_bpm, pitch, reverb)
  - Back→StemsReady, Render→Render
  - Layout: reduced(32,0) with 72px action bar at bottom; SECTION_GAP=16 between rows

## Decisions
- BPM row uses split label (TARGET BPM left | Source: N.N BPM right) within same label height
- All sliders use LinearHorizontal + TextBoxRight 60×24
- onEnter() calls resized()+repaint() to sync values from ctx_ on screen entry

## Acceptance criteria
- [x] ScreenModeParams.h created
- [x] All sliders and combos wired to ctx_ fields
- [x] onPresetChanged() syncs from preset defaults
- [x] Build clean
