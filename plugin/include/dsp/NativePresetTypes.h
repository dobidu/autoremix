#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// NativePresetTypes — Phase 25-02
//
// Header-only definitions of NativeRemixPreset + NativeMashupPreset.
// Faithfully mirrors v3 RemixPreset / MashupPreset Pydantic models from
// sidecar/server/models.py. Consumed by NativePresetLoaders.
// ─────────────────────────────────────────────────────────────────────────────

#include <string>
#include <unordered_map>
#include <vector>

#include "NativeRemixEngines.h"        // engines::RemixParams
#include "NativeEffectChainEngine.h"   // engines::StemMix, engines::EffectStep

namespace autoremix::dsp::presets {

struct NativeRemixPreset {
    std::string id;
    std::string version     = "1.0";
    std::string name;
    std::string description;
    std::string author      = "AutoRemix";
    std::vector<std::string> tags;
    std::string engine;                       // "chopped_screwed" | "slowed_reverb"
                                              // | "drum_and_bass" | "effect_chain"
    engines::RemixParams              params;
    engines::StemMix                  stem_mix;
    std::vector<engines::EffectStep>  effects;
};

struct NativeMashupPreset {
    std::string id;
    std::string version     = "1.0";
    std::string name;
    std::string description;
    std::string author      = "AutoRemix";
    std::vector<std::string> tags;

    // Per-stem gains keyed by {"vocals","drums","bass","other"}.
    std::unordered_map<std::string, float> stem_gains_a;
    std::unordered_map<std::string, float> stem_gains_b;

    // BPM target: "anchor_a" | "anchor_b" | "average" | "absolute"
    std::string target_bpm_mode      = "anchor_a";
    double      target_bpm_absolute  = 0.0;          // honored only if mode == "absolute"

    // Key target: "anchor_a" | "anchor_b" | "absolute"
    std::string target_key_mode      = "anchor_a";
    std::string target_key_absolute;                 // e.g. "C minor"

    // 5 feel knobs
    float bpm_modifier               = 1.0f;
    float master_pitch_offset_semi   = 0.0f;
    float master_reverb_mix          = 0.0f;
    float master_reverb_room         = 0.5f;
    float highpass_b_hz              = 0.0f;
};

} // namespace autoremix::dsp::presets
