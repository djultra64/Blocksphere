#include "tetrisphere/microcode.h"

#include <bit>
#include <cstddef>

namespace tetrisphere {

namespace {

std::uint32_t read_be32(const std::uint8_t* data) {
    return (std::uint32_t{data[0]} << 24) |
           (std::uint32_t{data[1]} << 16) |
           (std::uint32_t{data[2]} << 8) | std::uint32_t{data[3]};
}

} // namespace

std::vector<AudioOutputRange> derive_audio_output_ranges(
        std::span<const std::uint8_t> commands) {
    if (commands.empty() || commands.size() % 8 != 0) {
        return {};
    }
    std::vector<AudioOutputRange> result;
    std::size_t active_size = 0;
    for (std::size_t offset = 0; offset < commands.size(); offset += 8) {
        const auto word0 = read_be32(commands.data() + offset);
        const auto word1 = read_be32(commands.data() + offset + 4);
        const auto opcode = static_cast<std::uint8_t>(word0 >> 24);
        if (opcode == 0x08) {
            active_size = word1 & 0xFFFFu;
        } else if (opcode == 0x06) {
            if (active_size == 0) {
                return {};
            }
            result.push_back({word1 & 0x00FFFFFFu, active_size});
        }
    }
    return result;
}

std::optional<std::vector<std::int16_t>> decode_pcm_samples(
        std::span<const std::uint8_t> rdram, AudioOutputRange range) {
    if (range.bytes == 0 || range.bytes % sizeof(std::int16_t) != 0 ||
        range.address > rdram.size() ||
        range.bytes > rdram.size() - range.address) {
        return std::nullopt;
    }
    std::vector<std::int16_t> samples;
    samples.reserve(range.bytes / sizeof(std::int16_t));
    for (std::size_t offset = 0; offset < range.bytes; offset += 2) {
        const auto low = std::uint16_t{rdram[range.address + offset]};
        const auto high = static_cast<std::uint16_t>(
            std::uint16_t{rdram[range.address + offset + 1]} << 8);
        const auto bits = static_cast<std::uint16_t>(low | high);
        samples.push_back(std::bit_cast<std::int16_t>(bits));
    }
    return samples;
}

bool AudioBridge::queue_samples(std::span<const std::int16_t> samples) {
    if (samples.empty() || samples.size() % 2 != 0) {
        return false;
    }
    ++callback_count_;
    sample_count_ += samples.size();
    for (const auto sample : samples) {
        nonzero_sample_count_ += sample != 0;
    }
    return true;
}

std::size_t AudioBridge::callback_count() const { return callback_count_; }
std::size_t AudioBridge::sample_count() const { return sample_count_; }
std::size_t AudioBridge::frame_count() const { return sample_count_ / 2; }
std::size_t AudioBridge::nonzero_sample_count() const {
    return nonzero_sample_count_;
}

} // namespace tetrisphere
