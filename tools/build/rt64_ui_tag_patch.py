"""Pinned RT64 fragments carrying guest display-list provenance to DrawCall.uid."""


def replace_once(source: str, old: str, new: str) -> str:
    count = source.count(old)
    if count != 1:
        raise ValueError(f"RT64 UI tag anchor occurs {count} times: {old[:80]!r}")
    return source.replace(old, new)


def patch_ui_tag_source(name: str, source: str) -> str:
    if name == "rt64_interpreter.cpp":
        source = replace_once(source, "#include <cassert>\n",
            "#include <cassert>\n#include <cstdint>\n\n"
            "extern \"C\" void tetrisphere_rt64_ui_tag_update(\n"
            "    std::uint32_t command, const std::uint32_t* parents, std::size_t count);\n")
        anchor = "        while (dl != nullptr) {\n            opCode = (dl->w0 >> 24);"
        source = replace_once(source, anchor, '''        while (dl != nullptr) {
            // Resolve the current command, then the nearest tagged G_DL caller.
            // A child list may live outside the dynamic producer's command span.
            const auto base = reinterpret_cast<std::uintptr_t>(state->RDRAM);
            const auto cursor = reinterpret_cast<std::uintptr_t>(dl);
            std::uint32_t parents[64]{};
            std::size_t parentCount = 0;
            for (auto *returnAddress : state->returnAddressStack) {
                const auto address = reinterpret_cast<std::uintptr_t>(returnAddress);
                if (address >= base && address - base < 0x01000000u &&
                    parentCount < 64) parents[parentCount++] = std::uint32_t(address - base);
            }
            const std::uint32_t command =
                (cursor >= base && cursor - base < 0x01000000u) ?
                std::uint32_t(cursor - base) : 0xFFFFFFFFu;
            tetrisphere_rt64_ui_tag_update(command, parents, parentCount);
            opCode = (dl->w0 >> 24);''')
        anchor = "        state->dlCpuProfiler.end();\n    }\n};"
        source = replace_once(source, anchor,
            "        tetrisphere_rt64_ui_tag_update(0xFFFFFFFFu, nullptr, 0);\n"
            "        state->dlCpuProfiler.end();\n    }\n};")
    elif name == "rt64_rdp.cpp":
        source = replace_once(source, "#include <cassert>\n",
            "#include <cassert>\n#include <cstdint>\n\n"
            "extern \"C\" std::uint32_t tetrisphere_rt64_ui_tag_current();\n")
        anchor = "        DrawCall &drawCall = state->drawCall;\n        drawCall.minWorldMatrix = 0;"
        source = replace_once(source, anchor,
            "        DrawCall &drawCall = state->drawCall;\n"
            "        drawCall.uid = tetrisphere_rt64_ui_tag_current();\n"
            "        drawCall.minWorldMatrix = 0;")
    elif name == "rt64_rsp.cpp":
        source = replace_once(source, "#include <cassert>\n",
            "#include <cassert>\n#include <cstdint>\n\n"
            "extern \"C\" std::uint32_t tetrisphere_rt64_ui_tag_current();\n")
        anchor = "        auto &drawCall = state->drawCall;\n        if ((drawCall.textureOn != textureState.on)"
        source = replace_once(source, anchor, '''        auto &drawCall = state->drawCall;
        const std::uint32_t producerTag = tetrisphere_rt64_ui_tag_current();
        if (drawCall.triangleCount && drawCall.uid != producerTag) {
            state->flush(); // Never merge left, right, center, or untagged tris.
        }
        drawCall.uid = producerTag;
        if ((drawCall.textureOn != textureState.on)''')
    else:
        raise ValueError(f"no RT64 UI tag source patch for {name}")
    return source
