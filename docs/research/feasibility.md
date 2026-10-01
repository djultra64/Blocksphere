# Tetrisphere recompilation feasibility — M1

## Decision

The PC-port route is technically viable and may proceed to M2. This is based
on native execution, not merely compilation: one N64Recomp-generated program
ran on Linux/AMD and Windows/NVIDIA, submitted real Tetrisphere display lists
to RT64 Vulkan, executed the identified audio microcode through RSPRecomp,
opened a host audio device, queued music/effect PCM and consumed gameplay
input in visible scenes.

## Demonstrated

- The exact NTPE rev0 ROM is recognized before guest execution; other identity
  and byte-order cases are rejected.
- Static mapping and ares traces agree on startup, the initial load image,
  direct/indirect control flow and the absence of a demonstrated overlay load.
- Pinned N64Recomp generation is byte-reproducible. Runtime/build identities
  include generated C, symbol/config input and pinned checkout receipts.
- Runtime routes used by the demonstration are real ultramodern/librecomp
  bindings. Unknown calls and indirect targets terminate with named JSON
  diagnostics instead of returning fabricated success.
- Real F3DEX-family graphics commands were accepted by pinned RT64. The game
  audio task at ucode `0x800DE7D0` produces nonzero stereo PCM through the
  recompiled RSP path.
- The identity-bound final runs reached interactive Training on Linux and
  Windows; an earlier Linux run also reached Single/Rescue. Both final runs
  show a visible state change after input, active graphics, and
  nonzero music/effect PCM accepted by an opened SDL playback device.
- ROM-free diagnostic packages for both platforms were reproduced byte for
  byte and passed a paired privacy/license/identity audit.

## Not demonstrated

M1 is not a complete port. Long-session stability, every mode, persistent
saves, two-player input, reconnection/focus cases, end-user release packaging,
widescreen, higher presentation rates and dynamic on-screen controller glyphs
remain later milestones. The current game frame still contains original N64
button art; the runtime already separates logical actions from physical Xbox,
PlayStation, Switch and keyboard labels, but replacement of in-game textures
is backlog work.

## Main risks

The largest M2 risk is breadth: additional game paths may expose unbound
libultra calls or indirect targets. RT64 accepted the sampled real command
graph, but other modes may use unsampled state combinations. Audio correctness
is proven for a representative task through device queue acceptance, not for
every sequence, long-session pacing, or human audibility of the HITL build.
Linux diagnostic teardown also aborts after evidence capture and needs a clean
runtime shutdown path before it can be treated as release behavior.
None of these findings invalidates feasibility; each
has a fail-closed diagnostic and a bounded validation strategy.
