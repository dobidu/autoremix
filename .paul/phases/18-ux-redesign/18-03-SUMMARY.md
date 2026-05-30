---
phase: 18-ux-redesign
plan: 03
status: complete
date: 2026-05-16
---

# 18-03 Summary — ScreenSeparating + ScreenStemsReady

## What was built
- `plugin/include/ScreenSeparating.h` (new file):
  - juce::Timer (1s tick for elapsed counter)
  - 4 StemRow structs: name, color, status (Waiting/Separating/Done/Failed)
  - `startSeparation()`: selects separator_id from ctx_.separators, calls separate_fn_ on background thread
  - On success: ctx_.stems = result, navigates to StemsReady; on cancel: navigates to Empty
  - Stem temp paths via juce::File::tempDirectory/"autoremix"/"stems"
  - cancel_requested_ atomic flag; Cancel button stops timer and navigates back

- `plugin/include/ScreenStemsReady.h` (new file):
  - ChangeListener for 4 AudioThumbnails (vocals/drums/bass/other)
  - `std::array<StemRowWidgets, 4>`: JUCE_DECLARE_NON_COPYABLE + default ctor, configured in initRows()
  - paintRowBackground(): colored dot, stem name, waveform, unity tick on gain slider track
  - layoutRow(): right column = [SLIDER_W=220] [gap] [S=24] [gap] [M=24]
  - M button: toggles ctx_.*_muted; S button: solos one stem (mutes all others, clears other solos)
  - Gain slider 0–2, writes to ctx_.*_gain; no textbox
  - Back→Empty, Choose Style→ModeParams

## Decisions
- std::array with non-copyable JUCE members: must add JUCE_DECLARE_NON_COPYABLE + StemRowWidgets() = default; configure in initRows() not aggregate-init
- LEFT_COL_W=156, RIGHT_COL_W=280, ROW_H=56

## Acceptance criteria
- [x] ScreenSeparating.h created with timer, 4-stem status display, background separation thread
- [x] ScreenStemsReady.h created with per-stem M/S/gain controls and waveforms
- [x] Build clean
