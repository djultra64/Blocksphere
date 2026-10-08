# SDL community controller mappings

The 2026-10-07 user request extends the Blitz arcade-controller correction to
Tetrisphere. The current source baseline is main at `7933c57`; historical M1
sources and its uncommitted README/branding work remain untouched.

The game uses SDL2 GameController input and previously ignored endpoints SDL
could not map. It did not explicitly load SDL_GameControllerDB. The correction
loads the bundled community database before SDL controller initialization, then
an optional `data/gamecontrollerdb.txt` (or the explicit TETRISPHERE_DATA_DIR).
Explicit SDL mapping hints retain their initialization priority. The loader reads
bounded files through memory streams to support Unicode installation paths.
It never downloads mappings, rewrites settings or guesses an unknown layout.
Unrecognized devices retain their name/GUID in stderr diagnostics.

The database is preserved unchanged from
[SDL_GameControllerDB revision c1d5289](https://github.com/mdqinc/SDL_GameControllerDB/tree/c1d5289a1f713b30a2c121e9fe6529d39360be0f),
SHA-256 `e606134678e6b3fdbdec9081289a1f0ba3e25d53b59b2aa3cdfe69bf491ad18f`.
The community contributors retain authorship; the exact Zlib notice and pinned
provenance are under `licenses/controller-db/`. The Windows AtGames Legends Gamer
Pro USB mapping is included; a Windows mapping is not applied to Linux by SDL.

CMake includes the loader and database in the build identity and copies the
database beside the game. The portable Linux/Windows packaging tool includes
it, the notice and provenance, checks source hashes and audits their presence.
The game keeps logical actions, physical positions, family prompts, remaps and
independent player identities in the existing input layer.

Six CTest input cases passed with dummy video/audio: community recognition,
explicit SDL precedence, reconnect, independent ports, action mapping and
keyboard focus. The new test starts with unknown virtual joysticks and verifies
raw buttons, diagonal hats, Start, simultaneous digital triggers, local override,
unchanged mapping bytes and missing/malformed/oversized optional files. It runs
from paths with spaces; the standalone check also used a Unicode directory.
The changed game platform translation unit compiled successfully. Package tests
create and audit deterministic archives using declared fixture executables;
they do not constitute newly built game packages.

The final Python suite passed 197/197 tests in 21.584 seconds. The broader
verification and environment-specific attempts are recorded in
[the receipt](evidence/sdl-controller-mappings-2026-10-07.json). Tests needing
private historical inputs used independent copies. Flatpak checks initially
failed because a /tmp checkout produced an invalid root filesystem argument,
then due to sandbox namespace restrictions. Moving the isolated test checkout
and executing the SDK checks with authorized host access resolved those issues.
No test assertion was weakened to obtain a pass.

This is a source correction. Existing shipped executables and packages are not
replaced, no release is published, and no new full game executable is claimed.
Rebuild from this source to apply it to a distributable binary. Physical arcade
panels, native Windows execution and device-specific family/prompts acceptance
remain pending. Single-agent self-review is not an independent review.

For physical acceptance, use a separate rebuilt installation and existing
settings. Check both player inputs, movement/diagonals, Confirm/Cancel/Magic,
Start, saved remaps and effective family labels, reconnect and relaunch. If SDL
still rejects the panel, collect its name/GUID and supply a matching local entry;
raw button numbers alone are not sufficient for automatic assignments.
