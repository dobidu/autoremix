#pragma once
#include "PluginTypes.h"
#include <string>
#include <optional>
#include <filesystem>
#include <vector>

namespace autoremix {

/**
 * HTTP bridge between the JUCE plugin and the Python sidecar.
 * All methods are synchronous and block until response received.
 * Call from a background thread only.
 */
class AudioBridge {
public:
    explicit AudioBridge(
        const std::string& host = "http://127.0.0.1",
        int port = 17432
    );

    /** Check if the Python sidecar is alive */
    bool isServerAlive() const;

    /** Fetch available remix presets from the sidecar. Returns empty on failure. */
    std::vector<PresetInfo> getPresets();

    /**
     * Ask the sidecar to separate stems.
     * separator_id: "spleeter_4stems", "algorithmic", ...
     */
    StemPaths separateStems(
        const std::filesystem::path& input_path,
        const std::filesystem::path& output_dir,
        const std::string& separator_id = "spleeter_4stems"
    );

    /**
     * Ask the sidecar to apply remix.
     * Takes the 4 stem paths, returns path to output WAV.
     */
    ProcessResult applyRemix(
        const StemPaths& stems,
        const RemixParams& params,
        const std::filesystem::path& output_path
    );

    /** Start the Python sidecar as a child process */
    bool startSidecar(const std::filesystem::path& server_script_path);

    /** Stop the Python sidecar */
    void stopSidecar();

private:
    std::string base_url_;
    int port_;
    int sidecar_pid_ = -1;

    std::string makeUrl(const std::string& endpoint) const;
};

} // namespace autoremix
