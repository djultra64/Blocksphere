#include "tetrisphere/diagnostic_log.h"

#include <json.hpp>

#include <algorithm>
#include <cctype>

namespace tetrisphere {
namespace {

std::string safe_gpu_name(const std::string& input) {
    std::string result;
    for (const unsigned char character : input) {
        if (result.size() >= 128) break;
        if (std::isalnum(character) || character == ' ' || character == '-' ||
            character == '_' || character == '.' || character == '(' ||
            character == ')') result.push_back(static_cast<char>(character));
    }
    return result.empty() ? "unknown" : result;
}

} // namespace

bool DiagnosticLog::start(const std::filesystem::path& path,
                          const std::string& run_id, const std::string& build_id,
                          const std::string& rom_sha256, const Settings& settings,
                          std::string& error) {
    std::lock_guard lock(mutex_);
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    if (ec) {
        error = "cannot create diagnostics directory";
        return false;
    }
    stream_.open(path, std::ios::app);
    if (!stream_) {
        error = "cannot open diagnostics file";
        return false;
    }
    const auto document = nlohmann::json{
        {"schema_version", 1}, {"event", "session_started"},
        {"run_id", run_id}, {"build_id", build_id},
        {"rom_sha256", rom_sha256}, {"backend", "rt64-vulkan"},
        {"graphics", {{"resolution", graphics_resolution_name(settings.graphics.resolution)},
                      {"aspect", graphics_aspect_name(settings.graphics.aspect)},
                      {"refresh", graphics_refresh_name(settings.graphics.refresh)},
                      {"status", "requested_unverified"}}},
        {"settings", {{"family_override", settings.family_override},
                      {"bindings", {
                          static_cast<int>(settings.bindings.confirm),
                          static_cast<int>(settings.bindings.cancel),
                          static_cast<int>(settings.bindings.face_left),
                          static_cast<int>(settings.bindings.face_up)}}}}};
    append_locked(document.dump());
    if (!stream_) {
        error = "cannot write diagnostics file";
        return false;
    }
    error.clear();
    return true;
}

void DiagnosticLog::append_locked(const std::string& line) {
    if (stream_) {
        stream_ << line << '\n';
        stream_.flush();
    }
}

void DiagnosticLog::gpu(const std::string& name) {
    std::lock_guard lock(mutex_);
    append_locked(nlohmann::json{{"event", "gpu_ready"},
                                 {"gpu", safe_gpu_name(name)}}.dump());
}

void DiagnosticLog::error(const char* code) {
    std::lock_guard lock(mutex_);
    append_locked(nlohmann::json{{"event", "error"},
                                 {"code", code == nullptr ? "unknown" : code}}.dump());
}

void DiagnosticLog::audio(const char* status) {
    std::lock_guard lock(mutex_);
    append_locked(nlohmann::json{{"event", "audio_device_change"},
                                 {"status", status}}.dump());
}

DiagnosticLog& diagnostic_log() {
    static DiagnosticLog value;
    return value;
}

} // namespace tetrisphere
