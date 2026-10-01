#!/usr/bin/env python3
"""Build and audit deterministic, ROM-free Tetrisphere 1.0 archives."""

from __future__ import annotations

import argparse
import gzip
import hashlib
import io
import json
from pathlib import Path
import re
import stat
import tarfile
from typing import Any, Mapping, Sequence
import zipfile

PLATFORMS = {"linux-x86_64", "windows-x86_64"}
DOCUMENT_NAMES = {
    "readme": "README.md",
    "controls": "docs/CONTROLS.md",
    "troubleshooting": "docs/TROUBLESHOOTING.md",
    "release_notes": "docs/RELEASE_NOTES.md",
}
OTHER_PLATFORM_TEXT = {
    "linux-x86_64": re.compile(rb"\bwindows\b|\.exe\b|\.dll\b|\brog ally\b", re.IGNORECASE),
    "windows-x86_64": re.compile(
        rb"\blinux\b|\.so(?:\.\d+)*\b|\bwayland\b|\bx11\b|"
        rb"\./tetrisphere\b|tools/tetrisphere-config\b", re.IGNORECASE),
}
FORBIDDEN_SUFFIXES = {".z64", ".v64", ".n64", ".rom", ".eep", ".sra",
                      ".fla", ".pak", ".state", ".sav", ".png", ".jpg",
                      ".jpeg", ".wav", ".mp3", ".jsonl", ".trace"}
ROM_MAGICS = {bytes.fromhex(value) for value in ("80371240", "37804012", "40123780")}
PRIVATE_TEXT = re.compile(
    rb"(?:/var/home/|/home/(?!linuxbrew/)[^/\s]+/|[A-Za-z]:\\Users\\|"
    rb"password\s*=|BEGIN (?:RSA |OPENSSH )?PRIVATE KEY)",
    re.IGNORECASE,
)
ASSETS_README = (
    "# Assets\n\n"
    "This beta does not bundle original game assets or a ROM. Place your own "
    "similarly named supported ROM beside the game executable "
    "for automatic first-launch import, or choose it in the file selector. The validated copy is stored in private "
    "data/ beside the game executable. After importing a ROM, keep the personal game folder private; distribute the clean archive.\n"
).encode()


def _hash(payload: bytes) -> str:
    return hashlib.sha256(payload).hexdigest()


def _validate_name(name: str) -> None:
    if not name or "\\" in name or any(part in ("", ".", "..", ".local")
                                             for part in name.split("/")):
        raise ValueError(f"unsafe archive member path: {name!r}")


def _validate_payload(name: str, payload: bytes) -> None:
    if Path(name).suffix.lower() in FORBIDDEN_SUFFIXES or \
            (len(payload) >= 8 * 1024 * 1024 and payload[:4] in ROM_MAGICS):
        raise ValueError(f"archive contains ROM, save, or captured media: {name}")
    if PRIVATE_TEXT.search(payload):
        raise ValueError(f"archive contains a private path or credential marker: {name}")


def _validate_platform_document(platform: str, name: str, payload: bytes) -> None:
    if OTHER_PLATFORM_TEXT[platform].search(payload):
        raise ValueError(f"document mentions the other platform: {name}")


def _tar_info(name: str, payload: bytes, executable: bool) -> tarfile.TarInfo:
    info = tarfile.TarInfo(name)
    info.size = len(payload)
    info.uid = info.gid = info.mtime = 0
    info.uname = info.gname = ""
    info.mode = 0o755 if executable else 0o644
    return info


def create_package(*, platform: str, binary: Path, configurator: Path,
                   runtime_files: Sequence[Path], documents: Mapping[str, Path],
                   notices: Path, source_commit: str, build_id: str,
                   output: Path) -> dict[str, Any]:
    if platform not in PLATFORMS:
        raise ValueError("unsupported package platform")
    if not re.fullmatch(r"[0-9a-f]{40}", source_commit) or \
            not re.fullmatch(r"[0-9a-f]{64}", build_id):
        raise ValueError("source commit or build ID has an invalid format")
    if set(documents) != set(DOCUMENT_NAMES):
        raise ValueError("package requires README, controls, troubleshooting, and release notes")
    inputs = [binary, configurator, notices, *runtime_files, *documents.values()]
    if not all(Path(path).is_file() for path in inputs):
        raise ValueError("package input is missing")
    windows = platform.startswith("windows")
    executable = "tetrisphere.exe" if windows else "tetrisphere"
    config_name = "tetrisphere-config.exe" if windows else "tetrisphere-config"
    files: dict[str, bytes] = {
        executable: Path(binary).read_bytes(),
        config_name: Path(configurator).read_bytes(),
        "licenses/THIRD_PARTY_NOTICES.md": Path(notices).read_bytes(),
        "assets/README.md": ASSETS_README,
    }
    for key, name in DOCUMENT_NAMES.items():
        files[name] = Path(documents[key]).read_bytes()
        _validate_platform_document(platform, name, files[name])
    for path in runtime_files:
        path = Path(path)
        if path.name in ("", ".", ".."):
            raise ValueError("runtime file name is invalid")
        name = f"lib/{path.name}"
        if name in files:
            raise ValueError(f"duplicate package member: {name}")
        files[name] = path.read_bytes()
    for name, payload in files.items():
        _validate_name(name)
        _validate_payload(name, payload)
    required_runtime = {"lib/SDL2.dll", "lib/dxcompiler.dll", "lib/dxil.dll"} if windows else {
        "lib/libSDL2-2.0.so.0"}
    if not required_runtime.issubset(files):
        raise ValueError("package lacks required runtime libraries")
    metadata = {
        "schema_version": 1,
        "version": "1.0.0",
        "platform": platform,
        "source_commit": source_commit,
        "build_id": build_id,
        "requires_user_supplied_rom": True,
        "files": {name: _hash(payload) for name, payload in sorted(files.items())},
    }
    files["docs/BUILD.json"] = (json.dumps(metadata, sort_keys=True, indent=2) + "\n").encode()
    files["docs/SHA256SUMS"] = "".join(
        f"{_hash(payload)}  {name}\n" for name, payload in sorted(files.items())
    ).encode()
    root = f"tetrisphere-1.0-{platform}"
    output = Path(output)
    output.parent.mkdir(parents=True, exist_ok=True)
    executable_names = {executable, config_name}
    if windows:
        with zipfile.ZipFile(output, "w", compression=zipfile.ZIP_DEFLATED,
                             compresslevel=9, strict_timestamps=True) as archive:
            for name, payload in sorted(files.items()):
                info = zipfile.ZipInfo(f"{root}/{name}", (1980, 1, 1, 0, 0, 0))
                info.create_system = 3
                info.external_attr = (0o100755 if name in executable_names else 0o100644) << 16
                info.compress_type = zipfile.ZIP_DEFLATED
                archive.writestr(info, payload, compress_type=zipfile.ZIP_DEFLATED,
                                 compresslevel=9)
    else:
        with output.open("wb") as destination:
            with gzip.GzipFile(filename="", mode="wb", fileobj=destination,
                               mtime=0) as compressed:
                with tarfile.open(fileobj=compressed, mode="w", format=tarfile.PAX_FORMAT) as archive:
                    for name, payload in sorted(files.items()):
                        archive.addfile(_tar_info(f"{root}/{name}", payload,
                                                       name in executable_names),
                                        io.BytesIO(payload))
    return audit_package(output)


def audit_package(path: Path) -> dict[str, Any]:
    path = Path(path)
    files: dict[str, bytes] = {}
    roots: set[str] = set()
    casefolded: set[str] = set()

    def add(name: str, payload: bytes) -> None:
        _validate_name(name)
        parts = name.split("/")
        if len(parts) < 2 or not parts[0].startswith("tetrisphere-1.0-"):
            raise ValueError("archive member is outside its package root")
        roots.add(parts[0])
        relative = "/".join(parts[1:])
        if relative in files or relative.casefold() in casefolded:
            raise ValueError("archive contains duplicate member paths")
        casefolded.add(relative.casefold())
        _validate_payload(relative, payload)
        files[relative] = payload

    if path.suffix.lower() == ".zip":
        with zipfile.ZipFile(path) as archive:
            for info in archive.infolist():
                if info.is_dir() or stat.S_IFMT(info.external_attr >> 16) == stat.S_IFLNK:
                    raise ValueError("archive contains a directory or link")
                add(info.filename, archive.read(info))
    else:
        with tarfile.open(path, "r:gz") as archive:
            for member in archive.getmembers():
                if not member.isfile():
                    raise ValueError("archive contains a directory or link")
                payload = archive.extractfile(member)
                if payload is None:
                    raise ValueError("archive member cannot be read")
                add(member.name, payload.read())
    if len(roots) != 1 or "docs/BUILD.json" not in files or "docs/SHA256SUMS" not in files:
        raise ValueError("archive lacks a single root or manifests")
    metadata = json.loads(files["docs/BUILD.json"])
    platform = metadata.get("platform")
    if platform not in PLATFORMS or roots != {f"tetrisphere-1.0-{platform}"}:
        raise ValueError("package root differs from build metadata")
    for name in DOCUMENT_NAMES.values():
        if name in files:
            _validate_platform_document(platform, name, files[name])
    expected = metadata.get("files")
    if not isinstance(expected, dict) or set(expected) != set(files) - {"docs/BUILD.json", "docs/SHA256SUMS"}:
        raise ValueError("archive file inventory differs from build metadata")
    for name, digest in expected.items():
        if not isinstance(digest, str) or _hash(files[name]) != digest:
            raise ValueError(f"archive member hash mismatch: {name}")
    expected_sums = "".join(
        f"{_hash(payload)}  {name}\n" for name, payload in sorted(files.items())
        if name != "docs/SHA256SUMS"
    ).encode()
    if files["docs/SHA256SUMS"] != expected_sums:
        raise ValueError("internal SHA256SUMS differs from archive content")
    required = {"licenses/THIRD_PARTY_NOTICES.md", "assets/README.md",
                "docs/BUILD.json", "docs/SHA256SUMS", *DOCUMENT_NAMES.values()}
    required.update({"tetrisphere.exe", "tetrisphere-config.exe",
                     "lib/SDL2.dll", "lib/dxcompiler.dll", "lib/dxil.dll"}
                    if platform.startswith("windows") else
                    {"tetrisphere", "tetrisphere-config", "lib/libSDL2-2.0.so.0"})
    if not required.issubset(files):
        raise ValueError("archive lacks required beta files")
    if metadata.get("requires_user_supplied_rom") is not True:
        raise ValueError("archive claims bundled or unnecessary ROM")
    return {"platform": platform, "source_commit": metadata.get("source_commit"),
            "build_id": metadata.get("build_id"), "member_count": len(files),
            "sha256": _hash(path.read_bytes())}


def audit_pair(packages: Sequence[dict[str, Any]]) -> dict[str, Any]:
    if len(packages) != 2 or {package["platform"] for package in packages} != PLATFORMS:
        raise ValueError("package pair requires Linux and Windows")
    for key in ("source_commit", "build_id"):
        if packages[0][key] != packages[1][key]:
            raise ValueError(f"package pair has mismatched {key}")
    return {"valid": True, "source_commit": packages[0]["source_commit"],
            "build_id": packages[0]["build_id"]}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)
    create = sub.add_parser("create")
    create.add_argument("--platform", choices=sorted(PLATFORMS), required=True)
    create.add_argument("--binary", type=Path, required=True)
    create.add_argument("--configurator", type=Path, required=True)
    create.add_argument("--runtime", type=Path, action="append", default=[])
    for key in DOCUMENT_NAMES:
        create.add_argument(f"--{key.replace('_', '-')}", type=Path, required=True)
    create.add_argument("--notices", type=Path, required=True)
    create.add_argument("--source-commit", required=True)
    create.add_argument("--build-id", required=True)
    create.add_argument("--output", type=Path, required=True)
    audit = sub.add_parser("audit")
    audit.add_argument("packages", type=Path, nargs="+")
    args = parser.parse_args()
    try:
        if args.command == "create":
            result = create_package(
                platform=args.platform, binary=args.binary,
                configurator=args.configurator, runtime_files=args.runtime,
                documents={key: getattr(args, key) for key in DOCUMENT_NAMES},
                notices=args.notices, source_commit=args.source_commit,
                build_id=args.build_id, output=args.output,
            )
        else:
            packages = [audit_package(package) for package in args.packages]
            result = audit_pair(packages) if len(packages) == 2 else packages[0]
    except (OSError, ValueError, json.JSONDecodeError, tarfile.TarError,
            zipfile.BadZipFile) as error:
        print(json.dumps({"valid": False, "error": str(error)}, sort_keys=True))
        return 2
    print(json.dumps(result, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
