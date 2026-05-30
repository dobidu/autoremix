#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// NativeStructuredRemixEngine — Phase 31-04
//
// Skeleton of the structure-aware remix engine. Uses analyze_structure() to
// map StructureConfig arrangement slots to concrete bar regions, then extracts
// and assembles them into an output buffer.
//
// Stubs (implemented in later phases):
//   - Scratch          → Phase 34
//   - Build-up/riser   → Phase 35
//   - Per-section effects chains → Phase 35
//   - Per-stem layer application → Phase 35 (simplified gain only for now)
//
// Fallback: if structure.enabled == false, delegates to chopped_and_screwed().
// ─────────────────────────────────────────────────────────────────────────────

#include <algorithm>
#include <cmath>
#include <string>

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>

#include "NativeAnalysis.h"
#include "NativePresetTypes.h"
#include "NativeRemixEngines.h"     // chopped_and_screwed, mix_stems_equal, TimePitchStretcher
#include "TimePitchStretcher.h"

namespace autoremix::dsp::engines {

namespace detail_structured {

// Resolve an anchor string to a bar index within the SongStructure.
inline int
resolve_anchor(const std::string& anchor,
               const analysis::SongStructure& s,
               int /*slot_idx*/)
{
    if (s.num_bars <= 0) return 0;

    if (anchor == "first")       return 0;
    if (anchor == "second")      return std::max(0, s.num_bars / 4);
    if (anchor == "last")        return std::max(0, s.num_bars - 1);
    if (anchor == "energy_peak") {
        if (!s.energy_peaks.empty())
            return std::min(s.energy_peaks[0], s.num_bars - 1);
        return s.num_bars / 2;
    }
    if (anchor.substr(0, 4) == "bar:") {
        try { return std::min(std::stoi(anchor.substr(4)), s.num_bars - 1); }
        catch (...) {}
    }
    return 0;
}

// Extract a bar range [start_bar, start_bar + length_bars) from src.
inline juce::AudioBuffer<float>
extract_bars(const juce::AudioBuffer<float>& src, double sr,
             const analysis::SongStructure& s,
             int start_bar, int length_bars)
{
    const int N  = src.getNumSamples();
    const int ch = src.getNumChannels();
    if (N == 0 || ch == 0 || s.bar_times.empty()) return src;

    start_bar  = std::max(0, std::min(start_bar,  s.num_bars - 1));
    length_bars = std::max(1, length_bars);
    const int end_bar = std::min(start_bar + length_bars, s.num_bars);

    const int samp_start = (int) std::round(s.bar_times[(size_t) start_bar] * sr);
    const int samp_end   = (end_bar < s.num_bars)
        ? (int) std::round(s.bar_times[(size_t) end_bar] * sr)
        : N;

    const int len = std::max(0, std::min(samp_end, N) - samp_start);
    if (len <= 0) return juce::AudioBuffer<float>(ch, 0);

    juce::AudioBuffer<float> out(ch, len);
    for (int c = 0; c < ch; ++c)
        out.copyFrom(c, 0, src, c, samp_start, len);
    return out;
}

// Apply simplified section layer gain to a mixed buffer.
// Full per-stem application deferred to Phase 35.
inline void
apply_section_layer(juce::AudioBuffer<float>& buf,
                    const presets::SectionLayer& layer)
{
    const float gain = (layer.vocals + layer.drums + layer.bass + layer.other) * 0.25f;
    if (std::abs(gain - 1.0f) > 1e-3f)
        buf.applyGain(gain);
}

// Append src to dst (concatenate along time axis).
inline void
append_buffer(juce::AudioBuffer<float>& dst,
              const juce::AudioBuffer<float>& src)
{
    if (src.getNumSamples() == 0) return;
    const int oldLen = dst.getNumSamples();
    const int addLen = src.getNumSamples();
    const int ch     = std::min(dst.getNumChannels(), src.getNumChannels());

    dst.setSize(dst.getNumChannels(), oldLen + addLen, true, true, false);
    for (int c = 0; c < ch; ++c)
        dst.copyFrom(c, oldLen, src, c, 0, addLen);
}

} // namespace detail_structured

// ─────────────────────────────────────────────────────────────────────────────
// structured_remix() — main entry point
// ─────────────────────────────────────────────────────────────────────────────

inline juce::AudioBuffer<float>
structured_remix(const separators::NativeStems& stems, double sr,
                 const RemixParams& p,
                 const presets::NativeRemixPreset& preset)
{
    // Fallback when structure is disabled or arrangement is empty
    if (!preset.structure.enabled || preset.structure.arrangement.empty())
        return chopped_and_screwed(stems, sr, p);

    // 1. Mix stems → mono/stereo buffer for analysis
    auto mix = detail::mix_stems_equal(stems);
    if (mix.getNumSamples() == 0)
        return chopped_and_screwed(stems, sr, p);

    // 2. Structural analysis
    const auto song = analysis::analyze_structure(mix, sr);
    juce::Logger::writeToLog(
        "[StructuredRemix] bpm=" + juce::String(song.bpm, 1)
        + " bars=" + juce::String(song.num_bars)
        + " sections=" + juce::String((int) song.section_boundaries.size()));

    if (song.num_bars == 0)
        return chopped_and_screwed(stems, sr, p);

    // 3. Assemble arrangement
    juce::AudioBuffer<float> output(mix.getNumChannels(), 0);

    for (const auto& slot : preset.structure.arrangement) {
        const int start_bar  = detail_structured::resolve_anchor(
            slot.source_anchor, song, 0);
        const int length_bars = slot.length_bars > 0
            ? slot.length_bars
            : std::max(4, song.num_bars / (int) preset.structure.arrangement.size());

        auto section = detail_structured::extract_bars(mix, sr, song,
                                                       start_bar, length_bars);
        if (section.getNumSamples() == 0) continue;

        detail_structured::apply_section_layer(section, slot.layer);

        // Loop slot.loop_count times
        const int loops = std::max(1, slot.loop_count);
        for (int lp = 0; lp < loops; ++lp)
            detail_structured::append_buffer(output, section);
    }

    if (output.getNumSamples() == 0)
        return chopped_and_screwed(stems, sr, p);

    // 4. Global time-stretch + pitch-shift
    if (std::abs(p.tempo_factor - 1.0f) > 1e-3f
        || std::abs(p.pitch_shift_semi) > 1e-3f)
    {
        TimePitchStretcher tps((int) sr, output.getNumChannels());
        tps.setTimeRatio(1.0 / (double) p.tempo_factor);
        if (std::abs(p.pitch_shift_semi) > 1e-3f)
            tps.setPitchSemitones((double) p.pitch_shift_semi);
        output = tps.process(output);
    }

    // 5. Light reverb
    if (p.reverb_mix > 0.0f)
        output = detail::apply_reverb(output, sr, p.reverb_mix, 0.55f,
                                       0.5f, 1.0f - p.reverb_mix);

    // 6. LUFS normalize
    analysis::normalize_lufs(output, sr, -14.0);

    return output;
}

} // namespace autoremix::dsp::engines
