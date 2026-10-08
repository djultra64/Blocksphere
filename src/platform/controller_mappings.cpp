#include "tetrisphere/controller_mappings.h"
#include "tetrisphere/data_directory.h"
#include <SDL.h>
#include <array>
#include <cstdio>
#include <fstream>
#include <stdexcept>
#include <vector>

namespace tetrisphere {
std::vector<ControllerMappingReceipt> load_controller_mappings(
    const std::filesystem::path& bundled, const std::filesystem::path& local) {
    std::vector<ControllerMappingReceipt> receipts;
    for (const auto& path : std::array{bundled, local}) {
        ControllerMappingReceipt receipt{path, "missing", 0, {}};
        try {
            if (!path.empty() && std::filesystem::exists(path)) {
                const auto size = std::filesystem::file_size(path);
                if (size > 4 * 1024 * 1024) throw std::runtime_error("Mapping file exceeds 4 MiB");
                std::ifstream input(path, std::ios::binary);
                if (!input) throw std::runtime_error("Cannot open mapping file");
                std::vector<char> bytes(static_cast<std::size_t>(size));
                if (size != 0 && !input.read(bytes.data(), static_cast<std::streamsize>(size)))
                    throw std::runtime_error("Cannot read complete mapping file");
                // Memory streams avoid SDL fopen path encoding limitations on Windows.
                if (!bytes.empty()) {
                    auto* stream = SDL_RWFromConstMem(bytes.data(), static_cast<int>(bytes.size()));
                    if (stream == nullptr) throw std::runtime_error(SDL_GetError());
                    receipt.added = SDL_GameControllerAddMappingsFromRW(stream, 1);
                    if (receipt.added < 0) throw std::runtime_error(SDL_GetError());
                }
                receipt.status = "loaded";
            }
        } catch (const std::exception& error) {
            receipt.status = "error";
            receipt.error = error.what();
        }
        receipts.push_back(std::move(receipt));
    }
    return receipts;
}

void initialize_controller_mappings() {
    char* base = SDL_GetBasePath();
    const auto bundled = base == nullptr ? std::filesystem::path{} :
        std::filesystem::u8path(base) / "gamecontrollerdb.txt";
    SDL_free(base);
    const auto data = application_data_directory();
    const auto local = data.empty() ? std::filesystem::path{} : data / "gamecontrollerdb.txt";
    for (const auto& receipt : load_controller_mappings(bundled, local)) {
        if (receipt.status == "missing") continue;
        std::fprintf(stderr, "SDL controller mappings: %s, added=%d%s%s\n",
            receipt.status.c_str(), receipt.added,
            receipt.error.empty() ? "" : ": ", receipt.error.c_str());
    }
}
}
