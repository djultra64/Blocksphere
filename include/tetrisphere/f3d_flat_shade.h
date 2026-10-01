#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace tetrisphere {

// RT64 ignores the F3D TRI1 flat-shading vertex selector. Rotate each
// triangle so Vulkan's first vertex receives the same shade as the N64 flag.
class F3DFlatShadePatch {
public:
    F3DFlatShadePatch(std::uint8_t* rdram, std::size_t rdram_bytes,
                      std::uint32_t display_list_address,
                      std::size_t display_list_bytes);
    ~F3DFlatShadePatch();
    F3DFlatShadePatch(const F3DFlatShadePatch&) = delete;
    F3DFlatShadePatch& operator=(const F3DFlatShadePatch&) = delete;
    std::size_t count() const { return changes_.size(); }

private:
    struct Change { std::size_t offset; std::uint32_t original; };
    std::uint8_t* rdram_;
    std::vector<Change> changes_;
};

// The medal builder writes alpha for only vertex 1 of each quad. Its other
// vertices start at zero, but the same guest vertex arena is reused by
// gameplay. RT64 interpolates alpha even for a flat-shaded triangle, so
// stale bytes make alternating arena buffers render with different opacity.
// Apply the initial zero layout only while the exact medal display list is
// consumed, then restore the guest memory.
class F3DMedalAlphaPatch {
public:
    F3DMedalAlphaPatch(std::uint8_t* rdram, std::size_t rdram_bytes,
                      std::uint32_t display_list_address,
                      std::size_t display_list_bytes);
    ~F3DMedalAlphaPatch();
    F3DMedalAlphaPatch(const F3DMedalAlphaPatch&) = delete;
    F3DMedalAlphaPatch& operator=(const F3DMedalAlphaPatch&) = delete;
    std::size_t count() const { return changes_.size(); }

private:
    struct Change { std::size_t offset; std::uint8_t original; };
    std::uint8_t* rdram_;
    std::vector<Change> changes_;
};

} // namespace tetrisphere
