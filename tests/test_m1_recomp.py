"""Task 4 contracts for reproducible N64Recomp generation and diagnostics."""
from __future__ import annotations

import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import tomllib
import unittest

from tools.recomp.generate import (
    GenerationError,
    build_generation_manifest,
    compare_manifests,
    validate_artifact_paths,
    validate_generation_paths,
    validate_supported_rom,
    verify_build_receipt,
    normalize_cpp_jump_temporaries,
    build_dispatch_include,
)


ROOT = Path(__file__).resolve().parents[1]
PRIVATE_ROM = ROOT / ".local/m0/tetrisphere-us.z64"
RECOMPILER = ROOT / ".local/m1/toolchain/n64recomp-build/N64Recomp"


class RecompConfigTests(unittest.TestCase):
    def test_committed_config_uses_symbol_toml_and_real_entrypoint(self):
        config = tomllib.loads(
            (ROOT / "config/recomp/tetrisphere.us.rev0.toml").read_text(encoding="utf-8")
        )
        inputs = config["input"]
        self.assertEqual(inputs["entrypoint"], 0x80025C50)
        self.assertEqual(inputs["symbols_file_path"], "symbols.toml")
        self.assertNotIn("elf_path", inputs)
        self.assertEqual(inputs["rom_file_path"], "ROM_PATH_REQUIRED")
        self.assertEqual(inputs["output_func_path"], "OUTPUT_PATH_REQUIRED")
        self.assertEqual(config.get("patches", {}).get("stubs", []), [])

    def test_vi_framebuffer_queries_are_both_runtime_boundaries(self):
        symbols = tomllib.loads(
            (ROOT / "config/recomp/symbols.toml").read_text(encoding="utf-8")
        )
        functions = {
            function["vram"]: function["name"]
            for section in symbols["section"]
            for function in section.get("functions", [])
        }
        self.assertEqual(functions[0x800D18A0], "osViGetNextFramebuffer")
        self.assertEqual(functions[0x800D18E0], "osViGetCurrentFramebuffer")

    def test_exact_rom_rejects_modified_or_non_normalized_input(self):
        with tempfile.TemporaryDirectory() as tmp:
            bogus = Path(tmp) / "bogus.z64"
            bogus.write_bytes(bytes.fromhex("80371240") + bytes(60))
            with self.assertRaisesRegex(GenerationError, "supported normalized NTPE rev0"):
                validate_supported_rom(bogus, ROOT)

    def test_output_cannot_alias_rom_or_committed_input(self):
        with tempfile.TemporaryDirectory(dir=ROOT / ".local") as tmp:
            out = Path(tmp) / "generated"
            with self.assertRaisesRegex(GenerationError, "output tree"):
                validate_generation_paths(PRIVATE_ROM, PRIVATE_ROM, ROOT)
            with self.assertRaisesRegex(GenerationError, "committed input"):
                validate_generation_paths(PRIVATE_ROM, ROOT / "config/recomp", ROOT)
            validate_generation_paths(PRIVATE_ROM, out, ROOT)

    def test_manifest_is_disjoint_from_output_and_protected_toolchain(self):
        before = hashlib.sha256(RECOMPILER.read_bytes()).hexdigest()
        with tempfile.TemporaryDirectory(dir=ROOT / ".local") as tmp:
            output = Path(tmp) / "generated"
            hardlink = Path(tmp) / "manifest.json"
            hardlink.hardlink_to(RECOMPILER)
            existing = Path(tmp) / "existing.json"
            existing.write_text("preserve", encoding="utf-8")
            with self.assertRaisesRegex(GenerationError, "manifest.*output"):
                validate_artifact_paths(PRIVATE_ROM, output, output / "functions/funcs_0.c",
                                        RECOMPILER, ROOT)
            with self.assertRaisesRegex(GenerationError, "protected"):
                validate_artifact_paths(PRIVATE_ROM, output, RECOMPILER, RECOMPILER, ROOT)
            with self.assertRaisesRegex(GenerationError, "protected"):
                validate_artifact_paths(PRIVATE_ROM, output, hardlink, RECOMPILER, ROOT)
            with self.assertRaisesRegex(GenerationError, "already exists"):
                validate_artifact_paths(PRIVATE_ROM, output, existing, RECOMPILER, ROOT)
            self.assertEqual(existing.read_text(encoding="utf-8"), "preserve")
        self.assertEqual(hashlib.sha256(RECOMPILER.read_bytes()).hexdigest(), before)


class ManifestTests(unittest.TestCase):
    def test_runtime_identity_inputs_trigger_cmake_reconfigure(self):
        cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
        self.assertIn("CMAKE_CONFIGURE_DEPENDS", cmake)
        self.assertIn("TETRISPHERE_BUILD_INPUT_PATHS", cmake)

    def test_dispatch_include_contains_only_address_named_callable_functions(self):
        symbols = {"section": [{"functions": [
            {"name": "func_80001000", "vram": 0x80001000, "size": 8},
            {"name": "osCreateThread", "vram": 0x80002000, "size": 8},
        ]}]}
        self.assertEqual(
            build_dispatch_include(symbols),
            "        case 0x80001000u: return func_80001000;\n",
        )

    def test_dispatch_include_adds_generator_discovered_callable_functions(self):
        symbols = {"section": [{"functions": [
            {"name": "func_80001000", "vram": 0x80001000, "size": 8},
        ]}]}
        header = """void func_80001000(uint8_t*, recomp_context*);
void func_800D66F0(uint8_t*, recomp_context*);
void osAiGetLength_recomp(uint8_t*, recomp_context*);
"""
        self.assertEqual(
            build_dispatch_include(symbols, header),
            "        case 0x80001000u: return func_80001000;\n"
            "        case 0x800D66F0u: return func_800D66F0;\n",
        )

    def test_generated_jump_temporary_is_declared_before_any_goto_target(self):
        source = """RECOMP_FUNC void func(uint8_t* rdram, recomp_context* ctx) {
    uint64_t hi = 0;
    goto L_end;
    gpr jr_addend_80001234 = ctx->r25;
L_end:
    return;
;}\n"""
        normalized = normalize_cpp_jump_temporaries(source)
        self.assertIn("    gpr jr_addend_80001234 = 0;\n    goto L_end;", normalized)
        self.assertIn("    jr_addend_80001234 = ctx->r25;", normalized)
        self.assertEqual(normalized.count("gpr jr_addend_80001234"), 1)

    def test_generated_signed_division_uses_signed_result_pointers(self):
        source = """RECOMP_FUNC void func(uint8_t* rdram, recomp_context* ctx) {
    uint64_t hi = 0;
    uint64_t lo = 0;
    DDIV(S64(ctx->r4), S64(ctx->r5), &lo, &hi);
;}
"""
        normalized = normalize_cpp_jump_temporaries(source)
        self.assertIn(
            "DDIV(S64(ctx->r4), S64(ctx->r5), "
            "reinterpret_cast<int64_t*>(&lo), reinterpret_cast<int64_t*>(&hi));",
            normalized,
        )

    def test_manifest_is_stable_relative_and_contains_no_private_paths(self):
        with tempfile.TemporaryDirectory(dir=ROOT / ".local") as tmp:
            output = Path(tmp) / "output"
            output.mkdir()
            (output / "z.c").write_text("z\n", encoding="utf-8")
            (output / "a.c").write_text("a\n", encoding="utf-8")
            manifest = build_generation_manifest(
                output,
                config_path=ROOT / "config/recomp/tetrisphere.us.rev0.toml",
                symbols_path=ROOT / "config/recomp/symbols.toml",
                sections_path=ROOT / "config/recomp/sections.json",
                recompiler_commit="ffb39cdad1da5de07eaaa48bd1db4a89a7986771",
            )
            self.assertEqual([f["path"] for f in manifest["generated_files"]], ["a.c", "z.c"])
            serialized = json.dumps(manifest, sort_keys=True)
            self.assertNotIn(str(ROOT), serialized)
            self.assertNotIn(str(PRIVATE_ROM), serialized)
            self.assertNotIn("rom_sha256", serialized)
            self.assertRegex(manifest["generation_id"], r"^[0-9a-f]{64}$")
            self.assertEqual(manifest, json.loads(json.dumps(manifest, sort_keys=True)))

    def test_manifest_comparison_names_changed_generated_file(self):
        first = {"generated_files": [{"path": "func.c", "sha256": "a" * 64}]}
        second = {"generated_files": [{"path": "func.c", "sha256": "b" * 64}]}
        with self.assertRaisesRegex(GenerationError, "func.c"):
            compare_manifests(first, second)


@unittest.skipUnless(PRIVATE_ROM.is_file() and RECOMPILER.is_file(), "private M1 inputs unavailable")
class RealRecompilerTests(unittest.TestCase):
    def test_broken_manifest_symlink_is_rejected_before_tool_execution(self):
        tool_before = hashlib.sha256(RECOMPILER.read_bytes()).hexdigest()
        with tempfile.TemporaryDirectory(dir=ROOT / ".local") as tmp:
            tmp_path = Path(tmp)
            output = tmp_path / "generated"
            manifest = tmp_path / "manifest.json"
            manifest.symlink_to(output / "functions/funcs_0.c")
            self.assertTrue(manifest.is_symlink())
            self.assertFalse(manifest.exists())
            proc = subprocess.run(
                [sys.executable, "-m", "tools.recomp.generate", "generate",
                 "--rom", str(PRIVATE_ROM), "--output", str(output),
                 "--manifest", str(manifest), "--recompiler", str(RECOMPILER)],
                cwd=ROOT, capture_output=True, text=True,
            )
            self.assertNotEqual(proc.returncode, 0)
            self.assertIn("manifest", proc.stderr)
            self.assertFalse(output.exists())
            self.assertTrue(manifest.is_symlink())
        self.assertEqual(hashlib.sha256(RECOMPILER.read_bytes()).hexdigest(), tool_before)

    def test_receipt_rejects_unbound_or_replaced_executable(self):
        with tempfile.TemporaryDirectory(dir=ROOT / ".local") as tmp:
            receipt = Path(tmp) / "receipt.json"
            receipt.write_text(json.dumps({
                "schema_version": 1,
                "tool": "N64Recomp",
                "commit": "ffb39cdad1da5de07eaaa48bd1db4a89a7986771",
                "lock_sha256": hashlib.sha256(
                    (ROOT / "config/dependencies-m1.json").read_bytes()).hexdigest(),
                "executable_sha256": "0" * 64,
            }), encoding="utf-8")
            with self.assertRaisesRegex(GenerationError, "receipt.*digest"):
                verify_build_receipt(RECOMPILER, receipt,
                                     "ffb39cdad1da5de07eaaa48bd1db4a89a7986771")
            with self.assertRaisesRegex(GenerationError, "receipt"):
                verify_build_receipt(RECOMPILER, Path(tmp) / "missing.json",
                                     "ffb39cdad1da5de07eaaa48bd1db4a89a7986771")

    def test_cli_rejects_executable_outside_pinned_toolchain_path(self):
        with tempfile.TemporaryDirectory(dir=ROOT / ".local") as tmp:
            tmp_path = Path(tmp)
            proc = subprocess.run(
                [sys.executable, "-m", "tools.recomp.generate", "generate",
                 "--rom", str(PRIVATE_ROM), "--output", str(tmp_path / "generated"),
                 "--manifest", str(tmp_path / "manifest.json"),
                 "--recompiler", sys.executable],
                cwd=ROOT, capture_output=True, text=True,
            )
            self.assertNotEqual(proc.returncode, 0)
            self.assertIn("pinned N64Recomp executable", proc.stderr)

    def test_cli_invokes_pinned_recompiler_and_emits_genuine_entrypoint(self):
        with tempfile.TemporaryDirectory(dir=ROOT / ".local") as tmp:
            output = Path(tmp) / "generated"
            manifest = Path(tmp) / "manifest.json"
            proc = subprocess.run(
                [sys.executable, "-m", "tools.recomp.generate", "generate",
                 "--rom", str(PRIVATE_ROM), "--output", str(output),
                 "--manifest", str(manifest), "--recompiler", str(RECOMPILER)],
                cwd=ROOT, capture_output=True, text=True,
            )
            self.assertEqual(proc.returncode, 0, proc.stderr)
            generated = json.loads(manifest.read_text(encoding="utf-8"))["generated_files"]
            names = {entry["path"] for entry in generated}
            self.assertIn("funcs.h", names)
            emitted = "\n".join(
                path.read_text(encoding="utf-8", errors="strict")
                for path in output.rglob("*.c")
            )
            self.assertIn("recomp_entrypoint", emitted)

    def test_cmake_rejects_generated_source_changed_after_manifest(self):
        flatpak = shutil.which("flatpak")
        if flatpak is None:
            self.skipTest("Flatpak SDK unavailable")
        with tempfile.TemporaryDirectory(dir=ROOT / ".local") as tmp:
            tmp_path = Path(tmp)
            generated = tmp_path / "generated"
            manifest = tmp_path / "manifest.json"
            generation = subprocess.run(
                [sys.executable, "-m", "tools.recomp.generate", "generate",
                 "--rom", str(PRIVATE_ROM), "--output", str(generated),
                 "--manifest", str(manifest), "--recompiler", str(RECOMPILER)],
                cwd=ROOT, capture_output=True, text=True,
            )
            self.assertEqual(generation.returncode, 0, generation.stderr)
            with (generated / "functions/funcs_0.c").open("a", encoding="utf-8") as stream:
                stream.write("\n/* mutation */\n")
            configure = subprocess.run(
                [flatpak, "run", "--user", f"--filesystem={ROOT.parents[1]}",
                 "--command=cmake", "org.freedesktop.Sdk//25.08",
                 "-S", str(ROOT), "-B", str(tmp_path / "build"), "-G", "Ninja",
                 f"-DTETRISPHERE_GENERATED_DIR={generated / 'functions'}",
                 f"-DTETRISPHERE_GENERATION_MANIFEST={manifest}"],
                capture_output=True, text=True,
            )
            self.assertNotEqual(configure.returncode, 0)
            self.assertIn("generated file hash mismatch", configure.stdout + configure.stderr)

    def test_cmake_requires_manifested_funcs_header(self):
        flatpak = shutil.which("flatpak")
        if flatpak is None:
            self.skipTest("Flatpak SDK unavailable")
        with tempfile.TemporaryDirectory(dir=ROOT / ".local") as tmp:
            tmp_path = Path(tmp)
            generated = tmp_path / "generated"
            manifest = tmp_path / "manifest.json"
            generation = subprocess.run(
                [sys.executable, "-m", "tools.recomp.generate", "generate",
                 "--rom", str(PRIVATE_ROM), "--output", str(generated),
                 "--manifest", str(manifest), "--recompiler", str(RECOMPILER)],
                cwd=ROOT, capture_output=True, text=True,
            )
            self.assertEqual(generation.returncode, 0, generation.stderr)
            data = json.loads(manifest.read_text(encoding="utf-8"))
            data["generated_files"] = [
                entry for entry in data["generated_files"] if entry["path"] != "funcs.h"
            ]
            manifest.write_text(json.dumps(data, indent=2, sort_keys=True) + "\n",
                                encoding="utf-8")
            with (generated / "functions/funcs.h").open("a", encoding="utf-8") as stream:
                stream.write("\n/* unmanifested mutation */\n")
            configure = subprocess.run(
                [flatpak, "run", "--user", f"--filesystem={ROOT.parents[1]}",
                 "--command=cmake", "org.freedesktop.Sdk//25.08",
                 "-S", str(ROOT), "-B", str(tmp_path / "build"), "-G", "Ninja",
                 f"-DTETRISPHERE_GENERATED_DIR={generated / 'functions'}",
                 f"-DTETRISPHERE_GENERATION_MANIFEST={manifest}"],
                capture_output=True, text=True,
            )
            self.assertNotEqual(configure.returncode, 0)
            self.assertIn("funcs.h exactly once", configure.stdout + configure.stderr)

    def test_manifest_path_cannot_overwrite_validated_rom(self):
        with tempfile.TemporaryDirectory(dir=ROOT / ".local") as tmp:
            tmp_path = Path(tmp)
            rom_copy = tmp_path / "copy.z64"
            shutil.copyfile(PRIVATE_ROM, rom_copy)
            before = rom_copy.stat().st_size
            proc = subprocess.run(
                [sys.executable, "-m", "tools.recomp.generate", "generate",
                 "--rom", str(rom_copy), "--output", str(tmp_path / "generated"),
                 "--manifest", str(rom_copy), "--recompiler", str(RECOMPILER)],
                cwd=ROOT, capture_output=True, text=True,
            )
            self.assertNotEqual(proc.returncode, 0)
            self.assertIn("private ROM", proc.stderr)
            self.assertEqual(rom_copy.stat().st_size, before)


class NativeDiagnosticTests(unittest.TestCase):
    def test_unknown_indirect_target_is_structured_and_nonzero(self):
        compiler = shutil.which("c++")
        if compiler is None:
            self.skipTest("C++ compiler unavailable")
        with tempfile.TemporaryDirectory(dir=ROOT / ".local") as tmp:
            tmp_path = Path(tmp)
            harness = tmp_path / "harness.cpp"
            harness.write_text(
                '#include "tetrisphere/diagnostics.h"\n'
                'int main() { tetrisphere::dispatch_indirect(0x81234567u, 0x80026498u); }\n',
                encoding="utf-8",
            )
            binary = tmp_path / "dispatch-test"
            build = subprocess.run(
                [compiler, "-std=c++20", "-I", str(ROOT / "include"),
                 str(ROOT / "src/diagnostic/indirect_dispatch.cpp"), str(harness),
                 "-o", str(binary)], capture_output=True, text=True,
            )
            self.assertEqual(build.returncode, 0, build.stderr)
            run = subprocess.run([str(binary)], capture_output=True, text=True)
            self.assertNotEqual(run.returncode, 0)
            record = json.loads(run.stderr)
            self.assertEqual(record["event"], "fatal_unresolved_indirect")
            self.assertEqual(record["phase"], "m1-recompiled-entry")
            self.assertEqual(record["target"], "0x81234567")
            self.assertEqual(record["callsite"], "0x80026498")

    @unittest.skipUnless(PRIVATE_ROM.is_file() and RECOMPILER.is_file(), "private M1 inputs unavailable")
    def test_real_generated_entry_runs_beyond_closed_boot_thread_gap(self):
        flatpak = shutil.which("flatpak")
        if flatpak is None:
            self.skipTest("Flatpak SDK unavailable")
        with tempfile.TemporaryDirectory(dir=ROOT / ".local") as tmp:
            tmp_path = Path(tmp)
            generated = tmp_path / "generated"
            manifest = tmp_path / "manifest.json"
            generation = subprocess.run(
                [sys.executable, "-m", "tools.recomp.generate", "generate",
                 "--rom", str(PRIVATE_ROM), "--output", str(generated),
                 "--manifest", str(manifest), "--recompiler", str(RECOMPILER)],
                cwd=ROOT, capture_output=True, text=True,
            )
            self.assertEqual(generation.returncode, 0, generation.stderr)
            build = tmp_path / "build"
            configure = subprocess.run(
                [flatpak, "run", "--user", f"--filesystem={ROOT.parents[1]}",
                 "--command=cmake", "org.freedesktop.Sdk//25.08",
                 "-S", str(ROOT), "-B", str(build), "-G", "Ninja",
                 f"-DTETRISPHERE_GENERATED_DIR={generated / 'functions'}",
                 f"-DTETRISPHERE_GENERATION_MANIFEST={manifest}"],
                capture_output=True, text=True,
            )
            self.assertEqual(configure.returncode, 0, configure.stderr)
            compile_result = subprocess.run(
                [flatpak, "run", "--user", f"--filesystem={ROOT.parents[1]}",
                 "--command=cmake", "org.freedesktop.Sdk//25.08",
                 "--build", str(build), "--target", "tetrisphere_diag"],
                capture_output=True, text=True,
            )
            self.assertEqual(compile_result.returncode, 0,
                             compile_result.stdout + compile_result.stderr)
            with self.assertRaises(subprocess.TimeoutExpired) as stopped:
                subprocess.run([str(build / "tetrisphere_diag"), str(PRIVATE_ROM)],
                               capture_output=True, text=True, timeout=3)
            stdout = stopped.exception.stdout or ""
            stderr = stopped.exception.stderr or ""
            if isinstance(stdout, bytes):
                stdout = stdout.decode("utf-8")
            if isinstance(stderr, bytes):
                stderr = stderr.decode("utf-8")
            events = [json.loads(line) for line in stdout.splitlines()]
            self.assertEqual([event["event"] for event in events],
                             ["rom_validated", "diagnostic_start"])
            self.assertNotIn("fatal_unsupported", stderr)


if __name__ == "__main__":
    unittest.main()
