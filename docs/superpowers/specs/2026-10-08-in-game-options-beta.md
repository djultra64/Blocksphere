# In-game PC settings beta

The 2026-10-08 user request authorizes a test version replacing the separate
configuration application with settings under Options. Prototype privately;
keep stable SDL mapping publication separate. Single-agent implementation and
one independent review. Preserve original Options entries and personal data.

## Chosen approach

Reuse the original submenu drawing, navigation and Confirm input path. Add a
fifth PC OPTIONS row after Audio, Data Edit, Scores and Controls. Open host-owned
nested menus (Graphics, Controls, Back), with Display/Quality and Buttons
submenus and paged choices capped at five visible rows with the original rendering functions.
Short labels fit the existing guest frame scratch storage; hooks substitute
labels before width/draw calls. Do not edit generated C or the original label table.
Expand the Options count and intercept only the added row and active PC menus.
Back returns one level; cancel from the PC root returns to original Options.

Expose all seven graphics fields, controller family, four logical face actions
and Magic from Settings schema 6. Choices preserve unrelated saved fields. The
last active family determines physical button names, including Switch B/A/Y/X.
Changes are queued from the guest thread to the host event loop, saved through
SettingsStore, and only then applied. No writes from the renderer thread.

## Live application

Window mode and input bindings/family can be applied through existing services.
Graphics buffers/aspect/MSAA/refresh initially save for next launch. The existing
RT64 inspector proves two filters can change live, but connecting persistent
settings to that path needs a synchronized renderer update. Do not claim live
filter application until the exact runtime bridge is exercised. Display LIVE or RESTART within choices. F11/Alt+Enter and the F1 inspector
are disabled in this beta by the user request of 2026-10-08.

## Acceptance

Tests cover navigation, Back/cancel, unchanged settings on cancel, every choice,
preservation of remaps/graphics, and physical-name labels. Hook tests must prove
existing Options/main menu dispatch is unchanged and added selections cannot
enter guest jump tables. Regenerate through the pinned recompiler, compile the
full game, then exercise an isolated keyboard/virtual-controller installation.
No configurator subprocess is allowed. Physical controller acceptance remains
pending; native Windows acceptance depends on the existing PC being available.
