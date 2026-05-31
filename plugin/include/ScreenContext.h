#pragma once
#include <juce_core/juce_core.h>
#include <atomic>
#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>
#include "PluginTypes.h"

// ─────────────────────────────────────────────────────────────────────────────
// ScreenId — identifies which screen is currently displayed in the main area.
// ─────────────────────────────────────────────────────────────────────────────
enum class ScreenId {
    Empty,        // no file loaded; drag-drop zone
    Separating,   // stem separation in progress
    StemsReady,   // 4 stem rows with waveforms + mix controls
    ModeParams,   // preset + BPM/pitch/reverb/chop params
    Render,       // render progress + done state
    Mashup        // pairwise mashup: pick stems from A or B + target BPM/key
};

// ─────────────────────────────────────────────────────────────────────────────
// ScreenContext — single mutable state bag shared by all screens and the editor.
// The editor owns one instance; screens hold a reference.
// All fields are safe to read/write on the message thread only.
// ─────────────────────────────────────────────────────────────────────────────
struct ScreenContext {
    // ── Audio file state ──────────────────────────────────────────────────────
    juce::String           file_path;
    float                  detected_bpm  = 120.0f;
    juce::String           detected_key;
    autoremix::StemPaths   stems;         // populated after separation completes
    juce::String           output_path;   // populated after render completes

    // ── Registry data (populated from sidecar on connect) ─────────────────────
    std::vector<autoremix::PresetInfo>    presets;
    std::vector<autoremix::SeparatorInfo> separators;
    int selected_preset_idx    = 0;
    int selected_separator_idx = 0;

    // ── Per-stem mix state ────────────────────────────────────────────────────
    float vocals_gain  = 1.0f;
    float drums_gain   = 1.0f;
    float bass_gain    = 1.0f;
    float other_gain   = 1.0f;

    bool  vocals_muted = false;
    bool  drums_muted  = false;
    bool  bass_muted   = false;
    bool  other_muted  = false;

    bool  vocals_solo  = false;
    bool  drums_solo   = false;
    bool  bass_solo    = false;
    bool  other_solo   = false;

    // ── Remix parameters ──────────────────────────────────────────────────────
    float target_bpm    = 120.0f;
    float pitch_semi    = 0.0f;
    float reverb_mix    = 0.0f;
    float chop_beats    = 2.0f;
    int   chop_mode_idx = 0;

    // ── Navigation callback — set by editor before first use ──────────────────
    std::function<void(ScreenId)> navigate;

    // ── Status bar callback — set by editor ───────────────────────────────────
    std::function<void(const juce::String&)> set_status;

    // ── Audio preview callbacks — set by editor, wired to processor ───────────
    // Preview (original / remix) — exclusive: one file at a time
    std::function<void(const juce::File&)> play_preview;   // load + start
    std::function<void()>                  stop_preview;
    std::function<bool()>                  is_preview_playing;

    // Stem playback — 4 simultaneous streams, mixable
    std::function<void(int, const juce::File&)> play_stem;
    std::function<void(int)>                    stop_stem;
    std::function<bool(int)>                    is_stem_playing;
    std::function<void()>                       stop_all_stems;

    // Playback positions (0.0–1.0) — for cursor rendering
    std::function<double()>    get_preview_position;
    std::function<double(int)> get_stem_position;

    // Seek (ratio 0.0–1.0) — click-to-seek on waveforms
    std::function<void(int, double)> seek_stem;    // (stem_idx, ratio)
    std::function<void(double)>      seek_preview; // (ratio)

    // ── Track B + mashup state (populated by mashup flow; see Phase 21) ──────
    juce::String         file_path_b;
    float                detected_bpm_b = 120.0f;
    juce::String         detected_key_b;
    autoremix::StemPaths stems_b;
    bool                 mashup_mode_separating = false;  // ScreenSeparating reads this
    bool                 render_is_mashup       = false;  // ScreenRender reads this
    juce::String         mashup_output_path;
    float                mashup_target_bpm = 0.0f;
    juce::String         mashup_target_key;
    // Mashup template registry (Phase 21-05). populated by the editor at startup.
    std::vector<autoremix::MashupPresetInfo> mashup_presets;
    int   selected_mashup_preset_idx       = -1;     // -1 = Custom
    float mashup_bpm_modifier              = 1.0f;
    float mashup_master_pitch_offset_semi  = 0.0f;
    float mashup_master_reverb_mix         = 0.0f;
    float mashup_master_reverb_room        = 0.5f;
    float mashup_highpass_b_hz             = 0.0f;
    // Per-stem gain for each track (0.0 = silent, 1.0 = unity, 2.0 = +6 dB).
    // Defaults: track A vocals/other on, track B drums/bass on — vocal-over-instrumental.
    std::unordered_map<std::string, float> mashup_gains_a{
        {"vocals", 1.0f}, {"drums", 0.0f}, {"bass", 0.0f}, {"other", 1.0f}
    };
    std::unordered_map<std::string, float> mashup_gains_b{
        {"vocals", 0.0f}, {"drums", 1.0f}, {"bass", 1.0f}, {"other", 0.0f}
    };

    std::function<void(autoremix::MashupParams,
                       std::function<void(autoremix::MashupResult)>)> run_mashup;

    // Invoked by ScreenStemsReady to open the track-B file picker and
    // run the analyze+separate pipeline for the second track.
    std::function<void()> start_mashup_flow;

    // Cancellation token for in-progress Demucs separation.
    // ScreenSeparating creates and owns the atomic; sets this shared_ptr in onEnter,
    // clears it in onExit. AutoRemixEditor::separateNative reads it for the demucs path.
    std::shared_ptr<std::atomic<bool>> separation_cancel_token;
};
