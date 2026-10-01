# M1 Linux native demonstration protocol

This protocol proves one native Linux run from the authorized NTPE rev0 ROM.
It does not accept emulator output, a replayed capture, a synthetic frame, or
precomputed PCM. Private ROM-derived inputs and evidence remain under
`.local/m1/` and are never committed.

## Build

Run these commands from a clean checkout root. The ROM and captured task inputs
are private prerequisites produced by M0/Tasks 3 and 6; the commands never copy
them into Git. Fetch, build, and bind the exact locked tool revisions, generate
the C, regenerate the captured audio microcode, and configure with matching
paths:

```sh
python3 tools/build/fetch_dependencies.py --root .local/deps
cmake -S .local/deps/N64Recomp -B .local/m1/toolchain/n64recomp-build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release
cmake --build .local/m1/toolchain/n64recomp-build \
  --target N64Recomp RSPRecomp
python3 -m tools.recomp.generate create-receipt

python3 -m tools.recomp.generate generate \
  --rom .local/m0/tetrisphere-us.z64 \
  --output .local/m1/linux-generated \
  --manifest .local/m1/linux-generated-manifest.json \
  --recompiler .local/m1/toolchain/n64recomp-build/N64Recomp
.local/m1/toolchain/n64recomp-build/RSPRecomp config/recomp/audio-rsp.toml

cmake -S . -B .local/m1/build-linux -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DTETRISPHERE_ROM=.local/m0/tetrisphere-us.z64 \
  -DTETRISPHERE_GENERATED_DIR=.local/m1/linux-generated/functions \
  -DTETRISPHERE_GENERATION_MANIFEST=.local/m1/linux-generated-manifest.json \
  -DTETRISPHERE_RSP_RECOMP_SOURCE=.local/m1/microcode/audio-rsp-recompiled.c \
  -DTETRISPHERE_GRAPHICS_SNAPSHOT=.local/m1/microcode/live-rdram.bin \
  -DTETRISPHERE_GRAPHICS_COMMANDS=.local/m1/microcode/final-private/gfx-command-list.bin \
  -DTETRISPHERE_AUDIO_SNAPSHOT=.local/m1/microcode/audio-live-rdram.bin
cmake --build .local/m1/build-linux --target tetrisphere-m1
```

If the host has X11 development headers in an isolated sysroot, add that
sysroot to `CPLUS_INCLUDE_PATH` while compiling RT64. The executable refuses a
ROM whose exact identity is not `tetrisphere-us-rev0`.

## Run and observe

```sh
.local/m1/build-linux/tetrisphere-m1 .local/m0/tetrisphere-us.z64
```

The final acceptance route is: boot, create a temporary in-memory profile,
enter Training, wait for the sphere, rotate it, move the active piece, and
perform the tutorial action. Capture the RT64 window before and after the final
actions. Record the exact PCM accepted by the opened SDL device during the
same process. Keep the bounded WAV, screenshots, stdout/stderr, and evidence
manifest together. A prior native Linux run also reached Single/Rescue, but it
is supplementary rather than the final cross-platform build evidence.

The required stdout chain is `rom_validated`, `native_linux_started`,
`rt64_device_ready`, at least one `rt64_display_list`, at least one
`rt64_present`, and the non-zero `logical_input_sample` values used by the
route. A valid visual pair shows the real Training sphere and a changed
sphere/piece/life state. Audio analysis must show sustained non-silent game
music and a distinct transient following the piece action; task submissions
must identify the verified audio ucode at `0x800DE7D0`.

Validate the sanitized manifest without copying private paths into it:

```sh
git worktree add /tmp/tetrisphere-m1-build-source a7c369f8
python3 tools/qa/run_m1_demo.py validate \
  .local/m1/evidence/linux-final/manifest.json \
  --evidence-root .local/m1/evidence/linux-final \
  --binary .local/m1/build-linux/tetrisphere-m1 \
  --source-root /tmp/tetrisphere-m1-build-source \
  --rt64-receipt .local/m1/build-linux/rt64-verification.json
```

## Input contract

Logical Confirm/Cancel are bound to physical south/east positions. The label
interface reports Xbox `A/B`, PlayStation `Cross/Circle`, and Nintendo Switch
`B/A`; west/north similarly report `X/Y`, `Square/Triangle`, and `Y/X`.
`TETRISPHERE_CONTROLLER_FAMILY` accepts `xbox`, `playstation`,
`nintendo-switch`, or `keyboard` as a manual fallback. The recorded M1 route
was automated with the keyboard: Z=Confirm, X=Cancel, Return=Start, and arrow
keys. Its evidence must therefore declare `keyboard` and prompt label `Z`; it
does not claim a physical Xbox input test.

M1 exposes the family/label boundary and a programmatic per-action remapping
interface used by both input and emitted prompt labels. The accepted game frame
still contains Tetrisphere's original N64 `A` texture; M1 does not claim that it
was replaced with `Z`. The runtime log proves that the same active binding used
for Confirm resolves to `Z`, and the integrated contract covers Xbox,
PlayStation, Switch, keyboard, and a remapped Switch binding. Dynamic
texture/overlay replacement and its settings UI remain later compatibility/UI
work.
