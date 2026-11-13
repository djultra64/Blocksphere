#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace tetrisphere {

// Render submission only. Construct after the medal alpha and flat-shade
// patches and destroy before them. The caller enables this only for a menu
// task, and grants exclusive use of the supplied scratch range until return
// from processDisplayLists. No guest simulation or persistent state changes.
class MenuMedalWrapPatch {
public:
    MenuMedalWrapPatch(std::uint8_t* rdram, std::size_t rdram_bytes,
                       std::uint32_t display_list_address,
                       std::size_t display_list_bytes, float aspect_ratio,
                       std::uint32_t scratch_address, std::size_t scratch_bytes);
    ~MenuMedalWrapPatch();
    MenuMedalWrapPatch(const MenuMedalWrapPatch&) = delete;
    MenuMedalWrapPatch& operator=(const MenuMedalWrapPatch&) = delete;
    std::size_t duplicate_count() const { return duplicates_; }

private:
    std::uint8_t* rdram_ = nullptr;
    std::size_t redirect_offset_ = 0;
    std::uint32_t original_command_ = 0, original_argument_ = 0;
    std::size_t scratch_offset_ = 0;
    std::vector<std::uint8_t> scratch_original_;
    std::size_t duplicates_ = 0;
};

} // namespace tetrisphere
