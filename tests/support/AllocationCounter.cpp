// See AllocationCounter.h. Replaces the global allocation functions for the
// whole test executable; they forward to malloc/free, which the sanitizers
// still intercept.

#include "AllocationCounter.h"

#include <atomic>
#include <cstdlib>
#include <new>

#if defined(__linux__) && !defined(__SANITIZE_ADDRESS__) && !defined(__SANITIZE_THREAD__)
 #define AUTOREMIX_COUNT_LOCKS 1
 #include <dlfcn.h>
 #include <pthread.h>
#else
 #define AUTOREMIX_COUNT_LOCKS 0
#endif

namespace {
thread_local bool         t_audio_thread = false;
std::atomic<uint64_t>     g_allocs { 0 };
std::atomic<uint64_t>     g_locks  { 0 };

inline void note_alloc() noexcept
{
    if (t_audio_thread) g_allocs.fetch_add(1, std::memory_order_relaxed);
}

void* alloc_or_throw(std::size_t n)
{
    note_alloc();
    if (void* p = std::malloc(n ? n : 1)) return p;
    throw std::bad_alloc();
}

void* aligned_alloc_or_throw(std::size_t n, std::align_val_t al)
{
    note_alloc();
    const std::size_t a = static_cast<std::size_t>(al);
    void* p = nullptr;
    if (posix_memalign(&p, a < sizeof(void*) ? sizeof(void*) : a, n ? n : 1) != 0) throw std::bad_alloc();
    return p;
}
} // namespace

namespace autoremix::test::rt {
bool     lock_counting_available() { return AUTOREMIX_COUNT_LOCKS != 0; }
uint64_t allocations()             { return g_allocs.load(); }
uint64_t locks()                   { return g_locks.load(); }
void     reset()                   { g_allocs = 0; g_locks = 0; }
ScopedAudioThread::ScopedAudioThread()  { t_audio_thread = true; }
ScopedAudioThread::~ScopedAudioThread() { t_audio_thread = false; }
} // namespace autoremix::test::rt

// ── Global allocation functions ─────────────────────────────────────────────
void* operator new  (std::size_t n)                                   { return alloc_or_throw(n); }
void* operator new[](std::size_t n)                                   { return alloc_or_throw(n); }
void* operator new  (std::size_t n, const std::nothrow_t&) noexcept   { note_alloc(); return std::malloc(n ? n : 1); }
void* operator new[](std::size_t n, const std::nothrow_t&) noexcept   { note_alloc(); return std::malloc(n ? n : 1); }
void* operator new  (std::size_t n, std::align_val_t a)               { return aligned_alloc_or_throw(n, a); }
void* operator new[](std::size_t n, std::align_val_t a)               { return aligned_alloc_or_throw(n, a); }
void* operator new  (std::size_t n, std::align_val_t a, const std::nothrow_t&) noexcept
{ try { return aligned_alloc_or_throw(n, a); } catch (...) { return nullptr; } }
void* operator new[](std::size_t n, std::align_val_t a, const std::nothrow_t&) noexcept
{ try { return aligned_alloc_or_throw(n, a); } catch (...) { return nullptr; } }

void operator delete  (void* p) noexcept                              { std::free(p); }
void operator delete[](void* p) noexcept                              { std::free(p); }
void operator delete  (void* p, std::size_t) noexcept                 { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept                 { std::free(p); }
void operator delete  (void* p, const std::nothrow_t&) noexcept       { std::free(p); }
void operator delete[](void* p, const std::nothrow_t&) noexcept       { std::free(p); }
void operator delete  (void* p, std::align_val_t) noexcept            { std::free(p); }
void operator delete[](void* p, std::align_val_t) noexcept            { std::free(p); }
void operator delete  (void* p, std::size_t, std::align_val_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t, std::align_val_t) noexcept { std::free(p); }
void operator delete  (void* p, std::align_val_t, const std::nothrow_t&) noexcept { std::free(p); }
void operator delete[](void* p, std::align_val_t, const std::nothrow_t&) noexcept { std::free(p); }

#if AUTOREMIX_COUNT_LOCKS
// Interpose pthread_mutex_lock in the executable so that locks taken on a
// thread marked as audio are counted (std::mutex, juce::CriticalSection, ...).
// The real function is resolved once by a constructor that runs before
// main(), while the process is single-threaded. If dlsym itself takes a lock
// during that resolution, the recursive call finds real_lock == nullptr and
// returns without locking, which is harmless with only one thread.
namespace { using LockFn = int (*)(pthread_mutex_t*); LockFn real_lock = nullptr; }

__attribute__((constructor(101))) static void autoremix_resolve_mutex_lock()
{
    real_lock = reinterpret_cast<LockFn>(dlsym(RTLD_NEXT, "pthread_mutex_lock"));
}

extern "C" int pthread_mutex_lock(pthread_mutex_t* m)
{
    if (real_lock == nullptr) return 0;
    if (t_audio_thread) g_locks.fetch_add(1, std::memory_order_relaxed);
    return real_lock(m);
}
#endif
