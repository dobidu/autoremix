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

TEST_CASE("R-06: hostile op parameters are clamped at load", "[unit][preset][R-06]")
{
    const auto p = presets::parse_remix_preset(hostile_preset());
    CHECK(step(p, "chop_bars").params.beats_per_bar >= 1);
    CHECK(step(p, "sidechain").params.duck_rate_hz <= 50.0);
    CHECK(step(p, "time_stretch").params.factor >= 0.25);
    CHECK(step(p, "chop_beats").params.repeat <= 64);
    CHECK(step(p, "delay_tape").params.feedback <= 0.95);
    CHECK(step(p, "delay_tape").params.delay_ms <= 4000.0);
}

// Render-level check. On the unfixed code this divided by zero (SIGFPE),
// overflowed a buffer size, or looped forever.
TEST_CASE("R-06: hostile preset renders finite audio in bounded time", "[preset][R-06]")
{
    const auto p  = presets::parse_remix_preset(hostile_preset());
    const auto st = synthetic_stems(44100.0);
    auto out = std::make_shared<juce::AudioBuffer<float>>();

    const bool ok = finishes_within(std::chrono::seconds(10 * kTimeScale), [&p, &st, out] {
        *out = engines::process_effect_chain(st, p.effects, p.stem_mix, 44100.0);
    });
    if (!ok) { FAIL_CHECK("render did not return within " << 10 * kTimeScale << " s"); std::quick_exit(1); }

    REQUIRE(out->getNumSamples() > 0);
    for (int ch = 0; ch < out->getNumChannels(); ++ch)
        for (int i = 0; i < out->getNumSamples(); ++i)
            REQUIRE(std::isfinite(out->getSample(ch, i)));
}

TEST_CASE("every op parameter row has a sane range containing its default", "[unit][preset]")
{
    const autoremix::dsp::ops::OpParams defaults;
    for (const auto& r : presets::detail::OpParamKeys::doubles()) {
        CAPTURE(r.key);
        REQUIRE(r.lo < r.hi);
        REQUIRE(defaults.*(r.member) >= r.lo);
        REQUIRE(defaults.*(r.member) <= r.hi);
    }
    for (const auto& r : presets::detail::OpParamKeys::ints()) {
        CAPTURE(r.key);
        REQUIRE(r.lo < r.hi);
        REQUIRE((double) (defaults.*(r.member)) >= r.lo);
        REQUIRE((double) (defaults.*(r.member)) <= r.hi);
    }
    REQUIRE(presets::detail::op_param_range("feedback").known);
    REQUIRE_FALSE(presets::detail::op_param_range("no_such_key").known);
}

TEST_CASE("clamping leaves every built-in preset's values unchanged", "[unit][preset]")
{
    const juce::File dir(AUTOREMIX_PRESET_SOURCE_DIR);
    REQUIRE(dir.isDirectory());
    int checked = 0;
    for (const auto& f : dir.findChildFiles(juce::File::findFiles, false, "*.json")) {
        if (f.getFileName() == "schema.json") continue;
        const auto raw = nlohmann::json::parse(f.loadFileAsString().toStdString());
        if (!raw.contains("effects")) continue;
        const auto p = presets::parse_remix_preset(raw);
        size_t k = 0;
        for (const auto& eff : raw["effects"]) {
            if (!engines::is_known_op(eff.value("op", std::string{}))) continue;
            REQUIRE(k < p.effects.size());
            const auto& loaded = p.effects[k++].params;
            if (!eff.contains("params")) continue;
            for (const auto& r : presets::detail::OpParamKeys::doubles())
                if (eff["params"].contains(r.key)) {
                    CAPTURE(f.getFileName(), r.key);
                    REQUIRE(loaded.*(r.member) == eff["params"][r.key].get<double>());
                    ++checked;
                }
            for (const auto& r : presets::detail::OpParamKeys::ints())
                if (eff["params"].contains(r.key)) {
                    CAPTURE(f.getFileName(), r.key);
                    REQUIRE(loaded.*(r.member) == eff["params"][r.key].get<int>());
                    ++checked;
                }
        }
    }
    CAPTURE(checked);
    REQUIRE(checked > 20);
}
