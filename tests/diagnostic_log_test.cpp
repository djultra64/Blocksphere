#include "tetrisphere/diagnostic_log.h"

#include <json.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
    if (argc != 2) return 2;
    const auto path = std::filesystem::path(argv[1]) / "diagnostic.jsonl";
    std::filesystem::remove(path);
    tetrisphere::DiagnosticLog log;
    tetrisphere::Settings settings;
    settings.family_override = "nintendo-switch";
    settings.graphics.resolution = tetrisphere::GraphicsResolution::P2160;
    settings.graphics.aspect = tetrisphere::GraphicsAspect::Expand16x9;
    settings.graphics.refresh = tetrisphere::GraphicsRefresh::Fps120;
    std::string error;
    if (!log.start(path, "run-1", "build-1", std::string(64, 'a'), settings, error)) {
        std::cerr << error << '\n';
        return 1;
    }
    log.gpu("AMD Radeon RX 7800 XT");
    log.error("audio_unavailable");
    log.audio("reopened");
    std::ifstream input(path);
    std::string line;
    int lines = 0;
    while (std::getline(input, line)) {
        const auto document = nlohmann::json::parse(line);
        if (lines == 0 &&
            (document.at("build_id") != "build-1" ||
             document.at("rom_sha256") != std::string(64, 'a') ||
             document.at("settings").at("family_override") != "nintendo-switch" ||
             document.at("graphics").at("resolution") != "2160p" ||
             document.at("graphics").at("aspect") != "16:9" ||
             document.at("graphics").at("refresh") != "120" ||
             document.at("backend") != "rt64-vulkan")) return 1;
        if (lines == 1 && document.at("gpu") != "AMD Radeon RX 7800 XT") return 1;
        if (line.find("secret") != std::string::npos) return 1;
        ++lines;
    }
    if (lines != 4) return 1;
}
