#include "tetrisphere/close_game_menu.h"

#include <array>
#include <cassert>
#include <cstring>
#include <vector>

int main() {
    std::vector<std::uint8_t> memory(0x800000, 0xCD);
    auto* rdram = memory.data();
    recomp_context ctx{};
    ctx.r29 = static_cast<std::int32_t>(0x807FF000u);
    MEM_H(0, 0xFFFFFFFF800E1220ull) = -1;
    MEM_W(0, 0xFFFFFFFF800E1218ull) = -1;

    // The sixth row must participate in the real guest draw loop.
    ctx.r11 = 5;
    ctx.r1 = 0;
    tetrisphere_close_game_menu_hook(rdram, &ctx, 0x8004E0BCu);
    assert(ctx.r1 == 1);
    ctx.r11 = 6;
    tetrisphere_close_game_menu_hook(rdram, &ctx, 0x8004E0BCu);
    assert(ctx.r1 == 0);

    // Extend the guest separator loop through Training, but not after Close.
    ctx.r8 = 4;
    ctx.r1 = 0;
    tetrisphere_close_game_menu_hook(rdram, &ctx, 0x8004E020u);
    assert(ctx.r1 == 1);
    ctx.r8 = 5;
    tetrisphere_close_game_menu_hook(rdram, &ctx, 0x8004E020u);
    assert(ctx.r1 == 0);

    // Only the model selector uses OPTIONS; the actual menu selection stays 5.
    MEM_H(0, 0xFFFFFFFF800E121Cull) = 5;
    ctx.r9 = 5;
    tetrisphere_close_game_menu_hook(rdram, &ctx, 0x8004D538u);
    assert(ctx.r9 == 3);
    assert(MEM_H(0, 0xFFFFFFFF800E121Cull) == 5);
    for (int selection = 0; selection < 5; ++selection) {
        ctx.r9 = selection;
        tetrisphere_close_game_menu_hook(rdram, &ctx, 0x8004D538u);
        assert(ctx.r9 == static_cast<gpr>(selection));
    }

    // Down from Training reaches Close game, then wraps to Single.
    ctx.r8 = 4;
    ctx.r1 = 0;
    tetrisphere_close_game_menu_hook(rdram, &ctx, 0x8004F030u);
    assert(ctx.r1 == 1);
    ctx.r8 = 5;
    tetrisphere_close_game_menu_hook(rdram, &ctx, 0x8004F030u);
    assert(ctx.r1 == 0);
    // Up from Single wraps to Close game.
    ctx.r8 = 4;
    tetrisphere_close_game_menu_hook(rdram, &ctx, 0x8004F0CCu);
    assert(ctx.r8 == 5);

    const std::array<std::uint32_t, 6> measure_sites{
        0x8004DC04u, 0x8004DC98u, 0x8004DD60u,
        0x8004DE28u, 0x8004DEF0u, 0x8004DF80u};
    const std::array<std::uint32_t, 6> draw_sites{
        0x8004DC74u, 0x8004DD44u, 0x8004DE0Cu,
        0x8004DEB8u, 0x8004DF5Cu, 0x8004E00Cu};
    for (const auto site : measure_sites) {
        MEM_W(0x6C, ctx.r29) = 5;
        ctx.r4 = static_cast<std::int32_t>(0x800EC9A0u);
        tetrisphere_close_game_menu_hook(rdram, &ctx, site);
        for (unsigned i = 0; i < sizeof("CLOSE GAME"); ++i)
            assert(MEM_BU(i, ctx.r4) == "CLOSE GAME"[i]);
        // Scratch does not overlap saved registers or the submenu buffer.
        assert(MEM_BU(0x47, ctx.r29) == 0xCD);
        assert(MEM_BU(0x53, ctx.r29) == 0xCD);
        assert(MEM_BU(0x54, ctx.r29) == 0xCD);
        assert(MEM_BU(0x63, ctx.r29) == 0xCD);
        MEM_W(0x6C, ctx.r29) = 4;
        ctx.r4 = static_cast<std::int32_t>(0x800EC994u);
        tetrisphere_close_game_menu_hook(rdram, &ctx, site);
        assert(static_cast<std::uint32_t>(ctx.r4) == 0x800EC994u);
    }
    for (const auto site : draw_sites) {
        // Shift every main-menu row together, including transition draw paths.
        for (int row = 0; row < 6; ++row) {
            MEM_W(0x6C, ctx.r29) = row;
            ctx.r6 = 60 + 19 * row;
            tetrisphere_close_game_menu_hook(rdram, &ctx, site);
            assert(ctx.r6 == static_cast<gpr>(50 + 19 * row));
        }
        MEM_W(0x6C, ctx.r29) = 5;
        ctx.r7 = static_cast<std::int32_t>(0x800EC9A0u);
        tetrisphere_close_game_menu_hook(rdram, &ctx, site);
        for (unsigned i = 0; i < sizeof("CLOSE GAME"); ++i)
            assert(MEM_BU(i, ctx.r7) == "CLOSE GAME"[i]);
        MEM_W(0x6C, ctx.r29) = 0;
        ctx.r7 = static_cast<std::int32_t>(0x800EC974u);
        tetrisphere_close_game_menu_hook(rdram, &ctx, site);
        assert(static_cast<std::uint32_t>(ctx.r7) == 0x800EC974u);
    }

    // Separators follow the same shift while retaining their row spacing.
    ctx.r7 = 74 + 19 * 4;
    tetrisphere_close_game_menu_hook(rdram, &ctx, 0x8004E0A4u);
    assert(ctx.r7 == 64 + 19 * 4);

    // The shared RESCUE entry and its string remain untouched.
    assert(MEM_BU(0, 0xFFFFFFFF800E10CCull) == 0xCD);
    assert(MEM_BU(0, 0xFFFFFFFF800EC9A0ull) == 0xCD);
    assert(!tetrisphere::take_close_game_request());
    for (int selection = 0; selection < 5; ++selection) {
        MEM_H(0, 0xFFFFFFFF800E121Cull) = selection;
        ctx.r12 = selection;
        tetrisphere_close_game_menu_hook(rdram, &ctx, 0x8004F110u);
        assert(!tetrisphere::take_close_game_request());
    }
    MEM_H(0, 0xFFFFFFFF800E121Cull) = 5;
    ctx.r12 = 5;
    tetrisphere_close_game_menu_hook(rdram, &ctx, 0x8004F110u);
    assert(tetrisphere::take_close_game_request());
    assert(!tetrisphere::take_close_game_request());
    // A submenu or active transition cannot request application shutdown.
    MEM_H(0, 0xFFFFFFFF800E1220ull) = 0;
    tetrisphere_close_game_menu_hook(rdram, &ctx, 0x8004F110u);
    assert(!tetrisphere::take_close_game_request());
    MEM_H(0, 0xFFFFFFFF800E1220ull) = -1;
    MEM_W(0, 0xFFFFFFFF800E1218ull) = 15;
    tetrisphere_close_game_menu_hook(rdram, &ctx, 0x8004F110u);
    assert(!tetrisphere::take_close_game_request());
}
