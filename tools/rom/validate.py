#!/usr/bin/env python3
"""Read-only Tetrisphere ROM identification. Python standard library only."""
import argparse
import hashlib
import json
from pathlib import Path
import zipfile
import zlib

MANIFEST = Path(__file__).resolve().parents[2] / "config/roms/tetrisphere-us-rev0.json"
FORMATS = {b"\x80\x37\x12\x40": ("z64", 1),
           b"\x37\x80\x40\x12": ("v64", 2),
           b"\x40\x12\x37\x80": ("n64", 4)}


def reject(reason, **details):
    return {"schema_version": 1, "recognized": False, "reason": reason, **details}


def validate_bytes(data, manifest):
    """Identify canonical content, never relying on a filename or header alone."""
    if len(data) < 64:
        return reject("truncated_header", size_bytes=len(data))
    fmt = FORMATS.get(bytes(data[:4]))
    if fmt is None:
        return reject("invalid_header", size_bytes=len(data))
    name, width = fmt
    details = {"format": name, "size_bytes": len(data)}
    if len(data) % 4:
        return reject("unaligned_size", **details)
    if len(data) != manifest["size_bytes"]:
        return reject("truncated_rom" if len(data) < manifest["size_bytes"] else "unexpected_size", **details)
    if width > 1:
        normalized = bytearray(len(data))
        for offset in range(width):
            normalized[offset::width] = data[width - offset - 1::width]
        data = normalized
    details["sha256"] = hashlib.sha256(data).hexdigest()
    details["header"] = {
        "title": bytes(data[32:52]).decode("ascii", errors="replace").rstrip(" \0"),
        "game_code": bytes(data[59:63]).decode("ascii", errors="replace"),
        "country_code": f"0x{data[62]:02x}",
        "revision": data[63],
        "crc1": bytes(data[16:20]).hex(),
        "crc2": bytes(data[20:24]).hex(),
        "entry_point": f"0x{int.from_bytes(data[8:12], 'big'):08x}",
    }
    if details["sha256"] != manifest["sha256"]:
        return reject("unknown_revision_or_modified", **details)
    return {"schema_version": 1, "recognized": True, "reason": "recognized",
            "revision_id": manifest["id"], **details}


def inspect_path(path, manifest):
    """Read at most the supported size + 1; inspect ZIPs without extracting."""
    path = Path(path)
    limit = manifest["size_bytes"]
    try:
        with path.open("rb") as f:
            magic = f.read(4)
            f.seek(0)
            if magic.startswith(b"PK") or path.suffix.lower() == ".zip":
                with zipfile.ZipFile(f) as archive:
                    candidates = [i for i in archive.infolist() if not i.is_dir()
                                  and Path(i.filename).suffix.lower() in (".z64", ".v64", ".n64")]
                    if len(candidates) != 1:
                        return reject("ambiguous_archive" if candidates else "no_rom_in_archive")
                    info = candidates[0]
                    if info.file_size > limit:
                        return reject("unexpected_size", size_bytes=info.file_size)
                    if info.flag_bits & 1:
                        return reject("encrypted_archive")
                    with archive.open(info) as member:
                        data = member.read(limit + 1)
                    if len(data) > limit:
                        return reject("unexpected_size", size_bytes=len(data))
                    result = validate_bytes(data, manifest)
                    result["container"] = "zip"
                    return result
            if path.stat().st_size > limit:
                return reject("unexpected_size", size_bytes=path.stat().st_size)
            return validate_bytes(f.read(limit + 1), manifest)
    except (zipfile.BadZipFile, zipfile.LargeZipFile, RuntimeError, NotImplementedError, EOFError, zlib.error):
        return reject("invalid_archive")
    except OSError:
        return reject("io_error")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("rom", type=Path, help="Local .z64/.v64/.n64 dump or ZIP with exactly one ROM")
    args = parser.parse_args()
    try:
        manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
        result = inspect_path(args.rom, manifest)
    except (OSError, ValueError, KeyError, TypeError):
        result = reject("manifest_error")
    print(json.dumps(result, indent=2, ensure_ascii=True))
    return 0 if result["recognized"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
