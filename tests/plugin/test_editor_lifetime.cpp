// Lifetime tests: UI objects destroyed while background work is still in
// flight must never be touched afterwards (REVIEW R-01..R-04).
//
// Linked against the plugin's shared-code library (target AutoRemix), so the
// real editor, screens and status dot are exercised. Run under the asan
// preset: a use-after-free shows up as an ASan abort.

#include <catch2/catch_test_macros.hpp>

#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "ModelStatusDot.h"
#include "ScreenEmpty.h"
#include "ScreenMashup.h"

#include "SignalFactory.h"
#include "TestHelpers.h"

#include <atomic>
#include <chrono>
#include <thread>

using namespace autoremix::test;

/** Test-only access to editor internals (declared a friend in PluginEditor.h). */
struct EditorTestAccess {
    static ScreenContext& ctx(AutoRemixAudioProcessorEditor& e) { return e.ctx_; }
};

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
                            (unsigned) buf.getNumChannels(), 16, {}, 0));
    REQUIRE(w != nullptr);
    w->writeFromAudioSampleBuffer(buf, 0, buf.getNumSamples());
    return f;
}

juce::Button* find_button(juce::Component& root, const juce::String& text)
{
    for (auto* c : root.getChildren()) {
        if (auto* b = dynamic_cast<juce::Button*>(c); b && b->getButtonText() == text) return b;
        if (auto* deeper = find_button(*c, text)) return deeper;
    }
    return nullptr;
}

} // namespace

TEST_CASE("R-01: editor status callback after editor deletion", "[editor][lifetime][R-01][.][repro]")
{
    REQUIRE_DISPLAY();
    auto proc = std::make_unique<AutoRemixAudioProcessor>();
    std::unique_ptr<juce::AudioProcessorEditor> ed(proc->createEditor());
    auto* editor = dynamic_cast<AutoRemixAudioProcessorEditor*>(ed.get());
    REQUIRE(editor != nullptr);

    // A worker thread reporting status is exactly what set_status is for.
    auto set_status = EditorTestAccess::ctx(*editor).set_status;
    pump(500);      // let the status dot's startup check deliver first (that is R-03)
    ed.reset();                                     // host closes the plugin window
    set_status("late status from a worker");        // posts callAsync([this]...)
    pump(500);                                      // R-01: label of a dead editor
    proc.reset();
    SUCCEED("no use-after-free");
}

TEST_CASE("R-02: mashup completion after the screen is gone", "[editor][lifetime][R-02][.][repro]")
{
    REQUIRE_DISPLAY();
    ScreenContext ctx;
    ctx.file_path   = "a.wav";
    ctx.file_path_b = "b.wav";
    // Stand-in for the editor's run_mashup: the job finishes later, after the
    // user has left the screen (or closed the editor).
    std::function<void(autoremix::MashupResult)> pending;
    ctx.run_mashup = [&pending](autoremix::MashupParams,
                                std::function<void(autoremix::MashupResult)> done) {
        pending = std::move(done);
    };

    auto screen = std::make_unique<ScreenMashup>(ctx);
    screen->setSize(960, 520);
    auto* gen = find_button(*screen, "Generate >");
    REQUIRE(gen != nullptr);
    gen->triggerClick();                            // async click
    pump(100);                                      // click -> handleGenerate -> run_mashup
    REQUIRE(pending);
    screen.reset();                                 // user navigates away / editor closes
    juce::MessageManager::callAsync([pending] {     // the job completes afterwards
        autoremix::MashupResult r;
        r.error_message = "synthetic";
        pending(r);
    });
    pump(500);                                      // R-02: handleResult on a dead screen
    SUCCEED("no use-after-free");
}

TEST_CASE("R-03: model status dot background check after deletion", "[editor][lifetime][R-03][.][repro]")
{
    REQUIRE_DISPLAY();
    // Heap-allocated: ASan only tracks use-after-free on the heap by default.
    auto dot = std::make_unique<ModelStatusDot>();  // constructor starts a background check
    std::this_thread::sleep_for(std::chrono::milliseconds(300)); // check finishes, posts back
    dot.reset();                                    // destroyed before the post is delivered
    pump(1000);                                     // R-03: lambda reads alive_ via dead this
    SUCCEED("no use-after-free");
}

TEST_CASE("R-04: a newer file's analysis is not overwritten by an older one",
          "[editor][lifetime][R-04][!shouldfail]")
{
    REQUIRE_DISPLAY();
    const double sr = 44100.0;
    const auto slow = write_wav(click_track(90.0, 8.0, sr), sr, "r04_slow_90");
    const auto fast = write_wav(click_track(128.0, 2.0, sr), sr, "r04_fast_128");

    ScreenContext ctx;
    // Deterministic analysis: the first file takes longer than the second.
    ScreenEmpty::AnalyzeFn analyze = [slow](const juce::String& path) {
        autoremix::FileAnalysis fa;
        if (juce::File(path) == slow) {
            std::this_thread::sleep_for(std::chrono::milliseconds(3000));
            fa.bpm = 90.0f;
        } else {
            fa.bpm = 128.0f;
        }
        fa.key = "C";
        return fa;
    };
    ScreenEmpty screen(ctx, analyze, [] {});
    screen.setSize(960, 520);

    screen.fileWasSelected(slow);
    screen.fileWasSelected(fast);                   // user changes their mind quickly
    pump(8000);                                     // both analyses finish, slow one last

    CHECK(ctx.file_path == fast.getFullPathName());
    CHECK(ctx.detected_bpm == 128.0f);              // R-04b: stale result must be dropped
}
