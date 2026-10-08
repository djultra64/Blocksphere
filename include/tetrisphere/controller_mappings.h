#pragma once
#include <filesystem>
#include <string>
#include <vector>

namespace tetrisphere {
struct ControllerMappingReceipt {
    std::filesystem::path path;
    std::string status;
    int added = 0;
    std::string error;
};
// Call before SDL controller initialization: explicit SDL hints retain priority.
std::vector<ControllerMappingReceipt> load_controller_mappings(
    const std::filesystem::path& bundled, const std::filesystem::path& local);
void initialize_controller_mappings();
}
