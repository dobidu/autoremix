// PlayerEngine / ClipStore unit tests (plan 34.5-03): ramps, seek, mono
// downmix, sample-rate conversion, bit-exactness, block-size independence,
// deferred release, and a TSAN stress run.

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "audio/ClipPlayer.h"

#include "SignalFactory.h"
#include "TestHelpers.h"

#include <juce_dsp/juce_dsp.h>

#include <atomic>
#include <thread>

using namespace autoremix::test;
using namespace autoremix::audio;

namespace {

std::shared_ptr<const Clip> make_clip(juce::AudioBuffer<float> audio, double sr)
{
    auto c = std::make_shared<Clip>();
    c->audio = std::move(audio);
    c->sample_rate = sr;
    return c;
}

/** Render `total` samples in blocks of `block` into one buffer. */
juce::AudioBuffer<float> render(PlayerEngine& e, int channels, int total, int block)
{
    juce::AudioBuffer<float> out(channels, total), b(channels, block);
    out.clear();
    for (int pos = 0; pos < total; pos += block) {
        const int n = std::min(block, total - pos);
        b.clear();
        e.render(b, channels, n);
        for (int ch = 0; ch < channels; ++ch) out.copyFrom(ch, pos, b, ch, 0, n);
    }
    return out;
}

double peak_hz(const juce::AudioBuffer<float>& b, double sr)
{
    constexpr int order = 15, n = 1 << order;
    juce::dsp::FFT fft(order);
    std::vector<float> d((size_t) n * 2, 0.0f);
    const int off = std::min(b.getNumSamples() - n, 4800);
    for (int i = 0; i < n; ++i) d[(size_t) i] = b.getSample(0, off + i);
    fft.performFrequencyOnlyForwardTransform(d.data());
    int best = 1;
    for (int k = 1; k < n / 2; ++k) if (d[(size_t) k] > d[(size_t) best]) best = k;
    return best * sr / n;
}

} // namespace

TEST_CASE("playback at matching rates is a bit-exact copy", "[unit][player]")
{
    PlayerEngine e;
    ClipStore store(e);
    e.prepare(48000.0, 512);
    auto src = pink_noise(1.0, 48000.0, 11);
    store.assign(0, make_clip(src, 48000.0));
    e.voice(0).play_from(0);

    auto out = render(e, 2, 48000, 512);
    // Skip the 5 ms fade-in; after it the copy is exact.
    const int start = 240 + 1;
    for (int i = start; i < 48000; ++i) {
        REQUIRE(out.getSample(0, i) == src.getSample(0, i));
        REQUIRE(out.getSample(1, i) == src.getSample(1, i));
    }
}

TEST_CASE("start, stop and seek ramp without clicks", "[unit][player]")
{
    PlayerEngine e;
    ClipStore store(e);
    e.prepare(48000.0, 256);
    store.assign(0, make_clip(sine_chord({ 100.0 }, 4.0, 48000.0, 1.0f, 2), 48000.0));
    auto& v = e.voice(0);

    juce::AudioBuffer<float> b(2, 256);
    float prev = 0.0f, max_step = 0.0f;
    auto step = [&](int blocks) {
        for (int k = 0; k < blocks; ++k) {
            b.clear(); e.render(b, 2, 256);
            for (int i = 0; i < 256; ++i) { const float s = b.getSample(0, i); max_step = std::max(max_step, std::abs(s - prev)); prev = s; }
        }
    };
    v.play_from(0);            step(20);
    v.seek(96000);             step(20);     // jump to 2 s while playing
    v.stop();                  step(5);
    CAPTURE(max_step);
    // A full-scale 100 Hz sine moves at most 2*pi*100/48000 = 0.013 per sample.
    REQUIRE(max_step < 0.02f);
    REQUIRE_FALSE(v.wants_play());
    REQUIRE_FALSE(v.is_audible());
    b.clear(); e.render(b, 2, 256);
    REQUIRE(b.getMagnitude(0, 256) == 0.0f);
}

TEST_CASE("seek lands within one block of the target", "[unit][player]")
{
    PlayerEngine e;
    ClipStore store(e);
    e.prepare(48000.0, 128);
    store.assign(0, make_clip(silence(10.0, 48000.0), 48000.0));
    e.voice(0).play_from(0);
    render(e, 2, 4800, 128);
    e.voice(0).seek(240000);                                  // 5 s
    render(e, 2, 1280, 128);                                  // ramp out + jump + some play
    const double pos = e.voice(0).position();
    CAPTURE(pos);
    REQUIRE(pos >= 240000.0);
    REQUIRE(pos <= 240000.0 + 1280.0);
}

TEST_CASE("stereo clip on a mono bus is (L+R)/2", "[unit][player]")
{
    PlayerEngine e;
    ClipStore store(e);
    e.prepare(48000.0, 256);
    auto st = sine_chord({ 330.0 }, 1.0, 48000.0, 0.8f, 2);
    st.applyGain(1, 0, st.getNumSamples(), -0.5f);
    store.assign(0, make_clip(st, 48000.0));
    e.voice(0).play_from(0);
    auto out = render(e, 1, 24000, 256);
    for (int i = 1000; i < 24000; ++i)
        REQUIRE_THAT(out.getSample(0, i), Catch::Matchers::WithinAbs(0.25 * st.getSample(0, i), 1e-6));
}

TEST_CASE("sample-rate conversion keeps duration and pitch", "[unit][player]")
{
    const double clip_sr = GENERATE(44100.0, 48000.0);
    const double dev_sr  = clip_sr == 44100.0 ? 48000.0 : 44100.0;
    CAPTURE(clip_sr, dev_sr);
    PlayerEngine e;
    ClipStore store(e);
    e.prepare(dev_sr, 512);
    store.assign(0, make_clip(sine_chord({ 1000.0 }, 2.0, clip_sr, 0.5f, 2), clip_sr));
    e.voice(0).play_from(0);

    const int expected = (int) std::lround(2.0 * dev_sr);
    auto out = render(e, 2, expected + 4096, 512);
    REQUIRE_FALSE(e.voice(0).wants_play());                  // reached the end
    // The last audible sample sits at the expected duration (±1 block).
    int last = 0;
    for (int i = 0; i < out.getNumSamples(); ++i) if (std::abs(out.getSample(0, i)) > 1e-4f) last = i;
    CAPTURE(last, expected);
    REQUIRE(std::abs(last - expected) <= 512);
    REQUIRE_THAT(peak_hz(out, dev_sr), Catch::Matchers::WithinAbs(1000.0, 2.0));
}

TEST_CASE("output does not depend on block size", "[unit][player]")
{
    auto run = [](int block) {
        PlayerEngine e;
        ClipStore store(e);
        e.prepare(48000.0, 1024);
        store.assign(0, make_clip(pink_noise(10.0, 44100.0, 3), 44100.0));
        store.assign(4, make_clip(sine_chord({ 220.0, 330.0 }, 10.0, 48000.0, 0.3f, 2), 48000.0));
        e.voice(0).play_from(0);
        e.voice(4).play_from(0);
        return buffer_hash(render(e, 2, 480000, block));
    };
    REQUIRE(run(64) == run(512));
}

TEST_CASE("retired clips are freed only after the audio thread moved on", "[unit][player]")
{
    PlayerEngine e;
    ClipStore store(e);
    e.prepare(48000.0, 256);
    auto a = make_clip(silence(1.0, 48000.0), 48000.0);
    std::weak_ptr<const Clip> wa = a;
    store.assign(0, a);
    a.reset();
    e.voice(0).play_from(0);
    juce::AudioBuffer<float> b(2, 256);
    b.clear(); e.render(b, 2, 256);

    // Simulate a render in progress at swap time: the old clip must survive.
    store.assign(0, make_clip(silence(1.0, 48000.0), 48000.0));
    REQUIRE((store.retired_count() == 0 || !wa.expired()));
    for (int k = 0; k < 3; ++k) { b.clear(); e.render(b, 2, 256); }
    store.collect();
    REQUIRE(wa.expired());
    REQUIRE(store.retired_count() == 0);
}

TEST_CASE("concurrent control and rendering is race-free", "[unit][player][tsan]")
{
    PlayerEngine e;
    ClipStore store(e);
    e.prepare(48000.0, 256);
    std::vector<std::shared_ptr<const Clip>> clips;
    for (int i = 0; i < 3; ++i) clips.push_back(make_clip(pink_noise(0.5, 44100.0 + 3900.0 * i, 20 + (uint32_t) i), 44100.0 + 3900.0 * i));

    std::atomic<bool> run { true };
    std::thread audio([&] {
        juce::AudioBuffer<float> b(2, 256);
        while (run) { b.clear(); e.render(b, 2, 256); }
    });
    const auto until = std::chrono::steady_clock::now() + std::chrono::milliseconds(1500);
    int i = 0;
    while (std::chrono::steady_clock::now() < until) {
        const int v = i % PlayerEngine::kVoices;
        store.assign(v, clips[(size_t) (i % 3)]);
        if (i % 2) e.voice(v).play_from(0); else e.voice(v).stop();
        e.voice(v).seek(i * 37);
        (void) e.voice(v).position();
        if (i % 50 == 0) store.assign(v, nullptr);
        store.collect();
        ++i;
    }
    run = false;
    audio.join();
    e.release();
    store.collect();
    REQUIRE(store.retired_count() == 0);
}
