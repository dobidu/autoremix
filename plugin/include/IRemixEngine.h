#pragma once
#include "PluginTypes.h"
#include <juce_core/juce_core.h>

namespace autoremix {

/**
 * Abstract interface for remix engine backends.
 * Implementations: ChoppedAndScrewedEngine, SlowedReverbEngine, DrumAndBassEngine, ...
 *
 * Each engine takes a StemPaths (4 stem files) + RemixParams and produces
 * a single mixed output WAV file.
 */
class IRemixEngine {
public:
    virtual ~IRemixEngine() = default;

    /** Unique identifier (e.g. "chopped_screwed") */
    virtual std::string getId() const = 0;

    /** Human-readable name */
    virtual std::string getDisplayName() const = 0;

    /** Default params for this engine */
    virtual RemixParams getDefaultParams() const = 0;

    /**
     * Apply remix transformation to the stem collection.
     * @param stems      Paths to the 4 stem WAV files
     * @param params     Processing parameters
     * @param output_path Where to write the final mixed WAV
     */
    virtual ProcessResult process(
        const StemPaths& stems,
        const RemixParams& params,
        const std::filesystem::path& output_path
    ) = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(IRemixEngine)
};

} // namespace autoremix
