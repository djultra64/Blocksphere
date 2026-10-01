# Troubleshooting and known limits

- **The game does not start or reports a missing library:** Extract the complete archive. Run `./tetrisphere`, check that a Vulkan driver is installed, and keep `lib/` beside the executable. Include the exact error and distribution, GPU, and driver details in a report.
- **ROM rejected or cache damaged:** Only US NTPE rev0 is supported. Select your valid ROM again. An interrupted import must not silently replace a valid cached copy.
- **Settings damaged:** Back up your game data directory. `./tetrisphere-config show` reports settings errors; storage retains a recoverable copy when one exists. Do not delete saves to repair a setting.
- **Fullscreen:** F11 and Alt+Enter affect only the current session. If the display or driver rejects the mode, the game reports the failure and keeps the window. Physical 4K output, multiple displays, and suspend/resume have not yet been confirmed for this build.
- **Filters and MSAA:** Texture filtering is not limited to the HUD or 2D elements. MSAA may fall back if the GPU cannot provide the requested sample count. Check startup diagnostics for the effective setting.
- **Compatibility still to be checked:** 3840×2160 output, Wayland and X11, two-hour sessions, suspend/resume, reconnection, and abnormal close. Results from older builds do not automatically apply to this package.

For a useful report, include the package hash, distribution, GPU and driver, game mode, steps, expected result, and observed result. Logs and screenshots are optional. Do not attach a ROM, saves, or credentials.

- **Cannot save or import:** Move the complete game folder to a writable location. This release does not fall back to shared user data.
