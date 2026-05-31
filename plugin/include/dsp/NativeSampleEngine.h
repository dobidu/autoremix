#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// NativeSampleEngine — Phase 33-02
//
// Placement resolution + sample mixing using SongStructure from Phase 31.
//
// Pipeline:
//   resolve_placement(anchor, SongStructure, SampleEntry) → position_sec
//   load_and_stretch_sample(SampleEntry, target_sr, target_bpm, fmt) → AudioBuffer
//   mix_sample_into(dest, sample, position_sec, sr, gain, fade_ms)
//   apply_samples(dest, sr, dest_bpm, SongStructure, placements, fmt)
// ─────────────────────────────────────────────────────────────────────────────

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_core/juce_core.h>

#include "NativeAnalysis.h"
#include "NativeSampleLibrary.h"
#include "TimePitchStretcher.h"

namespace autoremix::dsp::samples {

// ─────────────────────────────────────────────────────────────────────────────
// SamplePlacement — resolved position for one sample in the output buffer
// ─────────────────────────────────────────────────────────────────────────────

struct SamplePlacement {
    SampleEntry entry;
    double      position_sec = 0.0;
    float       gain         = 1.0f;
    float       fade_ms      = 10.0f;
};

// ─────────────────────────────────────────────────────────────────────────────
// resolve_placement — anchor string → position in seconds
// ─────────────────────────────────────────────────────────────────────────────

inline double
resolve_placement(const std::string& anchor,
                  const analysis::SongStructure& song,
                  const SampleEntry& entry)
{
    if (anchor == "pre_drop") {
        if (song.energy_peaks.empty()) return 0.0;
        const int bar = std::max(0, song.energy_peaks[0] - 2);
        return song.bar_time_at(bar);
    }
    if (anchor == "drop" || anchor == "energy_peak") {
        if (song.energy_peaks.empty()) return song.duration_seconds * 0.5;
        return song.bar_time_at(song.energy_peaks[0]);
    }
    if (anchor == "vocal_gap") {
        for (int i = 0; i < (int)song.vocal_presence_per_bar.size(); ++i)
            if (song.vocal_presence_per_bar[(size_t)i] < 0.3f)
                return song.bar_time_at(i);
        return 0.0;
    }
    if (anchor.size() > 4 && anchor.substr(0, 4) == "bar:") {
        try {
            int n = std::stoi(anchor.substr(4));
            return song.bar_time_at(n);
        } catch (...) {}
    }
    if (anchor.size() > 7 && anchor.substr(0, 7) == "phrase:") {
        try {
            size_t n = (size_t)std::stoi(anchor.substr(7));
            if (n < song.phrase_times.size()) return song.phrase_times[n];
        } catch (...) {}
    }
    if (anchor == "end") {
        const double dur = entry.end_sec > 0.0 ? entry.end_sec - entry.start_sec
                                                : entry.duration_sec;
        return std::max(0.0, song.duration_seconds - dur);
    }
    return 0.0;
}

// ─────────────────────────────────────────────────────────────────────────────
// load_and_stretch_sample — load audio + BPM time-stretch if needed
// ─────────────────────────────────────────────────────────────────────────────

inline juce::AudioBuffer<float>
load_and_stretch_sample(const SampleEntry& entry,
                        double             /*target_sr*/,
                        double             target_bpm,
                        juce::AudioFormatManager& fmt)
{
    juce::AudioBuffer<float> empty;
    std::unique_ptr<juce::AudioFormatReader> reader(
        fmt.createReaderFor(juce::File(entry.path)));
    if (!reader) return empty;

    const double sr      = reader->sampleRate;
    const double cue_in  = entry.start_sec;
    const double cue_out = (entry.end_sec > 0.0) ? entry.end_sec : entry.duration_sec;
    const int    start   = (int)(cue_in  * sr);
    const int    len     = std::max(1, (int)((cue_out - cue_in) * sr));

    const int ch = std::min((int)reader->numChannels, 2);
    juce::AudioBuffer<float> buf(ch, len);
    buf.clear();
    reader->read(&buf, 0, len, start, true, ch > 1);
    if (ch == 1) {
        buf.setSize(2, len, true, false, true);
        buf.copyFrom(1, 0, buf, 0, 0, len);
    }

    // Time-stretch if BPM is known and differs from target
    if (entry.bpm > 0.0 && target_bpm > 0.0
        && std::abs(entry.bpm - target_bpm) > 1.0)
    {
        const double ratio = std::clamp(entry.bpm / target_bpm, 0.5, 2.0);
        TimePitchStretcher tps((int)sr, 2);
        tps.setTimeRatio(ratio);
        buf = tps.process(buf);
    }

    return buf;
}

// ─────────────────────────────────────────────────────────────────────────────
// mix_sample_into — add sample to dest at position_sec with gain + fade
// ─────────────────────────────────────────────────────────────────────────────

inline void
mix_sample_into(juce::AudioBuffer<float>&       dest,
                const juce::AudioBuffer<float>& sample,
                double  position_sec,
                double  dest_sr,
                float   gain,
                float   fade_ms)
{
    const int dest_N   = dest.getNumSamples();
    const int dest_ch  = dest.getNumChannels();
    const int samp_N   = sample.getNumSamples();
    const int samp_ch  = sample.getNumChannels();
    const int start    = (int)(position_sec * dest_sr);
    const int fade_n   = std::max(1, (int)(fade_ms / 1000.0 * dest_sr));

    for (int i = 0; i < samp_N; ++i) {
        const int dst_i = start + i;
        if (dst_i < 0 || dst_i >= dest_N) continue;

        // Linear fade-in / fade-out envelope
        float env = 1.0f;
        if (i < fade_n)
            env = (float)i / (float)fade_n;
        else if (i >= samp_N - fade_n)
            env = (float)(samp_N - 1 - i) / (float)fade_n;

        for (int c = 0; c < dest_ch; ++c) {
            const int src_c = (samp_ch == 1) ? 0 : std::min(c, samp_ch - 1);
            const float s   = sample.getSample(src_c, i) * gain * env;
            dest.setSample(c, dst_i,
                juce::jlimit(-1.0f, 1.0f, dest.getSample(c, dst_i) + s));
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// apply_samples — apply all placements to the rendered output buffer
// ─────────────────────────────────────────────────────────────────────────────

inline void
apply_samples(juce::AudioBuffer<float>&             dest,
              double                                dest_sr,
              double                                dest_bpm,
              const analysis::SongStructure&        song,
              const std::vector<SamplePlacement>&   placements,
              juce::AudioFormatManager&             fmt)
{
    juce::ignoreUnused(song);  // reserved for future per-placement analysis

    for (const auto& p : placements) {
        auto buf = load_and_stretch_sample(p.entry, dest_sr, dest_bpm, fmt);
        if (buf.getNumSamples() == 0) {
            juce::Logger::writeToLog("[SampleEngine] skip (load failed): "
                + p.entry.path);
            continue;
        }
        mix_sample_into(dest, buf, p.position_sec, dest_sr, p.gain, p.fade_ms);
    }
}

} // namespace autoremix::dsp::samples
