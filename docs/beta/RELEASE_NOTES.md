# Tetrisphere 1.0 release notes

The configuration executable is beside the game in the package root. New installations default to widescreen (16:9), while saved preferences remain unchanged. Pause-menu text stays inside the white frame in widescreen.

This release keeps imported ROMs, progress, settings and diagnostics in `data/` beside the game executable. Separate game folders have separate data. Incompatible ROM selections show an error and reopen the selector; cancelling exits cleanly.

Opening the configurator without arguments now shows desktop dialogs for saved resolution, aspect ratio, refresh request, and window mode. Each choice saves immediately. Command-line options for graphics and controller mappings remain available.

This release adds F11 and Alt+Enter fullscreen controls during play, an RT64 inspector opened with F1 and limited to safe live filter changes, command-line options for window mode, MSAA, and filters, one-time ROM import into the game folder’s `data/` directory, and portable packages with source and checksum manifests. Inspector changes and command-line options last for one session. Use `tetrisphere-config` (or `tetrisphere-config.exe` on Windows) for persistent settings.

The game retains the M5 16:9 viewport, HUD, and framebuffer fixes and the mode and two-player support demonstrated in earlier builds. Physical 4K output and 120 FPS have not been validated. This release does not add 120 FPS, enlarge the sphere, or change culling.
