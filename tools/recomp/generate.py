#!/usr/bin/env python3
"""Validate NTPE rev0 and drive the pinned N64Recomp reproducibly."""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tomllib
import re

from tools.rom.validate import inspect_path
from tools.build.fetch_dependencies import load_lock, verify_checkout
from tools.recomp.ui_anchor_hooks import (
    UiAnchorManifestError,
    render_ui_anchor_hooks,
    validate_ui_anchor_manifest,
)


ROOT = Path(__file__).resolve().parents[2]
ROM_MANIFEST = ROOT / "config/roms/tetrisphere-us-rev0.json"
CONFIG = ROOT / "config/recomp/tetrisphere.us.rev0.toml"
UI_ANCHORS = ROOT / "config/recomp/ui_anchor_callsites.json"
SYMBOLS = ROOT / "config/recomp/symbols.toml"
SECTIONS = ROOT / "config/recomp/sections.json"
LOCK = ROOT / "config/dependencies-m1.json"
PINNED_RECOMPILER = ROOT / ".local/m1/toolchain/n64recomp-build/N64Recomp"
BUILD_RECEIPT = ROOT / ".local/m1/toolchain/N64Recomp-receipt.json"
PINNED_NAME = "N64Recomp"


class GenerationError(RuntimeError):
    """A safe, actionable generation failure."""


_RECOMP_FUNCTION = re.compile(
    r"RECOMP_FUNC void [^(]+\([^)]*\) \{.*?^;\}", re.MULTILINE | re.DOTALL)
_JUMP_TEMPORARY = re.compile(
    r"^(?P<indent>\s*)gpr (?P<name>jr_addend_[0-9A-F]{8}) = (?P<value>[^;]+);$",
    re.MULTILINE,
)
_SIGNED_DIVISION = re.compile(
    r"DDIV\((?P<dividend>[^,]+), (?P<divisor>[^,]+), "
    r"&(?P<quotient>[A-Za-z_][A-Za-z0-9_]*), &(?P<remainder>[A-Za-z_][A-Za-z0-9_]*)\);"
)


def normalize_cpp_jump_temporaries(source: str) -> str:
    """Make N64Recomp jump-table temporaries legal across C++ gotos.

    The pinned generator declares these at their first use.  A MIPS branch may
    jump over that declaration, which is valid in C but ill-formed in C++.
    Generated functions include C++ support headers, so declare each temporary
    in the function prologue and retain only the assignment at its original
    location.
    """
    def normalize_function(match: re.Match[str]) -> str:
        function = match.group(0)
        temporaries = list(_JUMP_TEMPORARY.finditer(function))
        if temporaries:
            names = list(dict.fromkeys(item.group("name") for item in temporaries))
            function = _JUMP_TEMPORARY.sub(
                lambda item: f'{item.group("indent")}{item.group("name")} = {item.group("value")};',
                function,
            )
            lines = function.splitlines(keepends=True)
            insert_at = 1
            while insert_at < len(lines) and re.match(
                    r"^    (?:uint64_t|int)\s+", lines[insert_at]):
                insert_at += 1
            declarations = [f"    gpr {name} = 0;\n" for name in names]
            lines[insert_at:insert_at] = declarations
            function = "".join(lines)
        return _SIGNED_DIVISION.sub(
            lambda item: (
                f'DDIV({item.group("dividend")}, {item.group("divisor")}, '
                f'reinterpret_cast<int64_t*>(&{item.group("quotient")}), '
                f'reinterpret_cast<int64_t*>(&{item.group("remainder")}));'
            ),
            function,
        )

    return _RECOMP_FUNCTION.sub(normalize_function, source)


def normalize_generated_sources(functions: Path) -> None:
    """Apply deterministic host-C++ compatibility fixes to generated units."""
    for source_path in sorted(functions.glob("funcs_*.c")):
        source = source_path.read_text(encoding="utf-8")
        normalized = normalize_cpp_jump_temporaries(source)
        if normalized != source:
            source_path.write_text(normalized, encoding="utf-8", newline="\n")


def build_dispatch_include(symbols: dict, generated_header: str = "") -> str:
    """Build deterministic cases for every generated address-named function.

    N64Recomp can discover callable functions that are not explicitly present
    in the input symbol file.  They are valid indirect-call targets too, so the
    generated prototype list is the authoritative supplement to the reviewed
    symbol map.
    """
    functions = symbols.get("section", [{}])[0].get("functions", [])
    callable_functions: dict[int, str] = {}
    for function in functions:
        name = function["name"]
        if name.startswith("func_") and function["vram"] != 0x80025C50:
            callable_functions[function["vram"]] = name
    for match in re.finditer(r"\bvoid (func_([0-9A-Fa-f]{8}))\s*\(", generated_header):
        address = int(match.group(2), 16)
        if address != 0x80025C50:
            callable_functions[address] = match.group(1)
    return "".join(
        f"        case 0x{address:08X}u: return {callable_functions[address]};\n"
        for address in sorted(callable_functions)
    )


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def _resolved_future(path: Path) -> Path:
    path = path.absolute()
    missing: list[str] = []
    while not path.exists():
        missing.append(path.name)
        parent = path.parent
        if parent == path:
            break
        path = parent
    resolved = path.resolve(strict=True)
    for component in reversed(missing):
        resolved /= component
    return resolved


def _contains(parent: Path, child: Path) -> bool:
    try:
        child.relative_to(parent)
        return True
    except ValueError:
        return False


def validate_generation_paths(rom_path: Path, output_path: Path, root: Path = ROOT) -> None:
    """Require ignored output and prevent aliases to the ROM/committed inputs."""
    root = root.resolve(strict=True)
    rom = rom_path.resolve(strict=True)
    output = _resolved_future(output_path)
    if output == rom or _contains(output, rom):
        raise GenerationError("output tree aliases or contains the private ROM")
    committed_inputs = [root / "config", root / "tools", root / "src", root / "include", root / "tests"]
    if any(
        output == item.resolve()
        or _contains(output, item.resolve())
        or _contains(item.resolve(), output)
        for item in committed_inputs
    ):
        raise GenerationError("output tree aliases or contains a committed input")
    allowed = [root / ".local", root / "generated", root / "build"]
    if not any(_contains(base.resolve(strict=False), output) for base in allowed):
        raise GenerationError("output tree must be under ignored .local/, generated/, or build/")
    if output.exists():
        try:
            if output.samefile(rom):
                raise GenerationError("output tree aliases the private ROM")
        except OSError as exc:
            raise GenerationError(f"cannot validate output tree: {exc}") from exc


def validate_artifact_paths(rom_path: Path, output_path: Path, manifest_path: Path,
                            recompiler: Path, root: Path = ROOT) -> None:
    """Validate both artifacts and keep them disjoint from all protected inputs."""
    if os.path.lexists(manifest_path) and manifest_path.is_symlink():
        raise GenerationError("manifest path already exists as a symlink")
    validate_generation_paths(rom_path, output_path, root)
    validate_generation_paths(rom_path, manifest_path, root)
    output = _resolved_future(output_path)
    manifest = _resolved_future(manifest_path)
    if output == manifest or _contains(output, manifest) or _contains(manifest, output):
        raise GenerationError("manifest and output tree must be disjoint")
    if manifest.exists() and manifest.is_dir():
        raise GenerationError("manifest path must identify a file")
    protected = [
        (root / ".local/deps").resolve(strict=False),
        (root / ".local/m1/toolchain").resolve(strict=False),
        recompiler.resolve(strict=True),
    ]
    for artifact_name, artifact in (("output", output), ("manifest", manifest)):
        aliases_protected = artifact.exists() and any(
            item.exists() and artifact.samefile(item) for item in protected
        )
        if aliases_protected or any(
            artifact == item or _contains(item, artifact) or _contains(artifact, item)
            for item in protected
        ):
            raise GenerationError(f"{artifact_name} path overlaps a protected dependency or toolchain input")
    if os.path.lexists(manifest_path):
        raise GenerationError("manifest path already exists; refusing to overwrite it")


def validate_supported_rom(path: Path, root: Path = ROOT) -> dict:
    manifest_path = root / "config/roms/tetrisphere-us-rev0.json"
    try:
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        result = inspect_path(path, manifest)
    except (OSError, ValueError, KeyError, TypeError) as exc:
        raise GenerationError(f"cannot validate supported normalized NTPE rev0 ROM: {exc}") from exc
    if not result.get("recognized") or result.get("revision_id") != "tetrisphere-us-rev0":
        raise GenerationError(
            f"input is not the supported normalized NTPE rev0 ROM ({result.get('reason', 'invalid')})"
        )
    if result.get("format") != "z64":
        raise GenerationError("input is not the supported normalized NTPE rev0 z64 byte order")
    return result


def _input_hash(path: Path) -> dict[str, str]:
    return {"path": path.relative_to(ROOT).as_posix(), "sha256": sha256_file(path)}


def build_generation_manifest(
    output_path: Path,
    *,
    config_path: Path = CONFIG,
    symbols_path: Path = SYMBOLS,
    sections_path: Path = SECTIONS,
    recompiler_commit: str,
    recompiler_executable_sha256: str = "0" * 64,
    build_receipt_sha256: str = "0" * 64,
) -> dict:
    files = [
        {"path": path.relative_to(output_path).as_posix(), "sha256": sha256_file(path)}
        for path in sorted(p for p in output_path.rglob("*") if p.is_file())
    ]
    manifest = {
        "schema_version": 1,
        "generator": "tools/recomp/generate.py",
        "rom_id": "tetrisphere-us-rev0",
        "recompiler": {
            "name": PINNED_NAME,
            "commit": recompiler_commit,
            "executable_sha256": recompiler_executable_sha256,
            "build_receipt_sha256": build_receipt_sha256,
        },
        "command_identity": "N64Recomp <private-runtime-config>",
        "committed_inputs": [
            _input_hash(ROOT / "tools/recomp/generate.py"),
            _input_hash(LOCK),
            _input_hash(config_path),
            _input_hash(UI_ANCHORS),
            _input_hash(ROOT / "tools/recomp/ui_anchor_hooks.py"),
            _input_hash(symbols_path),
            _input_hash(sections_path),
        ],
        "generated_files": files,
    }
    identity_bytes = json.dumps(manifest, sort_keys=True, separators=(",", ":")).encode("utf-8")
    manifest["generation_id"] = hashlib.sha256(identity_bytes).hexdigest()
    return manifest


def compare_manifests(first: dict, second: dict) -> None:
    left = {item["path"]: item["sha256"] for item in first.get("generated_files", [])}
    right = {item["path"]: item["sha256"] for item in second.get("generated_files", [])}
    if left != right:
        names = sorted(set(left) | set(right))
        changed = [name for name in names if left.get(name) != right.get(name)]
        raise GenerationError("non-deterministic generated files: " + ", ".join(changed))
    if first.get("generation_id") != second.get("generation_id"):
        raise GenerationError("non-deterministic generation identity")


def pinned_commit(lock_path: Path = LOCK) -> str:
    lock = json.loads(lock_path.read_text(encoding="utf-8"))
    for dependency in lock["dependencies"]:
        if dependency["name"] == PINNED_NAME:
            return dependency["commit"]
    raise GenerationError(f"{PINNED_NAME} is absent from dependency lock")


def verify_build_receipt(recompiler: Path, receipt_path: Path,
                         expected_commit: str) -> dict:
    try:
        receipt = json.loads(receipt_path.read_text(encoding="utf-8"))
    except (OSError, ValueError, TypeError) as exc:
        raise GenerationError(f"N64Recomp build receipt is missing or invalid: {exc}") from exc
    expected = {
        "schema_version": 1,
        "tool": PINNED_NAME,
        "commit": expected_commit,
        "lock_sha256": sha256_file(LOCK),
        "executable_sha256": sha256_file(recompiler),
        "build_cache_sha256": sha256_file(recompiler.parent / "CMakeCache.txt"),
        "build_graph_sha256": sha256_file(recompiler.parent / "build.ninja"),
    }
    for key, value in expected.items():
        if receipt.get(key) != value:
            label = "digest" if key == "executable_sha256" else key
            raise GenerationError(f"N64Recomp build receipt {label} mismatch")
    return receipt


def create_build_receipt(recompiler: Path = PINNED_RECOMPILER,
                         receipt_path: Path = BUILD_RECEIPT) -> dict:
    expected_commit = pinned_commit()
    lock = load_lock(LOCK)
    entry = next(item for item in lock["dependencies"] if item["name"] == PINNED_NAME)
    checkout = ROOT / ".local/deps/N64Recomp"
    issues = verify_checkout(checkout, entry)
    if issues:
        raise GenerationError("N64Recomp checkout cannot bind receipt: " + ", ".join(issues))
    if recompiler.resolve(strict=True) != PINNED_RECOMPILER.resolve(strict=True):
        raise GenerationError("receipt requires the pinned N64Recomp executable path")
    cache_path = recompiler.parent / "CMakeCache.txt"
    cache = cache_path.read_text(encoding="utf-8", errors="replace")
    home_line = next((line for line in cache.splitlines()
                      if line.startswith("CMAKE_HOME_DIRECTORY:INTERNAL=")), "")
    if not home_line:
        raise GenerationError("N64Recomp build cache has no source binding")
    configured_source = Path(home_line.split("=", 1)[1]).resolve(strict=True)
    if configured_source != checkout.resolve(strict=True):
        raise GenerationError("N64Recomp build cache is not bound to the locked checkout")
    receipt = {
        "schema_version": 1,
        "tool": PINNED_NAME,
        "commit": expected_commit,
        "lock_sha256": sha256_file(LOCK),
        "executable_sha256": sha256_file(recompiler),
        "build_cache_sha256": sha256_file(cache_path),
        "build_graph_sha256": sha256_file(recompiler.parent / "build.ninja"),
    }
    receipt_path.parent.mkdir(parents=True, exist_ok=True)
    receipt_path.write_text(json.dumps(receipt, indent=2, sort_keys=True) + "\n",
                            encoding="utf-8", newline="\n")
    return receipt


def _quote_toml(value: str) -> str:
    return json.dumps(value)


def _materialize_diagnostic_symbols(path: Path) -> None:
    """Copy the reviewed whole-program map into the private runtime config."""
    source_text = SYMBOLS.read_text(encoding="utf-8")
    source = tomllib.loads(source_text)
    sections = source.get("section", [])
    if len(sections) != 1 or len(sections[0].get("functions", [])) < 1000:
        raise GenerationError("whole-program symbol map is incomplete")
    path.write_text(source_text, encoding="utf-8", newline="\n")


def materialize_config(output: Path, rom: Path) -> tuple[Path, Path]:
    runtime_dir = output / ".n64recomp"
    runtime_dir.mkdir(parents=True, exist_ok=False)
    _materialize_diagnostic_symbols(runtime_dir / "symbols.toml")
    functions = output / "functions"
    template = CONFIG.read_text(encoding="utf-8")
    parsed = tomllib.loads(template)
    inputs = parsed.get("input", {})
    if inputs.get("rom_file_path") != "ROM_PATH_REQUIRED" or inputs.get("output_func_path") != "OUTPUT_PATH_REQUIRED":
        raise GenerationError("committed N64Recomp template is missing required path sentinels")
    runtime = template.replace('"ROM_PATH_REQUIRED"', _quote_toml(str(rom.resolve())))
    runtime = runtime.replace('"OUTPUT_PATH_REQUIRED"', _quote_toml(str(functions.resolve())))
    anchor_manifest = json.loads(UI_ANCHORS.read_text(encoding="utf-8"))
    try:
        validate_ui_anchor_manifest(anchor_manifest,
                                    rom.read_bytes()[0x1000:0x101000])
    except UiAnchorManifestError as error:
        raise GenerationError(f"UI anchor callsites do not match ROM: {error}") from error
    runtime += render_ui_anchor_hooks(anchor_manifest)
    tomllib.loads(runtime)
    config_path = runtime_dir / "config.toml"
    config_path.write_text(runtime, encoding="utf-8", newline="\n")
    return config_path, functions


def generate(rom: Path, output: Path, manifest_path: Path, recompiler: Path) -> dict:
    validate_supported_rom(rom)
    validate_artifact_paths(rom, output, manifest_path, recompiler)
    expected_commit = pinned_commit()
    if not recompiler.is_file() or recompiler.resolve() != PINNED_RECOMPILER.resolve():
        raise GenerationError(f"pinned N64Recomp executable is missing: {recompiler}")
    lock = load_lock(LOCK)
    entry = next(item for item in lock["dependencies"] if item["name"] == PINNED_NAME)
    checkout = ROOT / ".local/deps/N64Recomp"
    checkout_issues = verify_checkout(checkout, entry)
    if checkout_issues:
        raise GenerationError("N64Recomp checkout does not match dependency lock: "
                              + ", ".join(checkout_issues))
    verify_build_receipt(recompiler, BUILD_RECEIPT, expected_commit)
    if output.exists():
        if any(output.iterdir()):
            raise GenerationError("output tree must not already contain files")
        output.rmdir()
    output.mkdir(parents=True)
    config_path, functions = materialize_config(output, rom)
    proc = subprocess.run([str(recompiler.resolve()), str(config_path)], cwd=ROOT,
                          capture_output=True, text=True, check=False)
    evidence = output / ".n64recomp" / "invocation.json"
    evidence.write_text(json.dumps({
        "schema_version": 1,
        "returncode": proc.returncode,
        "stdout": proc.stdout,
        "stderr": proc.stderr,
    }, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    if proc.returncode != 0:
        raise GenerationError(f"N64Recomp failed with exit {proc.returncode}: {proc.stderr.strip()}")
    normalize_generated_sources(functions)
    symbols = tomllib.loads((output / ".n64recomp" / "symbols.toml").read_text(
        encoding="utf-8"))
    (functions / "dispatch.inc").write_text(
        build_dispatch_include(
            symbols, (functions / "funcs.h").read_text(encoding="utf-8")),
        encoding="utf-8", newline="\n")
    result = build_generation_manifest(
        functions,
        recompiler_commit=expected_commit,
        recompiler_executable_sha256=sha256_file(recompiler),
        build_receipt_sha256=sha256_file(BUILD_RECEIPT),
    )
    manifest_path.parent.mkdir(parents=True, exist_ok=True)
    validate_artifact_paths(rom, output, manifest_path, recompiler)
    with manifest_path.open("x", encoding="utf-8", newline="\n") as manifest_stream:
        manifest_stream.write(json.dumps(result, indent=2, sort_keys=True) + "\n")
    return result


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)
    generate_parser = sub.add_parser("generate")
    generate_parser.add_argument("--rom", type=Path, required=True)
    generate_parser.add_argument("--output", type=Path, required=True)
    generate_parser.add_argument("--manifest", type=Path, required=True)
    generate_parser.add_argument("--recompiler", type=Path, required=True)
    compare_parser = sub.add_parser("compare")
    compare_parser.add_argument("first", type=Path)
    compare_parser.add_argument("second", type=Path)
    sub.add_parser("create-receipt")
    args = parser.parse_args(argv)
    try:
        if args.command == "generate":
            generate(args.rom, args.output, args.manifest, args.recompiler)
        elif args.command == "compare":
            compare_manifests(
                json.loads(args.first.read_text(encoding="utf-8")),
                json.loads(args.second.read_text(encoding="utf-8")),
            )
        else:
            create_build_receipt()
    except (GenerationError, OSError, ValueError, KeyError, TypeError) as exc:
        print(f"generation error: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
