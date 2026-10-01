#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdio>

namespace tetrisphere {

class WavCapture {
public:
    WavCapture() = default;
    ~WavCapture();
    WavCapture(const WavCapture&) = delete;
    WavCapture& operator=(const WavCapture&) = delete;

    bool open(const char* path, std::uint32_t sample_rate);
    bool append(const std::int16_t* samples, std::size_t sample_count);
    bool active() const { return file_ != nullptr; }

private:
    bool update_header();
    void close();

    std::FILE* file_ = nullptr;
    std::uint32_t sample_rate_ = 0;
    std::uint32_t data_bytes_ = 0;
};

} // namespace tetrisphere
