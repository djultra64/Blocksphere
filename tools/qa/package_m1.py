#!/usr/bin/env python3
"""Create and audit deterministic, ROM-free M1 diagnostic archives."""
from __future__ import annotations

import argparse
import gzip
import hashlib
import io
import json
from pathlib import Path, PurePosixPath
import re
import tarfile
from typing import Any, Iterable

FORBIDDEN_SUFFIXES = {".z64", ".v64", ".n64", ".rom", ".eep", ".sra",
                      ".fla", ".pak", ".state"}
ROM_MAGICS = {bytes.fromhex(value) for value in ("80371240", "37804012", "40123780")}
PRIVATE_TEXT = re.compile(
    rb"(?:/var/home/|/home/(?!linuxbrew/)|[A-Za-z]:\\Users\\|\.local[/\\]m0|"
    rb"password\s*=|BEGIN (?:RSA |OPENSSH )?PRIVATE KEY)", re.IGNORECASE)


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _manifest(path: Path) -> dict[str, Any]:
    value = json.loads(path.read_text(encoding="utf-8-sig"))
    build = value.get("build") if isinstance(value, dict) else None
    run = value.get("run") if isinstance(value, dict) else None
    if not isinstance(build, dict) or not isinstance(run, dict):
        raise ValueError("evidence manifest lacks build/run identity")
    if (run.get("build_id") != build.get("id") or run.get("native") is not True or
            run.get("emulator") is not False or run.get("compatibility_layer") is not False):
        raise ValueError("evidence manifest is not one validated native run")
    return value


def _tar_info(name: str, size: int, executable: bool = False) -> tarfile.TarInfo:
    info = tarfile.TarInfo(name)
    info.size = size
    info.mtime = 0
    info.uid = info.gid = 0
    info.uname = info.gname = ""
    info.mode = 0o755 if executable else 0o644
    return info


def create_package(platform: str, binary: Path | str,
                   runtime_files: Iterable[Path | str], manifest: Path | str,
                   notice: Path | str, output: Path | str) -> dict[str, Any]:
    if platform not in ("linux-x86_64", "windows-x86_64"):
        raise ValueError("unsupported package platform")
    binary, manifest, notice, output = map(Path, (binary, manifest, notice, output))
    runtime_files = [Path(path) for path in runtime_files]
    for path in (binary, manifest, notice, *runtime_files):
        if not path.is_file():
            raise ValueError(f"package input is missing: {path.name}")
    evidence = _manifest(manifest)
    build = evidence["build"]
    if build.get("platform") != platform:
        raise ValueError("package platform differs from evidence")
    binary_hash = _sha256(binary)
    if build.get("binary_sha256") != binary_hash:
        raise ValueError("binary hash differs from validated evidence")
    expected_runtime = build.get("runtime_sha256")
    if not isinstance(expected_runtime, dict) or not all(
            isinstance(name, str) and isinstance(digest, str)
            for name, digest in expected_runtime.items()):
        raise ValueError("evidence lacks runtime hash inventory")
    actual_runtime = {path.name: _sha256(path) for path in runtime_files}
    if len(actual_runtime) != len(runtime_files) or actual_runtime != expected_runtime:
        raise ValueError("runtime file hash inventory differs from validated evidence")
    notice_payload = notice.read_bytes()
    if b"M1_LICENSE_BUNDLE_COMPLETE=1" not in notice_payload:
        raise ValueError("third-party license bundle is incomplete")
    root = f"tetrisphere-m1-{platform}"
    executable_name = "tetrisphere-m1.exe" if platform == "windows-x86_64" else "tetrisphere-m1"
    metadata = json.dumps({
        "schema_version": 1, "platform": platform, "build_id": build.get("id"),
        "source_commit": build.get("source_commit"), "binary_sha256": binary_hash,
        "runtime_sha256": actual_runtime, "notice_sha256": _sha256(notice),
        "requires_user_supplied_rom": True,
    }, indent=2, sort_keys=True).encode() + b"\n"
    run = ("Supply your own validated NTPE rev0 ROM; it is never included.\n"
           f"Run: ./{executable_name} /path/to/tetrisphere-us.z64\n").encode()
    members = [(f"{root}/{executable_name}", binary.read_bytes(), True),
               (f"{root}/BUILD.json", metadata, False),
               (f"{root}/RUN.txt", run, False),
               (f"{root}/THIRD_PARTY_NOTICES.md", notice_payload, False)]
    members.extend((f"{root}/{path.name}", path.read_bytes(), False)
                   for path in sorted(runtime_files, key=lambda item: item.name.lower()))
    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open("wb") as raw:
        with gzip.GzipFile(filename="", mode="wb", fileobj=raw, mtime=0) as compressed:
            with tarfile.open(fileobj=compressed, mode="w", format=tarfile.PAX_FORMAT) as archive:
                for name, payload, executable in sorted(members):
                    archive.addfile(_tar_info(name, len(payload), executable), io.BytesIO(payload))
    return audit_package(output)


def audit_package(path: Path | str) -> dict[str, Any]:
    path = Path(path)
    payloads: dict[str, bytes] = {}
    metadata = None
    with tarfile.open(path, "r:gz") as archive:
        for member in archive.getmembers():
            pure = PurePosixPath(member.name)
            if pure.is_absolute() or ".." in pure.parts or member.issym() or member.islnk():
                raise ValueError("unsafe archive member")
            if not member.isfile():
                continue
            payload = archive.extractfile(member).read()
            suffix = pure.suffix.lower()
            if pure.name in payloads:
                raise ValueError("archive contains duplicate member names")
            if suffix in FORBIDDEN_SUFFIXES or any(magic in payload for magic in ROM_MAGICS):
                raise ValueError("archive contains ROM/save material")
            if PRIVATE_TEXT.search(payload):
                raise ValueError("archive contains a private path or credential marker")
            payloads[pure.name] = payload
            if pure.name == "BUILD.json":
                metadata = json.loads(payload.decode("utf-8"))
    if metadata is None or not {"RUN.txt", "THIRD_PARTY_NOTICES.md"}.issubset(payloads):
        raise ValueError("archive lacks build, run or license metadata")
    expected_executable = ("tetrisphere-m1.exe" if metadata.get("platform") == "windows-x86_64"
                           else "tetrisphere-m1")
    runtime_hashes = metadata.get("runtime_sha256")
    if not isinstance(runtime_hashes, dict):
        raise ValueError("archive metadata lacks runtime hash inventory")
    expected_names = {expected_executable, "BUILD.json", "RUN.txt",
                      "THIRD_PARTY_NOTICES.md", *runtime_hashes.keys()}
    if set(payloads) != expected_names:
        raise ValueError("archive member allowlist differs from build metadata")
    if expected_executable not in payloads:
        raise ValueError("archive lacks native executable")
    expected_hashes = {expected_executable: metadata.get("binary_sha256"),
                       "THIRD_PARTY_NOTICES.md": metadata.get("notice_sha256"),
                       **runtime_hashes}
    for name, expected_hash in expected_hashes.items():
        if not isinstance(expected_hash, str) or hashlib.sha256(payloads[name]).hexdigest() != expected_hash:
            raise ValueError(f"archive member hash mismatch: {name}")
    if b"M1_LICENSE_BUNDLE_COMPLETE=1" not in payloads["THIRD_PARTY_NOTICES.md"]:
        raise ValueError("third-party license bundle is incomplete")
    return {"platform": metadata.get("platform"), "build_id": metadata.get("build_id"),
            "source_commit": metadata.get("source_commit"), "member_count": len(payloads)}


def audit_pair(packages: list[dict[str, Any]]) -> dict[str, Any]:
    if len(packages) != 2:
        raise ValueError("package pair requires exactly two packages")
    left, right = packages
    if {left.get("platform"), right.get("platform")} != {
            "linux-x86_64", "windows-x86_64"}:
        raise ValueError("package pair must contain Linux and Windows")
    for field in ("build_id", "source_commit"):
        if left.get(field) != right.get(field):
            raise ValueError(f"package pair has mismatched {field}")
    return {"valid": True, "build_id": left["build_id"],
            "source_commit": left["source_commit"]}


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser()
    sub = parser.add_subparsers(dest="command", required=True)
    create = sub.add_parser("create")
    create.add_argument("--platform", required=True)
    create.add_argument("--binary", type=Path, required=True)
    create.add_argument("--runtime", type=Path, action="append", default=[])
    create.add_argument("--manifest", type=Path, required=True)
    create.add_argument("--notice", type=Path, required=True)
    create.add_argument("--output", type=Path, required=True)
    audit = sub.add_parser("audit")
    audit.add_argument("packages", type=Path, nargs="+")
    args = parser.parse_args(argv)
    try:
        if args.command == "create":
            result = create_package(args.platform, args.binary, args.runtime,
                                    args.manifest, args.notice, args.output)
        else:
            results = [audit_package(path) for path in args.packages]
            if len(results) > 1:
                result = audit_pair(results)
            else:
                result = results[0]
    except (OSError, ValueError, json.JSONDecodeError, tarfile.TarError) as error:
        print(json.dumps({"valid": False, "error": str(error)}, sort_keys=True))
        return 2
    print(json.dumps(result, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
