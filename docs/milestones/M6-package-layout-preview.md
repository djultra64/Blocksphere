# M6 package layout preview — 2026-10-01

The user requested English documentation in distributed packages and new documents, a tidy extracted-package root, and direct launch of the Linux executable. This preview implements those packaging changes. It does not replace the matched M6 candidate recorded in [M6.md](M6.md).

| Platform | Archive | SHA-256 | Executable source commit |
| --- | --- | --- | --- |
| Linux x86-64 | `.local/m6/packages-layout-preview/tetrisphere-0.9-linux-x86_64.tar.gz` | `d0fdfd756211f64a92b558faab0d443f94f13cefce920820b56e81d207103bf0` | `50113cfe5670b04f0186b622235afff0e460793c` |
| Windows x86-64 | `.local/m6/packages-layout-preview/tetrisphere-0.9-windows-x86_64.zip` | `75ae68c4fc13431e4e59452eea9af9ee05fc66c24301cd704a7458d2575dd3ac` | `730b01bc5605635ec28b26aea08ca17a16b185d1` |

The Windows PC was shut down at the user's request before this packaging change. The Windows preview therefore repackages the verified older native binaries with the new English documents and folder layout. **The two previews are not a matched build pair.** Rebuild Windows from `50113cf` when that PC is next available, then generate and audit a matched pair before replacing the candidate packages.

The Linux archive root contains only `tetrisphere` and `README.md`; `lib/` holds SDL2, `tools/` the configurator, `docs/` instructions and manifests, `licenses/` third-party notices, and `assets/` an explanation that original assets and the ROM are not included. Its executables use origin-relative ELF runpaths, so `run.sh` and `config.sh` are no longer needed. Windows keeps `SDL2.dll`, `dxcompiler.dll`, and `dxil.dll` beside `tetrisphere.exe` for runtime loading; the configurator has its own SDL2 copy in `tools/`.

Both archives passed the M6 package audit individually. The four package unit tests passed. From a Linux extraction path containing spaces and a non-ASCII character, `./tetrisphere --help` and `./tools/tetrisphere-config --data-dir PATH show` exited successfully, and `ldd` resolved each SDL2 dependency to the archive's `lib/`. No game-mode, graphics, audio, controller, or Windows runtime testing was performed for this preview; the user performs the hands-on acceptance checks.
