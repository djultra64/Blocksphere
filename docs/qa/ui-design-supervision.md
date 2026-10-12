# Game UI design supervision

## Authority and direction

The user appointed a graphic design supervisor on 2026-10-09 for every game UI
change, with authority to redesign screens when necessary. The supervisor selects
the design and reviews native rendered captures. This is a design review role;
it does not create an additional user approval gate.

The selected direction is a restrained 1990s console settings interface using
Tetrisphere's own pixel lettering, animated sphere backdrop, cyan heading,
amber directional cursor and narrow mechanical frame. The existing large amber
footer wedge is decoration rather than a reliable focus indicator and must be
removed. Controls and Graphics must look like two pages of the same interface.

## Shared visual specification

All coordinates are in the game's 320 by 240 UI space.

- Keep the exact ROM font, the established fixed half scale and point sampling.
  Do not squeeze individual strings or substitute a font.
- Keep the original animated background visible. Do not add opaque panel fills.
- Use one chamfered frame for both pages: `(26,22)`, `(294,22)`, `(300,28)`,
  `(300,208)`, `(294,214)`, `(26,214)`, `(20,208)`, `(20,28)`, closing to the start.
  Draw a one-pixel ivory stroke and a restrained one-pixel dark teal shadow.
- Palette: ivory `#F4F0DE` for ordinary labels and frame, cyan `#49C8D6` for
  page titles, amber `#F4DA32` for values and focus, muted teal `#4E6263` for
  structural rules, white `#FFFFFF` for the focused label/value. Orange
  `#E6611E` is reserved for failure/warning text, not every inactive option.
- Both titles start at `(31,30)`. Controls moves its title from the old footer
  tab to this header. A single muted rule spans `x=28..292` at `y=44`.
- The footer starts with a muted rule at `y=195`, `x=28..292`. Footer action
  lettering sits at `y=200`, with no colored polygon behind any button.
- The selected item gets an amber geometric marker plus white lettering.
  Selection must never depend on text color alone. Use small amber corner
  brackets around the selected action/value region, not a filled rectangle.
  Brackets occupy two pixels in thickness and four pixels in each arm.
- Outside-frame prompts remain at `y=223`; put the primary instruction at
  `x=31` and the Back instruction at `x=206` on both pages. Preserve the actual
  last active device's physical labels.

## Controls page

- Keep the controller bitmap at `(29,50)` with its established 112 by 84 size.
  It must neither move down nor overlap the reference key beneath it.
- Keep Layout at `(155,30)` and its value at `(209,30)`; the title fits the
  left side of the shared header. Show the pair of adjustment arrows only
  while Layout has focus. Give the focused Layout value the same corner
  bracket treatment used elsewhere.
- Editable rows retain action `x=149`, value `x=240`, and baselines
  `y=57,82,107,132`. Remove the repeated dotted/plus-mark rules completely.
  Whitespace separates rows; action and value share exactly one baseline.
- Focused editable row: one small amber arrow at `x=139..144`, centered on
  `y+4`, and value brackets around `x=236..296`, `y-3..y+11`. Keep the button
  correspondence marker on the controller. The connector should be a quiet
  muted-teal dotted route, with amber only at its source and destination.
  Route its horizontal leg at `y=47`, below the header rule.
- Fixed reference labels retain `x=32`, values `x=78`, baselines
  `148,160,172,184`. Use ordinary ivory labels and amber values.
- Status stays in the right column, wrapping at word boundaries with
  baselines `174` and `184`. Ordinary/help/success status uses cyan; unsaved
  changes amber; explicit failure orange. No status overlaps a footer action.
- Put DEFAULTS at `(35,200)`, DISCARD at `(139,200)`, APPLY at `(249,200)`.
  Focus brackets enclose only the active action with four pixels of padding.
  All three actions have equal visual weight when unfocused. APPLY receives
  no permanent highlight merely because it is the last action.

## Graphics page

- Keep all seven fields visible together: action `x=35`, value `x=188`,
  baselines `51 + 19*n`, for `n=0..6`.
- Give the focused row one amber cursor at `x=27..32`, and corner brackets
  around its value region `x=183..297`, `y-3..y+11`. Within that region, retain
  the right adjustment arrow at `x=290..295`. Put the left adjustment arrow
  outside the bracket at `(179,y)`, `(174,y+4)`, `(179,y+8)`, leaving four
  pixels between arrow and bracket. White text,
  geometry and arrows together identify focus clearly.
- The status baseline moves to `183` so the line clears the footer rule.
  Ordinary/help/saved status is cyan, restart notices amber, failures orange.
- BACK sits at `(249,200)`, uses the same footer action treatment as Controls,
  and gets brackets only when focused.

## Basis for the decisions

Microsoft's [Xbox Accessibility Guideline 112: UI navigation](https://learn.microsoft.com/en-us/xbox/accessibility/xbox-accessibility-guidelines/112)
supports consistent component placement, predictable focus order and navigation
by controller or keyboard. Those principles justify a shared header, footer,
action treatment and stable seven-row layout.

Microsoft's [Xbox Accessibility Guideline 113: UI focus handling](https://learn.microsoft.com/en-us/xbox/accessibility/xbox-accessibility-guidelines/113)
calls for a visible indicator on every focused element. A visible shape together
with white lettering gives redundant feedback; a fixed amber wedge behind APPLY
does not identify which element currently owns input.

The period-specific art direction is a supervisor's judgment based on the
game's existing private menu captures and its actual ROM assets, not an assertion
that Microsoft prescribes a 1990s visual style. Small pixel cursors, chamfered
mechanical borders and the original slanted lettering retain that vocabulary.

## Required review and acceptance

For every game UI modification, involve the graphic design supervisor before
implementation and send native rendered captures after implementation. The
supervisor may require a full redesign within the authorized UI scope. A review
must assess visual hierarchy, period consistency, readability, alignment,
spacing, focus, navigation and physical-device prompt correctness. Do not mark
visual work complete on source-level tests alone. Record screenshots and the
supervisor's decision privately; keep original game assets and captures out
of Git/publication.

Accept these two pages only after reviewing native captures at both 4:3 and
16:9, including a focused editable row, Layout, DEFAULTS, DISCARD, APPLY,
Graphics BACK, an unsaved state, and a failed/save-success state. Inspect the
long PS TRIANGLE / Nintendo SWITCH / controller OPTIONS labels. No glyph
clipping, footer overlap, permanent false highlight or value misalignment is
acceptable. Real-controller verification is recorded separately when hardware
is available; manual family selection is not claimed as physical-device testing.

## Review findings on the first native implementation

Controls captures at 4:3 and 16:9 showed coherent hierarchy, aligned baselines,
clear footer focus, complete ROM glyphs, and readable PlayStation TRIANGLE,
OPTIONS and Nintendo SWITCH/PLUS labels. Saved, failed-save and two-line
unsaved-exit messages fitted without footer overlap. The shared-button badge
collided with the focused value's right bracket; the supervisor required its
removal because the explicit SHARED BUTTON status and duplicate visible values
already communicate the condition.

Actual Graphics captures showed all seven fields, restart/help/saved states,
and BACK focus clearly. The first implementation placed the left adjustment
arrow against the left focus bracket, producing an ambiguous combined shape.
The supervisor required the four-pixel separation specified above. Final
acceptance requires captures of these two small corrections from the final
packaged executable. Misrouted captures named graphics-* that still showed
Controls were excluded from review evidence.

Controls received visual acceptance after inspection of the packaged beta 5
captures in the private design-final-169 and design-final-43 runs. The removed
badge no longer collides with focus. The shared-button message, PlayStation
TRIANGLE focus and two-line unsaved-exit notice remained clear at both aspect
ratios. These captures used build identifier prefix `31c18920`.

Graphics received final visual acceptance after inspection of native packaged
beta 5 captures in the private design-approved-169 and design-approved-43 runs,
using build identifier prefix `3af71cc9`. Evidence was the genuine
graphics-confirmed, graphics-texture-confirmed, graphics-saved-confirmed,
graphics-back-confirmed, graphics-failed-confirmed and graphics-retry-confirmed
captures. The arrow and bracket are visibly separated; all seven rows remain
aligned and legible; BACK focus, orange inline failure and cyan successful retry
are clear at both aspect ratios. No further geometry changes were requested.

The final build changed only the Graphics arrow coordinates after the accepted
Controls build; the Controls visual acceptance remains based on `31c18920`.
Files named controls-ps-final-build and controls-apply-final-build that showed
the original Options menu were excluded. This record does not treat filenames
as proof of which screen was exercised.

## Main menu order review

The user explicitly selected SINGLE, VS, PRACTICE, TRAINING, OPTIONS, CLOSE GAME.
The supervisor approved retaining the original visuals and guest identities,
moving only the displayed Training and Options positions across every draw
path. The 19-pixel vertical and seven-pixel horizontal offsets preserve the
menu's original slanted arrangement; keeping the original identities preserves
label measurement, preview models, submenu dispatch and return focus.

Visual acceptance was granted after reviewing native captures from the private
menu-order-169 and menu-order-43 runs, build
`1809a43696ccb4cf0582c59586ab200a48a72aa02b3d160eab6d8946ff459cb4`.
The six rows appear in the requested order with consistent spacing and no
overlap. Training selection shows its graduation preview and enters the
Basics/Advanced/Puzzle submenu; returning restores Training on the fourth
displayed row. Options selection shows its television preview and enters the
Audio/Data Edit/Scores/Controls/Graphics submenu; returning restores Options on
the fifth displayed row. Close selection and Single/Close wrap captures retain
clear focus and the original frame, type and color treatment. No additional
aesthetic change was requested for this reorder. Application shutdown is a
functional verification recorded by the implementation agent, not inferred
from the selected Close screenshot.

## Options entry transition review

The supervisor accepted the Options entry consistency fix after comparing
native dense frame sheets from the private options-entry-before-v3 recording
against options-entry-fixed-169-v2 and options-entry-fixed-43-v2. In the original
recording the incoming submenu shows four rows around 2.233 to 2.300 seconds,
then GRAPHICS appears around 2.333 seconds with a layout change. In both corrected
aspect ratios GRAPHICS arrives with the other submenu labels, and the five-row
geometry remains consistent while the original crossfade and rotating preview
settle. No additional aesthetic redesign was required.

The accepted build is
`f8b21c982b579de2723a36b7cbca5d38d1c3a5f3679c7ab56175782c48ff82ec`.
Source review confirmed that the Options confirmation path now uses the same
five-row count as initialization and navigation. The additional transition
exception applies only to the ten known label measurement/drawing hook sites
under the original Options identity; input dispatch guards remain unchanged.

Earlier recordings that began after entry had settled or lost the confirmation
keypress to the recorder were excluded. The accepted dense sheets include the
selected main-menu Options row before confirmation, incoming transition frames,
and the settled submenu. This acceptance is specifically based on transition
frames, not only the final static screen.

## Final merged beta 7 release visual acceptance

On 2026-10-11 the graphic design supervisor reviewed new native captures from
the merged main source `4ec5ea60f0bee9a2143d33f797e76a0f6de80a55`, build
`f8b21c982b579de2723a36b7cbca5d38d1c3a5f3679c7ab56175782c48ff82ec`.
The private evidence runs were release-beta7-169 and release-beta7-43.

Visual release acceptance was granted for both aspect ratios. Controls retains
complete original ROM lettering, aligned action/value rows, the raised controller
diagram and clear Layout, editable-row and APPLY focus. PlayStation TRIANGLE
and OPTIONS fit without clipping. PRESS BUTTON, the two-line unsaved-exit
notice and SAVED feedback clear the footer. Graphics shows all seven fields,
separated adjustment arrows and corner brackets, readable resolution/filter
values, clear saved feedback and an unambiguous BACK focus. The shared frame,
cyan headings and restrained amber focus treatment match the accepted design;
the former permanent orange APPLY decoration remains absent.

The main menu shows SINGLE, VS, PRACTICE, TRAINING, OPTIONS, CLOSE GAME in the
requested order; selected Options and Close are clearly distinguished. Options
entry was reviewed in the 16:9 whole-video entry-sheet (incoming frames around
1.500 to 1.750 seconds) and the 4:3 dense sheet. Both show GRAPHICS arriving with
the other incoming labels and a consistent five-row layout through settling.
The 16:9 dense sheet beginning at 1.800 seconds was already settled and was not
credited as proof of entry behavior.

No material visual finding or further UI modification was required. This is
visual acceptance of the reviewed merged executable, not a claim of exhaustive
late-game transition coverage or physical-controller testing. Manual family
selection and keyboard navigation were used for these native menu captures.

## Windows 1.1.0 visual release acceptance

On 2026-10-12 the graphic design supervisor granted visual acceptance to the
actual extracted Windows release package at both 4:3 and 16:9. The reviewed
source was `fe99cdb3473f1697a4d7b9b24a667a707811bb54`, build
`927325c80fd96b70139a21dc93f78219f16faffa4762c4a2c08e57b773ece6c6`,
with executable SHA-256
`29afe28bcc279e15c1a15862f6fbc8aa59ccd5981aff8c02f3b5a7b809a70e97`.
Private evidence remains in the native-169 and native-43 directories of the
release-1.1.0-windows-20261012 QA run.

Reviewed native images show complete original ROM lettering, aligned Controls
action/value rows, readable PlayStation TRIANGLE and OPTIONS labels, clear
capture/unsaved/SAVED feedback and focus brackets following the active item.
Graphics keeps all seven fields visible, adjustment arrows separated from
brackets, readable edited resolution/filter values, saved feedback and clear
BACK focus. Main-menu order, selected Options preview and Close focus match
the accepted design. Options entry frames include the original main menu,
incoming crossfade with GRAPHICS alongside the other submenu labels, and
stable five-row settling at both aspect ratios. No material visual finding or
implementation change was required.

The 4:3 result metadata independently matches the source, build and executable
hash above and records application exit code zero. The 16:9 shutdown exit code
is a separate implementation-agent verification; visual acceptance does not
infer it from the Close screenshot. These menu captures use keyboard input
and manual controller-family selection and do not claim physical-controller
testing or exhaustive late-game transition coverage.
