#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace tetrisphere {

// Stable DrawCall.uid values. Animation/P1/P2 are reserved until producer
// inventory and renderer policy establish their exact behavior.
enum class DlUiAnchor : std::uint8_t {
    None = 0, Left = 1, Right = 2, Center = 3,
    Animation = 4, PlayerOne = 5, PlayerTwo = 6, PauseText = 7,
};

struct DlUiTagSpan {
    std::uint32_t first = 0;
    std::uint32_t past_last = 0;
    DlUiAnchor anchor = DlUiAnchor::None;
    std::uint64_t recorded_after_task = 0;
    std::uint64_t fingerprint = 0;
};

struct DlUiTagBatch {
    std::uint64_t task_generation = 0;
    std::vector<DlUiTagSpan> spans;

    DlUiAnchor lookup(std::uint32_t command, const std::uint32_t* parents = nullptr,
                      std::size_t parent_count = 0) const {
        const auto direct = lookup_direct(command);
        if (direct != DlUiAnchor::None) return direct;
        // A child display list inherits the closest tagged G_DL caller. An
        // explicitly tagged Center command always wins over that inheritance.
        for (std::size_t i = parent_count; i > 0; --i) {
            const auto inherited = lookup_direct(parents[i - 1]);
            if (inherited != DlUiAnchor::None) return inherited;
        }
        return DlUiAnchor::None;
    }

private:
    DlUiAnchor lookup_direct(std::uint32_t command) const {
        DlUiAnchor found = DlUiAnchor::None;
        std::uint32_t narrowest = 0xFFFFFFFFu;
        for (const auto& span : spans) {
            if (command < span.first || command >= span.past_last) continue;
            const std::uint32_t width = span.past_last - span.first;
            if (width == narrowest && found != DlUiAnchor::None &&
                found != span.anchor) {
                return DlUiAnchor::None; // Conflicting producer ranges fail closed.
            }
            if (width < narrowest) {
                narrowest = width;
                found = span.anchor;
            }
        }
        return found;
    }
};

// The guest can fill two alternating command buffers before their OSTasks
// arrive. Keep an unconsumed span briefly, and verify its exact bytes before
// using it so reuse of the same RDRAM address cannot inherit an old anchor.
class DlUiTagQueue {
public:
    static std::uint64_t fingerprint(const std::uint8_t* rdram,
                                     std::size_t rdram_size,
                                     std::uint32_t first,
                                     std::uint32_t past_last) {
        if (rdram == nullptr || first >= past_last || past_last > rdram_size)
            return 0;
        std::uint64_t value = 14695981039346656037ull;
        for (std::uint32_t i = first; i < past_last; i++) {
            value ^= rdram[i];
            value *= 1099511628211ull;
        }
        return value;
    }

    bool record(std::uint32_t first, std::uint32_t past_last, DlUiAnchor anchor,
                const std::uint8_t* rdram, std::size_t rdram_size) {
        if (anchor == DlUiAnchor::None || first >= past_last ||
            past_last > 0x01000000u || pending_.size() >= 8192) return false;
        const std::uint64_t hash = fingerprint(rdram, rdram_size, first, past_last);
        if (hash == 0) return false;
        pending_.push_back({first, past_last, anchor, task_generation_, hash});
        return true;
    }

    DlUiTagBatch take(std::uint32_t task_first, std::uint32_t task_size,
                      const std::uint8_t* rdram, std::size_t rdram_size) {
        DlUiTagBatch batch;
        batch.task_generation = ++task_generation_;
        const std::uint64_t task_past_last =
            std::uint64_t(task_first) + std::uint64_t(task_size);
        for (auto it = pending_.begin(); it != pending_.end();) {
            const bool stale = batch.task_generation > it->recorded_after_task + 3;
            const bool changed = fingerprint(rdram, rdram_size, it->first,
                                             it->past_last) != it->fingerprint;
            const bool in_task = task_past_last <= 0x01000000ull &&
                it->first >= task_first && it->past_last <= task_past_last;
            if (!stale && !changed && in_task) batch.spans.push_back(*it);
            if (stale || changed || in_task) it = pending_.erase(it);
            else ++it;
        }
        return batch;
    }

    std::size_t pending_count() const { return pending_.size(); }

private:
    std::uint64_t task_generation_ = 0;
    std::vector<DlUiTagSpan> pending_;
};

} // namespace tetrisphere
