#!/usr/bin/env python3
"""Fetch exact M1 source revisions into the private local dependency directory."""

import argparse
import json
import ntpath
from pathlib import Path
import re
import subprocess


WORKSPACE = Path(__file__).resolve().parents[2]
LOCK = WORKSPACE / "config/dependencies-m1.json"
COMMIT = re.compile(r"[0-9a-f]{40}\Z")
NAME = re.compile(r"[A-Za-z][A-Za-z0-9_-]*\Z")


def load_lock(path: Path) -> dict:
    """Load a lock with immutable Git commits and safe checkout names."""
    data = json.loads(Path(path).read_text(encoding="utf-8"))
    if (not isinstance(data, dict) or data.get("schema_version") != 1
            or not isinstance(data.get("dependencies"), list)):
        raise ValueError("unsupported dependency lock schema")
    seen = set()
    if not data["dependencies"]:
        raise ValueError("dependency lock is empty")
    for entry in data["dependencies"]:
        if not isinstance(entry, dict):
            raise ValueError("dependency entry must be an object")
        name, repository, commit = (entry.get(key) for key in ("name", "repository", "commit"))
        if not isinstance(name, str) or not NAME.fullmatch(name) or name in seen:
            raise ValueError("invalid or duplicate dependency name")
        if not isinstance(repository, str) or not re.fullmatch(
                r"https://github\.com/[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+(?:\.git)?", repository):
            raise ValueError(f"invalid repository for {name}")
        if not isinstance(commit, str) or not COMMIT.fullmatch(commit):
            raise ValueError(f"invalid immutable commit for {name}")
        seen.add(name)
    return data


def validate_private_root(workspace: Path, root: Path) -> Path:
    """Require lexical .local/deps and its authorized on-disk destination."""
    workspace = Path(workspace).resolve()
    expected = workspace / ".local" / "deps"
    if Path(root).absolute() != expected:
        raise ValueError("dependency root must be the workspace .local/deps directory")
    private = workspace / ".local"
    if private.is_symlink():
        common = _git(workspace, "rev-parse", "--git-common-dir")
        if common.returncode:
            raise ValueError("cannot verify private dependency root")
        git_dir = Path(common.stdout.strip())
        if not git_dir.is_absolute():
            git_dir = workspace / git_dir
        authorized = git_dir.resolve().parent / ".local"
        if authorized.is_symlink() or private.resolve() != authorized:
            raise ValueError("private dependency root points outside the workspace")
    else:
        authorized = private
    actual = expected.resolve()
    if actual != authorized / "deps":
        raise ValueError("private dependency root points outside the workspace")
    return actual


def _git(root: Path, *args: str) -> subprocess.CompletedProcess:
    return subprocess.run(["git", "-C", str(root), *args], text=True,
                          capture_output=True, check=False)


def paths_refer_to_same_root(left: Path | str, right: Path | str) -> bool:
    """Compare checkout roots using the path semantics Git reports."""
    left_text, right_text = str(left), str(right)
    windows_absolute = re.compile(r"^[A-Za-z]:[\\/]")
    msys_absolute = re.compile(r"^/([A-Za-z])/(.*)$")

    def windows_form(value: str) -> str | None:
        if windows_absolute.match(value):
            return value
        match = msys_absolute.match(value)
        if match:
            return f"{match.group(1)}:/{match.group(2)}"
        return None

    left_windows, right_windows = windows_form(left_text), windows_form(right_text)
    if left_windows is not None and right_windows is not None:
        return ntpath.normcase(ntpath.normpath(left_windows)) == ntpath.normcase(
            ntpath.normpath(right_windows))
    return Path(left).resolve() == Path(right).resolve()


def verify_checkout(root: Path, entry: dict) -> list[str]:
    """Return problems for an absent, invalid, dirty, or wrong-revision checkout."""
    root = Path(root)
    if not root.is_dir():
        return ["missing"]
    head = _git(root, "rev-parse", "--verify", "HEAD")
    if head.returncode:
        return ["not_git_checkout"]
    top = _git(root, "rev-parse", "--show-toplevel")
    if top.returncode or not paths_refer_to_same_root(top.stdout.strip(), root):
        return ["not_git_checkout"]
    problems = []
    if head.stdout.strip() != entry["commit"]:
        problems.append("wrong_revision")
    dirty = _git(root, "status", "--porcelain", "--ignore-submodules=none",
                 "--untracked-files=no")
    if dirty.returncode or dirty.stdout.strip():
        problems.append("modified_checkout")
    submodules = _git(root, "submodule", "status", "--recursive")
    if submodules.returncode:
        problems.append("submodule_status_failed")
    else:
        markers = {line[:1] for line in submodules.stdout.splitlines() if line}
        if "-" in markers:
            problems.append("missing_submodule")
        if "+" in markers:
            problems.append("wrong_submodule_revision")
        if "U" in markers or markers - {" ", "-", "+", "U"}:
            problems.append("invalid_submodule_state")
    return problems


def fetch_dependency(root: Path, entry: dict) -> list[str]:
    """Clone a missing source, check out its locked commit, and initialize submodules."""
    checkout = root / entry["name"]
    if checkout.exists():
        return verify_checkout(checkout, entry)
    cloned = subprocess.run(["git", "clone", "--no-checkout", entry["repository"],
                             str(checkout)], text=True, capture_output=True, check=False)
    if cloned.returncode:
        return ["clone_failed: " + cloned.stderr.strip().splitlines()[-1]]
    switched = _git(checkout, "checkout", "--detach", entry["commit"])
    if switched.returncode:
        return ["checkout_failed: " + switched.stderr.strip().splitlines()[-1]]
    submodules = _git(checkout, "submodule", "update", "--init", "--recursive")
    if submodules.returncode:
        return ["submodule_failed: " + submodules.stderr.strip().splitlines()[-1]]
    return verify_checkout(checkout, entry)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--lock", type=Path, default=LOCK)
    parser.add_argument("--root", type=Path, default=WORKSPACE / ".local/deps")
    parser.add_argument("--name", action="append", help="Fetch only this locked dependency")
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args()
    try:
        lock = load_lock(args.lock)
        root = validate_private_root(WORKSPACE, args.root)
        selected = [e for e in lock["dependencies"] if not args.name or e["name"] in args.name]
        if args.name and len(selected) != len(set(args.name)):
            raise ValueError("unknown dependency name")
        root.mkdir(parents=True, exist_ok=True)
        report = {"schema_version": 1, "dependencies": []}
        for entry in selected:
            errors = fetch_dependency(root, entry)
            report["dependencies"].append({"name": entry["name"],
                                           "status": "ready" if not errors else "error",
                                           "issues": errors})
    except (OSError, ValueError, json.JSONDecodeError) as exc:
        report = {"schema_version": 1, "error": str(exc)}
    print(json.dumps(report, indent=2) if args.json else json.dumps(report))
    return 1 if "error" in report or any(e["status"] == "error" for e in report["dependencies"]) else 0


if __name__ == "__main__":
    raise SystemExit(main())
