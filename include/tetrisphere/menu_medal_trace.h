#pragma once

#include <cstddef>
#include <optional>

namespace tetrisphere {

// Bounded diagnostic samples after gameplay returns to the menu.
class MenuMedalTrace {
public:
    struct Sample {
        std::size_t sequence;
        std::size_t patched_triangles;
    };

    explicit MenuMedalTrace(bool enabled, std::size_t max_samples = 240)
        : enabled_(enabled), max_samples_(max_samples) {}

    std::optional<Sample> observe(bool menu_path, std::size_t patched_triangles) {
        if (!menu_path) {
            was_gameplay_ = true;
            active_ = false;
            return std::nullopt;
        }
        if (was_gameplay_) {
            was_gameplay_ = false;
            active_ = true;
            sample_count_ = 0;
        }
        if (!enabled_ || !active_ || sample_count_ >= max_samples_) {
            return std::nullopt;
        }
        return Sample{++sample_count_, patched_triangles};
    }

private:
    bool enabled_;
    std::size_t max_samples_;
    bool was_gameplay_ = false;
    bool active_ = false;
    std::size_t sample_count_ = 0;
};

} // namespace tetrisphere
