#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// ModelDownloader — Phase 26-02
//
// First-launch download of htdemucs.onnx into the user cache dir.
// SHA256-verified, retried up to 3 times. If the cached file exists and
// the SHA matches the pinned constant, ensure_htdemucs() returns
// immediately without touching the network.
//
// Public API:
//   autoremix::dsp::models::DownloadStatus
//   autoremix::dsp::models::htdemucs_cache_path() → juce::File
//   autoremix::dsp::models::ensure_htdemucs(progress_cb) → DownloadStatus
//
// Hosting URL + SHA256 of the ONNX file are pinned compile-time
// constants. To bump the model, regenerate via tools/export_demucs_onnx.py,
// upload to a new release tag, and update both constants.
// ─────────────────────────────────────────────────────────────────────────────

#include <cstdint>
#include <fstream>
#include <functional>
#include <string>

#include <cpr/cpr.h>
#include <juce_core/juce_core.h>
#include <juce_cryptography/juce_cryptography.h>

namespace autoremix::dsp::models {

// Hosting URL — pinned to a specific release tag so old plugin
// versions keep finding their model even after the next ship.
inline constexpr const char* kHtdemucsURL =
    "https://github.com/dobidu/autoremix/releases/download/"
    "v4.0.0-models/htdemucs.onnx";

// SHA256 of the .onnx file produced by tools/export_demucs_onnx.py
// at Phase 26-01.
inline constexpr const char* kHtdemucsSHA256 =
    "83a570fb273a411348a94564fb726d67b483f6c30f09fb95d1d7ef4dacc17c36";

// Sanity-check file-size bound. Catches truncated downloads quickly
// before running an SHA256 over hundreds of MB.
inline constexpr std::int64_t kHtdemucsMinBytes = 300LL * 1024 * 1024;   // 300 MB
inline constexpr std::int64_t kHtdemucsMaxBytes = 450LL * 1024 * 1024;   // 450 MB

struct DownloadStatus {
    bool         ok = false;
    juce::String error;
    juce::File   path;
};

inline juce::File htdemucs_cache_path()
{
    return juce::File::getSpecialLocation(
               juce::File::userApplicationDataDirectory)
           .getChildFile("autoremix")
           .getChildFile("models")
           .getChildFile("htdemucs.onnx");
}

namespace detail {

inline juce::String sha256_of_file(const juce::File& f)
{
    juce::FileInputStream stream(f);
    if (!stream.openedOk())
        return {};
    juce::SHA256 hash(stream);
    return hash.toHexString();
}

inline bool size_is_plausible(const juce::File& f)
{
    const auto sz = f.getSize();
    return sz >= kHtdemucsMinBytes && sz <= kHtdemucsMaxBytes;
}

// Streaming download via libcpr → write straight to file. Yields the
// in-flight progress fraction to the optional caller callback.
inline bool download_with_progress(
    const std::string& url,
    const juce::File&  out_file,
    std::function<void(double)> progress_cb)
{
    std::ofstream out(out_file.getFullPathName().toStdString(),
                      std::ios::binary | std::ios::trunc);
    if (!out.is_open())
        return false;

    cpr::Response r = cpr::Download(
        out,
        cpr::Url{url},
        cpr::ProgressCallback{
            [&progress_cb](cpr::cpr_off_t downloadTotal,
                           cpr::cpr_off_t downloadNow,
                           cpr::cpr_off_t /*uploadTotal*/,
                           cpr::cpr_off_t /*uploadNow*/,
                           intptr_t /*userdata*/) -> bool {
                if (progress_cb && downloadTotal > 0) {
                    progress_cb(static_cast<double>(downloadNow)
                                / static_cast<double>(downloadTotal));
                }
                return true; // keep going
            }});

    out.close();
    return r.status_code >= 200 && r.status_code < 300;
}

} // namespace detail

inline DownloadStatus
ensure_htdemucs(std::function<void(double)> progress_cb = {})
{
    DownloadStatus status;
    status.path = htdemucs_cache_path();

    // Fast path: cached + valid.
    if (status.path.existsAsFile()
        && detail::size_is_plausible(status.path)
        && detail::sha256_of_file(status.path).equalsIgnoreCase(kHtdemucsSHA256))
    {
        status.ok = true;
        return status;
    }

    // Cached but corrupted / wrong SHA → wipe and re-download.
    if (status.path.existsAsFile())
        status.path.deleteFile();

    auto dir = status.path.getParentDirectory();
    if (!dir.exists() && !dir.createDirectory().wasOk()) {
        status.error = "Could not create model cache directory: "
                       + dir.getFullPathName();
        return status;
    }

    juce::File tmp = status.path.getSiblingFile("htdemucs.onnx.tmp");

    for (int attempt = 0; attempt < 3; ++attempt) {
        tmp.deleteFile();

        const bool got = detail::download_with_progress(
            kHtdemucsURL, tmp, progress_cb);

        if (got
            && detail::size_is_plausible(tmp)
            && detail::sha256_of_file(tmp).equalsIgnoreCase(kHtdemucsSHA256))
        {
            tmp.moveFileTo(status.path);
            status.ok = true;
            return status;
        }

        // Attempt failed — clean up and retry.
        tmp.deleteFile();
    }

    status.error = "Download failed after 3 attempts (URL=" +
                   juce::String(kHtdemucsURL) + ")";
    return status;
}

} // namespace autoremix::dsp::models
