# Release source and build prerequisites

The release executable source is local development commit
7933c575b1b650bff7efb5f471877fb77ac5d6d8. The initial public commit
8a974115e00f9d5c6866f7b728578944459e3cef contains the same runtime source,
tests, tools, and dependency lock. Release documentation and license additions
do not change the compiled executables. Build identifiers retain their original
values; they are not relabeled as binaries compiled from a later public commit.

The release's source archive contains the port source, build
scripts, and pinned N64Recomp (including RSPRecomp), N64ModernRuntime, and RT64
sources with recursive submodule sources. DEPENDENCIES.json records revisions.
Repository metadata and prebuilt dependency libraries/tools are excluded.
Their original licenses and copyright notices are retained. SDL2 is dynamically
linked and separately credited; its exact upstream license texts are included.

## Building

Use the source scripts and config/dependencies-m1.json to obtain the exact Git
checkouts, including recursive submodules, under .local/deps. The current build
verification checks Git revisions; extracted dependency source directories alone
do not provide Git metadata. The source bundle supplies those sources for
inspection and modification without distributing upstream Git history.

Follow docs/qa/m1-linux-protocol.md for the CPU and RSP toolchain and ROM
recompilation, and tools/build/build_windows.ps1 for the native Windows build.
The required toolchains are Python 3.11+, CMake, Ninja and a C++20 compiler on
Linux, or Visual Studio 2022 with its C++ workload on Windows. Linux also needs
SDL2, Vulkan and the X11/DBus development headers used by RT64.

You must supply your own supported US NTPE revision 0 ROM. The historical
CMake build also requires private graphics/audio task captures and the RSP
output described in docs/qa/m1-linux-protocol.md and config/recomp/microcodes.json.
Those original-game inputs and generated game code are not distributed.
The protocols describe a prepared development environment; this release does
not provide an automated build from a fresh ROM alone. Game-derived inputs
must be produced privately and never submitted in issues or pull requests.

## Windows runtime libraries

The public Windows package includes dxcompiler.dll and dxil.dll from the official
Microsoft DXC v1.8.2505.1 distribution (binary version 1.8.2505.32):
https://github.com/microsoft/DirectXShaderCompiler/releases/tag/v1.8.2505.1

The matching release's MIT, LLVM, and Microsoft terms are preserved in licenses/.
The older package's dxil.dll was not redistributable under its bundled terms
and is excluded. The public package therefore updates these two runtime
libraries while retaining the original game and configurator executable bytes.
BUILD.json records their new hashes and runtime distribution version.
The exact public Windows package passed a native desktop test on 2026-10-01,
including module hashes, ROM import, a cached second launch, portable settings,
RT64 presentation, and normal closure. See qa/release-1.0-public-installation.md.

## Verification limits

Historical release records report 197 Python tests and 48 Linux CTests passing,
with native Windows verification. Publication checks include archive, checksum, licensing, and identity checks.
A separate native Windows test exercised the public package; its scope and
results are recorded in qa/release-1.0-public-installation.md. Physical 4K, high refresh rates, the complete controller matrix, and
extended stability remain open areas for bug reports and further testing.
