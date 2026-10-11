#include "tetrisphere/close_game_menu.h"

#include <atomic>

namespace {
std::atomic_bool close_requested{false};
// Preserve guest identities (Options=3, Training=4) while changing their order.
constexpr int display_row(int row) { return row == 3 ? 4 : row == 4 ? 3 : row; }

// NTPE rev0 func_8004C7E8 owns a 0x70-byte frame. 0x48..0x52 is
// unused; saved registers end at 0x47, a submenu buffer starts at 0x54.
// A frame-local string avoids modifying the shared submenu pointer table.
gpr label_address(std::uint8_t* rdram, recomp_context* ctx) {
    constexpr char label[] = "CLOSE GAME";
    const auto address = ADD32(ctx->r29, 0x48);
    for (unsigned i = 0; i < sizeof(label); ++i)
        MEM_B(i, address) = label[i];
    return address;
}
}

namespace tetrisphere {
bool take_close_game_request() { return close_requested.exchange(false); }
}

extern "C" void tetrisphere_close_game_menu_hook(std::uint8_t* rdram,
                                                recomp_context* ctx,
                                                std::uint32_t callsite) {
    switch (callsite) {
        case 0x8004D538u: // Model ID store; do not change the menu selection.
            if (ctx->r9 == 5) ctx->r9 = 3;
            return;
        case 0x8004E020u: // Reuse the original separator after Training.
            ctx->r1 = static_cast<std::int64_t>(ctx->r8) < 5;
            return;
        case 0x8004E0A4u: // Separator Y; this path only draws the main menu.
            ctx->r7 = ADD32(ctx->r7, -10);
            return;
        case 0x8004E0BCu: // Draw-loop bound, after the original slti.
            ctx->r1 = static_cast<std::int64_t>(ctx->r11) < 6;
            return;
        case 0x8004F030u: // Down: include the sixth row before wrapping.
            {
                const int next = display_row((display_row(int(ctx->r8)) + 1) % 6);
                ctx->r1 = next != 0;
                // The positive guest branch reloads this field and adds one.
                if (next != 0) MEM_H(0, 0xFFFFFFFF800E121Cull) = next - 1;
            }
            return;
        case 0x8004F0A8u: // Up: preserve the guest decrement/store path.
            {
                const int previous = display_row((display_row(int(ctx->r25)) + 5) % 6);
                MEM_H(0, 0xFFFFFFFF800E121Cull) = previous + 1;
                ctx->r25 = 1; // Always use the positive branch, including wrap.
            }
            return;
        case 0x8004F110u:
            // Only reached after the guest's confirm/Start/Z input check.
            // Keep the original five-entry dispatch table and its bound.
            if (ctx->r12 == 5 && MEM_H(0, 0xFFFFFFFF800E121Cull) == 5 &&
                MEM_H(0, 0xFFFFFFFF800E1220ull) < 0 &&
                MEM_W(0, 0xFFFFFFFF800E1218ull) < 0)
                close_requested.store(true);
            return;
        case 0x8004DC04u:
        case 0x8004DC98u:
        case 0x8004DD60u:
        case 0x8004DE28u:
        case 0x8004DEF0u:
        case 0x8004DF80u:
            if (MEM_W(0x6C, ctx->r29) == 5)
                ctx->r4 = label_address(rdram, ctx);
            return;
        case 0x8004DC74u:
        case 0x8004DD44u:
        case 0x8004DE0Cu:
        case 0x8004DEB8u:
        case 0x8004DF5Cu:
        case 0x8004E00Cu:
            // All six rows and animation paths share the same vertical shift.
            {
                const int row = MEM_W(0x6C, ctx->r29);
                const int delta = display_row(row) - row;
                ctx->r5 = ADD32(ctx->r5, -7 * delta);
                ctx->r6 = ADD32(ctx->r6, -10 + 19 * delta);
            }
            if (MEM_W(0x6C, ctx->r29) == 5)
                ctx->r7 = label_address(rdram, ctx);
            return;
    }
}
