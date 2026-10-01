#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace tetrisphere {

struct MicrocodeCommand {
    std::size_t offset;
    std::uint8_t opcode;
    std::size_t size;
};

struct GraphicsTaskEvidence {
    std::string identity;
    std::size_t command_count = 0;
    bool accepted = false;
    std::vector<MicrocodeCommand> unsupported;
};

struct AudioOutputRange {
    std::uint32_t address;
    std::size_t bytes;
};

std::vector<AudioOutputRange> derive_audio_output_ranges(
    std::span<const std::uint8_t> commands);

std::optional<std::vector<std::int16_t>> decode_pcm_samples(
    std::span<const std::uint8_t> rdram, AudioOutputRange range);

GraphicsTaskEvidence inspect_rt64_commands(
    std::string identity, std::span<const MicrocodeCommand> commands,
    std::span<const std::uint8_t> supported_opcodes);

class AudioBridge {
public:
    bool queue_samples(std::span<const std::int16_t> samples);
    [[nodiscard]] std::size_t callback_count() const;
    [[nodiscard]] std::size_t sample_count() const;
    [[nodiscard]] std::size_t frame_count() const;
    [[nodiscard]] std::size_t nonzero_sample_count() const;

private:
    std::size_t callback_count_ = 0;
    std::size_t sample_count_ = 0;
    std::size_t nonzero_sample_count_ = 0;
};

} // namespace tetrisphere
