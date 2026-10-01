# Tetrisphere M6 Beta Implementation Plan

> **For agentic workers:** Implement task by task in the isolated `codex/m6-beta` worktree. Use red/green tests for behavior changes, verify the affected native build, and commit each independently testable result.

**Goal:** Deliver a portable Linux and Windows beta from one revision, with runtime fullscreen controls, a safe RT64 inspector, documented graphics flags, and auditable private-data-free packages.

**Architecture:** Parse run-only flags before ROM import and apply them after loading settings. Keep SDL window state and RT64 render state separately; allow only RT64 inspector controls that are safe with the game's wide HUD and framebuffer patches. Build packages from a committed revision, then validate outside the checkout and on real systems.

**Tech Stack:** C++20, SDL2, pinned RT64 `43373749`, CMake, Python package tools, Linux Vulkan, Windows Vulkan.

**Spec:** `docs/superpowers/specs/2026-09-27-tetrisphere-design.md`, M6 in `docs/superpowers/plans/2026-09-27-tetrisphere-pc-port.md`, and the user's 2026-10-01 M6 request.

## Global Constraints

- Preserve root `codex/m0-baseline`, M1 worktree, untracked instructions, and private `.local` contents.
- Keep the pinned dependency commits; do not edit generated C or the dependency lock for cosmetic reasons.
- Default settings → saved settings → run-only flags; a flag must not rewrite `settings.dat`.
- Separate physical button labels from logical actions, including Nintendo A/B and X/Y positions.
- Exclude ROM, extracted assets, saves, captures, audio, and credentials from Git and distributed packages.
- Keep physical 3840×2160, ROG Ally, two-hour sessions, and HITL-4 as explicit acceptance gates until measured.

## Review Focus

- Held Alt+Enter or F11: one transition per press, with no stuck guest key after release or focus loss.
- Inspector keyboard and mouse capture: no simultaneous game action, even on the frame the inspector opens or closes.
- Runtime fullscreen failure: remain windowed, with original window rectangle and saved preference intact.
- Unsupported MSAA sample count: clear diagnostic and safe fallback, with no invalid render target state.
- Corrupt settings and stale ROM cache: actionable error or recovery without silently replacing user data.

---

### Task 1: Run-only graphics options

**Files:** Create `include/tetrisphere/run_options.h`, `src/platform/run_options.cpp`, `tests/run_options_test.cpp`; modify `src/diagnostic/main.cpp`, `src/platform/linux_main.cpp`, `CMakeLists.txt`.

**Interfaces:** `parse_run_options(argc, argv)` returns options, optional ROM path, help/error; `apply_run_options(GraphicsSettings&, const RunOptions&)` changes only in-memory effective settings. Add `--fullscreen`, `--windowed`, `--msaa=off|2|4|8`, aliases `--msaa2x` etc., and only supported filter options.

- [ ] Write parser tests for valid combinations, last flag wins, unknown/missing values, help without ROM, Unicode ROM path, and settings immutability.
- [ ] Run the new test and observe failure.
- [ ] Implement parser and wire effective values through SDL and RT64 startup.
- [ ] Run parser, settings, graphics mapping, and native build tests; confirm help/error text.
- [ ] Commit.

### Task 2: Runtime fullscreen and input ownership

**Files:** Create `include/tetrisphere/window_mode_controller.h`, `src/platform/window_mode_controller.cpp`, `tests/window_mode_controller_test.cpp`; modify `src/platform/linux_main.cpp`, `src/platform/keyboard_state.cpp` as needed.

**Interfaces:** controller stores windowed rectangle and mode, consumes F11 and Alt+Enter key transitions, calls existing UHD/desktop fallback policy, and reports actual mode without saving it. Focus loss clears guest keys.

- [ ] Write transition tests for repeated keydown, keyup, focus loss, failed fullscreen, and rectangle restoration.
- [ ] Run the test and observe failure.
- [ ] Implement SDL event routing and mode transitions on Linux and Windows.
- [ ] Verify via native builds and targeted GUI actions that aspect, internal scale, audio, and pacing remain stable.
- [ ] Commit.

### Task 3: RT64 inspector policy

**Files:** Modify `src/graphics/rt64_context.cpp`, `include/tetrisphere/rt64_context.h`, pinned-source copy patch tooling and CMake; add policy tests.

**Interfaces:** F1 toggles inspector once per press. RT64 SDL event filter handles mouse and keyboard capture on both platforms. Safe inspector choices apply live; unsafe aspect, framebuffer, and 2D controls are disabled with explanation. Inspector changes are session-only and never write saved settings; saved settings and run flags are shown as effective start values.

- [ ] Write policy tests for allowed controls, repeat filtering, and preserved widescreen/HUD values.
- [ ] Run tests and observe failure.
- [ ] Implement pinned-source copy patch and host integration without changing the RT64 checkout.
- [ ] Build and exercise F1, mouse/keyboard capture, live filtering, and restart-only behavior on both native systems.
- [ ] Commit.

### Task 4: Portable packages and data recovery

**Files:** Add `tools/qa/package_m6.py` and package tests; update `README.md`, `docs/qa/`, notices, and CMake runtime staging if needed.

**Interfaces:** deterministic Linux tar and Windows ZIP from the same committed source; manifest hashes and full member-path audit; executable, configurator, libraries, and instructions included. ROM imports to per-user data once; saves and settings remain in user data across updates.

- [ ] Write failing package audit and clean-install tests, including spaces, non-ASCII paths, read-only package directory, corruption, and forbidden private files.
- [ ] Implement portable staging and audit, then run the tests.
- [ ] Build Release and diagnostic variants; inspect library dependencies, sanitizer/static-analysis results where supported.
- [ ] Smoke test extracted packages away from the checkout on Linux and real Windows; record commit and hashes.
- [ ] Commit packaging and documentation, then rebuild final packages from that commit.

### Task 5: Acceptance evidence and handoff

**Files:** Add `docs/milestones/M6.md` and private manifests under `.local/m6/`.

- [ ] Prepare the exact-package checklist for user-run visual/physical checks: Linux X11/Wayland where available, Windows, game modes, controllers/reconnection, focus, suspend/resume, normal/abnormal close, settings and save recovery.
- [ ] Leave two-hour CPU/GPU/memory sessions per system for the user and record their reported results; mark unavailable hardware paths pending.
- [ ] Audit final package contents, checksums, Git status, and shared revision; preserve earlier M5 evidence without repeating it unnecessarily.
- [ ] Give the user a short grouped physical/visual checklist tied to exact package hashes. HITL-4 remains pending until reported.
