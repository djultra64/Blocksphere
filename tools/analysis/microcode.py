#!/usr/bin/env python3
"""Bounded OSTask and command inventory utilities.

The tool consumes private capture files but emits only addresses, sizes, hashes,
and command metadata. It never embeds captured bytes in its JSON output.
"""
from __future__ import annotations

import argparse
from dataclasses import asdict, dataclass
import hashlib
import json
from pathlib import Path
import struct
from typing import Iterable, Mapping


RDRAM_BASE = 0x80000000
RDRAM_SIZE = 0x800000
TASK_FIELDS = (
    "type", "flags", "ucode_boot", "ucode_boot_size", "ucode",
    "ucode_size", "ucode_data", "ucode_data_size", "dram_stack",
    "dram_stack_size", "output_buff", "output_buff_size_ptr", "data_ptr",
    "data_size", "yield_data_ptr", "yield_data_size",
)
REGIONS = (
    ("ucode_boot", "ucode_boot_size"),
    ("ucode", "ucode_size"),
    ("ucode_data", "ucode_data_size"),
    ("dram_stack", "dram_stack_size"),
    ("data_ptr", "data_size"),
    ("yield_data_ptr", "yield_data_size"),
)


class MicrocodeError(ValueError):
    pass


@dataclass(frozen=True)
class OSTaskRecord:
    type: int
    flags: int
    ucode_boot: int
    ucode_boot_size: int
    ucode: int
    ucode_size: int
    ucode_data: int
    ucode_data_size: int
    dram_stack: int
    dram_stack_size: int
    output_buff: int
    output_buff_size_ptr: int
    data_ptr: int
    data_size: int
    yield_data_ptr: int
    yield_data_size: int


@dataclass(frozen=True)
class CommandRecord:
    offset: int
    opcode: int
    size: int
    supported: bool


def _validate_region(name: str, address: int, size: int, rdram_size: int) -> None:
    if size == 0:
        return
    if address < RDRAM_BASE or address >= RDRAM_BASE + rdram_size:
        raise MicrocodeError(f"{name} is outside RDRAM")
    offset = address - RDRAM_BASE
    if size > rdram_size or offset > rdram_size - size:
        raise MicrocodeError(f"{name} region is outside RDRAM")


def parse_ostask(data: bytes, rdram_size: int = RDRAM_SIZE) -> OSTaskRecord:
    if len(data) != 64:
        raise MicrocodeError("OSTask descriptor must be exactly 64 bytes")
    if rdram_size <= 0:
        raise MicrocodeError("RDRAM size must be positive")
    task = OSTaskRecord(**dict(zip(TASK_FIELDS, struct.unpack(">16I", data))))
    for pointer_name, size_name in REGIONS:
        _validate_region(pointer_name, getattr(task, pointer_name),
                         getattr(task, size_name), rdram_size)
    return task


def classify_task(task: OSTaskRecord) -> str:
    if task.type == 1:
        return "graphics"
    if task.type == 2:
        return "audio"
    raise MicrocodeError(f"unsupported OSTask type {task.type}")


def _rdp_command_size(opcode: int) -> int:
    if opcode in (0x24, 0x25):
        return 16
    if 0x08 <= opcode <= 0x0F:
        words = 4
        if opcode & 0x04:
            words += 8
        if opcode & 0x02:
            words += 8
        if opcode & 0x01:
            words += 2
        return words * 8
    return 8


def enumerate_commands(data: bytes, *, kind: str,
                       supported_opcodes: Iterable[int]) -> list[CommandRecord]:
    supported = set(supported_opcodes)
    if kind not in ("audio", "gbi", "rdp"):
        raise MicrocodeError(f"unsupported command kind {kind}")
    result: list[CommandRecord] = []
    offset = 0
    while offset < len(data):
        if len(data) - offset < 8:
            raise MicrocodeError(f"truncated command at offset {offset}")
        raw_opcode = data[offset]
        opcode = raw_opcode if kind in ("audio", "gbi") else raw_opcode & 0x3F
        size = 8 if kind in ("audio", "gbi") else _rdp_command_size(opcode)
        if size > len(data) - offset:
            raise MicrocodeError(f"truncated command at offset {offset}")
        result.append(CommandRecord(offset, opcode, size, opcode in supported))
        offset += size
    return result


def audio_output_ranges(data: bytes) -> list[tuple[int, int]]:
    """Derive ABI output DMAs from SETBUFF (0x08) and SAVEBUFF (0x06).

    Tetrisphere's captured audio ABI stores the active transfer length in the
    low half of SETBUFF's second word. SAVEBUFF then writes that many bytes to
    the 24-bit RDRAM address in its second word.
    """
    commands = enumerate_commands(data, kind="audio",
                                  supported_opcodes=range(256))
    active_size: int | None = None
    result: list[tuple[int, int]] = []
    for command in commands:
        word0, word1 = struct.unpack_from(">II", data, command.offset)
        if command.opcode == 0x08:
            active_size = word1 & 0xFFFF
        elif command.opcode == 0x06:
            if not active_size:
                raise MicrocodeError(
                    f"audio save at offset {command.offset} has no set buffer size")
            result.append((word1 & 0x00FFFFFF, active_size))
    return result


def _has_private_value(value: object) -> bool:
    if isinstance(value, Mapping):
        return any(_has_private_value(key) or _has_private_value(child)
                   for key, child in value.items())
    if isinstance(value, list):
        return any(_has_private_value(child) for child in value)
    return isinstance(value, str) and (".local" in value or "rom_sha256" in value)


def validate_manifest(value: object) -> None:
    if not isinstance(value, dict) or value.get("schema_version") != 1:
        raise MicrocodeError("unsupported microcode manifest schema")
    if _has_private_value(value):
        raise MicrocodeError("manifest contains private paths or ROM identity")
    for kind in ("graphics", "audio"):
        section = value.get(kind)
        if not isinstance(section, dict):
            raise MicrocodeError(f"manifest is missing {kind}")
        digest = section.get("capture_sha256")
        if not isinstance(digest, str) or len(digest) != 64:
            raise MicrocodeError(f"{kind}.capture_sha256 must be a SHA-256")
        try:
            int(digest, 16)
        except ValueError as exc:
            raise MicrocodeError(
                f"{kind}.capture_sha256 must be a SHA-256") from exc


def _inventory(descriptor_path: Path, commands_path: Path, kind: str,
               supported: set[int]) -> dict:
    descriptor_bytes = descriptor_path.read_bytes()
    command_bytes = commands_path.read_bytes()
    task = parse_ostask(descriptor_bytes)
    actual_kind = classify_task(task)
    if kind == "audio" and actual_kind != "audio":
        raise MicrocodeError("audio inventory requires an audio OSTask")
    if kind in ("gbi", "rdp") and actual_kind != "graphics":
        raise MicrocodeError(f"{kind.upper()} inventory requires a graphics OSTask")
    commands = enumerate_commands(command_bytes, kind=kind,
                                  supported_opcodes=supported)
    return {
        "schema_version": 1,
        "task_kind": actual_kind,
        "descriptor_sha256": hashlib.sha256(descriptor_bytes).hexdigest(),
        "command_sha256": hashlib.sha256(command_bytes).hexdigest(),
        "task": {key: f"0x{value:08x}" for key, value in asdict(task).items()},
        "commands": [asdict(command) for command in commands],
        "unsupported": [asdict(command) for command in commands
                        if not command.supported],
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--descriptor", type=Path, required=True)
    parser.add_argument("--commands", type=Path, required=True)
    parser.add_argument("--kind", choices=("audio", "gbi", "rdp"), required=True)
    parser.add_argument("--supported", required=True,
                        help="comma-separated hexadecimal opcode list")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    supported = {int(item, 0) for item in args.supported.split(",") if item}
    result = _inventory(args.descriptor, args.commands, args.kind, supported)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n",
                           encoding="utf-8")
    print(json.dumps({"commands": len(result["commands"]),
                      "unsupported": len(result["unsupported"]),
                      "output": str(args.output)}))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
