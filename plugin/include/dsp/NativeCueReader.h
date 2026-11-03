#pragma once
#include "NativeAnalysis.h"
#include <juce_core/juce_core.h>
#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

// ── NativeCueReader — Phase 34-02 ───────────────────────────────────────────
// Header-only. Three public functions:
//   parse_cue_sheet()    — reads .cue sidecar file
//   parse_dj_cue_geob()  — reads DJ-software hot cues stored in an ID3 GEOB frame
//   load_cues_for_file() — calls both, merges, deduplicates, sorts
//
// Every public entry point catches everything and returns {} on malformed
// input: metadata from user files must never throw on the message thread
// (REVIEW R-05).

using autoremix::dsp::analysis::CuePoint;

// ── .cue sheet parser ────────────────────────────────────────────────────────

inline std::vector<CuePoint> parse_cue_sheet_unchecked(const juce::File& audio_file)
{
    // Check for sidecar by replacing extension or appending ".cue"
    juce::File sidecar = audio_file.getSiblingFile(
        audio_file.getFileNameWithoutExtension() + ".cue");
    if (!sidecar.existsAsFile())
        sidecar = audio_file.withFileExtension(".cue");
    if (!sidecar.existsAsFile())
        return {};

    const juce::String content = sidecar.loadFileAsString();
    if (content.isEmpty())
        return {};

    std::vector<CuePoint> result;
    const juce::StringArray lines = juce::StringArray::fromLines(content);

    std::string pending_title;
    int cue_index = 0;

    for (const auto& line : lines)
    {
        const juce::String trimmed = line.trim();

        // TITLE "..." inside a TRACK block
        if (trimmed.startsWithIgnoreCase("TITLE "))
        {
            juce::String t = trimmed.substring(6).trim();
            if (t.startsWith("\"") && t.endsWith("\""))
                t = t.substring(1, t.length() - 1);
            pending_title = t.toStdString();
            continue;
        }

        // INDEX 01 MM:SS:FF
        if (trimmed.startsWithIgnoreCase("INDEX 01 "))
        {
            const juce::String ts = trimmed.substring(9).trim();
            const juce::StringArray parts = juce::StringArray::fromTokens(ts, ":", "");
            if (parts.size() < 3)
            {
                juce::Logger::writeToLog("NativeCueReader: malformed INDEX 01 timestamp: " + ts);
                continue;
            }
            const int mm = parts[0].getIntValue();
            const int ss = parts[1].getIntValue();
            const int ff = parts[2].getIntValue();
            const double position_sec = mm * 60.0 + ss + ff / 75.0;

            CuePoint cp;
            cp.position_sec = position_sec;
            cp.color_rgb    = 0xFF22AA44u; // green
            cp.source       = "cue_sheet";
            cp.name = pending_title.empty()
                          ? ("cue_" + std::to_string(cue_index))
                          : pending_title;
            result.push_back(std::move(cp));
            ++cue_index;
            pending_title.clear();
        }
    }

    return result;
}

// ── DJ hot cues in ID3 GEOB ───────────────────────────────────────────────────

/** Cues from a .cue sheet next to the audio file. Never throws. */
inline std::vector<CuePoint> parse_cue_sheet(const juce::File& audio_file)
{
    try { return parse_cue_sheet_unchecked(audio_file); }
    catch (...) {
        juce::Logger::writeToLog("NativeCueReader: ignoring unreadable cue sheet for "
                                 + audio_file.getFullPathName());
    }
    return {};
}

namespace detail
{
    // Read 4-byte big-endian uint32
    inline uint32_t read_be32(const uint8_t* p)
    {
        return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16)
             | (uint32_t(p[2]) << 8)  |  uint32_t(p[3]);
    }

    // Decode syncsafe int (ID3v2.4 frame sizes)
    inline uint32_t syncsafe_to_uint(const uint8_t* p)
    {
        return (uint32_t(p[0] & 0x7F) << 21) | (uint32_t(p[1] & 0x7F) << 14)
             | (uint32_t(p[2] & 0x7F) <<  7) |  uint32_t(p[3] & 0x7F);
    }

    // Skip a null-terminated string in a buffer, return pointer past the null
    inline const uint8_t* skip_cstring(const uint8_t* p, const uint8_t* end)
    {
        while (p < end && *p != 0) ++p;
        if (p < end) ++p; // skip null
        return p;
    }

    // GEOB description that marks the hot-cue blob. This is a file-format
    // identifier written by third-party DJ software and must match the bytes
    // on disk exactly; it is not used anywhere as a name.
    inline constexpr const char* kGeobMarkersDescription = "Serato Markers2";

    // Parse hot cues from the decoded markers blob
    inline std::vector<CuePoint> parse_dj_cue_markers(const juce::MemoryBlock& decoded)
    {
        std::vector<CuePoint> cues;
        const auto* data = static_cast<const uint8_t*>(decoded.getData());
        const size_t size = decoded.getSize();
        if (size < 2) return cues;

        // 2-byte version header — skip
        size_t pos = 2;

        while (pos + 5 < size)
        {
            const uint8_t entry_type = data[pos];
            ++pos;
            if (pos + 4 > size) break;
            const uint32_t entry_len = read_be32(data + pos);
            pos += 4;

            if (entry_len == 0 || pos + entry_len > size) break;
            const uint8_t* entry_start = data + pos;
            const uint8_t* entry_end   = entry_start + entry_len;

            if (entry_type == 0x01) // hot cue
            {
                // 1-byte index, 4-byte position_ms, 1-byte r, 1-byte g, 1-byte b,
                // null-terminated name
                if (entry_len < 8)
                {
                    pos += entry_len;
                    continue;
                }
                const uint8_t  idx          = entry_start[0];
                const uint32_t position_ms  = read_be32(entry_start + 1);
                const uint8_t  r            = entry_start[5];
                const uint8_t  g            = entry_start[6];
                const uint8_t  b            = entry_start[7];

                std::string label;
                const uint8_t* name_ptr = entry_start + 8;
                while (name_ptr < entry_end && *name_ptr != 0)
                    label += static_cast<char>(*name_ptr++);

                CuePoint cp;
                cp.position_sec = position_ms / 1000.0;
                cp.color_rgb    = 0xFF000000u | (uint32_t(r) << 16) | (uint32_t(g) << 8) | uint32_t(b);
                cp.source       = "id3_geob";
                cp.name         = label.empty() ? ("hotcue_" + std::to_string(idx)) : label;
                cues.push_back(std::move(cp));
            }

            pos += entry_len;
        }

        return cues;
    }
} // namespace detail

namespace detail {
inline std::vector<CuePoint> parse_dj_cue_geob_unchecked(const juce::File& audio_file)
{
    if (!audio_file.existsAsFile())
        return {};

    juce::FileInputStream stream(audio_file);
    if (stream.failedToOpen())
        return {};

    // Check ID3v2 magic
    uint8_t header[10];
    if (stream.read(header, 10) < 10) return {};
    if (header[0] != 'I' || header[1] != 'D' || header[2] != '3') return {};

    const uint8_t id3_version = header[3];
    // Total tag size (syncsafe)
    const uint32_t tag_size = detail::syncsafe_to_uint(header + 6);

    // Sanity caps: real tags are far below 16 MB and can't exceed the file.
    if (tag_size == 0 || tag_size > 16u * 1024u * 1024u) return {};
    if ((juce::int64) tag_size + 10 > audio_file.getSize()) return {};

    // Read entire ID3 tag body
    juce::MemoryBlock tag_body(tag_size);
    if ((size_t)stream.read(tag_body.getData(), (int)tag_size) < tag_size)
        return {};

    const auto* body   = static_cast<const uint8_t*>(tag_body.getData());
    size_t      offset = 0;

    while (offset + 10 <= tag_size)
    {
        // Frame header: 4-byte ID + 4-byte size + 2-byte flags
        const char* frame_id = reinterpret_cast<const char*>(body + offset);
        const uint8_t* size_bytes = body + offset + 4;

        uint32_t frame_size;
        if (id3_version >= 4)
            frame_size = detail::syncsafe_to_uint(size_bytes);
        else
            frame_size = detail::read_be32(size_bytes);

        offset += 10;

        if (frame_size == 0 || offset + frame_size > tag_size) break;

        // Only look at GEOB frames
        if (frame_id[0] == 'G' && frame_id[1] == 'E'
            && frame_id[2] == 'O' && frame_id[3] == 'B')
        {
            const uint8_t* fdata = body + offset;
            const uint8_t* fend  = fdata + frame_size;

            // GEOB structure: 1-byte text encoding + null-term MIME + null-term filename
            // + null-term description + object data
            const uint8_t* p = fdata;
            if (p >= fend) { offset += frame_size; continue; }
            ++p; // skip encoding byte

            p = detail::skip_cstring(p, fend); // MIME
            p = detail::skip_cstring(p, fend); // filename

            // Description — the markers blob is identified by its description.
            const uint8_t* desc_start = p;
            if (desc_start >= fend) { offset += frame_size; continue; }   // no description (R-05)
            p = detail::skip_cstring(p, fend);
            // skip_cstring stops after the terminator, or at fend if there is none.
            const size_t desc_len = (p > desc_start && p[-1] == 0)
                                  ? static_cast<size_t>(p - 1 - desc_start)
                                  : static_cast<size_t>(p - desc_start);
            const std::string description(reinterpret_cast<const char*>(desc_start), desc_len);

            if (description == detail::kGeobMarkersDescription && p < fend)
            {
                // Remainder is base64-encoded binary
                const juce::String b64_str(reinterpret_cast<const char*>(p),
                                           (size_t)(fend - p));
                juce::MemoryOutputStream mos;
                if (!juce::Base64::convertFromBase64(mos, b64_str))
                {
                    juce::Logger::writeToLog("NativeCueReader: failed to base64-decode the GEOB markers blob");
                    return {};
                }
                return detail::parse_dj_cue_markers(mos.getMemoryBlock());
            }
        }

        offset += frame_size;
    }

    return {}; // no markers frame found
}
} // namespace detail

/** Hot cues stored by DJ software in an ID3v2 GEOB frame. Never throws. */
inline std::vector<CuePoint> parse_dj_cue_geob(const juce::File& audio_file)
{
    try { return detail::parse_dj_cue_geob_unchecked(audio_file); }
    catch (const std::exception& e) {
        juce::Logger::writeToLog(juce::String("NativeCueReader: ignoring malformed GEOB tag: ") + e.what());
    }
    catch (...) {
        juce::Logger::writeToLog("NativeCueReader: ignoring malformed GEOB tag");
    }
    return {};
}

// ── load_cues_for_file ───────────────────────────────────────────────────────

namespace detail {
inline std::vector<CuePoint> load_cues_for_file_unchecked(const juce::File& audio_file)
{
    std::vector<CuePoint> geob    = parse_dj_cue_geob(audio_file);
    std::vector<CuePoint> cuesheet = parse_cue_sheet(audio_file);

    // Merge
    std::vector<CuePoint> merged;
    merged.reserve(geob.size() + cuesheet.size());
    for (auto& cp : geob)     merged.push_back(std::move(cp));
    for (auto& cp : cuesheet) merged.push_back(std::move(cp));

    if (merged.empty()) return {};

    // Sort by position
    std::sort(merged.begin(), merged.end(),
              [](const CuePoint& a, const CuePoint& b) {
                  return a.position_sec < b.position_sec;
              });

    // Deduplicate: within ±0.1s window, keep non-auto over auto;
    // between two non-auto, keep first (GEOB cues before cue_sheet per merge order)
    std::vector<CuePoint> deduped;
    deduped.reserve(merged.size());

    for (auto& cp : merged)
    {
        bool absorbed = false;
        for (auto& existing : deduped)
        {
            if (std::abs(existing.position_sec - cp.position_sec) <= 0.1)
            {
                // Keep the non-auto one
                if (existing.source == "auto" && cp.source != "auto")
                    existing = cp;
                absorbed = true;
                break;
            }
        }
        if (!absorbed)
            deduped.push_back(std::move(cp));
    }

    // Final sort after possible replacements
    std::sort(deduped.begin(), deduped.end(),
              [](const CuePoint& a, const CuePoint& b) {
                  return a.position_sec < b.position_sec;
              });

    return deduped;
}
} // namespace detail

/** All cues for a file (GEOB + .cue sheet), merged and deduplicated. Never throws. */
inline std::vector<CuePoint> load_cues_for_file(const juce::File& audio_file)
{
    try { return detail::load_cues_for_file_unchecked(audio_file); }
    catch (...) {
        juce::Logger::writeToLog("NativeCueReader: ignoring unreadable cue metadata for "
                                 + audio_file.getFullPathName());
    }
    return {};
}
