#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// NativeSampleLibrary — Phase 33-01
//
// Typed sample collection: scan a directory, auto-detect BPM + key for each
// audio file, and expose find_by_category / find_nearest_bpm for the
// NativeSampleEngine (Phase 33-02).
//
// User sample directory (cross-platform):
//   Linux:   ~/.config/autoremix/samples/
//   macOS:   ~/Library/Application Support/autoremix/samples/
//   Windows: %APPDATA%\autoremix\samples\
// ─────────────────────────────────────────────────────────────────────────────

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_core/juce_core.h>

#include "NativeAnalysis.h"

namespace autoremix::dsp::samples {

// ─────────────────────────────────────────────────────────────────────────────
// SampleEntry
// ─────────────────────────────────────────────────────────────────────────────

struct SampleEntry {
    juce::String path;
    std::string  category;      // "riser"|"impact"|"loop"|"adlib"|"stab"|"fx"|"one_shot"|"unknown"
    double       bpm          = 0.0;
    std::string  key;
    double       duration_sec = 0.0;
    double       start_sec    = 0.0;  // cue-in point
    double       end_sec      = 0.0;  // cue-out point (0 = end of file)
};

// ─────────────────────────────────────────────────────────────────────────────
// Category inference from filename/directory keywords
// ─────────────────────────────────────────────────────────────────────────────

inline std::string
infer_category(const juce::String& filename_stem, const juce::String& parent_dir)
{
    const juce::String s = filename_stem.toLowerCase();
    const juce::String d = parent_dir.toLowerCase();
    auto has = [&](const char* kw) {
        return s.containsIgnoreCase(kw) || d.containsIgnoreCase(kw);
    };

    if (has("riser") || has("rise")  || has("build"))          return "riser";
    if (has("impact") || has("hit")  || has("kick") || has("stab")) return "impact";
    if (has("loop")  || has("groove") || has("beat"))           return "loop";
    if (has("adlib") || has("ad_lib") || has("vocal") || has("vox")) return "adlib";
    if (has("fx")    || has("effect") || has("sfx")  || has("sweep")) return "fx";
    if (has("one_shot") || has("oneshot") || has("one-shot"))   return "one_shot";
    return "unknown";
}

// ─────────────────────────────────────────────────────────────────────────────
// SampleLibrary
// ─────────────────────────────────────────────────────────────────────────────

class SampleLibrary {
public:
    // Scan a directory recursively. Detects BPM + key for each audio file
    // using the first 20 s of audio.
    void scan(const juce::File& dir, juce::AudioFormatManager& fmt)
    {
        if (!dir.isDirectory()) return;
        auto files = dir.findChildFiles(
            juce::File::findFiles, true,
            "*.wav;*.mp3;*.flac;*.aiff;*.aif;*.ogg;*.m4a");
        for (auto& f : files) {
            auto e = analyse_file(f, fmt, "");
            if (e.duration_sec > 0.0)
                entries_.push_back(std::move(e));
        }
    }

    // Add a single file (useful for explicit template paths).
    void add(const juce::File& f, juce::AudioFormatManager& fmt,
             const std::string& category_override = "")
    {
        auto e = analyse_file(f, fmt, category_override);
        if (e.duration_sec > 0.0)
            entries_.push_back(std::move(e));
    }

    void clear() { entries_.clear(); }

    const std::vector<SampleEntry>& entries() const { return entries_; }
    std::size_t size() const { return entries_.size(); }

    // All entries with matching category (empty category → all entries).
    std::vector<SampleEntry> find_by_category(const std::string& cat) const
    {
        if (cat.empty()) return entries_;
        std::vector<SampleEntry> out;
        for (const auto& e : entries_)
            if (e.category == cat) out.push_back(e);
        return out;
    }

    // Entry with smallest |bpm - target_bpm| among entries in cat.
    // Returns nullptr if no matching entries.
    const SampleEntry* find_nearest_bpm(double target_bpm,
                                         const std::string& cat = "") const
    {
        const SampleEntry* best = nullptr;
        double best_diff = 1e30;
        for (const auto& e : entries_) {
            if (!cat.empty() && e.category != cat) continue;
            if (e.bpm <= 0.0) continue;
            double diff = std::abs(e.bpm - target_bpm);
            if (diff < best_diff) { best_diff = diff; best = &e; }
        }
        return best;
    }

    // Default user sample directory (cross-platform).
    static juce::File user_sample_dir()
    {
        return juce::File::getSpecialLocation(
                   juce::File::userApplicationDataDirectory)
               .getChildFile("autoremix")
               .getChildFile("samples");
    }

private:
    std::vector<SampleEntry> entries_;

    SampleEntry analyse_file(const juce::File& f,
                              juce::AudioFormatManager& fmt,
                              const std::string& cat_override)
    {
        SampleEntry e;
        std::unique_ptr<juce::AudioFormatReader> reader(fmt.createReaderFor(f));
        if (!reader) return e;

        e.path         = f.getFullPathName();
        e.duration_sec = (double)reader->lengthInSamples / reader->sampleRate;
        e.end_sec      = e.duration_sec;
        e.category     = cat_override.empty()
            ? infer_category(f.getFileNameWithoutExtension(), f.getParentDirectory().getFileName())
            : cat_override;

        // Analyse first 20 s for BPM + key (avoids loading full file for long samples)
        const int64_t max_samples = (int64_t)(std::min(20.0, e.duration_sec) * reader->sampleRate);
        if (max_samples < 512) return e;  // too short to analyse

        const int ch = std::min((int)reader->numChannels, 2);
        juce::AudioBuffer<float> buf(ch, (int)max_samples);
        reader->read(&buf, 0, (int)max_samples, 0, true, ch > 1);

        // Ensure stereo for analysis functions
        if (ch == 1) {
            buf.setSize(2, (int)max_samples, true, false, true);
            buf.copyFrom(1, 0, buf, 0, 0, (int)max_samples);
        }

        e.bpm = analysis::detect_bpm(buf, reader->sampleRate);
        e.key = analysis::detect_key(buf, reader->sampleRate);
        return e;
    }
};

} // namespace autoremix::dsp::samples
