# Blocksphere 1.0

The first public release of Blocksphere, a community Tetrisphere PC port for
Linux and Windows x86-64 through static recompilation. Development continues;
bug reports are welcome at https://github.com/djultra64/Blocksphere/issues.

## Downloads

- Linux: extract blocksphere-1.0-linux-x86_64.tar.gz and run ./tetrisphere.
- Windows: extract blocksphere-1.0-windows-x86_64.zip and run tetrisphere.exe.
  Keep the supplied lib/ folder beside the executables.
- Source: blocksphere-1.0-source.tar.gz includes port and pinned
  dependency source. See docs/RELEASE_SOURCE.md for private build prerequisites.
- SHA256SUMS verifies the three downloadable archives.

A user-supplied Tetrisphere US NTPE revision 0 ROM is required. No ROM, original
assets, saves, captured game media, or credentials are included.

## Included

Native graphics through RT64 and recompiled RSP audio; 4:3/16:9 presentation;
local two-player support; persistent graphics settings and action remapping;
prompts for the last active keyboard, Xbox, PlayStation, or Nintendo device;
portable data beside the executable; ROM import; fullscreen shortcuts and
the RT64 inspector; widescreen HUD and pause fixes; and a Close game menu item.
The graphics configurator sits beside the game executable.

## Known limits

Physical 4K, high-refresh presentation, extended stability, and the full
controller/device matrix remain incompletely verified. The Windows package now
includes the paired DXC/DXIL v1.8.2505.1 libraries; that updated runtime pair
has not received a new native playtest. The build
protocol requires private task captures as well as a ROM; building from only
a fresh ROM is not automated.

## License and credits

Newly authored port code is GPL-3.0-or-later. Original game material, project
branding, and upstream components retain their own rights and applicable
licenses. Full original-game, upstream, research, and AI-assistance attribution
is in the repository README; release notices are included in each package.
