// Cue metadata readers must never throw or read out of bounds on malformed
// input (REVIEW R-05).

#include <catch2/catch_test_macros.hpp>

#include "dsp/NativeCueReader.h"

#include "SignalFactory.h"
#include "TestHelpers.h"

#include <juce_core/juce_core.h>

using namespace autoremix::test;

namespace {

juce::File write_temp_audio(const std::vector<uint8_t>& bytes, const juce::String& tag)
{
    auto f = juce::File::getSpecialLocation(juce::File::tempDirectory)
                 .getChildFile("autoremix_tests")
                 .getChildFile("cue_" + tag + ".mp3");
    f.getParentDirectory().createDirectory();
    f.replaceWithData(bytes.data(), bytes.size());
    return f;
}

std::vector<uint8_t> id3_with_geob_payload(const std::vector<uint8_t>& frame_data,
                                           uint32_t declared_frame_size)
{
    std::vector<uint8_t> frame = { 'G', 'E', 'O', 'B',
                                   (uint8_t) (declared_frame_size >> 24),
                                   (uint8_t) (declared_frame_size >> 16),
                                   (uint8_t) (declared_frame_size >> 8),
                                   (uint8_t) declared_frame_size, 0x00, 0x00 };
    frame.insert(frame.end(), frame_data.begin(), frame_data.end());
    const uint32_t ts = (uint32_t) frame.size();
    std::vector<uint8_t> tag = { 'I', 'D', '3', 0x03, 0x00, 0x00,
                                 (uint8_t) ((ts >> 21) & 0x7f), (uint8_t) ((ts >> 14) & 0x7f),
                                 (uint8_t) ((ts >> 7) & 0x7f),  (uint8_t) (ts & 0x7f) };
    tag.insert(tag.end(), frame.begin(), frame.end());
    return tag;
}

} // namespace

TEST_CASE("R-05: GEOB frame without a description does not throw", "[unit][cue][R-05]")
{
    const auto f = write_temp_audio(truncated_geob_id3(), "truncated");
    std::vector<CuePoint> cues;
    REQUIRE_NOTHROW(cues = parse_dj_cue_geob(f));
    REQUIRE(cues.empty());
    REQUIRE_NOTHROW(cues = load_cues_for_file(f));
    REQUIRE(cues.empty());
}

TEST_CASE("R-05: GEOB frame whose size runs past the tag is ignored", "[unit][cue][R-05]")
{
    const std::vector<uint8_t> data = { 0x00, 'x', 0x00 };
    const auto f = write_temp_audio(id3_with_geob_payload(data, 4096), "pastend");
    std::vector<CuePoint> cues;
    REQUIRE_NOTHROW(cues = parse_dj_cue_geob(f));
    REQUIRE(cues.empty());
}

TEST_CASE("R-05: zero-length and header-only files are ignored", "[unit][cue][R-05]")
{
    std::vector<CuePoint> cues;
    REQUIRE_NOTHROW(cues = parse_dj_cue_geob(write_temp_audio({}, "empty")));
    REQUIRE(cues.empty());
    const std::vector<uint8_t> hdr = { 'I', 'D', '3', 0x03, 0x00, 0x00, 0x7f, 0x7f, 0x7f, 0x7f };
    REQUIRE_NOTHROW(cues = parse_dj_cue_geob(write_temp_audio(hdr, "hdronly")));
    REQUIRE(cues.empty());
}

TEST_CASE("R-05: seeded random GEOB payloads never throw", "[unit][cue][R-05][fuzz]")
{
    int threw = 0;
    for (uint32_t seed = 1; seed <= 200; ++seed) {
        uint32_t s = seed;
        auto next = [&s] { s = s * 1664525u + 1013904223u; return (uint8_t) (s >> 24); };
        std::vector<uint8_t> data((size_t) (next() % 96));
        for (auto& b : data) b = (next() & 3) == 0 ? 0x00 : next();   // plenty of terminators
        const auto f = write_temp_audio(id3_with_geob_payload(data, (uint32_t) data.size()),
                                        "fuzz" + juce::String((int) seed));
        try { (void) load_cues_for_file(f); }
        catch (...) { ++threw; }
    }
    CAPTURE(threw);
    REQUIRE(threw == 0);
}
