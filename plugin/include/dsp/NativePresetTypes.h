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

// ── Structure-aware template types (Phase 31-03) ─────────────────────────────

// Per-section stem layer: which stems are active + optional effect ops.
struct SectionLayer {
    float vocals = 1.0f, drums = 1.0f, bass = 1.0f, other = 1.0f;
    std::vector<std::string> ops;   // effect_chain op ids to apply in this section
};

// One slot in the arrangement sequence.
struct ArrangementSlot {
    // Section type label (informational): "intro", "verse", "chorus", etc.
    std::string section_type;

    // Desired length in bars. 0 = auto-detect from section_boundaries.
    int length_bars = 0;

    // Source anchor — which part of the song to extract:
    // "first" | "second" | "last" | "energy_peak" | "bar:N"
    std::string source_anchor = "first";

    // Stem layer for this slot (overrides top-level stem_mix for this section)
    SectionLayer layer;

    // Per-slot effect ops (applied after layer gain)
    std::vector<std::string> effects;

    // Loop this slot N times before advancing to next slot
    int loop_count = 1;
};

// Top-level structure configuration embedded in NativeRemixPreset.
struct StructureConfig {
    bool enabled = false;

    // Ordered sequence of sections to assemble
    std::vector<ArrangementSlot> arrangement;

    // Scratch config (Phase 34 implementation; parsed here for schema completeness)
    struct ScratchConfig {
        bool        enabled       = false;
        std::string position      = "pre_drop"; // "bar:N" | "phrase:N" | anchor name
        std::string style         = "baby";     // baby|transformer|chirp|forward
        int         duration_bars = 1;
        std::string stem          = "mix";      // mix|vocals|drums|bass|other
    } scratch;

    // Build-up config (Phase 35 implementation)
    struct BuildUpConfig {
        bool enabled      = false;
        int  length_bars  = 4;
        bool add_riser    = true;
        bool filter_sweep = true;
    } build_up;

    // Sample specs (Phase 33-03): auto-place samples from user library
    struct SampleSpec {
        std::string path_or_auto; // explicit path OR empty (auto-select from library)
        std::string placement;    // "pre_drop"|"drop"|"vocal_gap"|"bar:N"|"phrase:N"|"end"
        std::string category;     // "riser"|"impact"|"loop"|"adlib"|"fx"|"one_shot"
        float       gain    = 1.0f;
        float       fade_ms = 10.0f;
    };
    std::vector<SampleSpec> samples;
};

// ─────────────────────────────────────────────────────────────────────────────

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

    // Structure-aware config (Phase 31-03). Optional — empty = legacy behaviour.
    StructureConfig structure;
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
