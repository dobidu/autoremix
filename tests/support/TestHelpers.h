#pragma once

#include <catch2/catch_test_macros.hpp>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>

namespace autoremix::test {

/** True when a GUI can be created (Linux needs an X display). */
inline bool has_display()
{
   #if JUCE_LINUX || JUCE_BSD
    const char* d = std::getenv("DISPLAY");
    return d != nullptr && *d != '\0';
   #else
    return true;
   #endif
}

/** Lazily-created JUCE GUI initialiser. Pure DSP tests never create it, so
 *  they never touch X11. test_main.cpp destroys it before exit. */
inline std::unique_ptr<juce::ScopedJuceInitialiser_GUI>& gui_initialiser()
{
    static std::unique_ptr<juce::ScopedJuceInitialiser_GUI> init;
    return init;
}

inline void ensure_gui()
{
    if (!gui_initialiser()) gui_initialiser() = std::make_unique<juce::ScopedJuceInitialiser_GUI>();
}

inline void shutdown_gui() { gui_initialiser().reset(); }

/** FNV-1a 64-bit over the raw float data of every channel (determinism tests). */
inline uint64_t buffer_hash(const juce::AudioBuffer<float>& b)
{
    uint64_t h = 1469598103934665603ull;
    for (int ch = 0; ch < b.getNumChannels(); ++ch) {
        const auto* p = reinterpret_cast<const unsigned char*>(b.getReadPointer(ch));
        const size_t n = (size_t) b.getNumSamples() * sizeof(float);
        for (size_t i = 0; i < n; ++i) { h ^= p[i]; h *= 1099511628211ull; }
    }
    return h;
}

inline bool approx_db(double a, double b, double tol_db) { return std::abs(a - b) <= tol_db; }

} // namespace autoremix::test

/** Skip a GUI test cleanly when no display is available; otherwise make sure
 *  the JUCE GUI subsystem is up. */
#define REQUIRE_DISPLAY()                                              \
    do {                                                               \
        if (!autoremix::test::has_display()) SKIP("no display");       \
        autoremix::test::ensure_gui();                                 \
    } while (false)
