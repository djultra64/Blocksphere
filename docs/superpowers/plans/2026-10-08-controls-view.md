# Original-style controls view implementation plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [x]`) syntax for tracking.

**Goal:** Replace the beta's per-button submenus with the user's approved single-screen controls editor inside Options.

**Architecture:** Keep the guest's original backdrop and font renderer; draw a dedicated controls composition through a verified guest hook. A host-neutral editing model stages five bindings and family selection. SDL events drive it while game input is suppressed. The existing settings queue persists a complete controls transaction and acknowledges success before switching bindings and prompts.

**Tech Stack:** C++20, SDL2, existing N64Recomp guest hooks, RT64, SettingsStore.

**Art correction:** The user subsequently required one original unbranded generic pixel bitmap for all devices, with no vector/polygon controller drawing. Family-aware button labels remain independent.

**Spec:** The approved 2026-10-08 visual proposal (private `controls-screen-original-style-v2.png`) and `docs/OPTIONS_BETA.md`; this plan replaces its Controls submenu section.

## Global Constraints

- Match original typography, colors, frame geometry, background and low-resolution bitmap language; visual plausibility in the original 1997 game is mandatory.
- Preserve all five configurable actions, staged Apply/Discard/Defaults, family fallback and optional remaps. Movement and Pause remain reference controls.
- Xbox, PlayStation and Nintendo Switch labels follow physical position, including Nintendo A/B and X/Y inversion.
- Current navigation remains active until Apply succeeds. Capture input cannot reach the game or activate an editor button.
- Save failure retains edits and current active settings. No silent stealing of another assignment.
- Keep original assets, screenshots, ROM, saves and local evidence private. Never hand-edit generated C.
- No claim of physical controller validation from synthetic controller or keyboard tests.
- Continue in the existing `codex/in-game-options-beta` worktree; preserve stable/main and existing evidence.

## Review Focus

1. Remapping Confirm/Back must not trap navigation, including duplicate Confirm/Back.
2. Held capture/Apply input must not activate the resumed guest menu.
3. Save failure and device disconnection must retain the staged map safely.
4. Manual family and last-active prompts must remain correct while keyboard navigates controller editing.
5. Guest rendering must preserve stack, display-list bounds and the original resource lifetime.

### Task 1: staged editing model

**Files:** `include/tetrisphere/controls_editor.h`, `src/platform/controls_editor.cpp`, `tests/controls_editor_test.cpp`.

- [x] Write and run failing behavior tests for staging, discard/defaults, atomic save acknowledgement, unsupported capture and ambiguous Confirm/Back.
- [x] Implement ControlsEditor; expose focus, capturing, dirty/error, pending settings and take_apply/complete_apply.
- [x] Verify model tests, including literal Xbox/Switch/PlayStation labels.

### Task 2: input and persistence integration

**Files:** pc_settings_menu/hook, linux_main, input_mapping, CMake and integration tests.

- [x] Replace nested Controls navigation with a launch request; keep Graphics unchanged.
- [x] Test then integrate thread-safe editor snapshots, SDL capture/navigation and save acknowledgement. Merge only controls into latest settings.
- [x] Suppress guest input during editor and until held controls release; preserve last-device detection.
- [x] Verify focused model/hook/input tests.

### Task 3: original-style dedicated rendering

**Files:** `src/graphics/controls_view.cpp`, its header/test, recomp hook configuration.

- [x] Verify guest font ABI and rendering callsite from original instructions.
- [x] Review display-list bounds and test whole-screen composition; draw flat action rows, original font, tubular frame, amber detail and one generic pixel controller bitmap for every family.
- [x] Regenerate through pinned N64Recomp and build Linux; inspect native captures at 4:3 and widescreen.

### Task 4: reviewable beta deliverable

- [x] Run Python suite, full CTests and native remap/apply/discard/reopen/save-error checks.
- [x] Request independent whole-change review and fix material findings.
- [x] Record final build identity and hashes; prepare an audited reproducible Linux test package with no standalone configurator or original game assets.
- [x] Update English beta notes with the verified software and native Linux scope.

## Verification record

The implementation passed 200 Python tests and 56 C/C++ CTests. Native Linux
keyboard runs inspected the generic bitmap in 4:3 and widescreen, all family
labels, staged edits, successful Apply, reopen, Discard and save-error retry.
Independent code and native-capture review found no remaining material issue.
No physical controller or native Windows acceptance is claimed. Private logs
and captures remain in `.local/options-beta-20261008`; earlier failed attempts
and controller art variants are retained there, outside Git and packages.

The audited Linux test archive is `blocksphere-1.1.0-beta.2-linux-x86_64.tar.gz`.
It reproduces byte for byte, contains no configurator or original game assets,
and includes licenses, AI disclosure and full original/upstream/research credits.
Build source: `cc849a64b42d810cbad4da05a26217fdeb417019`.
Build ID: `d4b605987ba44e2ef2b9b606ee9eee45b9bc3becc115643f09efd5476870d806`.
Archive SHA-256: `47c84b7f77efb017144b185fdc4c58b1caf8a932b2ad3e98638ed0d0b10cf62a`.
This is a local experimental deliverable; stable 1.0.1 remains a draft pending
a fresh native Windows package. Existing public versions and local artifacts
are preserved.

## Follow-up: action names from the original manual

The user accepted action names after review of the USA Nintendo 64 booklet,
printed pages 6–7 and 14. The remapping screen will show Drop / Select,
Slide / Back, Restart Puzzle and Magic. Remove C Left from primary editing;
retain its saved field internally for existing settings. Display Zoom In and
Zoom Out with their existing fixed shoulder assignments, alongside Move/Pause.
No new persistence schema or input behavior is needed for this UI correction.

- [x] Update existing editor/navigation and composition checks; verify failure.
- [x] Use named row constants; preserve the hidden legacy binding on Defaults.
- [x] Update English notes; build and inspect native rendering; prepare an audited
      Linux test package retaining previous artifacts and evidence.

Follow-up software verification: 200 Python checks pass without skips, and
56/56 C/C++ CTests pass. Independent review identified an existing Restart/Magic
collision; its regression was reproduced and Apply now rejects it while retaining
pending and applied settings. The final Linux package was rebuilt from the committed source; package and native verification are recorded below.

The follow-up archive is `blocksphere-1.1.0-beta.3-linux-x86_64.tar.gz`.
Build source: `5726fa7cf8def386c8ceaeea2accd5be8687a5dc`.
Build ID: `b5eb3a2ca68dec6205a6a6ca9ba7ae9128b1d4a61b7ac45e48d7b83334a23d3b`.
Archive SHA-256: `fb51a7baca924bb98543f027cdb1a4d5041538b11e3b80fede76ab6df71f92f1`.
The final-source build passed 56 CTests; the archive reproduces byte for byte,
and every member hash and the executable identity were checked. Native Linux
keyboard runs from the extracted package verified the four logical rows,
PlayStation/Switch labels, fixed zoom references, collision rejection, successful
Magic remapping and persistence after reopening. Previous packages and private
evidence remain preserved. This is a local Linux test deliverable; physical
controller and native Windows validation remain pending.
Native captures in 4:3 and widescreen passed independent visual review without
material findings; both runs closed normally with exit code 0.
