# Tetrisphere 1.0 — Windows x86-64

This build requires your own **Tetrisphere US NTPE rev0** ROM. It does not contain a ROM, original game assets, saves, or captured media. You need a working Vulkan driver. Exact minimum GPU, driver, and Windows versions still need physical testing.

## Install and launch

Extract the complete ZIP to a folder of your choice, then open `tetrisphere.exe`. Without a valid cached ROM, the game first checks the folder containing the executable for a `.z64`, `.v64`, `.n64`, or `.zip` file whose name contains “Tetrisphere” (case-insensitive; spaces and punctuation are ignored, so “Tetri Sphere” also matches). It validates candidates and loads the first supported US NTPE rev0 dump immediately. The scan does not enter subfolders and is limited to 512 directory entries and 16 matching files. Invalid candidates are skipped; if no valid ROM is found, a file selector opens. You can also pass its path as the last command-line argument. The game validates the revision and copies it to the `data/` folder beside the game executable. Later launches use that cache, so the original file need not remain in place. Keep `data/` when updating the package; it also contains settings, progress and diagnostics. Each extracted copy is independent. Move or copy the whole game folder to carry your data with you. Older betas stored data in your user profile; this beta does not import it automatically. To keep old progress, copy the old data directory’s contents into this game’s `data/` folder before launching.

The package root contains the game, `tetrisphere-config.exe`, this README, and the `lib/` directory containing `SDL2.dll`, `dxcompiler.dll`, and `dxil.dll`. Keep `lib/` beside the executables. `docs/` holds instructions and checksums; `licenses/` holds third-party notices. `assets/` explains why no original game assets are included. Keep the folders together when moving the package.

New installations start in widescreen (16:9); previously saved aspect settings are preserved. The game and configuration utility are beside each other in the package root.

Open `tetrisphere-config.exe` to change saved graphics settings in desktop dialogs. The dialogs show resolution, aspect ratio, refresh request, window mode, MSAA, presentation filter, and texture filter; each selection saves immediately. Restart the game to apply settings that cannot change during play. Controller mappings remain available through the command line. To inspect all saved values from a terminal, run `tetrisphere-config.exe show`. The configurator also accepts `--data-dir PATH show` for another game data directory.

Saved image quality choices are MSAA `off`, `2x`, `4x`, or `8x`; presentation `nearest`, `linear`, or `pixel`; and texture `linear` or `three-point`. Presentation filtering affects the final image; texture filtering affects N64 textures. Defaults are MSAA off, pixel presentation, and three-point textures. For example, `tetrisphere-config.exe graphics msaa 4x` saves 4x MSAA; `graphics presentation-filter linear` and `graphics texture-filter three-point` save the other filters. Unsupported MSAA counts fall back to a supported count and are reported in diagnostics.

## Default controller mappings

Defaults use physical button positions. Prompts follow the most recently active device; Nintendo face-button names differ from Xbox at the same position. The left stick and D-pad feed identical game directions, including menu navigation, with a dead zone for stick drift. Saved per-action remapping takes precedence over this table.

| Action / original input | Xbox | PlayStation | Nintendo Switch | Keyboard |
| --- | --- | --- | --- | --- |
| Move / navigate | Left stick or D-pad | Left stick or D-pad | Left stick or D-pad | Arrow keys |
| Confirm / drop / N64 A | A (south) | Cross (south) | B (south) | Z |
| Cancel / slide / N64 B | B (east) | Circle (east) | A (east) | X |
| Start / pause | Menu / Start | Options | + | Enter |
| N64 Z | View / Back | Share / Create | − | C |
| N64 L | LB | L1 | L | J |
| N64 R | RB | R1 | R | S |
| N64 C-up | Y (north) | Triangle (north) | X (north) | I |
| Magic / N64 C-down | X (west) | Square (west) | Y (west) | A |
| N64 C-right | RT | R2 | ZR | L |

Magic now defaults to the west face button on controllers and A on the keyboard, beside Z and X. The keyboard N64 L input uses J. On update, the former left-trigger default moves to this button; other saved Magic assignments remain intact. The west button sends Magic only, without a second C-left input. An explicit `bind magic trigger-left` restores the former assignment, and `bind magic west` selects the new one. These commands are available in the settings utility.

## Options for one run

Run `tetrisphere.exe --help` for all options. For example, use `tetrisphere.exe --fullscreen --msaa4x`. Available options include `--fullscreen`, `--windowed`, `--msaa=off|2x|4x|8x`, `--presentation-filter=nearest|linear|pixel`, and `--texture-filter=linear|three-point`. Presentation filtering affects the final image; texture filtering affects N64 textures. Unsupported MSAA sample counts fall back safely and are reported in diagnostics.

The order of precedence is defaults, saved settings, then options for this run. F11 and Alt+Enter toggle fullscreen without saving the preference. F1 opens or closes the RT64 inspector. Its presentation and texture filter changes last only for the current session. Resolution, aspect, HUD, framebuffer, and MSAA settings are chosen before launch and may require a restart. Inspector controls that could invalidate the 16:9 and HUD fixes are disabled.

See [controls](docs/CONTROLS.md), [troubleshooting](docs/TROUBLESHOOTING.md), [release notes](docs/RELEASE_NOTES.md), and [third-party notices](licenses/THIRD_PARTY_NOTICES.md). [Build information](docs/BUILD.json) identifies this package's source revision; [checksums](docs/SHA256SUMS) cover every distributed file. Do not delete `data/` when replacing the package. The game folder must be writable. After importing a ROM, keep your personal folder private; share the original clean archive instead.
