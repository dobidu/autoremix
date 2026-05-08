#pragma once
#include "PluginTypes.h"
#include <juce_core/juce_core.h>

namespace autoremix {

/**
 * Abstract interface for stem separation backends.
 * Implementations: SpleeterSeparator, AlgorithmicSeparator, DemucseSeparator, ...
 *
 * Thread safety: process() may be called from a background thread.
 * Never call from the audio thread.
 */
class IStemSeparator {
public:
    virtual ~IStemSeparator() = default;

    /** Unique identifier for this separator (e.g. "spleeter_4stems") */
    virtual std::string getId() const = 0;

    /** Human-readable name */
    virtual std::string getDisplayName() const = 0;

    /** Returns true if this separator is available (model loaded, server up, etc.) */
    virtual bool isAvailable() const = 0;

    /**
     * Separate the given audio file into stems.
     * @param input_path   Path to the input audio file (WAV/AIFF/MP3)
     * @param output_dir   Directory where stem files will be written
     * @return StemPaths   Paths to the generated stem files
     */
    virtual StemPaths separate(
        const std::filesystem::path& input_path,
        const std::filesystem::path& output_dir
    ) = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(IStemSeparator)
};

} // namespace autoremix
