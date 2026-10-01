#include "tetrisphere/recomp_support.h"
#include "tetrisphere/prompt_atlas.h"

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <mutex>
#include <unordered_map>

namespace {

constexpr std::uint32_t rdram_size = 0x800000;
constexpr std::uint64_t expected_hash = 0x52057514f3d4d110ULL;
std::atomic<unsigned> reported_renderers{0};
std::mutex atlas_mutex;
std::unordered_map<std::uint32_t, std::uint64_t> last_written_hash;
std::uint64_t mismatched_addresses = 0;
bool reported_unknown_candidate = false;

std::uint64_t hash_atlas(const tetrisphere::PromptAtlas& atlas) {
    std::uint64_t hash = 14695981039346656037ULL;
    for (const auto byte : atlas) {
        hash ^= byte;
        hash *= 1099511628211ULL;
    }
    return hash;
}

} // namespace

extern "C" void tetrisphere_probe_prompt_atlas(std::uint8_t* rdram,
                                                  recomp_context* ctx,
                                                  std::uint32_t renderer_vram) {
    const auto address = static_cast<std::uint32_t>(ctx->r5);
    const auto offset = address - 0x80000000u;
    if (offset > rdram_size - 788) return;
    if (MEM_BU(0, ctx->r5) != 0 || MEM_BU(1, ctx->r5) != 65 ||
        MEM_BU(2, ctx->r5) != 0 || MEM_BU(3, ctx->r5) != 12 ||
        MEM_BU(4, ctx->r5) != 0 || MEM_BU(5, ctx->r5) != 2 ||
        MEM_BU(6, ctx->r5) != 0 || MEM_BU(7, ctx->r5) != 0) return;
    tetrisphere::PromptAtlas current{};
    for (unsigned i = 0; i < 788; ++i) {
        current[i] = MEM_BU(i, ctx->r5);
    }
    const auto hash = hash_atlas(current);
    std::lock_guard lock(atlas_mutex);
    const auto prior = last_written_hash.find(address);
    if (hash != expected_hash &&
        (prior == last_written_hash.end() || hash != prior->second)) {
        if (prior == last_written_hash.end() && !reported_unknown_candidate) {
            reported_unknown_candidate = true;
            std::fprintf(stderr,
                         "{\"event\":\"prompt_atlas_candidate_mismatch\","
                         "\"renderer\":\"0x%08X\",\"image\":\"0x%08X\","
                         "\"hash\":\"0x%016llX\"}\n", renderer_vram, address,
                         static_cast<unsigned long long>(hash));
            std::fflush(stderr);
        } else if (prior != last_written_hash.end() && mismatched_addresses++ == 0) {
            std::fprintf(stderr,
                         "{\"event\":\"prompt_atlas_invalid\",\"image\":\"0x%08X\","
                         "\"hash\":\"0x%016llX\"}\n", address,
                         static_cast<unsigned long long>(hash));
            std::fflush(stderr);
        }
        last_written_hash.erase(address);
        return;
    }
    const auto bit = renderer_vram == 0x8003396Cu ? 1u
        : renderer_vram == 0x800353F4u ? 2u : 4u;
    if (!(reported_renderers.fetch_or(bit) & bit)) {
        std::fprintf(stdout,
                 "{\"event\":\"prompt_atlas_renderer\",\"renderer\":\"0x%08X\","
                 "\"caller\":\"0x%08X\",\"image\":\"0x%08X\","
                 "\"a2\":\"0x%08X\",\"a3\":\"0x%08X\"}\n",
                 renderer_vram, static_cast<std::uint32_t>(ctx->r31), address,
                 static_cast<std::uint32_t>(ctx->r6), static_cast<std::uint32_t>(ctx->r7));
        std::fflush(stdout);
    }
#ifdef TETRISPHERE_LINUX_DEMO
    // Always derive from the unmodified ROM atlas. Reusing the last rendered
    // buffer as a template would retain the previous device's icon.
    static tetrisphere::PromptAtlas original{};
    static bool have_original = false;
    if (hash == expected_hash) {
        original = current;
        have_original = true;
    }
    if (!have_original) return;
    const auto family = tetrisphere::active_controller_family();
    const auto bindings = tetrisphere::controller_bindings();
    const auto desired = tetrisphere::render_prompt_atlas(original, family, bindings);
    const auto desired_hash = hash_atlas(desired);
    if (hash != desired_hash) {
        for (unsigned i = 0; i < desired.size(); ++i) {
            MEM_B(i, ctx->r5) = desired[i];
        }
        std::fprintf(stdout,
                     "{\"event\":\"prompt_atlas_updated\",\"image\":\"0x%08X\","
                     "\"family\":\"%s\",\"confirm\":\"%s\",\"cancel\":\"%s\"}\n",
                     address, tetrisphere::controller_family_name(family),
                     tetrisphere::logical_action_label(family, bindings,
                                                       tetrisphere::LogicalAction::Confirm),
                     tetrisphere::logical_action_label(family, bindings,
                                                       tetrisphere::LogicalAction::Cancel));
        std::fflush(stdout);
    }
    last_written_hash[address] = desired_hash;
#endif
}
