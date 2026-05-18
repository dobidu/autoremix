#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <string>
#include <filesystem>
#include <unordered_map>

namespace autoremix {

// --- Stem collection: paths to separated WAV files ---
struct StemPaths {
    std::filesystem::path vocals;
    std::filesystem::path drums;
    std::filesystem::path bass;
    std::filesystem::path other;
    bool valid = false;
};

// --- Remix parameters ---
struct RemixParams {
    float tempo_factor       = 1.0f;   // e.g. 0.7 = slowed 30%
    float pitch_shift_semi   = 0.0f;   // semitones, e.g. -4.0
    float reverb_mix         = 0.0f;   // 0.0–1.0
    float chop_interval_ms   = 0.0f;   // 0 = no chop
    float bass_boost_db      = 0.0f;
    float drums_tempo_factor = 1.0f;   // independent drum channel tempo
    std::string engine_id;             // "chopped_screwed" | "slowed_reverb" | "drum_and_bass"
    std::string separator_id;          // "spleeter" | "algorithmic"
    float vocals_gain = 1.0f;
    float drums_gain  = 1.0f;
    float bass_gain   = 1.0f;
    float other_gain  = 1.0f;
    std::string chop_mode = "fixed";   // "fixed"|"beat"|"onset"|"bar"|"energy"|"structural"
};

// --- Processing result ---
struct ProcessResult {
    bool success = false;
    std::string error_message;
    std::filesystem::path output_path;
};

// --- Mashup parameters (pairwise mashup: combine track A and track B) ---
struct MashupParams {
    std::filesystem::path file_a;
    std::filesystem::path file_b;
    std::string separator_id = "algorithmic";
    // Per-stem gain for each track. Keys: "vocals", "drums", "bass", "other".
    // 0.0 = silent, 1.0 = unity, 2.0 = +6 dB. Missing keys default to 1.0.
    std::unordered_map<std::string, float> stem_gains_a;
    std::unordered_map<std::string, float> stem_gains_b;
    float       target_bpm     = 0.0f;
    bool        has_target_bpm = false;
    std::string target_key;            // empty = anchor to A
    // Feel knobs (Phase 21-05). Defaults preserve pre-21-05 behavior.
    float bpm_modifier             = 1.0f;
    float master_pitch_offset_semi = 0.0f;
    float master_reverb_mix        = 0.0f;
    float master_reverb_room       = 0.5f;
    float highpass_b_hz            = 0.0f;
};

// --- Mashup preset metadata returned by AudioBridge::getMashupPresets() ---
struct MashupPresetInfo {
    std::string id;
    std::string name;
    std::string description;
    std::unordered_map<std::string, float> stem_gains_a;
    std::unordered_map<std::string, float> stem_gains_b;
    std::string target_bpm_mode      = "anchor_a";
    float       target_bpm_absolute  = 0.0f;     // 0 = unset
    std::string target_key_mode      = "anchor_a";
    std::string target_key_absolute;
    float       bpm_modifier             = 1.0f;
    float       master_pitch_offset_semi = 0.0f;
    float       master_reverb_mix        = 0.0f;
    float       master_reverb_room       = 0.5f;
    float       highpass_b_hz            = 0.0f;
};

// --- Mashup result ---
struct MashupResult {
    bool        success = false;
    std::string error_message;
    std::filesystem::path output_path;
    float       target_bpm = 0.0f;
    std::string target_key;
    float       length_sec = 0.0f;
};

// --- Preset metadata returned by AudioBridge::getPresets() ---
struct PresetInfo {
    std::string id;
    std::string name;
    RemixParams default_params;
};

// --- Separator metadata returned by AudioBridge::getAvailableSeparators() ---
struct SeparatorInfo {
    std::string id;           // "algorithmic", "demucs"
    std::string display_name; // "Algorithmic FFT", "Demucs (ML)"
};

// --- Audio file analysis result from AudioBridge::analyzeFile() ---
struct FileAnalysis {
    float       bpm          = 0.0f;
    std::string key;
    float       duration_sec = 0.0f;
    bool        valid() const { return bpm > 0.0f; }
};

} // namespace autoremix
