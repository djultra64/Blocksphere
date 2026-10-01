#include "tetrisphere/wav_capture.h"

#include <array>
#include <limits>

namespace tetrisphere {
namespace {

void put_u16(std::array<std::uint8_t, 44>& header, std::size_t offset,
             std::uint16_t value) {
    header[offset] = static_cast<std::uint8_t>(value);
    header[offset + 1] = static_cast<std::uint8_t>(value >> 8u);
}

void put_u32(std::array<std::uint8_t, 44>& header, std::size_t offset,
             std::uint32_t value) {
    for (std::size_t index = 0; index < 4; ++index) {
        header[offset + index] = static_cast<std::uint8_t>(value >> (index * 8u));
    }
}

} // namespace

WavCapture::~WavCapture() { close(); }

bool WavCapture::open(const char* path, std::uint32_t sample_rate) {
    close();
    if (path == nullptr || *path == '\0' || sample_rate == 0) return false;
    file_ = std::fopen(path, "wb+");
    if (file_ == nullptr) return false;
    sample_rate_ = sample_rate;
    data_bytes_ = 0;
    if (!update_header()) {
        close();
        return false;
    }
    return true;
}

bool WavCapture::append(const std::int16_t* samples, std::size_t sample_count) {
    constexpr auto max_data = std::numeric_limits<std::uint32_t>::max() - 36u;
    if (file_ == nullptr || samples == nullptr || (sample_count & 1u) != 0u ||
        sample_count > (max_data - data_bytes_) / sizeof(std::int16_t)) {
        return false;
    }
    if (std::fseek(file_, 0, SEEK_END) != 0 ||
        std::fwrite(samples, sizeof(std::int16_t), sample_count, file_) != sample_count) {
        return false;
    }
    data_bytes_ += static_cast<std::uint32_t>(sample_count * sizeof(std::int16_t));
    return update_header();
}

bool WavCapture::update_header() {
    if (file_ == nullptr) return false;
    std::array<std::uint8_t, 44> header{};
    const auto copy_tag = [&header](std::size_t offset, const char* tag) {
        for (std::size_t index = 0; index < 4; ++index) {
            header[offset + index] = static_cast<std::uint8_t>(tag[index]);
        }
    };
    copy_tag(0, "RIFF");
    put_u32(header, 4, 36u + data_bytes_);
    copy_tag(8, "WAVE");
    copy_tag(12, "fmt ");
    put_u32(header, 16, 16u);
    put_u16(header, 20, 1u);
    put_u16(header, 22, 2u);
    put_u32(header, 24, sample_rate_);
    put_u32(header, 28, sample_rate_ * 4u);
    put_u16(header, 32, 4u);
    put_u16(header, 34, 16u);
    copy_tag(36, "data");
    put_u32(header, 40, data_bytes_);
    return std::fseek(file_, 0, SEEK_SET) == 0 &&
           std::fwrite(header.data(), 1, header.size(), file_) == header.size() &&
           std::fflush(file_) == 0 && std::fseek(file_, 0, SEEK_END) == 0;
}

void WavCapture::close() {
    if (file_ != nullptr) {
        update_header();
        std::fclose(file_);
        file_ = nullptr;
    }
}

} // namespace tetrisphere
