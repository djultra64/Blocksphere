#include "tetrisphere/menu_medal_trace.h"

#include <iostream>

int main() {
    tetrisphere::MenuMedalTrace trace(true, 3);
    if (trace.observe(true, 18).has_value()) {
        std::cerr << "startup menu must not be traced\n";
        return 1;
    }
    if (trace.observe(false, 0).has_value()) {
        std::cerr << "gameplay must not be traced\n";
        return 1;
    }
    const auto first = trace.observe(true, 18);
    const auto second = trace.observe(true, 0);
    const auto third = trace.observe(true, 18);
    if (!first || !second || !third ||
        first->sequence != 1 || first->patched_triangles != 18 ||
        second->sequence != 2 || second->patched_triangles != 0 ||
        third->sequence != 3 || third->patched_triangles != 18 ||
        trace.observe(true, 18).has_value()) {
        std::cerr << "returned menu must trace bounded per-list counts\n";
        return 1;
    }
    tetrisphere::MenuMedalTrace disabled(false, 3);
    disabled.observe(false, 0);
    if (disabled.observe(true, 18).has_value()) {
        std::cerr << "disabled trace must remain silent\n";
        return 1;
    }
}
