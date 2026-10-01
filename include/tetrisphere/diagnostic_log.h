#pragma once

#include "tetrisphere/settings_store.h"

#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>

namespace tetrisphere {

class DiagnosticLog {
public:
    bool start(const std::filesystem::path& path, const std::string& run_id,
               const std::string& build_id, const std::string& rom_sha256,
               const Settings& settings, std::string& error);
    void gpu(const std::string& name);
    void error(const char* code);
    void audio(const char* status);

private:
    void append_locked(const std::string& line);
    std::mutex mutex_;
    std::ofstream stream_;
};

DiagnosticLog& diagnostic_log();

} // namespace tetrisphere
