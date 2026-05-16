# Phase 20-01 — Audio Playback — SUMMARY

## Status: COMPLETE (2026-05-16)

## What Was Built

Full in-plugin audio playback across two screens:

**ScreenStemsReady** — per-stem play buttons:
- 4 `TextButton` play toggles, one per stem row
- `MixerAudioSource` in processor mixes all 4 stem transports simultaneously
- 100ms `juce::Timer` polls `is_stem_playing(i)` → updates ▶/■ per row
- `onExit()` stops timer + all stems

**ScreenRender Done state** — dual exclusive play buttons:
- `▶ Original` plays `ctx_.file_path` via `play_preview`
- `▶ Remix` plays `ctx_.output_path` via `play_preview`
- `PreviewMode` enum (None/Original/Remix) tracks active stream
- Timer multiplexed: 1000ms during Rendering, 100ms during Done
- `onExit()` stops preview

## Acceptance Criteria

- [x] Per-stem play/stop toggle buttons in ScreenStemsReady
- [x] ▶ Original / ▶ Remix in ScreenRender Done state
- [x] ▶/■ text synced via Timer polling (handles natural EOF)
- [x] Exiting stops all active playback
- [x] No OS player launched anywhere

## Files Modified

- `plugin/include/PluginProcessor.h` — StemPlayer struct, MixerAudioSource, playStem/stopStem/isStemPlaying/stopAllStems
- `plugin/src/AutoRemixProcessor.cpp` — full multi-source mixer implementation
- `plugin/include/ScreenContext.h` — 7 audio callbacks
- `plugin/src/AutoRemixEditor.cpp` — wires all 7 callbacks to processor
- `plugin/include/ScreenStemsReady.h` — play_btn per row, juce::Timer, handlePlay()
- `plugin/include/ScreenRender.h` — original_btn_ + remix_btn_, PreviewMode, multiplexed Timer

## Decisions

- `StemPlayer` needs `StemPlayer() = default` for C-array DefaultConstructible (+ JUCE_DECLARE_NON_COPYABLE handles non-copy)
- `MixerAudioSource` initialized in `prepareToPlay`, all inputs removed in destructor/releaseResources
- `processBlock` guards on `anyPlaying` before calling mixer (avoids silent buffer fill overhead)
- Exclusive preview: one `AudioTransportSource` shared between Original and Remix; stop before switching
- 100ms Timer acceptable polling interval for ▶/■ sync — imperceptible to user

## Build

Clean: 4/4 targets (Standalone + VST3 + static lib + shared code). Warnings only (pre-existing).
