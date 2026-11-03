// JobRunner contract (plan 34.5-02, AC-3): completions run on the message
// thread, only for live receivers; cancellation is reported; shutdown is
// bounded; concurrent submit/cancel is race-free (run under TSAN).

#include <catch2/catch_test_macros.hpp>

#include "jobs/JobRunner.h"

#include "TestHelpers.h"

#include <atomic>
#include <chrono>
#include <thread>

using namespace autoremix::jobs;

namespace {
void pump(int ms) { juce::MessageManager::getInstance()->runDispatchLoopUntil(ms); }
}

TEST_CASE("completion runs on the message thread with the work result", "[unit][jobs]")
{
    REQUIRE_DISPLAY();
    JobRunner runner;
    auto alive = make_lifetime();
    int  got = 0;
    bool on_msg_thread = false;
    runner.submit<int>(alive, [](const CancelToken&) { return 42; },
                       [&](int v, bool cancelled) {
                           REQUIRE_FALSE(cancelled);
                           got = v;
                           on_msg_thread = juce::MessageManager::getInstance()->isThisTheMessageThread();
                       });
    for (int i = 0; i < 50 && got == 0; ++i) pump(20);
    REQUIRE(got == 42);
    REQUIRE(on_msg_thread);
}

TEST_CASE("completion is dropped when the receiver dies first", "[unit][jobs]")
{
    REQUIRE_DISPLAY();
    JobRunner runner;
    auto alive = make_lifetime();
    std::atomic<bool> release { false };
    bool called = false;
    runner.submit<int>(alive,
                       [&](const CancelToken&) { while (!release) std::this_thread::sleep_for(std::chrono::milliseconds(1)); return 1; },
                       [&](int, bool) { called = true; });
    alive.reset();                 // receiver destroyed while the job runs
    release = true;
    REQUIRE(runner.waitForAll(2000));
    pump(200);
    REQUIRE_FALSE(called);
}

TEST_CASE("cancel before start and cancel while running both report cancelled", "[unit][jobs]")
{
    REQUIRE_DISPLAY();
    JobRunner runner(1);
    auto alive = make_lifetime();
    std::atomic<bool> started { false };
    int reports = 0, cancelled_reports = 0, work_runs = 0;
    auto on_done = [&](int, bool c) { ++reports; if (c) ++cancelled_reports; };

    auto running = runner.submit<int>(alive, [&](const CancelToken& t) {
        started = true; ++work_runs;
        while (!is_cancelled(t)) std::this_thread::sleep_for(std::chrono::milliseconds(1));
        return 0;
    }, on_done);
    auto queued = runner.submit<int>(alive, [&](const CancelToken&) { ++work_runs; return 0; }, on_done);
    while (!started) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    queued->store(true);           // cancelled before it starts (single thread busy)
    running->store(true);          // cancelled while running
    for (int i = 0; i < 100 && reports < 2; ++i) pump(20);

    REQUIRE(reports == 2);
    REQUIRE(cancelled_reports == 2);
    REQUIRE(work_runs == 1);       // the queued job never ran its work
}

TEST_CASE("cancelAll + waitForAll bound shutdown", "[unit][jobs]")
{
    REQUIRE_DISPLAY();
    JobRunner runner(2);
    auto alive = make_lifetime();
    for (int i = 0; i < 4; ++i)
        runner.submit<int>(alive, [](const CancelToken& t) {
            while (!is_cancelled(t)) std::this_thread::sleep_for(std::chrono::milliseconds(1));
            return 0;
        }, {});
    const auto t0 = std::chrono::steady_clock::now();
    runner.cancelAll();
    REQUIRE(runner.waitForAll(2000));
    REQUIRE(std::chrono::steady_clock::now() - t0 < std::chrono::seconds(2));
}

TEST_CASE("100 concurrent submit/cancel cycles are race-free", "[unit][jobs][tsan]")
{
    REQUIRE_DISPLAY();
    JobRunner runner(4);
    auto alive = make_lifetime();
    std::atomic<int> done_count { 0 };
    std::vector<std::thread> producers;
    for (int p = 0; p < 4; ++p)
        producers.emplace_back([&] {
            for (int i = 0; i < 25; ++i) {
                auto t = runner.submit<int>(alive, [](const CancelToken& tok) {
                    for (int k = 0; k < 200 && !is_cancelled(tok); ++k) {}
                    return 1;
                }, [&](int, bool) { done_count.fetch_add(1); });
                if (i % 3 == 0) t->store(true);
                if (i % 7 == 0) runner.cancelAll();
            }
        });
    for (auto& t : producers) t.join();
    REQUIRE(runner.waitForAll(5000));
    for (int i = 0; i < 100 && done_count < 100; ++i) pump(20);
    REQUIRE(done_count == 100);
}

TEST_CASE("ProgressSink coalesces and delivers the latest value", "[unit][jobs]")
{
    REQUIRE_DISPLAY();
    auto alive = make_lifetime();
    double last = -1.0;
    int calls = 0;
    ProgressSink sink(alive, [&](double v) { last = v; ++calls; });
    std::thread([sink] { for (int i = 1; i <= 1000; ++i) sink.report(i / 1000.0); }).join();
    pump(200);
    REQUIRE(last == 1.0);
    REQUIRE(calls >= 1);
    REQUIRE(calls < 1000);
}
