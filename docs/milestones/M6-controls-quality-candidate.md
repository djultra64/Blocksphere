# M6 controls and image quality candidate — 2026-10-01

Runtime source: `beced4bab07f6ceabf89b78382b389c60beaaa8c`. Package pair build ID: `1f06e223ea87836d4cbc2219e59339d40a89cb0a347705d4087554e2d8585c6d`.

## Changes

- Desktop configuration and CLI now persist MSAA (off/2x/4x/8x), presentation filtering (nearest/linear/pixel), and texture filtering (linear/three-point). Saved choices reach RT64 before setup; launch options override them without modifying saved settings.
- Settings schema 6 reads versions 1–5. Existing display settings and action bindings survive migration. The former left-trigger Magic default moves to the west face button once; other Magic assignments remain intact. An explicitly saved left-trigger assignment in schema 6 stays unchanged.
- Magic defaults to Xbox X, PlayStation Square, Nintendo Switch Y, or keyboard J. Its face button does not also emit a spare C-left/C-up input. Existing family-aware Magic prompts use the selected physical button.
- Left-stick and D-pad directions produce the same digital directions and N64 stick values on both player ports, with an axis dead zone of 8192 and neutral output for opposing directions. Keyboard input remains confined to player one.
- With no explicit ROM path and no valid cache, startup scans only the executable directory for supported ROM/ZIP files with a normalized name containing `tetrisphere`. It checks at most 512 entries and 16 matching files, normalizes and validates candidates, skips invalid files, and imports a valid ROM into private per-user data. The selector remains the fallback. Explicit paths and valid caches keep priority.
- Each platform has its own English package guides. READMEs include default mappings, saved quality options, and automatic ROM discovery. The requested 2160p/120 FPS paragraph was removed. The package layout remains unchanged.

## Verification performed

Seven targeted CTests passed: `run_options`, `settings_store_roundtrip`, `config_gui_graphics`, `rom_input_formats`, `training_prompts`, `controller_port_mapping`, and `keyboard_focus_loss`. Five focused package tests passed. These checks cover migration, round trips, run-option precedence, invalid input handling, controller direction equivalence, drift neutrality, player separation, and Magic input behavior.

The extracted Linux package ran its help and configuration CLI directly. One isolated desktop dialog was opened and dismissed successfully. A brief isolated launch from a different working directory discovered a private ROM beside the executable, created its cache, initialized RT64, and reported effective MSAA 4x, linear presentation, and linear texture filtering from saved settings. No game modes were traversed during this check. The private ROM link was removed from the extracted package afterwards; it was never included in an archive or Git.

Both executables were built natively on the authorized Windows PC. The fetched binaries matched their build receipt and transfer hashes. Native help and saved MSAA/filter CLI checks passed, including the west-button Magic default. Windows desktop appearance and physical controller behavior remain for the user's testing.

Both final archives passed their individual audit and matched-pair audit. Previous candidate packages were preserved. The new Windows ZIP was copied to the authorized PC and extracted into a new delivery folder without replacing earlier packages.

## Archives

| Platform | Private archive | SHA-256 |
| --- | --- | --- |
| Linux x86-64 | `.local/m6/packages-controls-quality/tetrisphere-0.9-linux-x86_64.tar.gz` | `9e9a959e6790beeac2551ab22ed450a1ae6b5ba601b335e3a43b944750849fe7` |
| Windows x86-64 | `.local/m6/packages-controls-quality/tetrisphere-0.9-windows-x86_64.zip` | `db7dc33e46f45f90f62813f9c906797d73d752c2c4f412d83bd9fd770bd935ae` |

Hands-on M6 acceptance remains with the user. This candidate does not claim completion of the full beta review.
