#!/usr/bin/env python3
"""Collect and merge sanitized ares v148 GDB observations.

The collector is deliberately read-only with respect to emulated memory.  Raw
packet payloads and memory contents never enter the JSONL capture.
"""
from __future__ import annotations

import argparse
from collections import Counter
from dataclasses import asdict, dataclass, field
import hashlib
import json
import os
from pathlib import Path
import socket
import sys
import tempfile
import time
from typing import Any, Iterable, Mapping

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from tools.rom.validate import MANIFEST, inspect_path


EVENT_KINDS = {"breakpoint", "indirect_target", "load_range", "task"}
TASK_KINDS = {"graphics", "audio", "unknown"}
FORBIDDEN_KEYS = {
    "bytes", "data", "payload", "raw", "raw_bytes", "rom", "rom_bytes",
    "rom_sha256", "save", "credential", "password", "token",
}
TOP_LEVEL_KEYS = {
    "kind", "timestamp", "address", "target", "size", "rom_start",
    "task_kind", "descriptor_address", "metadata",
    "source_address",
}


@dataclass(frozen=True)
class TraceEvent:
    kind: str
    timestamp: float
    address: int | None = None
    target: int | None = None
    size: int | None = None
    rom_start: int | None = None
    task_kind: str | None = None
    descriptor_address: int | None = None
    source_address: int | None = None
    metadata: Mapping[str, Any] = field(default_factory=dict)


class GDBPacketParser:
    """Incremental parser for acknowledgement bytes and `$payload#xx`."""

    def __init__(self, max_packet_size: int = 0x4096):
        if max_packet_size < 1:
            raise ValueError("max_packet_size must be positive")
        self.max_packet_size = max_packet_size
        self.buffer = bytearray()
        self.acknowledgements: list[bytes] = []

    def feed(self, data: bytes) -> list[bytes]:
        self.buffer.extend(data)
        packets: list[bytes] = []
        while self.buffer:
            if self.buffer[0] in (ord("+"), ord("-")):
                self.acknowledgements.append(bytes(self.buffer[:1]))
                del self.buffer[:1]
                continue
            if self.buffer[0] != ord("$"):
                raise ValueError("malformed GDB stream")
            marker = self.buffer.find(b"#", 1)
            if marker < 0:
                if len(self.buffer) - 1 > self.max_packet_size:
                    raise ValueError("oversized GDB packet")
                break
            if marker - 1 > self.max_packet_size:
                raise ValueError("oversized GDB packet")
            if len(self.buffer) < marker + 3:
                break
            payload = bytes(self.buffer[1:marker])
            checksum_bytes = bytes(self.buffer[marker + 1:marker + 3])
            try:
                checksum = int(checksum_bytes, 16)
            except ValueError as exc:
                raise ValueError("malformed GDB checksum") from exc
            if sum(payload) & 0xFF != checksum:
                raise ValueError("GDB checksum mismatch")
            packets.append(payload)
            del self.buffer[:marker + 3]
        return packets


def _validate_metadata(value: Any, path: str = "metadata") -> None:
    if isinstance(value, Mapping):
        for key, child in value.items():
            if not isinstance(key, str):
                raise ValueError(f"{path} keys must be strings")
            lowered = key.lower()
            if lowered in FORBIDDEN_KEYS or any(
                    marker in lowered for marker in ("payload", "credential", "password")):
                raise ValueError(f"forbidden capture field: {key}")
            _validate_metadata(child, f"{path}.{key}")
    elif isinstance(value, list):
        for index, child in enumerate(value):
            _validate_metadata(child, f"{path}[{index}]")
    elif value is not None and not isinstance(value, (str, int, float, bool)):
        raise ValueError(f"unsupported capture value at {path}")


def _validate_event(event: TraceEvent) -> None:
    if event.kind not in EVENT_KINDS:
        raise ValueError(f"unknown trace event kind: {event.kind}")
    if not isinstance(event.timestamp, (int, float)) or event.timestamp < 0:
        raise ValueError("timestamp must be non-negative")
    if event.address is None or not 0 <= event.address <= 0xFFFFFFFFFFFFFFFF:
        raise ValueError("event address is required")
    if event.kind == "indirect_target" and event.target is None:
        raise ValueError("indirect_target requires target")
    if event.kind == "load_range":
        if event.size is None or event.size <= 0:
            raise ValueError("load_range requires positive size")
        if event.rom_start is None or event.rom_start < 0:
            raise ValueError("load_range requires rom_start")
    if event.kind == "task":
        if event.task_kind not in TASK_KINDS:
            raise ValueError("task requires graphics, audio, or unknown task_kind")
        if event.descriptor_address is None and event.source_address is None:
            raise ValueError("task requires descriptor_address or source_address")
    _validate_metadata(event.metadata)


def _event_object(event: TraceEvent) -> dict[str, Any]:
    _validate_event(event)
    result = {key: value for key, value in asdict(event).items()
              if value is not None and value != {}}
    return result


def _ensure_distinct_output(path: Path, inputs: Iterable[Path]) -> None:
    output_resolved = path.resolve(strict=False)
    for input_path in inputs:
        input_resolved = input_path.resolve(strict=False)
        aliases = output_resolved == input_resolved
        if not aliases and path.exists() and input_path.exists():
            aliases = os.path.samefile(path, input_path)
        if aliases:
            raise ValueError(f"output must not alias input: {input_path}")


def _atomic_write_text(path: Path, content: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    descriptor, temporary_name = tempfile.mkstemp(
        prefix=".trace-runtime-", dir=path.parent, text=True)
    temporary = Path(temporary_name)
    try:
        with os.fdopen(descriptor, "w", encoding="utf-8") as stream:
            stream.write(content)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary, path)
    finally:
        temporary.unlink(missing_ok=True)


def write_capture(path: Path, events: Iterable[TraceEvent]) -> None:
    if path.suffix != ".jsonl":
        raise ValueError("capture path must end in .jsonl")
    lines = [json.dumps(_event_object(event), sort_keys=True,
                       separators=(",", ":")) for event in events]
    _atomic_write_text(path, "".join(line + "\n" for line in lines))


def load_capture(path: Path) -> list[TraceEvent]:
    events: list[TraceEvent] = []
    for number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        try:
            value = json.loads(line)
        except json.JSONDecodeError as exc:
            raise ValueError(f"malformed capture line {number}") from exc
        if not isinstance(value, dict):
            raise ValueError(f"capture line {number} must be an object")
        forbidden = set(value) & FORBIDDEN_KEYS
        if forbidden:
            raise ValueError(f"forbidden capture field: {sorted(forbidden)[0]}")
        unexpected = set(value) - TOP_LEVEL_KEYS
        if unexpected:
            raise ValueError(f"unexpected capture field: {sorted(unexpected)[0]}")
        try:
            event = TraceEvent(**value)
        except TypeError as exc:
            raise ValueError(f"malformed capture event on line {number}") from exc
        _validate_event(event)
        events.append(event)
    return events


def _hex(value: int) -> str:
    return f"0x{value:08x}"


def is_stop_reply(payload: str) -> bool:
    if len(payload) < 3 or payload[0] not in "ST":
        return False
    try:
        int(payload[1:3], 16)
    except ValueError:
        return False
    return True


def merge_trace(static_map: dict, events: Iterable[TraceEvent]) -> dict:
    """Merge observations without promoting absence or requested breakpoints."""
    if not isinstance(static_map, dict) or not isinstance(static_map.get("detections"), dict):
        raise ValueError("static map must contain detections")
    checked = list(events)
    for event in checked:
        _validate_event(event)

    hits = Counter(event.address for event in checked if event.kind == "breakpoint")
    indirect = Counter(
        (event.address, event.target) for event in checked
        if event.kind == "indirect_target"
    )
    ranges = Counter(
        (event.rom_start, event.address, event.size) for event in checked
        if event.kind == "load_range"
    )
    tasks = Counter(
        (event.task_kind, event.address, event.descriptor_address, event.source_address)
        for event in checked
        if event.kind == "task"
    )

    static_overlays = static_map["detections"].get("overlays", [])
    overlay_rulings: list[dict[str, Any]] = []
    observed_keys = set(ranges)
    static_keys = {
        (candidate.get("rom_start"), candidate.get("ram_start"), candidate.get("size"))
        for candidate in static_overlays
    }
    for key, count in sorted(ranges.items()):
        rom_start, ram_start, size = key
        matches = key in static_keys
        overlay_rulings.append({
            "rom_start": rom_start,
            "ram_start": _hex(ram_start),
            "size": size,
            "status": "confirmed" if matches else "open",
            "evidence": (
                f"observed {count} time(s), matching a static candidate"
                if matches else
                f"observed {count} time(s), not present in the static candidates"
            ),
        })
    for rom_start, ram_start, size in sorted(static_keys - observed_keys):
        overlay_rulings.append({
            "rom_start": rom_start,
            "ram_start": _hex(ram_start),
            "size": size,
            "status": "open",
            "evidence": "static candidate not observed in this bounded trace",
        })

    static_indirect_call_sites = {
        item.get("address") for item in static_map["detections"].get("indirect_calls", [])
        if isinstance(item, dict) and isinstance(item.get("address"), int)
    }
    static_indirect_jump_sites = {
        item.get("address") for item in static_map["detections"].get("indirect_jumps", [])
        if isinstance(item, dict) and isinstance(item.get("address"), int)
    }
    observed_indirect_sites = {site for site, _target in indirect}
    indirect_call_rulings = [
        {
            "site": _hex(site),
            "status": "confirmed",
            "evidence": "static indirect-call candidate executed with observed target(s)",
        }
        for site in sorted(observed_indirect_sites & static_indirect_call_sites)
    ]
    indirect_jump_rulings = [
        {
            "site": _hex(site),
            "status": "confirmed",
            "evidence": "static indirect-jump candidate executed with observed target(s)",
        }
        for site in sorted(observed_indirect_sites & static_indirect_jump_sites)
    ]
    unknown_indirect_rulings = [
        {
            "site": _hex(site),
            "status": "open",
            "evidence": "runtime indirect transfer has no matching static call/jump candidate",
        }
        for site in sorted(observed_indirect_sites - static_indirect_call_sites
                           - static_indirect_jump_sites)
    ]
    unobserved_indirect_call_count = len(
        static_indirect_call_sites - observed_indirect_sites)
    unobserved_indirect_jump_count = len(
        static_indirect_jump_sites - observed_indirect_sites)
    entry_point = static_map.get("entry_point")
    startup_rulings = []
    if isinstance(entry_point, int):
        startup_rulings.append({
            "kind": "entry_point", "address": _hex(entry_point),
            "status": "confirmed" if hits[entry_point] else "open",
            "evidence": (
                f"observed {hits[entry_point]} breakpoint hit(s) after reset"
                if hits[entry_point] else "not observed in this bounded trace"
            ),
        })

    return {
        "schema_version": 1,
        "event_count": len(checked),
        "hit_counts": {_hex(address): count for address, count in sorted(hits.items())},
        "indirect_targets": [
            {"site": _hex(site), "target": _hex(target), "count": count,
             "transfer_kind": (
                 "call" if site in static_indirect_call_sites else
                 "jump" if site in static_indirect_jump_sites else "unknown")}
            for (site, target), count in sorted(indirect.items())
        ],
        "load_ranges": [
            {"rom_start": rom_start, "ram_start": _hex(ram_start),
             "size": size, "count": count}
            for (rom_start, ram_start, size), count in sorted(ranges.items())
        ],
        "tasks": [
            ({"kind": kind, "submission_site": _hex(site), "count": count}
             | ({"descriptor_address": _hex(descriptor)} if descriptor is not None else {})
             | ({"source_address": _hex(source)} if source is not None else {}))
            for (kind, site, descriptor, source), count in sorted(
                tasks.items(), key=lambda item: tuple(-1 if part is None else part
                                                       for part in item[0]))
        ],
        "rulings": {
            "startup": startup_rulings,
            "indirect_calls": indirect_call_rulings,
            "indirect_jumps": indirect_jump_rulings,
            "unknown_indirect_transfers": unknown_indirect_rulings,
            "unobserved_static_indirect_call_count": unobserved_indirect_call_count,
            "unobserved_static_indirect_jump_count": unobserved_indirect_jump_count,
            "overlays": overlay_rulings,
            "overlay_summary": {
                "status": "confirmed" if overlay_rulings and all(
                    item["status"] == "confirmed" for item in overlay_rulings) else "open",
                "evidence": (
                    "all observed/static overlay ranges agree"
                    if overlay_rulings and all(item["status"] == "confirmed"
                                               for item in overlay_rulings)
                    else "bounded observation does not prove absence of overlays"
                ),
            },
        },
        "limitations": [
            "A bounded trace cannot prove an unobserved route absent.",
            "Requested breakpoints without a hit are not runtime evidence.",
        ],
    }


class GDBRemote:
    """Small ares-compatible RSP client; it never issues memory writes."""

    def __init__(self, host: str, port: int, timeout: float = 10.0):
        self.socket = socket.create_connection((host, port), timeout=timeout)
        self.socket.sendall(b"+")
        self.parser = GDBPacketParser()
        self.pending: list[bytes] = []
        self.last_frame: bytes | None = None
        self.nak_retries = 0
        self.ack_index = 0
        self.max_nak_retries = 3

    @staticmethod
    def _frame(command: str) -> bytes:
        payload = command.encode("ascii")
        return b"$" + payload + b"#" + f"{sum(payload) & 0xff:02x}".encode()

    def send(self, command: str) -> None:
        self.last_frame = self._frame(command)
        self.nak_retries = 0
        self.socket.sendall(self.last_frame)

    def receive(self, timeout: float | None = None) -> bytes:
        if self.pending:
            return self.pending.pop(0)
        if timeout is not None:
            self.socket.settimeout(timeout)
        while True:
            chunk = self.socket.recv(65536)
            if not chunk:
                raise ConnectionError("ares closed the debug connection")
            packets = self.parser.feed(chunk)
            acknowledgements = self.parser.acknowledgements[self.ack_index:]
            self.ack_index += len(acknowledgements)
            saw_nak = False
            for acknowledgement in acknowledgements:
                if acknowledgement == b"-":
                    saw_nak = True
                    if self.last_frame is None:
                        raise ConnectionError("ares sent NAK without a transmitted frame")
                    self.nak_retries += 1
                    if self.nak_retries > self.max_nak_retries:
                        raise ConnectionError(
                            f"ares repeated GDB NAK more than {self.max_nak_retries} times")
                    self.socket.sendall(self.last_frame)
                elif not saw_nak:
                    self.nak_retries = 0
            if saw_nak:
                continue
            if packets:
                self.socket.sendall(b"+" * len(packets))
                self.pending.extend(packets[1:])
                return packets[0]

    def query(self, command: str) -> str:
        self.send(command)
        return self.receive().decode("ascii")

    def register(self, index: int) -> int:
        payload = self.query(f"p{index}")
        if len(payload) != 16:
            raise ValueError(f"unexpected register {index} reply")
        return int(payload, 16)

    def memory(self, address: int, size: int) -> bytes:
        payload = self.query(f"m{address:x},{size:x}")
        if len(payload) != size * 2:
            raise ValueError("unexpected memory reply length")
        return bytes.fromhex(payload)

    def close(self) -> None:
        try:
            self.query("D")
        finally:
            self.socket.close()


def _load_tracepoints(path: Path) -> list[dict[str, Any]]:
    value = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(value, dict) or not isinstance(value.get("tracepoints"), list):
        raise ValueError("tracepoint file must contain a tracepoints list")
    result: list[dict[str, Any]] = []
    for item in value["tracepoints"]:
        if not isinstance(item, dict) or item.get("kind") not in EVENT_KINDS:
            raise ValueError("malformed tracepoint")
        if not isinstance(item.get("address"), str):
            raise ValueError("tracepoint address must be hexadecimal text")
        copy = dict(item)
        copy["address"] = int(copy["address"], 0)
        result.append(copy)
    return result


def collect_trace(host: str, port: int, tracepoints: list[dict[str, Any]],
                  max_events: int, seconds: float,
                  code_fingerprints: Iterable[Mapping[str, Any]] = ()) -> list[TraceEvent]:
    """Collect breakpoint events until a bounded count/time limit."""
    remote = GDBRemote(host, port)
    started = time.monotonic()
    points = {point["address"]: point for point in tracepoints}
    enabled = set(points)
    point_hits: Counter[int] = Counter()
    events: list[TraceEvent] = []
    try:
        remote.query("?")
        for fingerprint in code_fingerprints:
            address = int(str(fingerprint["address"]), 0)
            size = int(fingerprint["size_bytes"])
            actual = hashlib.sha256(remote.memory(address, size)).hexdigest()
            if actual != fingerprint["sha256"]:
                raise ValueError("live code does not match the NTPE rev0 trace map")
        for address, point in points.items():
            breakpoint_type = int(point.get("breakpoint_type", 0))
            reply = remote.query(f"Z{breakpoint_type},{address:x},4")
            if reply != "OK":
                raise RuntimeError(f"ares rejected breakpoint {_hex(address)}: {reply}")
        while len(events) < max_events and time.monotonic() - started < seconds:
            remote.send("c")
            remaining = max(0.1, seconds - (time.monotonic() - started))
            try:
                stop = remote.receive(timeout=remaining).decode("ascii")
            except TimeoutError:
                break
            if not is_stop_reply(stop):
                raise RuntimeError(f"unexpected ares stop reply: {stop}")
            pc = remote.register(37) & 0xFFFFFFFF
            point_address = pc
            point = points.get(pc)
            if point is None and "watch:" in stop:
                try:
                    watched = int(stop.split("watch:", 1)[1].split(";", 1)[0], 16)
                except ValueError as exc:
                    raise RuntimeError(f"malformed ares watchpoint stop: {stop}") from exc
                point_address = next(
                    (address for address in enabled
                     if address <= watched < address + int(points[address].get("watch_size", 4))),
                    watched,
                )
                point = points.get(point_address)
            if point is None:
                continue
            now = time.monotonic() - started
            if point["kind"] == "indirect_target":
                target = remote.register(int(point["register"])) & 0xFFFFFFFF
                events.append(TraceEvent("indirect_target", now, address=pc,
                                         target=target))
            elif point["kind"] == "task":
                if "descriptor_register" in point:
                    descriptor = remote.register(int(point["descriptor_register"])) & 0xFFFFFFFF
                else:
                    descriptor = None
                source = None
                if "source_mmio" in point:
                    source = int.from_bytes(
                        remote.memory(int(point["source_mmio"]), 4), "big")
                    if point.get("source_virtual", False):
                        source = 0x80000000 | source
                if ("pc_min" in point and pc < int(point["pc_min"])) or (
                        "pc_max" in point and pc >= int(point["pc_max"])):
                    continue
                events.append(TraceEvent(
                    "task", now, address=pc,
                    task_kind=point.get("task_kind", "unknown"),
                    descriptor_address=descriptor, source_address=source,
                    metadata={"tracepoint": point.get("name", _hex(pc)),
                              "register": point.get("source_register", "unknown")},
                ))
            elif point["kind"] == "load_range":
                ram_start = int.from_bytes(remote.memory(int(point["ram_mmio"]), 4), "big")
                rom_address = int.from_bytes(remote.memory(int(point["rom_mmio"]), 4), "big")
                length_word = int.from_bytes(remote.memory(int(point["length_mmio"]), 4), "big")
                events.append(TraceEvent(
                    "load_range", now, address=0x80000000 | (ram_start & 0x7FFFFF),
                    size=(length_word & 0xFFFFFF) + 1,
                    rom_start=rom_address & 0x0FFFFFFF,
                    metadata={"submission_site": _hex(pc),
                              "tracepoint": point.get("name", _hex(point_address))},
                ))
            else:
                events.append(TraceEvent(point["kind"], now, address=pc))
            point_hits[point_address] += 1
            point_limit = int(point.get("max_hits", max_events))
            if point_hits[point_address] >= point_limit:
                breakpoint_type = int(point.get("breakpoint_type", 0))
                reply = remote.query(f"z{breakpoint_type},{point_address:x},4")
                if reply != "OK":
                    raise RuntimeError(
                        f"ares rejected breakpoint removal {_hex(point_address)}: {reply}")
                enabled.discard(point_address)
                if not enabled:
                    break
        return events
    finally:
        remote.close()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)
    collect = subparsers.add_parser("collect")
    collect.add_argument("--tracepoints", type=Path, required=True)
    collect.add_argument("--rom", type=Path, required=True)
    collect.add_argument("--output", type=Path, required=True)
    collect.add_argument("--host", default="::1")
    collect.add_argument("--port", type=int, default=19123)
    collect.add_argument("--max-events", type=int, default=200)
    collect.add_argument("--seconds", type=float, default=120)
    merge = subparsers.add_parser("merge")
    merge.add_argument("--static-map", type=Path, required=True)
    merge.add_argument("--capture", type=Path, action="append", required=True)
    merge.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if args.command == "collect":
        if args.max_events < 1 or args.seconds <= 0:
            parser.error("collection bounds must be positive")
        try:
            _ensure_distinct_output(args.output, [args.rom, args.tracepoints])
        except ValueError as exc:
            parser.error(str(exc))
        manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
        rom = inspect_path(args.rom, manifest)
        if not rom["recognized"]:
            parser.error("collection requires the recognized NTPE rev0 ROM")
        counter_map = json.loads(
            (Path(__file__).resolve().parents[2]
             / "config/roms/tetrisphere-us-rev0-counters.json").read_text(encoding="utf-8")
        )
        events = collect_trace(
            args.host, args.port, _load_tracepoints(args.tracepoints),
            args.max_events, args.seconds, counter_map["code_fingerprints"])
        write_capture(args.output, events)
        print(json.dumps({"events": len(events), "output": str(args.output)}))
        return 0
    try:
        _ensure_distinct_output(args.output, [args.static_map, *args.capture])
    except ValueError as exc:
        parser.error(str(exc))
    static_map = json.loads(args.static_map.read_text(encoding="utf-8"))
    events = [event for capture in args.capture for event in load_capture(capture)]
    result = merge_trace(static_map, events)
    _atomic_write_text(args.output,
                       json.dumps(result, indent=2, sort_keys=True) + "\n")
    print(json.dumps({"events": len(events), "output": str(args.output)}))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
