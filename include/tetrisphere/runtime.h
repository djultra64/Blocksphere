#ifndef TETRISPHERE_RUNTIME_H
#define TETRISPHERE_RUNTIME_H

#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>

namespace tetrisphere {

enum class RuntimeCategory {
    Boot,
    Thread,
    MessageQueue,
    Timer,
    PiDma,
    Controller,
    Vi,
    Rsp,
    Audio,
};

enum class RuntimeStatus {
    Implemented,
    Delegated,
    Unsupported,
};

struct RuntimeBinding {
    std::uint32_t address;
    bool has_guest_address;
    std::string_view name;
    RuntimeCategory category;
    RuntimeStatus status;
    std::string_view host_binding;
    std::string_view provenance;
    std::string_view detail;
};

std::span<const RuntimeBinding> runtime_bindings();
const RuntimeBinding* find_runtime_binding(std::uint32_t address);
bool validate_runtime_table_json(std::string_view serialized, std::string& error);
bool pi_raw_read_io(std::span<const std::uint8_t> rom, std::uint32_t device_address,
                    std::uint32_t& value);
bool load_initial_image(std::span<const std::uint8_t> rom,
                        std::span<std::uint8_t> rdram,
                        std::size_t rom_offset, std::size_t ram_offset,
                        std::size_t size);
void initialize_ipl3_state(std::uint8_t* rdram);
void set_runtime_rom(std::span<const std::uint8_t> rom);
bool initialize_eeprom(const std::filesystem::path& path, std::string& error);
[[noreturn]] void await_runtime_handoff();

[[noreturn]] void fatal_unsupported(std::string_view symbol, std::uint32_t address,
                                    std::uint32_t callsite, std::string_view phase);

} // namespace tetrisphere

#endif
