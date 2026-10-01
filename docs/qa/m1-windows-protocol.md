# M1 Windows native demonstration protocol

This protocol builds with MSVC and runs the same N64Recomp output and locked
RT64 revision on a real Windows 11 x86-64 host. The ROM, generated C, task
captures, screenshots, PCM and credentials remain private outside Git.

## Build

In PowerShell, with the exact dependencies and private inputs prepared as in
the M1 plan, run:

```powershell
tools\build\build_windows.ps1 `
  -SourceRoot $PWD -BuildRoot $PWD\.local\m1\build-windows-native `
  -CMakeExe C:\Tools\cmake\bin\cmake.exe -PythonExe C:\msys64\ucrt64\bin\python3.exe `
  -Rom $PWD\.local\m0\tetrisphere-us.z64 `
  -GeneratedDir $PWD\.local\m1\linux-generated\functions `
  -GenerationManifest $PWD\.local\m1\linux-generated-manifest.json `
  -RspRecompSource $PWD\.local\m1\microcode\audio-rsp-recompiled.c `
  -GraphicsSnapshot $PWD\.local\m1\microcode\live-rdram.bin `
  -GraphicsCommands $PWD\.local\m1\microcode\final-private\gfx-command-list.bin `
  -AudioSnapshot $PWD\.local\m1\microcode\audio-live-rdram.bin
```

The result is `Release\tetrisphere-m1.exe`, `SDL2.dll`, `dxcompiler.dll`,
`dxil.dll`, and `windows-build-receipt.json`. The script derives the commit
from a clean Git checkout and fails if tracked sources, either locked checkout,
or a private input differs from its receipt.

## Interactive run

The evidence runner must execute in the logged-on console session, not the SSH
service session. From that desktop run:

```powershell
tools\qa\run_m1_windows.ps1 `
  -SourceRoot $PWD -BuildRoot $PWD\.local\m1\build-windows-native `
  -Rom $PWD\.local\m0\tetrisphere-us.z64 `
  -EvidenceRoot $PWD\.local\m1\evidence\windows
```

For remote orchestration, register that exact command with a scheduled-task
principal using `-LogonType InteractiveToken`, start it, then remove only the
temporary task registration. The runner validates the build receipt, opens the
real SDL audio device, records the exact PCM accepted by it, drives the keyboard
route, captures the RT64 window before/after an action, and writes
`native-windows-run.json`.

After packaging the sanitized manifest beside those artifacts, validate it:

```sh
git worktree add /tmp/tetrisphere-m1-build-source a7c369f8
python3 tools/qa/run_m1_demo.py validate \
  .local/m1/evidence/windows/manifest-a7c369f.json \
  --evidence-root .local/m1/evidence/windows \
  --binary .local/m1/build-windows-native/tetrisphere-m1.exe \
  --source-root /tmp/tetrisphere-m1-build-source \
  --rt64-receipt .local/m1/build-windows-native/rt64-verification.json \
  --platform windows-x86_64
```

The run receipt and event log must identify one build ID and contain
`native_windows_started`, RT64 Vulkan presentation, verified audio-RSP tasks,
and non-zero logical input. Human HITL-1 remains a separate approval of the
exact final build; this protocol does not pre-approve it.
