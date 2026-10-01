#!/usr/bin/env python3
"""Report host tools, pinned dependency checkouts, and private ROM identity."""

import argparse
import json
from pathlib import Path
import shutil
import sys

WORKSPACE = Path(__file__).resolve().parents[2]
if str(WORKSPACE) not in sys.path:
    sys.path.insert(0, str(WORKSPACE))

from tools.build.fetch_dependencies import load_lock, verify_checkout, validate_private_root
from tools.rom.validate import inspect_path


HOST_TOOLS = ("git", "python3", "gcc", "g++", "cmake", "ninja", "clang")


def check_tools(names=HOST_TOOLS, which=shutil.which) -> dict:
    return {name: {"status": "present" if which(name) else "missing"}
            for name in names}


def check_rom(path: Path, manifest: dict) -> dict:
    """Report ROM validation without its path, hash, header, or payload."""
    result = inspect_path(path, manifest)
    if result["recognized"]:
        return {"status": "recognized", "revision_id": result["revision_id"],
                "format": result["format"]}
    return {"status": "missing" if result["reason"] == "io_error" and not Path(path).exists()
            else "rejected", "reason": result["reason"]}


def environment_report(workspace: Path, lock: dict, rom: Path) -> dict:
    root = validate_private_root(workspace, workspace / ".local/deps")
    dependencies = {}
    for entry in lock["dependencies"]:
        issues = verify_checkout(root / entry["name"], entry)
        dependencies[entry["name"]] = {
            "status": "missing" if issues == ["missing"] else "wrong_revision"
            if "wrong_revision" in issues else "invalid" if issues else "present",
            "issues": issues,
        }
    manifest = json.loads((workspace / "config/roms/tetrisphere-us-rev0.json").read_text())
    return {"schema_version": 1, "tools": check_tools(),
            "dependencies": dependencies, "rom": check_rom(rom, manifest)}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--lock", type=Path, default=WORKSPACE / "config/dependencies-m1.json")
    parser.add_argument("--rom", type=Path, default=WORKSPACE / ".local/m0/tetrisphere-us.z64")
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args()
    try:
        report = environment_report(WORKSPACE, load_lock(args.lock), args.rom)
    except (OSError, ValueError, KeyError, TypeError) as exc:
        report = {"schema_version": 1, "error": str(exc)}
    print(json.dumps(report, indent=2) if args.json else json.dumps(report))
    return 1 if "error" in report else 0


if __name__ == "__main__":
    raise SystemExit(main())
