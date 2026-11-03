#pragma once

// JobRunner — background work with guarded, message-thread completions.
//
// Rules this enforces (REVIEW R-01..R-04, R-19, R-38):
//  * Work runs on a pool owned by the processor, never on a detached thread.
//  * Work functions receive everything by value (or a reference to something
//    the processor owns and outlives the pool); they never capture a UI `this`.
//  * Completions and progress run on the message thread, and only while the
//    receiver's Lifetime is still alive. Receivers are created and destroyed on
//    the message thread, so checking the weak pointer there is race-free.
//  * Every job has a CancelToken; cancelAll() + waitForAll() bound shutdown.
//
// The audio thread never touches this class.

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>

#include <algorithm>
#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <type_traits>
#include <utility>
#include <vector>

namespace autoremix::jobs {

using CancelToken = std::shared_ptr<std::atomic<bool>>;
using Lifetime    = std::shared_ptr<int>;   // owned by the receiver (screen, editor, widget)
using Receiver    = std::weak_ptr<int>;     // what jobs and callbacks hold

inline Lifetime    make_lifetime()     { return std::make_shared<int>(0); }
inline CancelToken make_cancel_token() { return std::make_shared<std::atomic<bool>>(false); }
inline bool        is_cancelled(const CancelToken& t) { return t && t->load(std::memory_order_relaxed); }

/** Unique per-job output directory under the AutoRemix temp root, so a re-run
 *  never shares files with a cancelled or still-running job (R-19). */
inline juce::File unique_job_dir(const juce::String& kind, const juce::String& stem)
{
    static std::atomic<uint32_t> counter { 0 };
    const auto id = juce::String::toHexString((int) juce::Time::currentTimeMillis())
                  + "_" + juce::String((int) counter.fetch_add(1));
    auto dir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                   .getChildFile("autoremix").getChildFile(kind)
                   .getChildFile(stem + "_" + id);
    dir.createDirectory();
    return dir;
}

/** Post fn to the message thread; run it only if `receiver` is still alive.
 *  Safe to call from any thread. */
inline void post_guarded(Receiver receiver, std::function<void()> fn)
{
    juce::MessageManager::callAsync([receiver = std::move(receiver), fn = std::move(fn)] {
        if (receiver.lock()) fn();
    });
}

/** Wrap a message-thread callback so it becomes a no-op once `receiver` dies.
 *  The returned function must itself be invoked on the message thread. */
template <typename... Args, typename Fn>
std::function<void(Args...)> guarded(Receiver receiver, Fn fn)
{
    return [receiver = std::move(receiver), fn = std::move(fn)](Args... args) mutable {
        if (receiver.lock()) fn(std::forward<Args>(args)...);
    };
}

/** Coalescing progress reporter: worker calls report(frac) as often as it
 *  likes; at most one post is pending at a time and it carries the latest
 *  value. Cheap to copy (shared state). */
class ProgressSink
{
public:
    ProgressSink() = default;
    ProgressSink(Receiver receiver, std::function<void(double)> on_progress)
        : state_(std::make_shared<State>())
    {
        state_->receiver    = std::move(receiver);
        state_->on_progress = std::move(on_progress);
    }

    void report(double frac) const
    {
        if (!state_ || !state_->on_progress) return;
        state_->latest.store(frac, std::memory_order_relaxed);
        if (state_->pending.exchange(true)) return;          // a post is already queued
        auto st = state_;
        post_guarded(st->receiver, [st] {
            st->pending.store(false);
            st->on_progress(st->latest.load(std::memory_order_relaxed));
        });
    }

    explicit operator bool() const { return state_ && state_->on_progress != nullptr; }

private:
    struct State {
        Receiver                    receiver;
        std::function<void(double)> on_progress;
        std::atomic<double>         latest { 0.0 };
        std::atomic<bool>           pending { false };
    };
    std::shared_ptr<State> state_;
};

class JobRunner
{
public:
    explicit JobRunner(int threads = 2)
        : pool_(juce::ThreadPoolOptions{}
                    .withThreadName("AutoRemix jobs")
                    .withNumberOfThreads(threads))
    {}

    ~JobRunner()
    {
        cancelAll();
        waitForAll(5000);
    }

    /** Run work(token) on the pool, then done(result, cancelled) on the
     *  message thread if `receiver` is alive. A job cancelled before it
     *  starts still reports done(R{}, true). Returns the job's token. */
    template <typename R>
    CancelToken submit(Receiver receiver,
                       std::function<R(const CancelToken&)> work,
                       std::function<void(R, bool cancelled)> done)
    {
        static_assert(std::is_default_constructible_v<R>, "job results must be default-constructible");
        auto token = make_cancel_token();
        remember(token);
        std::function<juce::ThreadPoolJob::JobStatus()> job =
            [receiver, token, work = std::move(work), done = std::move(done)]() mutable {
            R result{};
            if (!is_cancelled(token)) {
                try { result = work(token); }
                catch (const std::exception& e) {
                    juce::Logger::writeToLog(juce::String("[jobs] uncaught: ") + e.what());
                }
                catch (...) {
                    juce::Logger::writeToLog("[jobs] uncaught non-std exception");
                }
            }
            if (done) {
                auto shared = std::make_shared<R>(std::move(result));
                post_guarded(receiver, [done = std::move(done), shared, token]() mutable {
                    done(std::move(*shared), is_cancelled(token));
                });
            }
            return juce::ThreadPoolJob::jobHasFinished;
        };
        pool_.addJob(std::move(job));
        return token;
    }

    /** Ask every live job to stop at its next cancellation check. */
    void cancelAll()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& w : tokens_)
            if (auto t = w.lock()) t->store(true);
    }

    /** Wait up to `timeout_ms` for the pool to drain. Queued jobs still run
     *  (a cancelled one skips its work and reports done(R{}, true)), so every
     *  submitted job reports exactly once while its receiver lives. Returns
     *  true if the pool is idle. Don't call it while holding anything a job
     *  waits for. */
    bool waitForAll(int timeout_ms)
    {
        const auto deadline = juce::Time::getMillisecondCounter() + (juce::uint32) juce::jmax(0, timeout_ms);
        while (pool_.getNumJobs() > 0) {
            if (juce::Time::getMillisecondCounter() >= deadline) return false;
            juce::Thread::sleep(1);
        }
        return true;
    }

    int numJobs() const { return pool_.getNumJobs(); }

private:
    void remember(const CancelToken& t)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        tokens_.erase(std::remove_if(tokens_.begin(), tokens_.end(),
                                     [](const std::weak_ptr<std::atomic<bool>>& w) { return w.expired(); }),
                      tokens_.end());
        tokens_.push_back(t);
    }

    juce::ThreadPool                                pool_;
    std::mutex                                      mutex_;
    std::vector<std::weak_ptr<std::atomic<bool>>>   tokens_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(JobRunner)
};

} // namespace autoremix::jobs
