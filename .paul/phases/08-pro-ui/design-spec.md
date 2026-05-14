# AutoRemix Pro UI — Design Spec

Single source of truth for Phase 08. All implementation plans (08-02 through 08-05) execute from this document.

---

## Window

| Property | Value |
|----------|-------|
| Fixed size | 600 × 400 px |
| Resizable range | 600–900 × 400–600 |
| Aspect ratio | 1.5 : 1 (enforced via `setFixedAspectRatio`) |
| JUCE call | `setResizeLimits(600, 400, 900, 600)` + `getConstrainer()->setFixedAspectRatio(1.5)` |

---

## Zone Layout (at 600 × 400)

```
y=  0 ┌──────────────────────────────────────────────────┐
      │  HEADER BAR                              h = 40  │
      │  [AutoRemix 16px bold]  [StyleTabBar]  [● dot]   │
y= 40 ├──────────────────────────────────────────────────┤
      │                                                   │
      │  WAVEFORM ZONE                           h = 160  │
      │  bg = #1E1F29  AudioThumbnail (purple)            │
      │                                                   │
y=200 ├──────────┬───────────────────────────────────────┤
      │ TRANSPORT│  PARAMS ZONE              h = 128      │
      │  w = 96  │  4 × LinearHorizontal sliders         │
      │  Load    │  Tempo / Pitch / Reverb / Chop        │
      │  Play    │                                        │
      │  Save    │                                        │
y=328 ├──────────┴───────────────────────────────────────┤
      │  STATUS BAR                              h = 48  │
      │  [ProgressBar 8px]  "status text 12px"            │
      │  left-aligned, 16px inset                         │
y=376 └──────────────────────────────────────────────────┘
      (376 + 24px outer padding top/bottom → visual 400)
```

**Zone padding:** 16px horizontal, 12px vertical inset from zone boundary.

---

## Spacing Grid

| Token | Value | Use |
|-------|-------|-----|
| spacing-xs | 4 px | Internal label-to-control gap |
| spacing-sm | 8 px | Between sibling controls |
| spacing-md | 16 px | Between sections within a zone |
| spacing-lg | 24 px | Zone inset padding (horizontal) |
| spacing-xl | 32 px | Between major zones |

---

## Color Tokens

All defined in `AR::` namespace in `AutoRemixLookAndFeel.h`.

| Token | Hex | Role |
|-------|-----|------|
| `BG_DEEP` | `#1E1F29` | Waveform zone bg, deepest inset areas |
| `BG` | `#282A36` | Main window background |
| `ELEVATED` | `#343746` | Header bar bg, transport zone bg |
| `SURFACE` | `#44475A` | Button default bg, slider track, borders |
| `FG` | `#F8F8F2` | Primary text, value readouts |
| `COMMENT` | `#6272A4` | Section labels, units, placeholders |
| `GREEN` | `#50FA7B` | Play button (CTA), success state |
| `CYAN` | `#8BE9FD` | Secondary interactive highlight |
| `PURPLE` | `#BD93F9` | Waveform, active controls, focused state |
| `RED` | `#FF5555` | Error state, sidecar disconnected |
| `ORANGE` | `#FFB86C` | Processing/busy state, sidecar connecting |

**Rules:**
- Only the primary CTA (Play when file loaded) gets full `GREEN` fill
- Load / Save / inactive controls use `SURFACE` bg + `FG` text
- Hover = +10% lightness on bg; pressed = -10% + accent tint; disabled = 40% alpha
- Waveform: `PURPLE` channels + `rgba(189,147,249,0.15)` area fill under curve

---

## Typography Scale

Implemented via `AR::font(AR::FontRole role)`.

| Role | Size | Weight | Colour |
|------|------|--------|--------|
| `header` | 16 px | Bold | `FG` |
| `section` | 11 px | Bold | `COMMENT` |
| `label` | 11 px | Plain | `PURPLE` |
| `value` | 13 px | Plain | `FG` |
| `secondary` | 10 px | Plain | `COMMENT` |
| `status` | 12 px | Plain | `FG` (OK) / `RED` (error) / `ORANGE` (busy) |
| `button` | 12 px | Bold | `FG` (or `BG` on green CTA) |

Minimum legible size: 10 px. Font family: system sans-serif for now; Inter TTF embedding deferred to 08-05 polish pass.

---

## Component List

| Component | Type | Phase |
|-----------|------|-------|
| `AutoRemixLookAndFeel` | `juce::LookAndFeel_V4` subclass | 08-01 (expanded), 08-02 (slider override) |
| `WaveformDisplay` | Custom `juce::Component` | 08-04 (overhaul of current thumbnail code) |
| `StyleTabBar` | Custom `juce::Component` | 08-03 (replaces ComboBox) |
| `SidecarHealthDot` | Custom `juce::Component` | 08-04 |
| `ParamSlider` | Thin wrapper: `juce::Slider` + label + readout | 08-03 |

---

## Component Specs

### StyleTabBar
- 3 tabs: "Chop & Screw", "Slowed + Reverb", "Drum & Bass"
- Each tab: ~160 px wide × 28 px tall, 6 px corner radius
- Default: transparent bg, `COMMENT` text
- Selected: `ELEVATED` bg, 3 px `PURPLE` left border, `FG` text
- Fires `std::function<void(int)> onChange` callback with 0-based index
- Lives in header bar, centered

### ParamSlider
- Wraps `juce::Slider::LinearHorizontal`
- Track: 4 px tall, `SURFACE` colour; fill: `PURPLE`
- Thumb: 10 px circle, `FG`
- Label: 11 px above, left-aligned, `COMMENT`
- Value readout: 12 px, right-aligned, `FG`
- Total height per slider: ~36 px (label 14px + slider 22px)
- 4 sliders stacked in params zone with 8px gap

### Transport Buttons
- Size: 64 × 26 px, 6 px radius
- Stacked vertically with 8 px gap, 12 px top inset in transport zone
- Load: `SURFACE` bg, `FG` text (secondary)
- Play: `GREEN` bg, `BG` text (primary CTA)
- Save: `SURFACE` bg, `FG` text (secondary)

### SidecarHealthDot
- 8 px filled circle, right edge of header bar (16 px from right edge)
- Vertically centred in header
- `GREEN` = connected, `RED` = disconnected, `ORANGE` = connecting/polling
- `startTimer(2000)` — polls `AudioBridge::isServerAlive()` every 2s
- No label; tooltip on hover: "Sidecar: connected/disconnected"

### WaveformDisplay
- Fills waveform zone entirely
- Background: `BG_DEEP` (rounded top corners 4 px to blend with header border)
- Waveform: `AudioThumbnail::drawChannels()` in `PURPLE`
- Area fill: `juce::Path` under waveform curve, `PURPLE` at 15% alpha
- Empty state: "No file loaded" centred, `AR::font(FontRole::secondary)`, `COMMENT`
- Playhead: 1 px vertical line in `FG`, future feature — placeholder only
- `setBufferedToImage(true)` — only repaints when source changes

---

## LookAndFeel Override Map

| Component | Method overridden | Notes |
|-----------|------------------|-------|
| `juce::TextButton` | `drawButtonBackground`, `drawButtonText` | Rounded, correct surface colours |
| `juce::Slider` | `drawLinearSlider` | 4px track, 10px thumb |
| `juce::ComboBox` | — | Replaced by StyleTabBar; ComboBox removed in 08-03 |
| `juce::ProgressBar` | `drawProgressBar` | Two-tone gradient `PURPLE`→`CYAN` at 100% |
| `juce::Label` | — | Set via `setColour()` in constructor |
| `juce::PopupMenu` | — | Set via `setColour()` in constructor |

---

## Inter Font (Deferred — 08-05)

```cmake
juce_add_binary_data(AutoRemixBinaryData SOURCES
    assets/fonts/Inter-Regular.ttf
    assets/fonts/Inter-SemiBold.ttf
    assets/fonts/Inter-Bold.ttf
)
target_link_libraries(AutoRemix PRIVATE AutoRemixBinaryData)
```

LookAndFeel constructor loads via `juce::Typeface::createSystemTypefaceFor(BinaryData::Inter_Regular_ttf, ...)`. System sans-serif used until 08-05.
