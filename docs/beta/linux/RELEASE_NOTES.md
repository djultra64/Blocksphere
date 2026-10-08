# Blocksphere 1.0.1 — SDL controller mappings

This update loads a pinned SDL community controller database before controller
initialization. Previously unmapped devices can be recognized when the database
contains a matching entry for the operating system. An optional
`data/gamecontrollerdb.txt` supplies local mappings. Explicit SDL mapping settings
keep their initialization precedence. Mapping files are never downloaded or rewritten.

Saved action remaps, independent player ports and prompts for the last active
keyboard, Xbox, PlayStation or Nintendo Switch remain in the existing input layer.
Nintendo south/east and west/north names remain B/A and Y/X respectively.
Unknown devices are reported by name and GUID; their layouts are not guessed.
Keep your complete `data/` folder when updating.

The database is SDL_GameControllerDB revision
`c1d5289a1f713b30a2c121e9fe6529d39360be0f`, credited to its community contributors.
Its Zlib license and pinned provenance accompany the packages.

## Verification scope

The original source checks passed 197 Python tests and six CTests with virtual
SDL devices. Those checks did not rebuild the full game. This release's build,
package and execution evidence is recorded separately. Software mapping tests
are not physical controller acceptance. Physical arcade panels, the complete
controller/family/prompt matrix, physical 4K and high refresh rates remain pending.

## Downloads and installation

Extract the complete package, keep `lib/` beside the executables, and launch
`./tetrisphere`. Supply your own supported Tetrisphere US NTPE revision 0 ROM.
Packages contain no original game content or personal data. The separate settings
utility is still included in this stable update. In-game configuration is a
separate beta investigation and is not part of 1.0.1.
