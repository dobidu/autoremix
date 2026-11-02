// User preset JSON must not be able to hang or crash a render (REVIEW R-06).
// Loader-level tests check that every op parameter ends up inside a safe
// range; render-level tests run the chain on synthetic stems with a timeout.

#include <catch2/catch_test_macros.hpp>

#include "dsp/NativePresetLoaders.h"
#include "dsp/NativeEffectChainEngine.h"

#include "SignalFactory.h"
#include "TestHelpers.h"

#include <chrono>
#include <cmath>
#include <future>
#include <thread>

using namespace autoremix::test;
namespace presets = autoremix::dsp::presets;
namespace engines = autoremix::dsp::engines;

namespace {

nlohmann::json hostile_preset()
{
    // JSON cannot carry NaN/inf (nlohmann rejects 1e999 as overflow), so the
    // "absurd number" case is a huge finite value.
    return nlohmann::json::parse(R"({
      "id": "hostile",
      "engine": "effect_chain",
      "effects": [
        { "op": "chop_bars",    "stems": "all", "params": { "beats_per_bar": 0, "repeat": 2 } },
        { "op": "sidechain",    "stems": "all", "params": { "duck_rate_hz": 1e9 } },
        { "op": "time_stretch", "stems": "all", "params": { "factor": 0 } },
        { "op": "chop_beats",   "stems": "all", "params": { "repeat": 1000000000 } },
        { "op": "delay_tape",   "stems": "all", "params": { "feedback": 5, "delay_ms": 1e300 } }
      ]
    })");
}

const engines::EffectStep& step(const presets::NativeRemixPreset& p, const std::string& op)
{
    for (const auto& s : p.effects) if (s.op == op) return s;
    FAIL("op not loaded: " << op);
    return p.effects.front();
}

autoremix::dsp::separators::NativeStems synthetic_stems(double sr)
{
    autoremix::dsp::separators::NativeStems st;
    st.drums  = click_track(120.0, 4.0, sr);
    st.bass   = sine_chord({ 55.0 }, 4.0, sr, 0.4f);
    st.vocals = sine_chord({ 440.0, 660.0 }, 4.0, sr, 0.3f);
    st.other  = pink_noise(4.0, sr, 7, 0.1f);
    return st;
}

/** Runs fn on a worker; returns false if it doesn't finish in `limit`. The
 *  worker is detached on timeout (the process is about to report a failure). */
template <typename Fn>
bool finishes_within(std::chrono::seconds limit, Fn fn)
{
    auto done = std::make_shared<std::promise<void>>();
    auto fut  = done->get_future();
    std::thread([done, fn]() mutable { fn(); done->set_value(); }).detach();
    return fut.wait_for(limit) == std::future_status::ready;
}

} // namespace

TEST_CASE("R-06: hostile op parameters are clamped at load", "[unit][preset][R-06][!shouldfail]")
{
    const auto p = presets::parse_remix_preset(hostile_preset());
    CHECK(step(p, "chop_bars").params.beats_per_bar >= 1);
    CHECK(step(p, "sidechain").params.duck_rate_hz <= 50.0);
    CHECK(step(p, "time_stretch").params.factor >= 0.25);
    CHECK(step(p, "chop_beats").params.repeat <= 64);
    CHECK(step(p, "delay_tape").params.feedback <= 0.95);
    CHECK(step(p, "delay_tape").params.delay_ms <= 4000.0);
}

// Render-level repro. Hidden: on unfixed code it divides by zero (SIGFPE),
// overflows a buffer size, or loops forever, which kills the test process.
TEST_CASE("R-06: hostile preset renders finite audio within 10 s", "[preset][R-06][.][repro]")
{
    const auto p  = presets::parse_remix_preset(hostile_preset());
    const auto st = synthetic_stems(44100.0);
    auto out = std::make_shared<juce::AudioBuffer<float>>();

    const bool ok = finishes_within(std::chrono::seconds(10), [&p, &st, out] {
        *out = engines::process_effect_chain(st, p.effects, p.stem_mix, 44100.0);
    });
    if (!ok) { FAIL_CHECK("render did not return within 10 s"); std::quick_exit(1); }

    REQUIRE(out->getNumSamples() > 0);
    for (int ch = 0; ch < out->getNumChannels(); ++ch)
        for (int i = 0; i < out->getNumSamples(); ++i)
            REQUIRE(std::isfinite(out->getSample(ch, i)));
}
