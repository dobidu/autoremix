// Audio-thread behaviour of the real processor (REVIEW R-07, R-16, R-17,
// R-18, R-36, R-37). Blocks are driven by the test, so results don't depend
// on real-time scheduling.

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "PluginProcessor.h"

#include "AllocationCounter.h"
#include "SignalFactory.h"
#include "TestHelpers.h"

#include <atomic>
#include <chrono>
#include <cmath>
#include <thread>

using namespace autoremix::test;

namespace {

void pump(int ms) { juce::MessageManager::getInstance()->runDispatchLoopUntil(ms); }

juce::File write_wav(const juce::AudioBuffer<float>& buf, double sr, const juce::String& name)
{
    auto f = juce::File::getSpecialLocation(juce::File::tempDirectory)
                 .getChildFile("autoremix_tests").getChildFile(name + ".wav");
    f.getParentDirectory().createDirectory();
    f.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> w(
        wav.createWriterFor(new juce::FileOutputStream(f), sr,
                            (unsigned) buf.getNumChannels(), 24, {}, 0));
    REQUIRE(w != nullptr);
    w->writeFromAudioSampleBuffer(buf, 0, buf.getNumSamples());
    return f;
}

struct Rig {
    std::unique_ptr<AutoRemixAudioProcessor> proc = std::make_unique<AutoRemixAudioProcessor>();
    double sr = 48000.0;
    int    block = 256;
    juce::MidiBuffer midi;

    explicit Rig(int channels = 2, int max_block = 1024)
    {
        const auto set = channels == 1 ? juce::AudioChannelSet::mono() : juce::AudioChannelSet::stereo();
        juce::AudioProcessor::BusesLayout layout;
        layout.inputBuses.add(set);
        layout.outputBuses.add(set);
        REQUIRE(proc->setBusesLayout(layout));
        proc->setRateAndBufferSizeDetails(sr, max_block);
        proc->prepareToPlay(sr, max_block);
    }
    ~Rig() { proc->releaseResources(); }

    void process(juce::AudioBuffer<float>& b) { proc->processBlock(b, midi); }

    /** Wait (pumping the message loop) until the stem reports playing and
     *  audio actually comes out — loading may be asynchronous. */
    bool wait_for_audio(int stem_idx, int channels, int timeout_ms = 5000)
    {
        juce::AudioBuffer<float> b(channels, block);
        for (int waited = 0; waited < timeout_ms; waited += 10) {
            b.clear();
            process(b);
            if (proc->isStemPlaying(stem_idx) && b.getMagnitude(0, block) > 1e-4f) return true;
            pump(10);
        }
        return false;
    }
};

} // namespace

TEST_CASE("R-07: input passes through when nothing is playing", "[editor][audio][R-07]")
{
    REQUIRE_DISPLAY();
    Rig rig;
    auto in = sine_chord({ 440.0 }, 256.0 / 48000.0, 48000.0, 0.5f, 2);
    juce::AudioBuffer<float> buf;
    buf.makeCopyOf(in);
    rig.process(buf);
    REQUIRE(buffer_hash(buf) == buffer_hash(in));
}

TEST_CASE("R-16: stopping a stem is immediate and click-free", "[editor][audio][R-16]")
{
    REQUIRE_DISPLAY();
    Rig rig;
    const auto f = write_wav(sine_chord({ 220.0 }, 10.0, 48000.0, 0.9f, 2), 48000.0, "r16_sine");
    rig.proc->playStem(0, f);
    REQUIRE(rig.wait_for_audio(0, 2));

    juce::AudioBuffer<float> b(2, rig.block);
    b.clear(); rig.process(b);
    float prev = b.getSample(0, rig.block - 1);

    const auto t0 = std::chrono::steady_clock::now();
    rig.proc->stopStem(0);                                   // message thread
    const auto stop_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    CAPTURE(stop_ms);
    CHECK(stop_ms < 50.0);

    float max_step = 0.0f;
    for (int k = 0; k < 4; ++k) {                            // ramp-out window
        b.clear(); rig.process(b);
        for (int i = 0; i < rig.block; ++i) {
            const float s = b.getSample(0, i);
            max_step = std::max(max_step, std::abs(s - prev));
            prev = s;
        }
    }
    CAPTURE(max_step);
    CHECK(max_step < 0.05f);                                 // a 220 Hz sine itself moves < 0.026/sample
}

namespace {
/** Plays 4 stems and runs 600 blocks of cycling sizes as the "audio thread". */
void run_rt_probe()
{
    Rig rig;
    const auto f = write_wav(pink_noise(10.0, 48000.0, 5), 48000.0, "r17_noise");
    for (int i = 0; i < 4; ++i) rig.proc->playStem(i, f);
    REQUIRE(rig.wait_for_audio(0, 2));
    const int sizes[] = { 32, 64, 128, 256, 512, 1024 };
    std::vector<juce::AudioBuffer<float>> blocks;
    for (int s : sizes) blocks.emplace_back(2, s);
    rt::reset();
    rt::ScopedAudioThread audio;
    for (int k = 0; k < 600; ++k) {
        auto& b = blocks[(size_t) (k % 6)];
        b.clear();
        rig.process(b);
    }
}
} // namespace

TEST_CASE("R-17: processBlock does not allocate while stems play", "[editor][audio][R-17]")
{
    REQUIRE_DISPLAY();
    run_rt_probe();
    CAPTURE(rt::allocations());
    CHECK(rt::allocations() == 0);
}

TEST_CASE("R-17: processBlock takes no locks while stems play", "[editor][audio][R-17]")
{
    REQUIRE_DISPLAY();
    // Lock counting interposes pthread_mutex_lock: Linux non-sanitizer builds only.
    if (!rt::lock_counting_available()) SKIP("lock counting unavailable in this build");
    run_rt_probe();
    CAPTURE(rt::locks());
    CHECK(rt::locks() == 0);
}

TEST_CASE("R-37: a mono output receives both channels of a stereo stem", "[editor][audio][R-37]")
{
    REQUIRE_DISPLAY();
    Rig rig(1);
    // L = +x, R = -x/2  ->  mono must be (L+R)/2 = x/4.
    auto st = sine_chord({ 330.0 }, 10.0, 48000.0, 0.8f, 2);
    st.applyGain(1, 0, st.getNumSamples(), -0.5f);
    const auto f = write_wav(st, 48000.0, "r37_lr");
    rig.proc->playStem(0, f);
    REQUIRE(rig.wait_for_audio(0, 1));

    juce::AudioBuffer<float> b(1, rig.block);
    float peak = 0.0f;
    for (int k = 0; k < 40; ++k) { b.clear(); rig.process(b); peak = std::max(peak, b.getMagnitude(0, rig.block)); }
    CAPTURE(peak);
    CHECK_THAT(peak, Catch::Matchers::WithinAbs(0.8 * 0.25, 0.03));   // left-only would be ~0.8
}

TEST_CASE("R-18/R-36: message-thread control racing the audio thread", "[editor][audio][R-18][R-36]")
{
    REQUIRE_DISPLAY();
    Rig rig;
    const auto f = write_wav(pink_noise(5.0, 48000.0, 9), 48000.0, "r18_noise");
    std::atomic<bool> run { true }, paused { false }, pause_req { false };
    std::thread audio([&] {
        juce::AudioBuffer<float> b(2, 256);
        juce::MidiBuffer m;
        while (run) {
            if (pause_req) { paused = true; std::this_thread::sleep_for(std::chrono::microseconds(200)); continue; }
            paused = false;
            b.clear(); rig.proc->processBlock(b, m);
            std::this_thread::sleep_for(std::chrono::microseconds(500));
        }
    });
    // Hosts never run processBlock concurrently with prepare/release, so the
    // audio thread is parked around those calls; everything else races.
    auto park   = [&] { pause_req = true; while (!paused) std::this_thread::yield(); };
    auto resume = [&] { pause_req = false; };
    const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(1);
    int i = 0;
    while (std::chrono::steady_clock::now() < until) {
        rig.proc->playStem(i % 4, f);
        rig.proc->seekStem(i % 4, 0.5);
        (void) rig.proc->getStemPosition(i % 4);
        (void) rig.proc->isStemPlaying(i % 4);
        if (i % 3 == 0) rig.proc->stopStem(i % 4);
        if (i % 20 == 0) { park(); rig.proc->releaseResources(); rig.proc->prepareToPlay(48000.0, 1024); resume(); }
        pump(1);
        ++i;
    }
    run = false;
    audio.join();
    SUCCEED("no data race");
}
