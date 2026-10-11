# Blocksphere 1.1.0-beta.7 validation

Validation date: 2026-10-11. Linux x86-64 prerelease.

The development merge is `4ec5ea60f0bee9a2143d33f797e76a0f6de80a55`.
A fresh full Linux build and 56 CTests passed after integration. All 201 Python
tests passed on the merged source and independently on the publication snapshot
with authentic, isolated private toolchain inputs. The publication snapshot
matches all 105 runtime build inputs and the genuine generation manifest inputs.

Runtime build ID:
`f8b21c982b579de2723a36b7cbca5d38d1c3a5f3679c7ab56175782c48ff82ec`.

Native software-Vulkan and keyboard runs at 4:3 and 16:9 exercised the requested
main menu order, incoming Options transition, Controls selection/capture,
PlayStation layout, unsaved-exit decision, saved confirmation, Graphics
resolution and texture changes, Back and return focus. Both runs requested
Close Game and exited with status zero. Captures and diagnostics remain private.
See the [design supervision record](ui-design-supervision.md) for visual acceptance.

Code review found no critical or important defects. A minor limitation remains:
if SDL rejects a saved window-mode change, the application failure is logged
rather than reported by an additional inline message.

These checks do not claim physical-controller acceptance, native Windows
execution for this build, exhaustive late-game transitions, audible sound
acceptance or extended stability. The release includes only a fresh Linux
binary and source; previous Windows binaries are not relabeled. The stable
release and separate 1.0.1 draft remain intact.

Project direction, original port work and playtesting: djultra64. The public
history is maintained separately without rewriting local development history.
