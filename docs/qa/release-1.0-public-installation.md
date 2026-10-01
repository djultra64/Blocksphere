# Public release installation verification — 2026-10-01

Tested release: https://github.com/djultra64/Blocksphere/releases/tag/v1.0.0
The test downloaded the published archives rather than using a local staging copy.

## Windows native desktop

The public ZIP SHA-256 is
983557defe59924228badce1a438f67158ab362eb8c3e22cfdf97e1c63cbc373.
It ran in the logged-on console session of the owner's real Windows PC,
without Wine or an emulator, in a separate installation and data directory.
All archive member hashes matched BUILD.json.

The executable help and portable-settings test passed. SDL2.dll, dxcompiler.dll,
and dxil.dll loaded from the package's lib/ directory with their expected hashes.
RT64 opened its native window and presented the original animated intro.
A supported private ROM was imported into the package's portable data directory;
a second launch without a ROM argument used that cache and presented frames.
Both launches closed normally with exit code 0. The machine-readable result
is release-1.0-public-windows.json. Captured images, audio, private ROM data,
and raw logs remain private and are not published.

This is an installation and native-rendering smoke test. It does not constitute
a new full gameplay, controller, or perceptual audio review.

## Linux package installation

The public tar.gz SHA-256 is
ddd8fe707455cd16cb9977856ee0bc8b9693c8b744f538f7cf0122ac3a9ba0e0.
The extracted single wrapper folder was flattened into Apps/Tetrisphere-Blocksphere
according to Quiver's GameInstallationService rule. The executable help passed.
In Xvfb with lavapipe and dummy audio, ROM import and a second cached launch
both opened RT64 and presented frames. The test harness ended each process
with SIGTERM; this check does not establish normal Linux GUI closure.

## Quiver catalog compatibility

The launcher source was inspected for release filtering, archive handling,
wrapper-folder flattening, and executable discovery. Both public packages
contain the game executable and configurator at the extracted root, with
runtime libraries under lib/. Choose tetrisphere (or tetrisphere.exe) as the
game executable if Quiver presents both executable candidates.

The proposed catalog entry uses releaseAssetFilter x86_64, selecting the
platform packages and excluding the source archive and SHA256SUMS. The source
archive's trailing -source.tar.gz also matches Quiver's auxiliary-asset rule.
The launcher GUI itself was not installed or exercised in this environment;
package installation checks reproduced its documented source behavior.
