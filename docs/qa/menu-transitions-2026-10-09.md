# Options layout and transition capture audit

## Scope

The user approved one Graphics editor with seven simultaneously visible settings,
the original menu font in both PC editors, and an audit of captured backgrounds
throughout the game. Follow-up feedback required action and button names on the
same row, more separation from dotted rules, a higher controller illustration,
and complete glyph edges. This is a local beta change, not a milestone closure.

## Menus and font

Graphics has Resolution, Aspect, Refresh, Window, MSAA, Present Filter and Texture
Filter rows, plus Back. Up/Down moves focus; Left/Right cycles a value. Each
request merges its own field into persisted settings and serializes edits until
the host acknowledges saving. Failure restores the persisted value and leaves
an inline retry message; an external error dialog must not block the event loop.

Controls uses fixed action and value columns at the same vertical coordinate.
The controller was raised by 28 native pixels. The subsequent supervised visual
revision removes dotted row rules and the amber footer wedge, unifies both
page frames and uses focus brackets. See [UI design supervision](ui-design-supervision.md)
for the current composition and its native acceptance. Device names follow the physical
position, including Nintendo's swapped face names. Both editors follow their
last navigation device, including an assigned player-two controller; manual
family selection remains available.

Every new text call uses ROM font 0 at reciprocal scale 2048. There is no host
font or per-string squeeze. The original scaled blitter `80036FE4` clipped the
exclusive rectangle bounds: for glyph width W and height H it emitted extents
`2W - 4` and `2H - 6` in quarter pixels. Completing only PC glyph E4 commands by
4 and 6 quarters restores W/2 and H/2. Texture data, coordinates and advances
remain original. Point sampling is set after the original UI setup, which would
otherwise reset it. Native captures confirm complete R, P, G and S edges and
readable PlayStation, Xbox, Switch and keyboard layouts.

## Complete source inventory of the capture entry

Generated code was inspected without editing it. There are three direct callers
of the verified framebuffer-copy function `8006F894`:

| Caller | Copy callsite |
| --- | --- |
| `8003FC6C` | `8003FC84` |
| `8005C410` | `8005C488` |
| `800685FC` | `80068988` |

The common capture helper `8003FC6C` has these 21 callsites:

| Caller | Transition callsite |
| --- | --- |
| `8003FE3C` | `80040374` |
| `8004127C` | `800419C8` |
| `8004C7E8` | `8004CEC8` |
| `8004FBF8` | `8004FFD8` |
| `80050BFC` | `80051AE8` |
| `80052F68` | `8005367C` |
| `80054098` | `800545E8` |
| `80055A80` | `80055D18` |
| `80058100` | `80058774` |
| `8005907C` | `80059D18`, `80059D48` |
| `8005AFE8` | `8005B908` |
| `8005C978` | `8005CB48` |
| `8005EE48` | `8005F1A4` |
| `80060630` | `800609D8` |
| `80062C3C` | `80062E50` |
| `80063F1C` | `800645B4` |
| `80064BBC` | `80064CD4` |
| `80064EBC` | `80064F64` |
| overlay `80046A9C` | `80047080` |
| overlay `8005D674` | `8005DD44` |

`80070118` also calls `8005C410` at `800701F4`. Independent review confirmed the
inventory and found no additional obvious indirect capture references in the
generated code or aligned ROM pointer references. Addresses identify source
coverage; they do not by themselves establish a particular playable ending.

## Background distortion and correction

The previous GPU capture rejected menus by scene number. Consumption additionally
required framebuffer pair 1, projection 0, exactly 40 calls and unchanged screen
rectangles. Moving, reordered, partial or repeated transition tiles could fall
back to the native 320x240 RAM image instead of preserving the wide GPU image.

Capture now validates aligned, bounded, nonoverlapping guest buffers and the
recorded source framebuffer writer. Consumption identifies the exact 64x30
RGBA16 source regions across every framebuffer pair and projection, independently
of changing screen geometry. Every matching call tile is overridden, including
different calls sampling the same region. Conflicting mappings for one tile index
are rejected. Original call tiles are restored before workload reuse, and frozen
GPU tiles remain retired until their last consuming workload has completed.

The original transition choreography is retained. The fix replaces its captured
image with the wide image already rendered, rather than stretching a 4:3 snapshot.
The 4:3 path uses the original capture because no wider GPU surface exists.

## Verification and limits

201 Python tests and 56 C/C++ CTests passed after the menu and glyph changes.
The final save-error change was rebuilt and all 56 CTests passed again. Tests
cover graphics choices and save rollback, per-field persistence, layout and glyph
rectangle bounds,
physical-position labels, P2 navigation, buffer eligibility, and transformed,
partial, repeated and reordered capture tiles. An independent final code review
reported no important or critical findings.

Native Linux tests use isolated Xvfb displays, Vulkan Lavapipe and copied private
save fixtures. Captures and recordings stay under the private
`.local/menu-transitions-20261009/` directory. The run `final-menu-169` exercises
Title Start, root and Single selection, the PC editors, Puzzle, Rescue, Hide &
Seek, Time Trial, Vs CPU, Practice and the three Training branches, with their
entry, pause and exit transitions. Contact sheets inspect the image during the
effect, not only after it. This transition candidate preceded the final glyph
edge correction; the transition algorithm was unchanged by that correction.
`aligned-font-169` and `aligned-font-43-v2` verify the corrected editor layout
and font in both aspects. Package execution is recorded separately.

Early misrouted screenshots are retained as raw evidence and are not counted as
the named scenario. A deliberate save failure in the first 4:3 run exposed the
external-dialog input block; commands issued after that block are not credited
as gameplay or Options coverage. The final `retry-final-43` package run verifies
navigation after a deliberate save failure, a successful retry, and the original
Audio, Data and Scores screens. The separate package run also records Rescue
progression from level 1:1 to 1:3; it is not credited as a results or ending screen.
Vs player selection reaches the one-controller warning, without establishing
two-player gameplay acceptance.

Source coverage includes all identified copy routes. Visual coverage is bounded
to the recorded scenarios; it does not establish every late campaign completion,
every secret/credits route or every possible effect timing. Physical controller
acceptance and native Windows acceptance remain separate outstanding checks.

## Local package

Beta 4 lives at `.local/play-beta4/` with an origin-relative library path. The
local test folder links its data directory to the previous `play-beta3/data`
directory to preserve settings, imported ROM data and progress. A clean archive
must exclude that data link and all personal or ROM-derived evidence.
`BUILD.json` records the actual build ID and a dirty source revision; the build
is not represented as an unchanged historical commit.
