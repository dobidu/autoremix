#pragma once
#include "NativeAnalysis.h"
#include <juce_core/juce_core.h>
#include <nlohmann/json.hpp>
#include <vector>

// ── NativeCueSidecar — Phase 34-03 ──────────────────────────────────────────
// Header-only. Persists user-placed CuePoints as a hidden JSON sidecar next to
// the source audio file. Only "user" source cues are saved.

using autoremix::dsp::analysis::CuePoint;

// Sidecar path: .{basename}.autoremix-cues.json next to source audio file
inline juce::File cue_sidecar_path(const juce::File& audio_file)
{
    return audio_file.getSiblingFile("." + audio_file.getFileName() + ".autoremix-cues.json");
}

inline void save_user_cues(const juce::File& audio_file,
                            const std::vector<CuePoint>& cues)
{
    nlohmann::json j = nlohmann::json::array();
    for (const auto& c : cues) {
        if (c.source != "user") continue;
        j.push_back({{"name",         c.name},
                     {"position_sec", c.position_sec},
                     {"color_rgb",    static_cast<uint32_t>(c.color_rgb)}});
    }
    cue_sidecar_path(audio_file).replaceWithText(j.dump(2));
}

inline std::vector<CuePoint> load_user_cues(const juce::File& audio_file)
{
    std::vector<CuePoint> result;
    const auto f = cue_sidecar_path(audio_file);
    if (!f.existsAsFile()) return result;
    try {
        auto j = nlohmann::json::parse(f.loadFileAsString().toStdString());
        for (const auto& item : j) {
            CuePoint c;
            c.name         = item.value("name", "cue");
            c.position_sec = item.value("position_sec", 0.0);
            c.color_rgb    = static_cast<uint32_t>(item.value("color_rgb",
                                 static_cast<uint32_t>(0xFFD4652Au)));
            c.source       = "user";
            result.push_back(std::move(c));
        }
    } catch (...) {
        juce::Logger::writeToLog("CueSidecar: failed to parse " + f.getFullPathName());
    }
    return result;
}
