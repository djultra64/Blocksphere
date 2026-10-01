#include "tetrisphere/recomp_support.h"
#include "tetrisphere/training_prompts.h"

#ifdef TETRISPHERE_LINUX_DEMO

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {

constexpr std::uint32_t font_copy_base = 0x80810000u;
constexpr std::size_t font_size = 8 + 20 * 1024;
constexpr std::uint64_t font_hash = 0x3e34707868992666ULL;
constexpr std::uint32_t rdram_original_size = 0x800000u;
constexpr std::size_t variants = 32;
static_assert(0x810000u + variants * font_size <= 9u * 1024u * 1024u);
thread_local tetrisphere::TrainingPromptMarks marks;
std::array<bool, variants> initialized{};
std::array<std::uint64_t, variants> variant_hashes{};

[[noreturn]] void fail(const char* reason, std::uint32_t address) {
    std::fprintf(stderr,
                 "{\"event\":\"fatal_training_prompt\",\"reason\":\"%s\","
                 "\"address\":\"0x%08X\"}\n", reason, address);
    std::fflush(stderr);
    std::exit(70);
}

std::uint64_t hash_guest_region(std::uint8_t* rdram, std::uint32_t address,
                                std::size_t size) {
    std::uint64_t hash = 14695981039346656037ULL;
    for (std::size_t i = 0; i < size; ++i) {
        hash ^= MEM_BU(i, tetrisphere::guest_virtual_address(address));
        hash *= 1099511628211ULL;
    }
    return hash;
}

std::uint64_t hash_guest(std::uint8_t* rdram, std::uint32_t address) {
    return hash_guest_region(rdram, address, font_size);
}

void probe_font_mismatch(std::uint8_t* rdram, std::uint32_t address,
                         std::uint64_t actual_hash) {
    const char* enabled = std::getenv("TETRISPHERE_TRAINING_FONT_PROBE");
    if (enabled == nullptr || enabled[0] != '1' || enabled[1] != '\0') return;
    constexpr std::size_t row_size = 16 * 20;
    constexpr std::size_t first_row = 33;
    std::fprintf(stderr,
                 "{\"event\":\"training_font_probe\",\"address\":\"0x%08X\","
                 "\"expected_hash\":\"0x%016llX\",\"actual_hash\":\"0x%016llX\","
                 "\"header\":[%u,%u,%u,%u,%u,%u,%u,%u],"
                 "\"row33_hash\":\"0x%016llX\",\"row34_hash\":\"0x%016llX\","
                 "\"row35_hash\":\"0x%016llX\"}\n",
                 address, static_cast<unsigned long long>(font_hash),
                 static_cast<unsigned long long>(actual_hash),
                 MEM_BU(0, tetrisphere::guest_virtual_address(address)),
                 MEM_BU(1, tetrisphere::guest_virtual_address(address)),
                 MEM_BU(2, tetrisphere::guest_virtual_address(address)),
                 MEM_BU(3, tetrisphere::guest_virtual_address(address)),
                 MEM_BU(4, tetrisphere::guest_virtual_address(address)),
                 MEM_BU(5, tetrisphere::guest_virtual_address(address)),
                 MEM_BU(6, tetrisphere::guest_virtual_address(address)),
                 MEM_BU(7, tetrisphere::guest_virtual_address(address)),
                 static_cast<unsigned long long>(hash_guest_region(
                     rdram, address + 8 + first_row * row_size, row_size)),
                 static_cast<unsigned long long>(hash_guest_region(
                     rdram, address + 8 + (first_row + 1) * row_size, row_size)),
                 static_cast<unsigned long long>(hash_guest_region(
                     rdram, address + 8 + (first_row + 2) * row_size, row_size)));
    std::fflush(stderr);
}

std::uint32_t copy_address(std::uint64_t source_hash,
                           tetrisphere::ControllerFamily family,
                           tetrisphere::PhysicalButton button) {
    const auto slot = tetrisphere::training_font_variant_slot(source_hash, family, button);
    if (!slot) fail("invalid_font_or_binding", static_cast<unsigned>(button));
    return font_copy_base + static_cast<std::uint32_t>(
        *slot * font_size);
}

} // namespace

extern "C" void tetrisphere_training_reset() {
    marks.clear();
}

extern "C" void tetrisphere_training_clear_cache() {
    initialized.fill(false);
    variant_hashes.fill(0);
    marks.clear();
}

extern "C" void tetrisphere_training_replace(std::uint8_t*,
                                               recomp_context* ctx,
                                               std::uint32_t register_number) {
    const auto source = static_cast<std::uint32_t>(ctx->r16);
    if (source != 0x800EF294u && source != 0x800EF295u) return;
    const auto text = tetrisphere::training_magic_text(
        tetrisphere::active_controller_family(), tetrisphere::magic_control());
    const auto replacement = source == 0x800EF294u ? text.first : text.second;
    if (register_number == 25) ctx->r25 = replacement;
    else if (register_number == 15) ctx->r15 = replacement;
}

extern "C" void tetrisphere_training_record(std::uint8_t* rdram,
                                              recomp_context* ctx,
                                              std::uint32_t destination) {
    const auto source = static_cast<std::uint32_t>(ctx->r16);
    auto prompt = tetrisphere::training_prompt_at(source);
    if (prompt == tetrisphere::TrainingPrompt::Magic &&
        !tetrisphere::training_magic_text(
            tetrisphere::active_controller_family(),
            tetrisphere::magic_control()).icon) prompt = tetrisphere::TrainingPrompt::None;
    // The guest parser reuses this destination for each successive word.
    // Recording every copied character also removes a prior prompt mark when
    // ordinary text overwrites it.
    if (!marks.record(destination, prompt)) fail("too_many_prompt_marks", source);
    if (prompt == tetrisphere::TrainingPrompt::None) return;
    const auto expected = prompt == tetrisphere::TrainingPrompt::Confirm ? 'A'
        : prompt == tetrisphere::TrainingPrompt::Cancel ? 'B' : 'C';
    if (MEM_BU(0, tetrisphere::guest_virtual_address(source)) != expected)
        fail("source_character_changed", source);
}

extern "C" void tetrisphere_training_render(std::uint8_t* rdram,
                                              recomp_context* ctx) {
    const auto stack = ctx->r29;
    const auto base = static_cast<std::uint32_t>(MEM_W(0x50, stack));
    const auto index = static_cast<std::uint32_t>(MEM_W(0x3C, stack));
    const auto character_address = base + index;
    const auto prompt = marks.at(character_address);
    if (prompt == tetrisphere::TrainingPrompt::None) return;

    const auto expected = prompt == tetrisphere::TrainingPrompt::Confirm ? 'A'
        : prompt == tetrisphere::TrainingPrompt::Cancel ? 'B' : 'C';
    if (MEM_BU(0, tetrisphere::guest_virtual_address(character_address)) != expected)
        fail("copied_character_changed", character_address);
    const auto original_address = static_cast<std::uint32_t>(ctx->r5);
    if (original_address < 0x80000000u ||
        original_address - 0x80000000u > rdram_original_size - font_size)
        fail("font_address_out_of_range", original_address);
    const auto original_hash = hash_guest(rdram, original_address);
    if (!tetrisphere::training_font_variant_slot(
            original_hash, tetrisphere::ControllerFamily::Keyboard,
            tetrisphere::PhysicalButton::South)) {
        probe_font_mismatch(rdram, original_address, original_hash);
        fail("font_identity_mismatch", original_address);
    }

    const auto family = tetrisphere::active_controller_family();
    const auto bindings = tetrisphere::controller_bindings();
    tetrisphere::PhysicalButton button;
    if (prompt == tetrisphere::TrainingPrompt::Magic) {
        const auto control = tetrisphere::magic_control();
        if (control < tetrisphere::MagicControl::South ||
            control > tetrisphere::MagicControl::North)
            fail("magic_binding_changed_during_render", character_address);
        button = static_cast<tetrisphere::PhysicalButton>(
            static_cast<unsigned>(control) - static_cast<unsigned>(
                tetrisphere::MagicControl::South));
    } else {
        const auto action = prompt == tetrisphere::TrainingPrompt::Confirm
            ? tetrisphere::LogicalAction::Confirm : tetrisphere::LogicalAction::Cancel;
        button = tetrisphere::physical_button_for(bindings, action);
    }
    const auto destination = copy_address(original_hash, family, button);
    const auto variant = (destination - font_copy_base) / font_size;
    if (!initialized[variant]) {
        std::array<std::uint8_t, font_size> original{};
        std::array<std::uint8_t, font_size> copy{};
        for (std::size_t i = 0; i < font_size; ++i)
            original[i] = MEM_BU(i, tetrisphere::guest_virtual_address(original_address));
        if (!tetrisphere::make_training_font_copy(
                original.data(), original.size(), copy.data(), copy.size(),
                family, button,
                original_hash == font_hash ? 7u : 20u,
                original_hash == font_hash ? 1u : 2u))
            fail("font_copy_failed", original_address);
        for (std::size_t i = 0; i < font_size; ++i)
            MEM_B(i, tetrisphere::guest_virtual_address(destination)) = copy[i];
        if (hash_guest(rdram, original_address) != original_hash)
            fail("original_font_mutated", original_address);
        variant_hashes[variant] = hash_guest(rdram, destination);
        initialized[variant] = true;
        std::fprintf(stdout,
                     "{\"event\":\"training_font_variant\",\"family\":\"%s\","
                     "\"button\":\"%s\",\"guest\":\"0x%08X\","
                     "\"source_hash\":\"0x%016llX\",\"original_untouched\":true,"
                     "\"copy_hash\":\"0x%016llX\"}\n",
                     tetrisphere::controller_family_name(family),
                     tetrisphere::physical_button_label(family, button), destination,
                     static_cast<unsigned long long>(original_hash),
                     static_cast<unsigned long long>(variant_hashes[variant]));
        std::fflush(stdout);
    }
    if (hash_guest(rdram, destination) != variant_hashes[variant])
        fail("font_variant_mutated", destination);
    ctx->r5 = static_cast<std::uint64_t>(static_cast<std::int64_t>(
        static_cast<std::int32_t>(destination)));
}

#else

extern "C" void tetrisphere_training_reset() {}
extern "C" void tetrisphere_training_clear_cache() {}
extern "C" void tetrisphere_training_replace(std::uint8_t*, recomp_context*,
                                               std::uint32_t) {}
extern "C" void tetrisphere_training_record(std::uint8_t*, recomp_context*,
                                              std::uint32_t) {}
extern "C" void tetrisphere_training_render(std::uint8_t*, recomp_context*) {}

#endif
