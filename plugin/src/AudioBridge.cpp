#include "AudioBridge.h"
#include <cpr/cpr.h>
#include <nlohmann/json.hpp>
#include <chrono>
#include <thread>
#include <unordered_map>
#include <vector>

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  define NOMINMAX
#  include <windows.h>
#else
#  include <sys/types.h>
#  include <sys/wait.h>
#  include <signal.h>
#  include <unistd.h>
#endif

namespace autoremix {

AudioBridge::AudioBridge(const std::string& host, int port)
    : base_url_(host), port_(port) {}

bool AudioBridge::isServerAlive() const {
    try {
        auto r = cpr::Get(cpr::Url{makeUrl("/api/v1/health")},
                          cpr::Timeout{3000});
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
            cpr::Timeout{300000}
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
            {"drums_tempo_factor", params.drums_tempo_factor},
            {"stem_mix_override",  {
                {"vocals", params.vocals_gain},
                {"drums",  params.drums_gain},
                {"bass",   params.bass_gain},
                {"other",  params.other_gain},
            }},
            {"chop_mode", params.chop_mode},
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

std::vector<PresetInfo> AudioBridge::getPresets() {
    try {
        auto r = cpr::Get(cpr::Url{makeUrl("/api/v1/presets")},
                          cpr::Timeout{2000});
        if (r.status_code != 200) return {};

        auto arr = nlohmann::json::parse(r.text);
        std::vector<PresetInfo> result;
        result.reserve(arr.size());

        for (auto& item : arr) {
            PresetInfo info;
            info.id   = item.at("id").get<std::string>();
            info.name = item.at("name").get<std::string>();
            auto& p   = item.at("params");
            info.default_params.tempo_factor       = p.value("tempo_factor",       1.0f);
            info.default_params.pitch_shift_semi   = p.value("pitch_shift_semi",   0.0f);
            info.default_params.reverb_mix         = p.value("reverb_mix",         0.0f);
            info.default_params.chop_interval_ms   = p.value("chop_interval_ms",   0.0f);
            info.default_params.bass_boost_db      = p.value("bass_boost_db",      0.0f);
            info.default_params.drums_tempo_factor = p.value("drums_tempo_factor", 1.0f);
            info.default_params.engine_id          = info.id;
            result.push_back(std::move(info));
        }
        return result;
    } catch (...) { return {}; }
}

FileAnalysis AudioBridge::analyzeFile(const juce::String& path) {
    try {
        std::string encoded;
        for (auto c : path.toStdString()) {
            if (std::isalnum(static_cast<unsigned char>(c))
                || c == '-' || c == '_' || c == '.' || c == '~' || c == '/')
                encoded += c;
            else {
                char buf[4];
                std::snprintf(buf, sizeof(buf), "%%%02X", static_cast<unsigned char>(c));
                encoded += buf;
            }
        }
        auto r = cpr::Get(
            cpr::Url{makeUrl("/api/v1/analyze?path=" + encoded)},
            cpr::Timeout{10000}
        );
        if (r.status_code != 200) return {};
        auto j = nlohmann::json::parse(r.text);
        FileAnalysis fa;
        fa.bpm          = j.value("bpm",          0.0f);
        fa.key          = j.value("key",           std::string{});
        fa.duration_sec = j.value("duration_sec",  0.0f);
        return fa;
    } catch (...) { return {}; }
}

std::vector<SeparatorInfo> AudioBridge::getAvailableSeparators() {
    static const std::unordered_map<std::string, std::string> kDisplayNames {
        {"algorithmic", "Algorithmic FFT"},
        {"demucs",      "Demucs (ML)"},
    };
    try {
        auto r = cpr::Get(cpr::Url{makeUrl("/api/v1/health")},
                          cpr::Timeout{3000});
        if (r.status_code != 200) return {};

        auto body = nlohmann::json::parse(r.text);
        auto& arr = body.at("available_separators");
        std::vector<SeparatorInfo> result;
        result.reserve(arr.size());
        for (auto& item : arr) {
            SeparatorInfo info;
            info.id = item.get<std::string>();
            auto it = kDisplayNames.find(info.id);
            info.display_name = (it != kDisplayNames.end()) ? it->second : info.id;
            result.push_back(std::move(info));
        }
        return result;
    } catch (...) { return {}; }
}

bool AudioBridge::savePreset(const std::string& name, const RemixParams& params) {
    try {
        nlohmann::json body = {
            {"name",               name},
            {"engine_id",          params.engine_id},
            {"tempo_factor",       params.tempo_factor},
            {"pitch_shift_semi",   params.pitch_shift_semi},
            {"reverb_mix",         params.reverb_mix},
            {"chop_interval_ms",   params.chop_interval_ms},
            {"bass_boost_db",      params.bass_boost_db},
            {"drums_tempo_factor", params.drums_tempo_factor},
            {"vocals_gain",        params.vocals_gain},
            {"drums_gain",         params.drums_gain},
            {"bass_gain",          params.bass_gain},
            {"other_gain",         params.other_gain},
        };
        auto r = cpr::Post(
            cpr::Url{makeUrl("/api/v1/presets")},
            cpr::Header{{"Content-Type", "application/json"}},
            cpr::Body{body.dump()},
            cpr::Timeout{3000}
        );
        if (r.status_code != 200) return false;
        auto resp = nlohmann::json::parse(r.text);
        return resp.value("success", false);
    } catch (...) { return false; }
}

// ---------------------------------------------------------------------------
// Sidecar lifecycle — platform-specific
// ---------------------------------------------------------------------------

#ifdef _WIN32

bool AudioBridge::startSidecar(const std::filesystem::path& server_script_path) {
    if (sidecar_handle_) return true;

    std::string script = server_script_path.string();
    const char* candidates[] = {"python", "python3"};

    for (const char* py : candidates) {
        std::string cmdline = std::string(py) + " \"" + script + "\"";
        std::vector<char> cmd(cmdline.begin(), cmdline.end());
        cmd.push_back('\0');

        STARTUPINFOA si = {};
        si.cb = sizeof(si);
        PROCESS_INFORMATION pi = {};

        if (CreateProcessA(nullptr, cmd.data(),
                           nullptr, nullptr,
                           FALSE, CREATE_NO_WINDOW,
                           nullptr, nullptr, &si, &pi)) {
            CloseHandle(pi.hThread);
            sidecar_handle_ = pi.hProcess;
            sidecar_pid_    = pi.dwProcessId;
            break;
        }
    }

    if (!sidecar_handle_) return false;

    using clock = std::chrono::steady_clock;
    auto deadline = clock::now() + std::chrono::seconds(10);
    while (clock::now() < deadline) {
        if (isServerAlive()) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }

    TerminateProcess(static_cast<HANDLE>(sidecar_handle_), 1);
    CloseHandle(static_cast<HANDLE>(sidecar_handle_));
    sidecar_handle_ = nullptr;
    sidecar_pid_    = 0;
    return false;
}

void AudioBridge::stopSidecar() {
    if (!sidecar_handle_) return;
    HANDLE h = static_cast<HANDLE>(sidecar_handle_);
    TerminateProcess(h, 0);
    WaitForSingleObject(h, 2000);
    CloseHandle(h);
    sidecar_handle_ = nullptr;
    sidecar_pid_    = 0;
}

#else  // POSIX

bool AudioBridge::startSidecar(const std::filesystem::path& server_script_path) {
    if (sidecar_pid_ > 0) return true;

    pid_t pid = fork();
    if (pid < 0) return false;

    if (pid == 0) {
        // server/main.py uses relative imports — must run as module from python/ dir
        std::string pydir = server_script_path.parent_path().parent_path().string();
        if (!pydir.empty()) chdir(pydir.c_str());
        execl("/usr/bin/python3", "python3", "-m", "server.main", nullptr);
        execlp("python3", "python3", "-m", "server.main", nullptr);
        _exit(127);
    }

    sidecar_pid_ = pid;

    using clock = std::chrono::steady_clock;
    auto deadline = clock::now() + std::chrono::seconds(10);
    while (clock::now() < deadline) {
        if (isServerAlive()) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }

    kill(sidecar_pid_, SIGKILL);
    waitpid(sidecar_pid_, nullptr, 0);
    sidecar_pid_ = -1;
    return false;
}

void AudioBridge::stopSidecar() {
    if (sidecar_pid_ <= 0) return;
    kill(sidecar_pid_, SIGTERM);
    for (int i = 0; i < 8; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
        if (waitpid(sidecar_pid_, nullptr, WNOHANG) == sidecar_pid_) {
            sidecar_pid_ = -1;
            return;
        }
    }
    kill(sidecar_pid_, SIGKILL);
    waitpid(sidecar_pid_, nullptr, 0);
    sidecar_pid_ = -1;
}

#endif  // _WIN32

std::string AudioBridge::makeUrl(const std::string& endpoint) const {
    return base_url_ + ":" + std::to_string(port_) + endpoint;
}

}  // namespace autoremix
