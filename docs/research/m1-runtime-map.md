# M1 runtime and libultra map

Date: 2026-09-28. Scope: native NTPE rev0 route from startup through the
interactive M1 scenes. `config/recomp/runtime-symbols.json` is the canonical
machine-readable inventory. ROM bytes, generated C and full captures remain
private under `.local/m1/`.

## Connected routes

| Address | Symbol | Status | M1 contract |
|---|---|---|---|
| `0x800D1DC0` | `osInitialize` | implemented | host owns hardware initialization |
| `0x800D2050` | `osPiRawReadIo` | implemented | bounded aligned cartridge PIO |
| `0x800D1250` | `osCreateThread` | delegated | ultramodern scheduler |
| `0x800D13A0` | `osStartThread` | delegated | ultramodern scheduler |
| `0x800D0D00` | `osCreateMesgQueue` | delegated | scheduler message queue |
| `0x800D9ED0` | `osSetTimer` | delegated | host timer contract |
| `0x800D2E90` | `osPiStartDma` | implemented | bounded ROM-to-RDRAM DMA and PI completion |
| `0x800D31E0` | `osContInit` | delegated | SDL input callbacks |
| `0x800D0D30` | `osCreateViManager` | delegated | ultramodern VI/event threads |
| `0x800D1C4C` | `osSpTaskStartGo` | implemented | verified RT64 or RSPRecomp submission |
| `0x800D5930` | `osAiSetNextBuffer` | implemented | checked AI FIFO feeding an opened SDL device |

These classifications describe the exercised final M1 route, not generic
support for every possible argument or game state. PIO/DMA bounds, message
delivery, timers, EEPROM memory behavior, VI waits, input labels, task
descriptors and audio pacing have focused unit/CTest contracts.

## Fail-closed boundary

`fatal_unsupported(symbol, address, callsite, phase)` emits one structured
terminal diagnostic and exits nonzero. `dispatch_indirect` accepts only
confirmed targets. Unknown libultra calls, invalid addresses, unsupported RSP
tasks, malformed DMA, failed SDL audio and failed RT64 setup do not return
fabricated success.

CMake verifies the exact clean N64ModernRuntime and RT64 checkouts and their
recursive submodules before compiling. The common build ID is derived from
canonical commit, lock and tracked-source identities plus project/generated
input hashes; it does not hash platform-specific JSON serialization.

## Demonstrated execution

The native runs progressed beyond initial thread creation to repeated real
graphics/audio tasks. Logs contain the build ID and a runner nonce on ROM
validation, platform start, RT64 readiness/presents, RSP tasks and input. The
Windows and Linux runners hash the complete log, screenshots and WAV in their
private evidence manifests.

The audio proof is deliberately precise: ucode `0x800DE7D0` executed through
RSPRecomp, produced nonzero stereo PCM, and `SDL_QueueAudio` accepted that PCM
for an opened playback device. Whether the exact HITL build is perceptually
audible remains a human check; no document treats queued PCM as prior approval.

## Remaining compatibility surface

Only routes exercised by the M1 demonstrations are promoted. Longer sessions,
other modes and device lifecycle events can reveal new targets or argument
combinations. These remain explicit backlog items in
`m1-compatibility-backlog.md` and must be added one at a time with a failing
regression and a trace-backed contract.

## Reproduction

Use `docs/qa/m1-linux-protocol.md` and `docs/qa/m1-windows-protocol.md`. The
complete closeout runs the Python suite, CTest, both native evidence validators
and the paired package audit from the same source commit/build identity.
