#pragma once

// Real-time violation counters for tests (plan 34.5-03).
//
// While a ScopedAudioThread is alive on a thread, every operator new on that
// thread increments allocations(), and (Linux, non-sanitizer builds) every
// pthread_mutex_lock increments locks(). Defined once in AllocationCounter.cpp,
// which is compiled only into autoremix_plugin_tests.

#include <cstdint>

namespace autoremix::test::rt {

/** True when lock counting is available in this build. */
bool lock_counting_available();

uint64_t allocations();
uint64_t locks();
void     reset();

struct ScopedAudioThread {
    ScopedAudioThread();
    ~ScopedAudioThread();
    ScopedAudioThread(const ScopedAudioThread&) = delete;
    ScopedAudioThread& operator=(const ScopedAudioThread&) = delete;
};

} // namespace autoremix::test::rt
