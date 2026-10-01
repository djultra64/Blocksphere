# Tetrisphere M1 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [x]`) syntax for tracking.

**Goal:** Deliver a reproducible Linux/Windows native diagnostic prototype of NTPE rev0 that boots into a real game screen and playable scene with input, RT64 graphics, music, and effects, backed by static/dynamic reverse-engineering evidence.

**Architecture:** A deterministic Python analysis pipeline consumes the locally supplied normalized ROM and emits a section/function/relocation model plus the pinned N64Recomp symbol/configuration TOML; a metadata ELF is reserved for relocation cases the native symbol format cannot represent. Generated C is linked to pinned N64ModernRuntime and RT64 integrations; unresolved game/runtime paths terminate with structured diagnostics. Private ROM-derived inputs and runtime captures remain under `.local/`; committed files contain only metadata, tooling, tests, patches, and reproducible instructions.

**Tech Stack:** Python 3 standard library tests, C/C++20, CMake/Ninja, N64Recomp/RSPRecomp, N64ModernRuntime, RT64, ares v148/GDB RSP, GCC/Clang/MSVC, PowerShell/SSH.

**Spec:** `docs/superpowers/specs/2026-09-27-tetrisphere-design.md`

## Global Constraints

- Support only the normalized 8 MiB NTPE rev0 ROM identified by `config/roms/tetrisphere-us-rev0.json`; reject all other inputs before execution.
- Never commit or package ROM bytes, extracted original assets, saves, captures, credentials, or ROM-derived generated C.
- Linux x86-64 and Windows x86-64 diagnostic builds must derive from the same committed source and pinned dependency revisions.
- RT64 must consume commands emitted by the real game; a synthetic triangle or framebuffer-only renderer is not acceptance evidence.
- Audio acceptance requires a real game audio task and audible music plus an effect through the candidate runtime path.
- Runtime gaps fail with a stable diagnostic naming the missing symbol/address and execution phase; no success-returning compatibility stubs.
- A build-only result, Wine result, or emulator result does not satisfy real Linux and real Windows execution.
- Stop at M1: do not implement M2 persistence, full-session completeness, configurable presentation, or packaging intended for release.

## Review Focus

- A modified or wrong-byte-order ROM must be rejected before generated code can execute; Task 1 tests normalized and rejected inputs.
- An unknown indirect target must terminate with a symbol/address diagnostic instead of jumping through host memory; Task 4 tests this boundary.
- An unsupported libultra call must identify its call site and phase and return nonzero; Task 5 tests fail-fast behavior.
- An unsupported RSP/RDP command in a captured real task must remain a recorded incompatibility and fail the diagnostic path; Task 6 tests command coverage.
- Remote Windows execution must prove process start, screen/scene markers, input consumption, graphics, and audio from one build identifier; Task 8 validates the signed evidence manifest.

---

### Task 1: Reproducible M1 workspace and dependency pins

**Files:**
- Create: `config/dependencies-m1.json`
- Create: `tools/build/fetch_dependencies.py`
- Create: `tools/build/check_environment.py`
- Create: `tests/test_m1_environment.py`
- Modify: `README.md`

**Interfaces:**
- Consumes: M0 ROM manifest and dependency candidate commits.
- Produces: `load_lock(path: Path) -> dict`, `verify_checkout(root: Path, entry: dict) -> list[str]`, and CLI JSON reports used by all later tasks.

- [x] **Step 1: Write tests first** for lock validation, wrong commit rejection, absent tools, private-path enforcement, and normalized ROM validation without exposing content.
- [x] **Step 2: Run** `python3 -m unittest tests.test_m1_environment -v`; **Expected:** failures because the build helpers do not exist.
- [x] **Step 3: Implement** the two Python tools and the M1 lock using the exact dependency commits validated during M1; dependency fetches go only to `.local/deps` and are idempotent.
- [x] **Step 4: Run** `python3 -m unittest tests.test_m1_environment -v` and `python3 tools/build/check_environment.py --json`; **Expected:** tests pass and the report distinguishes present, missing, and wrong-revision dependencies.
- [x] **Step 5: Fetch/build the pinned analysis tools** and record their actual version output in `.local/m1/toolchain/`; **Expected:** N64Recomp and RSPRecomp execute `--help` or their documented equivalent.
- [x] **Step 6: Commit** with message `build: pin M1 recompilation toolchain`.

### Task 2: Static ROM map and initial symbols

**Files:**
- Create: `tools/analysis/mips.py`
- Create: `tools/analysis/map_rom.py`
- Create: `config/recomp/sections.json`
- Create: `config/recomp/symbols.toml`
- Create: `docs/research/m1-static-map.md`
- Create: `tests/test_m1_analysis.py`

**Interfaces:**
- Consumes: normalized ROM path and M0 entry point `0x80025c50`.
- Produces: `analyze_rom(data: bytes) -> AnalysisResult`; deterministic JSON under `.local/m1/analysis/static-map.json` with boot copy/decompression, section, function, jump-table, indirect-call, compression, and overlay candidates plus confidence/provenance.

- [x] **Step 1: Write synthetic-fixture tests first** for MIPS direct calls, delay slots, jump tables, register-indirect calls, boot DMA/copy ranges, compression signatures, and overlapping section rejection.
- [x] **Step 2: Run** `python3 -m unittest tests.test_m1_analysis -v`; **Expected:** failures because the analyzer does not exist.
- [x] **Step 3: Implement** a deterministic analyzer that keeps detections and human rulings separate and never treats heuristic function starts as confirmed.
- [x] **Step 4: Run** the focused tests; **Expected:** all pass.
- [x] **Step 5: Analyze the private ROM**, manually inspect entry/boot/control-flow candidates against M0 disassembly, and commit only the non-copyrightable section/symbol metadata and prose findings.
- [x] **Step 6: Re-run twice and compare hashes** of the static map; **Expected:** byte-identical output.
- [x] **Step 7: Commit** with message `feat: map Tetrisphere ROM and control flow`.

### Task 3: Dynamic validation of startup and runtime targets

**Files:**
- Create: `tools/analysis/trace_runtime.py`
- Create: `config/recomp/tracepoints.json`
- Create: `tests/test_m1_trace_runtime.py`
- Create: `docs/research/m1-dynamic-map.md`

**Interfaces:**
- Consumes: Task 2 static-map schema and ares GDB remote snapshots.
- Produces: `merge_trace(static_map: dict, events: Iterable[TraceEvent]) -> dict`; `.local/m1/traces/*.jsonl`; committed trace summary containing addresses/counts without original bytes.

- [x] **Step 1: Write tests first** for fragmented GDB packets, hit counts, observed indirect targets, overlay load ranges, trace/static disagreement, and capture redaction.
- [x] **Step 2: Run** `python3 -m unittest tests.test_m1_trace_runtime -v`; **Expected:** failures because the merger does not exist.
- [x] **Step 3: Implement** the trace collector/merger by extending the proven M0 remote protocol without changing M0 evidence.
- [x] **Step 4: Run focused and full Python suites**; **Expected:** all tests pass.
- [x] **Step 5: Execute ares from reset through title and a representative playable scene**, collecting boot, DMA/decompression, indirect-call, graphics-task, and audio-task observations.
- [x] **Step 6: Resolve every static/dynamic disagreement** as confirmed, rejected, or open with evidence; update `sections.json`/`symbols.toml` only from a recorded ruling.
- [x] **Step 7: Commit** with message `feat: validate M1 map against execution traces`.

### Task 4: N64Recomp metadata, generation, and diagnostic core

**Files:**
- Create: `tools/recomp/generate.py`
- Create: `config/recomp/tetrisphere.us.rev0.toml`
- Create: `src/diagnostic/main.cpp`
- Create: `src/diagnostic/indirect_dispatch.cpp`
- Create: `include/tetrisphere/diagnostics.h`
- Create: `CMakeLists.txt`
- Create: `tests/test_m1_recomp.py`

**Interfaces:**
- Consumes: confirmed sections/symbols from Tasks 2–3 and pinned N64Recomp binary.
- Produces: native N64Recomp section/function TOML (and ELF32-MIPS metadata only if required for confirmed relocations), N64Recomp C under ignored `generated/`, `tetrisphere_diag`, and `dispatch_indirect(uint32_t target, uint32_t callsite)` that either resolves a confirmed target or emits structured fatal diagnostics.

- [x] **Step 1: Write tests first** for the pinned N64Recomp `[[section]]`/`functions` schema, stable generation manifest, unknown indirect targets, and absence of ROM bytes in committed/build manifests; add ELF structure tests only if confirmed relocation evidence requires that fallback.
- [x] **Step 2: Run** `python3 -m unittest tests.test_m1_recomp -v`; **Expected:** failures because generators are absent.
- [x] **Step 3: Implement** the symbol/configuration generator and N64Recomp driver; do not hand-edit generated C. Add a metadata ELF builder only when a confirmed relocation cannot be represented by the pinned symbol interface.
- [x] **Step 4: Run N64Recomp twice into separate ignored directories** and compare normalized file hashes; **Expected:** identical manifests and C output.
- [x] **Step 5: Build and run the Linux diagnostic executable**; **Expected:** it validates the ROM, reports build/map identifiers, enters the recompiled entry path, and either progresses or stops on a named unresolved route.
- [x] **Step 6: Run focused/full tests and `git diff --check`**, then commit `feat: generate reproducible Tetrisphere diagnostic core`.

### Task 5: N64ModernRuntime/libultra bindings and explicit gaps

**Files:**
- Create: `src/runtime/runtime.cpp`
- Create: `src/runtime/unsupported.cpp`
- Create: `config/recomp/runtime-symbols.json`
- Create: `tests/runtime_contract_test.cpp`
- Create: `docs/research/m1-runtime-map.md`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: generated recomp functions and confirmed libultra/system call sites.
- Produces: runtime binding table with statuses `implemented`, `delegated`, or `unsupported`; `fatal_unsupported(symbol, address, callsite, phase)` always terminates the diagnostic run nonzero.

- [x] **Step 1: Add failing CTest cases** for required boot/message/thread/timer/DMA/controller/audio/VI/RSP bindings and for a deliberately unsupported symbol.
- [x] **Step 2: Configure/build/run CTest**; **Expected:** failures for missing runtime integration.
- [x] **Step 3: Bind functions supported by pinned ultramodern/librecomp** and route genuine gaps to `fatal_unsupported`; never return fabricated success.
- [x] **Step 4: Iterate diagnostic execution using one failing path at a time**, adding a regression test before each compatibility fix.
- [x] **Step 5: Run CTest, Python suite, and diagnostic boot**; **Expected:** boot reaches graphics/audio task submission or produces a remaining-path report with no silent stubs.
- [x] **Step 6: Commit** with message `feat: connect recompilation to modern runtime`.

### Task 6: Real graphics/audio microcode characterization

**Files:**
- Create: `tools/analysis/microcode.py`
- Create: `config/recomp/microcodes.json`
- Create: `src/graphics/rt64_renderer.cpp`
- Create: `src/audio/audio_bridge.cpp`
- Create: `tests/test_m1_microcode.py`
- Create: `tests/microcode_contract_test.cpp`
- Create: `docs/research/m1-microcodes.md`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: real captured `OSTask` descriptors/data from Task 3, pinned RT64 and RSPRecomp.
- Produces: microcode identities/hashes without copyrighted blobs, decoded real command inventory, RT64 support matrix, RSPRecomp output when needed, and audio callback evidence counters.

- [x] **Step 1: Write failing tests** for task classification, safe bounds, command enumeration, unsupported command retention, and audio sample accounting.
- [x] **Step 2: Run focused tests/CTest**; **Expected:** failures because classifiers/bridges do not exist.
- [x] **Step 3: Implement** task extraction/classification and compare real graphics commands with the pinned RT64 parser; mark unsupported opcodes with task/offset evidence.
- [x] **Step 4: Run RSPRecomp on the identified audio microcode if it is not a runtime-supported ABI**, then execute one captured representative task against controlled memory and compare output/state invariants.
- [x] **Step 5: Integrate RT64 task submission and audio callbacks**, preserving fatal diagnostics for unsupported paths.
- [x] **Step 6: Run tests and a live diagnostic capture**; **Expected:** at least one real graphics task accepted by RT64 and one real audio task reaches nonzero PCM output.
- [x] **Step 7: Commit** with message `feat: prove Tetrisphere graphics and audio task paths`.

### Task 7: Linux native screen and playable-scene demonstration

**Files:**
- Create: `src/platform/linux_main.cpp`
- Create: `tools/qa/run_m1_demo.py`
- Create: `tests/test_m1_demo_evidence.py`
- Create: `docs/qa/m1-linux-protocol.md`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: Tasks 4–6 native core, RT64 renderer, audio bridge, controller callback.
- Produces: Linux `tetrisphere-m1`, structured demo event log, screenshot/audio evidence under `.local/m1/evidence/linux`, and a sanitized evidence manifest.

- [x] **Step 1: Write failing evidence-validator tests** requiring a single build ID and ordered markers for ROM validation, title screen, scene entry, input change, RT64 present, music, and effect.
- [x] **Step 2: Run** `python3 -m unittest tests.test_m1_demo_evidence -v`; **Expected:** failures because validator/manifest are absent.
- [x] **Step 3: Implement** the Linux host entry/input loop and evidence collector.
- [x] **Step 4: Execute the real native binary on Linux**, drive title to a representative playable scene, perform a visible input, and capture music/effect counters plus screenshot/audio.
- [x] **Step 5: Validate the evidence manifest**; **Expected:** all required markers from the same build/run and no emulator process.
- [x] **Step 6: Run full suites/build and commit** `feat: add Linux M1 playable diagnostic demo`.

### Task 8: Windows native build and real-machine demonstration

**Files:**
- Create: `tools/build/build_windows.ps1`
- Create: `tools/qa/run_m1_windows.ps1`
- Create: `docs/qa/m1-windows-protocol.md`
- Create: `tests/test_m1_windows_evidence.py`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: same source/dependency lock as Linux and authorized SSH configuration discovered in Meltdown without copying credentials.
- Produces: Windows x86-64 `tetrisphere-m1.exe`, `.local/m1/evidence/windows` capture, and sanitized evidence manifest matching Task 7 schema.

- [x] **Step 1: Write failing tests** for Windows manifest platform/build ID, required runtime markers, and rejection of Wine/Proton evidence.
- [x] **Step 2: Run focused test**; **Expected:** failure because Windows evidence tooling is absent.
- [x] **Step 3: Implement** idempotent PowerShell configure/build/run scripts with explicit dependency and GPU/audio diagnostics.
- [x] **Step 4: Copy only source/build inputs and the user-supplied ROM privately over authorized SSH**, build with MSVC on Windows, and copy back sanitized logs plus hashes.
- [x] **Step 5: Execute on the real Windows PC**, reach the same screen/scene, consume input, present RT64 graphics, and observe music/effect output.
- [x] **Step 6: Validate the evidence manifest** and compare source/config/build IDs with Linux; **Expected:** same commit/config, platform-specific binary hash, all runtime markers.
- [x] **Step 7: Commit** with message `feat: add Windows M1 build and execution proof`.

### Task 9: Deliverables, feasibility decision, and HITL-1 package

**Files:**
- Create: `tools/qa/package_m1.py`
- Create: `tests/test_m1_package.py`
- Create: `docs/research/feasibility.md`
- Create: `docs/research/m1-symbol-map.md`
- Create: `docs/research/m1-compatibility-backlog.md`
- Create: `docs/milestones/M1.md`
- Modify: `README.md`
- Modify: `docs/superpowers/plans/2026-09-27-tetrisphere-pc-port.md`

**Interfaces:**
- Consumes: Linux/Windows builds and validated evidence manifests.
- Produces: ROM-free diagnostic archives under `build/packages/`, SHA-256 manifest, exact reproduction/run commands, criterion-by-criterion M1 evidence, risks, and HITL-1 script.

- [x] **Step 1: Write failing package-audit tests** for forbidden extensions/magic, private paths/credentials, missing licenses, mismatched build IDs, absent symbol/runtime/microcode/backlog docs, and missing Linux/Windows execution evidence.
- [x] **Step 2: Run focused test**; **Expected:** failure because the packager/deliverables are absent.
- [x] **Step 3: Implement** deterministic ROM-free packaging and write feasibility/symbol/backlog/milestone documents with demonstrated results separated from hypotheses.
- [x] **Step 4: Update the master plan** only after every technical M1 checkbox has direct evidence; leave HITL-1 explicitly pending without attributing approval.
- [x] **Step 5: Build/package from a clean ignored build directory**, run package audit, Python suite, CTest, reproducibility comparison, and both evidence validators.
- [x] **Step 6: Commit** with message `docs: close technical M1 and prepare HITL-1`.
- [x] **Step 7: Request independent whole-branch review**, fix all Critical/Important findings with red-green regressions, re-run the complete verification, and commit fixes.
