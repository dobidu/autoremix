#include "AudioBridge.h"
#include <cpr/cpr.h>
#include <nlohmann/json.hpp>

namespace autoremix {

AudioBridge::AudioBridge(const std::string& host, int port)
    : base_url_(host), port_(port) {}

bool AudioBridge::isServerAlive() const {
    try {
        auto r = cpr::Get(cpr::Url{makeUrl("/api/v1/health")},
                          cpr::Timeout{500});
        return r.status_code == 200;
    } catch (...) { return false; }
}

StemPaths AudioBridge::separateStems(
    const std::filesystem::path& input_path,
    const std::filesystem::path& output_dir,
    const std::string& separator_id) {
    try {
        nlohmann::json body = {
            {"input_path",   input_path.string()},
            {"output_dir",   output_dir.string()},
            {"separator_id", separator_id}
        };

        auto r = cpr::Post(
            cpr::Url{makeUrl("/api/v1/separate")},
            cpr::Header{{"Content-Type", "application/json"}},
            cpr::Body{body.dump()},
            cpr::Timeout{30000}
        );

        if (r.status_code != 200) return {};

        auto resp = nlohmann::json::parse(r.text);
        if (!resp.value("success", false)) return {};

        auto& s = resp.at("stems");
        StemPaths result;
        result.vocals = s.at("vocals").get<std::string>();
        result.drums  = s.at("drums").get<std::string>();
        result.bass   = s.at("bass").get<std::string>();
        result.other  = s.at("other").get<std::string>();
        result.valid  = true;
        return result;

    } catch (...) { return {}; }
}

ProcessResult AudioBridge::applyRemix(
    const StemPaths& stems,
    const RemixParams& params,
    const std::filesystem::path& output_path) {
    try {
        nlohmann::json body = {
            {"vocals_path",        stems.vocals.string()},
            {"drums_path",         stems.drums.string()},
            {"bass_path",          stems.bass.string()},
            {"other_path",         stems.other.string()},
            {"output_path",        output_path.string()},
            {"engine_id",          params.engine_id},
            {"tempo_factor",       params.tempo_factor},
            {"pitch_shift_semi",   params.pitch_shift_semi},
            {"reverb_mix",         params.reverb_mix},
            {"chop_interval_ms",   params.chop_interval_ms},
            {"bass_boost_db",      params.bass_boost_db},
            {"drums_tempo_factor", params.drums_tempo_factor}
        };

        auto r = cpr::Post(
            cpr::Url{makeUrl("/api/v1/remix")},
            cpr::Header{{"Content-Type", "application/json"}},
            cpr::Body{body.dump()},
            cpr::Timeout{60000}
        );

        if (r.status_code != 200) return {};

        auto resp = nlohmann::json::parse(r.text);
        if (!resp.value("success", false))
            return {false, resp.value("error", "remix failed")};

        ProcessResult result;
        result.success     = true;
        result.output_path = resp.at("output_path").get<std::string>();
        return result;

    } catch (...) { return {}; }
}

bool AudioBridge::startSidecar(const std::filesystem::path&) { return false; }
void AudioBridge::stopSidecar() {}

std::string AudioBridge::makeUrl(const std::string& endpoint) const {
    return base_url_ + ":" + std::to_string(port_) + endpoint;
}

}  // namespace autoremix
