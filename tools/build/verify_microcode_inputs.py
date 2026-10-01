#!/usr/bin/env python3
"""Bind private Task 6 captures to the sanitized microcode manifest."""

import argparse
import hashlib
import json
from pathlib import Path

try:
    from tools.analysis.microcode import parse_ostask
except ModuleNotFoundError:
    import sys
    sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
    from tools.analysis.microcode import parse_ostask


def _sha(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def _expect(label: str, actual: str, expected: str) -> None:
    if actual.lower() != expected.lower():
        raise ValueError(f"{label} hash mismatch")


def _offset(value: object, label: str) -> int:
    try:
        result = int(value, 0) if isinstance(value, str) else int(value)
    except (TypeError, ValueError) as error:
        raise ValueError(f"invalid {label}") from error
    if result < 0:
        raise ValueError(f"invalid {label}")
    return result


def _bounded(data: bytes, offset: int, size: int, label: str) -> bytes:
    if size <= 0 or offset > len(data) or size > len(data) - offset:
        raise ValueError(f"{label} outside snapshot")
    return data[offset:offset + size]


def _task(snapshot: bytes, section: dict, label: str):
    descriptor_offset = _offset(section.get("descriptor_offset"),
                                f"{label} descriptor offset")
    descriptor = _bounded(snapshot, descriptor_offset, 64,
                          f"{label} descriptor")
    _expect(f"{label} descriptor", _sha(descriptor),
            section["capture_sha256"])
    return parse_ostask(descriptor, len(snapshot))


def verify_inputs(manifest_path: Path, graphics_snapshot_path: Path,
                  graphics_commands_path: Path,
                  audio_snapshot_path: Path) -> dict:
    manifest_path = Path(manifest_path)
    graphics = Path(graphics_snapshot_path).read_bytes()
    graphics_commands = Path(graphics_commands_path).read_bytes()
    audio = Path(audio_snapshot_path).read_bytes()
    manifest_bytes = manifest_path.read_bytes()
    manifest = json.loads(manifest_bytes)
    if manifest.get("schema_version") != 1:
        raise ValueError("unsupported microcode manifest schema")

    gfx = manifest["graphics"]
    aud = manifest["audio"]
    _expect("graphics snapshot", _sha(graphics), gfx["snapshot_sha256"])
    _expect("audio snapshot", _sha(audio), aud["snapshot_sha256"])
    gfx_task = _task(graphics, gfx, "graphics")
    audio_task = _task(audio, aud, "audio")
    if gfx_task.type != 1 or audio_task.type != 2:
        raise ValueError("capture OSTask type mismatch")

    gfx_dl = _bounded(graphics, gfx_task.data_ptr - 0x80000000,
                      gfx_task.data_size, "graphics display list")
    if gfx_dl != graphics_commands:
        raise ValueError("graphics command capture hash mismatch")
    _expect("graphics commands", _sha(graphics_commands),
            gfx["display_list"]["sha256"])
    if len(graphics_commands) != gfx["display_list"]["bytes"]:
        raise ValueError("graphics command size mismatch")

    gfx_text = _bounded(graphics, gfx_task.ucode - 0x80000000,
                        gfx["text"]["size"], "graphics ucode")
    gfx_data = _bounded(graphics, gfx_task.ucode_data - 0x80000000,
                        gfx["data"]["size"], "graphics ucode data")
    _expect("graphics ucode", _sha(gfx_text), gfx["text"]["sha256"])
    _expect("graphics ucode data", _sha(gfx_data), gfx["data"]["sha256"])

    audio_commands = _bounded(audio, audio_task.data_ptr - 0x80000000,
                              audio_task.data_size, "audio commands")
    if len(audio_commands) != aud["command_bytes"]:
        raise ValueError("audio command size mismatch")
    _expect("audio commands", _sha(audio_commands), aud["command_sha256"])
    audio_text = _bounded(audio, audio_task.ucode - 0x80000000,
                          aud["rsp_recomp"]["text_size"], "audio ucode")
    audio_ucode_sha = _sha(audio_text)
    _expect("audio ucode", audio_ucode_sha,
            aud["rsp_recomp"]["ucode_sha256"])

    material = {
        "manifest_sha256": _sha(manifest_bytes),
        "graphics_snapshot_sha256": _sha(graphics),
        "graphics_commands_sha256": _sha(graphics_commands),
        "audio_snapshot_sha256": _sha(audio),
        "audio_ucode_sha256": audio_ucode_sha,
    }
    identity = hashlib.sha256(json.dumps(
        material, sort_keys=True, separators=(",", ":")).encode()).hexdigest()
    return {"schema_version": 1, **material, "source_identity": identity}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--graphics-snapshot", type=Path, required=True)
    parser.add_argument("--graphics-commands", type=Path, required=True)
    parser.add_argument("--audio-snapshot", type=Path, required=True)
    parser.add_argument("--receipt", type=Path, required=True)
    parser.add_argument("--expected-source-identity")
    args = parser.parse_args()
    try:
        receipt = verify_inputs(args.manifest, args.graphics_snapshot,
                                args.graphics_commands, args.audio_snapshot)
        if (args.expected_source_identity is not None and
                receipt["source_identity"] != args.expected_source_identity):
            raise ValueError("configured_identity_mismatch:microcode_inputs")
        args.receipt.parent.mkdir(parents=True, exist_ok=True)
        temporary = args.receipt.with_name(args.receipt.name + ".tmp")
        temporary.write_text(json.dumps(receipt, indent=2, sort_keys=True) + "\n",
                             encoding="utf-8")
        temporary.replace(args.receipt)
    except (KeyError, OSError, ValueError, json.JSONDecodeError) as error:
        print(f"microcode input verification failed: {error}")
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
