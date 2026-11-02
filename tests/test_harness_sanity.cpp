// Sanity tests for the test harness itself: the signal factory must be exact
// before any analysis test can trust it.

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <juce_dsp/juce_dsp.h>

#include "SignalFactory.h"
#include "TestHelpers.h"

using namespace autoremix::test;
using Catch::Matchers::WithinAbs;

TEST_CASE("click_track places clicks at exact sample positions", "[unit][harness]")
{
    const double bpm = GENERATE(90.0, 125.0, 128.0, 174.0);
    const double sr  = GENERATE(44100.0, 48000.0);
    CAPTURE(bpm, sr);

    const double seconds = 10.0;
    const auto buf = click_track(bpm, seconds, sr);
    REQUIRE(buf.getNumSamples() == (int) std::llround(seconds * sr));
    REQUIRE(buf.getNumChannels() == 2);

    // Every click onset is exactly where click_position says, and the sample
    // just before it is silent (the previous click has fully decayed or is
    // more than click_len away for these tempos).
    int clicks = 0;
    for (int64_t n = 0;; ++n) {
        const auto pos = click_position(n, bpm, sr);
        if (pos >= buf.getNumSamples()) break;
        const float expected = (n % 4 == 0) ? 1.0f : 0.5f;
        REQUIRE(buf.getSample(0, (int) pos) == expected);
        REQUIRE(buf.getSample(1, (int) pos) == expected);
        if (pos > 0) REQUIRE(buf.getSample(0, (int) pos - 1) == 0.0f);
        ++clicks;
    }
    REQUIRE(clicks == (int) std::ceil(seconds * bpm / 60.0 - 1e-9));
}

TEST_CASE("click_track without accent has equal peaks", "[unit][harness]")
{
    const auto buf = click_track(120.0, 2.0, 48000.0, 1, 64, false);
    REQUIRE(buf.getNumChannels() == 1);
    for (int64_t n = 0; n < 4; ++n)
        REQUIRE(buf.getSample(0, (int) click_position(n, 120.0, 48000.0)) == 1.0f);
}

TEST_CASE("sine_chord has spectral peaks at the requested frequencies", "[unit][harness]")
{
    constexpr double sr = 48000.0;
    constexpr int order = 14;                 // 16384-point FFT, ~2.93 Hz bins
    constexpr int n = 1 << order;
    const std::vector<double> freqs = { 220.0, 277.18, 329.63 };   // A major triad
    const auto buf = sine_chord(freqs, 1.0, sr, 0.9f, 1);
    REQUIRE(buf.getMagnitude(0, buf.getNumSamples()) <= 0.9f + 1e-6f);

    juce::dsp::FFT fft(order);
    std::vector<float> data((size_t) n * 2, 0.0f);
    for (int i = 0; i < n; ++i) data[(size_t) i] = buf.getSample(0, i);
    fft.performFrequencyOnlyForwardTransform(data.data());

    const double binHz = sr / n;
    for (double f : freqs) {
        const int bin = (int) std::lround(f / binHz);
        float local = 0.0f;
        for (int b = bin - 1; b <= bin + 1; ++b) local = std::max(local, data[(size_t) b]);
        // Peak region must dominate a bin well away from all three partials.
        REQUIRE(local > 50.0f * data[(size_t) std::lround(1000.0 / binHz)]);
    }
}

TEST_CASE("silence is all zeros with the documented length", "[unit][harness]")
{
    const auto buf = silence(1.5, 44100.0, 2);
    REQUIRE(buf.getNumSamples() == 66150);
    REQUIRE(buf.getMagnitude(0, buf.getNumSamples()) == 0.0f);
}

TEST_CASE("splice_sections concatenates and reports boundaries", "[unit][harness]")
{
    const auto a = silence(1.0, 48000.0, 2);
    const auto b = sine_chord({ 440.0 }, 0.5, 48000.0, 0.5f, 1);
    const auto c = click_track(120.0, 2.0, 48000.0, 2);
    auto [out, bounds] = splice_sections({ a, b, c });

    REQUIRE(out.getNumChannels() == 2);
    REQUIRE(out.getNumSamples() == 48000 + 24000 + 96000);
    REQUIRE(bounds == std::vector<int>{ 48000, 72000 });
    REQUIRE(out.getMagnitude(0, 0, 48000) == 0.0f);
    REQUIRE(out.getMagnitude(1, 48000, 24000) == 0.0f);     // mono part -> right ch silent
    REQUIRE(out.getSample(0, 72000) == 1.0f);                // first click of section c
}

TEST_CASE("pink_noise is deterministic per seed", "[unit][harness]")
{
    const auto a = pink_noise(0.5, 44100.0, 42);
    const auto b = pink_noise(0.5, 44100.0, 42);
    const auto c = pink_noise(0.5, 44100.0, 43);
    REQUIRE(buffer_hash(a) == buffer_hash(b));
    REQUIRE(buffer_hash(a) != buffer_hash(c));
    REQUIRE(a.getMagnitude(0, a.getNumSamples()) > 0.0f);
}

TEST_CASE("buffer_hash is stable and content-sensitive", "[unit][harness]")
{
    auto buf = click_track(100.0, 1.0, 44100.0);
    const auto h1 = buffer_hash(buf);
    REQUIRE(buffer_hash(buf) == h1);
    buf.setSample(1, 12345, 0.001f);
    REQUIRE(buffer_hash(buf) != h1);
}

TEST_CASE("truncated_geob_id3 is a well-formed ID3v2.3 header with a cut GEOB frame",
          "[unit][harness]")
{
    const auto tag = truncated_geob_id3();
    REQUIRE(tag.size() > 20);
    REQUIRE(tag[0] == 'I');
    REQUIRE(tag[1] == 'D');
    REQUIRE(tag[2] == '3');
    REQUIRE(tag[3] == 0x03);

    const uint32_t body = ((uint32_t) tag[6] << 21) | ((uint32_t) tag[7] << 14)
                        | ((uint32_t) tag[8] << 7) | (uint32_t) tag[9];
    REQUIRE(body == tag.size() - 10);
    REQUIRE(std::string(tag.begin() + 10, tag.begin() + 14) == "GEOB");

    const uint32_t frame = ((uint32_t) tag[14] << 24) | ((uint32_t) tag[15] << 16)
                         | ((uint32_t) tag[16] << 8) | (uint32_t) tag[17];
    REQUIRE(frame == body - 10);
    REQUIRE(tag.back() == 0x00);          // ends on the filename terminator: no description
}

TEST_CASE("approx_db tolerance", "[unit][harness]")
{
    REQUIRE(approx_db(-14.0, -14.05, 0.1));
    REQUIRE_FALSE(approx_db(-14.0, -14.2, 0.1));
}

TEST_CASE("GUI components can be created headless or skip cleanly", "[editor][harness]")
{
    REQUIRE_DISPLAY();
    juce::Label label("probe", "AutoRemix");
    label.setSize(200, 24);
    REQUIRE(label.getText() == "AutoRemix");
    REQUIRE(label.getWidth() == 200);
}
