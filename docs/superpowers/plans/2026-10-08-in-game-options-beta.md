# In-game Options beta implementation plan

**Goal:** Exercise persistent PC configuration within original Options submenus.
**Spec:** docs/superpowers/specs/2026-10-08-in-game-options-beta.md
**Execution:** Native, private source copy; no integration into stable release.

- [ ] Add a tested PCSettingsMenu model with nested choices, family labels and preservation.
- [ ] Add tested guest hooks for count, added-row dispatch, text substitution and Back.
- [ ] Queue persistence and safe live input/window updates in the host event loop.
- [ ] Regenerate authentic C, compile full prototype and run software suites.
- [ ] Exercise Options-to-PC-Settings route with isolated data and keyboard/virtual inputs.
- [ ] Record live/restart behavior and physical/native Windows gaps; retain private evidence.

Known guest anchors: func_8004C7E8 Options count writes 8004CC0C and 8004F38C;
Options menu index 3 at 800E121C, row 800E1220, count 800E12FC. Label width and
draw paths 8004E7D0/E8BC/E8E4/E9CC/EABC/EAFC/EBF0/EC18/ED08/EE00.
Confirm dispatch after row read at 8004F680; cancellation input read is 800DFD98.
Never overwrite the original four-entry label table at 800E10E4.
