# In-game Options beta

This isolated experiment integrates the PC settings into the original Options
menu. Controls opens one dedicated remapping screen. Graphics shows all seven
settings together in one editor. Audio, Data Edit and Scores remain available. The experimental
distribution does not include the standalone configurator.

The main menu order is Single, VS, Practice, Training, Options and Close Game.
The original guest identities and submenu dispatch remain intact; rendering
and navigation map Training and Options into their new visible positions.

Options entry initializes all five rows before the transition begins. The Graphics
label participates in the original animation, preventing a four-row intermediate
layout and a late label replacement.

The Controls screen shows the generic controller illustration and all four configurable actions
at once: Drop / Select, Slide / Back, Restart Puzzle and Magic. Select a row and press the desired
physical button. Face actions accept the four face positions; Magic also accepts
shoulders and triggers. The keyboard equivalents are Z, X, A and I for the face
positions, J/S for shoulders and K/L for triggers. Escape cancels a capture;
Return can always select a row. Zoom In, Zoom Out, Movement and Pause are fixed reference controls. Zoom uses
the existing shoulder assignments (J/S on keyboard); Zoom and Restart Puzzle
are used in Puzzle.

Layout cycles through Auto, Keyboard, Xbox, PlayStation and Nintendo Switch.
Nintendo labels follow the actual swapped face positions. The screen uses the
original menu's private ROM font, rendered at a consistent half scale with
point sampling for its low-resolution pixels, and the animated menu background, with one static
generic pixel controller bitmap for every family. Both editors share a chamfered
ivory frame, cyan title and amber values. Small corner brackets and a directional
cursor follow focus; footer actions have equal visual weight and no permanent
filled highlight. Controls rows use whitespace instead of dotted separators.
The [design supervisor's record](qa/ui-design-supervision.md) defines the visual
direction and the required native review for every future game UI change.
No original game assets are included in the distribution.

Edits remain staged until Apply saves them successfully. Navigation continues
using the applied bindings while editing. Defaults stages the default layout;
Discard restores the applied layout. Back with unsaved edits presents an inline
Apply/Discard decision. Drop / Select and Slide / Back must use different physical positions;
Restart Puzzle and Magic must also use different physical positions, because
the input adapter suppresses Restart on a Magic button. Other shared assignments
are retained; focusing one shows SHARED BUTTON. The hidden
legacy auxiliary binding remains stored and is preserved by Defaults; raw N64
button names are absent from the primary UI. A failed save
keeps edits visible for retry. Prompts follow the last navigation device,
including a controller assigned to player two. The illustration is unbranded
and never changes with the family; button names continue to match physical
positions. Keyboard navigation preserves the selected editing layout.

Graphics shows resolution, aspect, refresh, window mode, MSAA, presentation
filtering and texture filtering on seven rows. Up/Down selects a row; Left/Right
cycles its values. There are no MORE pages or repeated options. Each change saves
before another edit is accepted; failed saves restore the persisted value and
show a retry message. Back returns to Options. Window mode and presentation/texture filters apply during play.
Resolution, aspect, refresh and MSAA are saved for the next launch. Controls apply
after successful saving without restarting the game.

F11, Alt+Enter and the F1 inspector are disabled in this beta. Select window mode
on the Graphics / Window row. A temporary launch override is excluded from
saved settings unless that field is explicitly selected. Settings use the existing
schema-6 SettingsStore. Each request merges only the selected field into persisted
settings, preserving unrelated settings and existing remaps.

Software validation and native execution are recorded separately from physical
controller acceptance. Native Windows and physical controller acceptance remain
pending. Software tests do not demonstrate compatibility with physical Xbox,
PlayStation, Switch or arcade hardware. Captures, ROM-derived source, ROMs and
personal data stay private.

Validated software checks for this implementation: 201 Python tests and 56
C/C++ CTests. Linux native captures in 4:3 and widescreen confirm the original backdrop and
transparent fonts, readable rows, the single generic bitmap, and physical-position
labels.
The private evidence also records staging, applying, reopening, discarding and save-error retry
checks. These results do not constitute physical controller acceptance.

The action list follows the original USA Nintendo 64 manual, printed pages
6–7 and 14. Reference review is separate from native gameplay acceptance.

The transition capture now covers the verified framebuffer-copy routine in menus
and gameplay. It preserves the wide GPU image when matching the original RGBA16
tile loads, including reordered, moving, partially visible and repeated tiles.
It no longer assumes a particular framebuffer-pair index, scene number or fixed
screen rectangle. The 4:3 path retains the original capture. See the
[transition audit](qa/menu-transitions-2026-10-09.md) for source coverage, native
scenarios and remaining visual acceptance.
