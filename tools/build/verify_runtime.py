#!/usr/bin/env python3
"""Verify and identify an exact locked dependency source used by CMake."""

import argparse
import hashlib
import json
from pathlib import Path
import subprocess

try:
    from tools.build.fetch_dependencies import load_lock, verify_checkout
except ModuleNotFoundError:  # Direct execution from tools/build.
    from fetch_dependencies import load_lock, verify_checkout


def _git(root: Path, *args: str) -> subprocess.CompletedProcess:
    return subprocess.run(["git", "-C", str(root), *args], capture_output=True,
                          check=False, text=False)


def source_identity(root: Path) -> str:
    """Hash actual tracked bytes plus recursive submodule revisions deterministically."""
    root = Path(root)
    tracked = _git(root, "ls-files", "-z")
    if tracked.returncode:
        raise ValueError("not_git_checkout")
    digest = hashlib.sha256()
    for raw_path in sorted(path for path in tracked.stdout.split(b"\0") if path):
        relative = raw_path.decode("utf-8", errors="strict")
        candidate = root / relative
        digest.update(len(raw_path).to_bytes(8, "big"))
        digest.update(raw_path)
        if candidate.is_file():
            contents = candidate.read_bytes()
            digest.update(b"F")
            digest.update(len(contents).to_bytes(8, "big"))
            digest.update(contents)
        elif candidate.is_dir():
            head = _git(candidate, "rev-parse", "--verify", "HEAD")
            if head.returncode:
                raise ValueError(f"unreadable_gitlink:{relative}")
            digest.update(b"G")
            digest.update(head.stdout.strip())
        else:
            raise ValueError(f"missing_tracked_path:{relative}")
    submodules = _git(root, "submodule", "status", "--recursive")
    if submodules.returncode:
        raise ValueError("submodule_status_failed")
    digest.update(b"S")
    digest.update(submodules.stdout)
    return digest.hexdigest()


def verify_dependency(root: Path, lock_path: Path, name: str) -> dict:
    """Return a deterministic receipt for one named locked dependency."""
    lock = load_lock(lock_path)
    entries = [entry for entry in lock["dependencies"]
               if entry["name"] == name]
    if len(entries) != 1:
        raise ValueError(f"{name} lock entry missing or duplicated")
    entry = entries[0]
    issues = verify_checkout(root, entry)
    if issues:
        raise ValueError(",".join(issues))
    return {
        "schema_version": 1,
        "name": name,
        "commit": entry["commit"],
        "lock_digest": hashlib.sha256(Path(lock_path).read_bytes()).hexdigest(),
        "source_identity": source_identity(root),
    }


def verify_runtime(root: Path, lock_path: Path) -> dict:
    """Backward-compatible N64ModernRuntime receipt helper."""
    return verify_dependency(root, lock_path, "N64ModernRuntime")


def require_configured_identity(receipt: dict, expected_commit: str,
                                expected_source_identity: str,
                                expected_lock_digest: str) -> None:
    """Reject a valid current checkout that differs from configure-time inputs."""
    expected = {
        "commit": expected_commit,
        "source_identity": expected_source_identity,
        "lock_digest": expected_lock_digest,
    }
    mismatches = [key for key, value in expected.items() if receipt[key] != value]
    if mismatches:
        raise ValueError("configured_identity_mismatch:" + ",".join(mismatches))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, required=True)
    parser.add_argument("--lock", type=Path, required=True)
    parser.add_argument("--receipt", type=Path, required=True)
    parser.add_argument("--name", default="N64ModernRuntime")
    parser.add_argument("--expected-commit")
    parser.add_argument("--expected-source-identity")
    parser.add_argument("--expected-lock-digest")
    args = parser.parse_args()
    try:
        receipt = verify_dependency(args.root, args.lock, args.name)
        expectations = (args.expected_commit, args.expected_source_identity,
                        args.expected_lock_digest)
        if any(value is not None for value in expectations):
            if not all(value is not None for value in expectations):
                raise ValueError("incomplete_configured_identity")
            require_configured_identity(receipt, *expectations)
        serialized = json.dumps(receipt, indent=2, sort_keys=True) + "\n"
        args.receipt.parent.mkdir(parents=True, exist_ok=True)
        temporary = args.receipt.with_name(args.receipt.name + ".tmp")
        temporary.write_text(serialized, encoding="utf-8")
        temporary.replace(args.receipt)
    except (OSError, ValueError, json.JSONDecodeError) as error:
        print(f"{args.name} checkout verification failed: {error}")
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
