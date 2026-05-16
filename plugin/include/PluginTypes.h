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
};

// --- Processing result ---
struct ProcessResult {
    bool success = false;
    std::string error_message;
    std::filesystem::path output_path;
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

} // namespace autoremix
