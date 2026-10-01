#!/usr/bin/env python3
"""Validate a private, real-artifact M1 native demonstration receipt."""
from __future__ import annotations

import argparse
import array
import hashlib
import json
import math
from pathlib import Path, PurePosixPath
import re
import subprocess
import sys
from typing import Any
import wave

REQUIRED_ARTIFACTS = ("screenshot_before", "screenshot", "audio", "event_log")
SHA256_RE = re.compile(r"^[0-9a-f]{64}$")
COMMIT_RE = re.compile(r"^[0-9a-f]{40}$")
REAL_DEMO_DETAILS = {
    "rom_validated": {"rom_id": "tetrisphere-us-rev0", "exact": True},
    "game_screen_presented": {"source": "game"},
    "playable_scene_entered": {"source": "game"},
    "logical_input_consumed": {"visible_change": True},
    "rt64_gpu_presented": {"backend": "rt64-vulkan", "full_sync": True},
    "music_output": {"backend": "sdl-device", "non_silent": True},
    "effect_output": {"backend": "sdl-device", "distinct_transient": True},
}


def _mapping(value: Any, name: str) -> dict[str, Any]:
    if not isinstance(value, dict):
        raise ValueError(f"{name} must be an object")
    return value


def _digest(value: Any, name: str, pattern: re.Pattern[str] = SHA256_RE) -> str:
    if not isinstance(value, str) or pattern.fullmatch(value) is None:
        raise ValueError(f"{name} must be lowercase hexadecimal")
    return value


def _sanitized_path(value: Any, name: str) -> str:
    if not isinstance(value, str) or not value:
        raise ValueError(f"{name} must be a sanitized relative path")
    path = PurePosixPath(value)
    if path.is_absolute() or ".." in path.parts or ".local" in path.parts or "\\" in value:
        raise ValueError(f"{name} must be a sanitized relative path")
    return value


def _sha256(path: Path) -> str:
    result = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            result.update(chunk)
    return result.hexdigest()


def _artifact(root: Path, artifacts: dict[str, Any], name: str) -> Path:
    descriptor = _mapping(artifacts.get(name), f"artifacts.{name}")
    relative = _sanitized_path(descriptor.get("path"), f"artifacts.{name}.path")
    expected = _digest(descriptor.get("sha256"), f"artifacts.{name}.sha256")
    path = (root / relative).resolve()
    if root != path and root not in path.parents:
        raise ValueError(f"artifact path escapes evidence root: {name}")
    if not path.is_file() or _sha256(path) != expected:
        raise ValueError(f"artifact hash mismatch: {name}")
    return path


def _platform_contract(platform: str) -> tuple[str, str, str]:
    if platform == "linux-x86_64":
        return "native_linux_started", "tetrisphere-m1", "Linux"
    if platform == "windows-x86_64":
        return "native_windows_started", "tetrisphere-m1.exe", "Windows"
    raise ValueError("unsupported native evidence platform")


def _read_native_log(path: Path, build_id: str, start_marker: str,
                     run_id: str) -> dict[str, str]:
    try:
        records = []
        for line in path.read_text(encoding="utf-8").splitlines():
            stripped = line.strip()
            # RT64 prints adapter diagnostics beside the structured host events.
            if stripped.startswith("{"):
                records.append(json.loads(stripped))
    except (OSError, json.JSONDecodeError) as error:
        raise ValueError(f"native event log is invalid: {error}") from error
    records = [record for record in records if isinstance(record, dict)]
    events = [record.get("event") for record in records]
    required = {"rom_validated", "diagnostic_start", start_marker,
                "rt64_device_ready", "rsp_task_submit", "rt64_present",
                "logical_input_sample"}
    if not required.issubset(events):
        raise ValueError("native event log is missing runtime markers")
    bound_records = [record for record in records if record.get("event") in required]
    if any(record.get("run_id") != run_id for record in bound_records):
        raise ValueError("native event log run identity differs from evidence")
    rom = next(record for record in records if record.get("event") == "rom_validated")
    start = next(record for record in records if record.get("event") == "diagnostic_start")
    device = next(record for record in records if record.get("event") == "rt64_device_ready")
    rsp = [record for record in records if record.get("event") == "rsp_task_submit"]
    inputs = [record for record in records
              if record.get("event") == "logical_input_sample" and
              str(record.get("buttons", "0x0")) not in ("0", "0x0", "0x0000")]
    if (rom.get("rom_id") != "tetrisphere-us-rev0" or
            start.get("build_id") != build_id or device.get("api") != "vulkan" or
            not any(task.get("type") == 2 and task.get("ucode") == "0x800DE7D0"
                    for task in rsp) or not inputs):
        raise ValueError("native event log does not match the claimed build/run")
    input_event = inputs[-1]
    family, prompt = input_event.get("family"), input_event.get("prompt_confirm")
    if not isinstance(family, str) or not family or not isinstance(prompt, str) or not prompt:
        raise ValueError("native event log lacks derived input family/prompt")
    return {"input_family": family, "prompt_label": prompt}


def _wav_metric(path: Path, window: Any) -> tuple[float, int]:
    if (not isinstance(window, list) or len(window) != 2 or
            not all(isinstance(value, (int, float)) for value in window)):
        raise ValueError("audio metric window must have two numeric bounds")
    with wave.open(str(path), "rb") as source:
        if source.getnchannels() != 2 or source.getsampwidth() != 2:
            raise ValueError("audio metric source must be stereo 16-bit PCM")
        rate = source.getframerate()
        start, end = (round(float(value) * rate) for value in window)
        if start < 0 or end <= start or end > source.getnframes():
            raise ValueError("audio metric window is outside WAV")
        source.setpos(start)
        raw = source.readframes(end - start)
    samples = array.array("h")
    samples.frombytes(raw)
    if sys.byteorder != "little":
        samples.byteswap()
    if not samples:
        raise ValueError("audio metric window is empty")
    rms = math.sqrt(sum(sample * sample for sample in samples) / len(samples))
    return rms, max(abs(sample) for sample in samples)


def validate_evidence(value: Any, *, evidence_root: Path | str | None = None,
                      binary: Path | str | None = None, source_commit: str | None = None,
                      rt64_receipt: Path | str | None = None,
                      expected_platform: str = "linux-x86_64") -> dict[str, Any]:
    """Fail closed unless *value* is tied to one set of real private artifacts."""
    if evidence_root is None or binary is None or source_commit is None or rt64_receipt is None:
        raise ValueError("explicit evidence root, binary, source commit and RT64 receipt are required")
    evidence_root = Path(evidence_root).resolve()
    binary = Path(binary).resolve()
    rt64_receipt = Path(rt64_receipt).resolve()
    start_marker, expected_executable, platform_name = _platform_contract(expected_platform)
    required_markers = (
        "rom_validated", start_marker, "game_screen_presented",
        "playable_scene_entered", "logical_input_consumed", "rt64_gpu_presented",
        "music_output", "effect_output",
    )
    real_demo_details = dict(REAL_DEMO_DETAILS)
    real_demo_details[start_marker] = {
        "backend": "native", "executable": expected_executable,
    }
    if not evidence_root.is_dir():
        raise ValueError("explicit evidence root is not a directory")

    root = _mapping(value, "manifest")
    if root.get("schema_version") != 1:
        raise ValueError("unsupported evidence schema")
    build = _mapping(root.get("build"), "build")
    run = _mapping(root.get("run"), "run")
    build_id = _digest(build.get("id"), "build.id")
    binary_hash = _digest(build.get("binary_sha256"), "build.binary_sha256")
    manifest_commit = _digest(build.get("source_commit"), "build.source_commit", COMMIT_RE)
    rt64_commit = _digest(build.get("rt64_commit"), "build.rt64_commit", COMMIT_RE)
    receipt_hash = _digest(build.get("rt64_receipt_sha256"), "build.rt64_receipt_sha256")
    if manifest_commit != source_commit:
        raise ValueError("source commit does not match checked-out source")
    if not binary.is_file() or _sha256(binary) != binary_hash:
        raise ValueError("binary hash does not match executable")
    if not rt64_receipt.is_file() or _sha256(rt64_receipt) != receipt_hash:
        raise ValueError("RT64 receipt hash does not match")
    try:
        receipt = json.loads(rt64_receipt.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise ValueError(f"RT64 receipt is invalid: {error}") from error
    if not isinstance(receipt, dict) or receipt.get("commit") != rt64_commit:
        raise ValueError("RT64 receipt commit does not match")
    if build.get("platform") != expected_platform:
        raise ValueError(f"evidence is not a native {platform_name} x86-64 build")

    run_id = run.get("id")
    if not isinstance(run_id, str) or not run_id or len(run_id) > 128:
        raise ValueError("run.id must be a bounded nonempty string")
    compatibility_claim = (run.get("wine") is True or run.get("proton") is True)
    if (run.get("build_id") != build_id or run.get("platform") != expected_platform or
            run.get("native") is not True or run.get("emulator") is not False or
            run.get("compatibility_layer") is not False or compatibility_claim):
        raise ValueError(
            f"evidence must come from one native {platform_name} run without compatibility layer")
    if expected_platform == "windows-x86_64" and (
            run.get("wine") is not False or run.get("proton") is not False):
        raise ValueError("evidence must explicitly reject Wine/Proton for one native Windows run")

    events = root.get("events")
    if not isinstance(events, list) or not events:
        raise ValueError("events must be a nonempty array")
    markers: list[str] = []
    event_details: dict[str, dict[str, Any]] = {}
    for index, raw_event in enumerate(events, start=1):
        event = _mapping(raw_event, f"events[{index - 1}]")
        if event.get("sequence") != index:
            raise ValueError("event sequences must be contiguous and start at one")
        if event.get("build_id") != build_id or event.get("run_id") != run_id:
            raise ValueError("every event must have a single build/run identity")
        marker = event.get("marker")
        if not isinstance(marker, str) or not marker:
            raise ValueError("event marker must be nonempty")
        details = _mapping(event.get("details"), f"event {marker} details")
        for field, expected in real_demo_details.get(marker, {}).items():
            if details.get(field) != expected:
                raise ValueError(f"event {marker} violates real demo contract: {field}")
        markers.append(marker)
        event_details[marker] = details
    missing = [marker for marker in required_markers if marker not in markers]
    if missing:
        raise ValueError("missing required marker: " + ", ".join(missing))
    positions = [markers.index(marker) for marker in required_markers]
    if positions != sorted(positions):
        raise ValueError("required events violate ordered marker contract")

    artifacts = _mapping(root.get("artifacts"), "artifacts")
    paths = {name: _artifact(evidence_root, artifacts, name) for name in REQUIRED_ARTIFACTS}
    if (not paths["screenshot_before"].read_bytes().startswith(b"\x89PNG\r\n\x1a\n") or
            not paths["screenshot"].read_bytes().startswith(b"\x89PNG\r\n\x1a\n") or
            _sha256(paths["screenshot_before"]) == _sha256(paths["screenshot"])):
        raise ValueError("screenshots are invalid or do not demonstrate a visible change")

    derived = _read_native_log(paths["event_log"], build_id, start_marker, run_id)
    native_family = event_details[start_marker].get("controller_family")
    logical = event_details["logical_input_consumed"]
    if (native_family != derived["input_family"] or
            logical.get("input_family") != derived["input_family"] or
            logical.get("prompt_label") != derived["prompt_label"]):
        raise ValueError("input family/prompt claim differs from native event log")

    measured: dict[str, tuple[float, int]] = {}
    for marker in ("music_output", "effect_output"):
        details = event_details[marker]
        actual = _wav_metric(paths["audio"], details.get("window_seconds"))
        measured[marker] = actual
        if (not isinstance(details.get("rms"), (int, float)) or
                abs(float(details["rms"]) - actual[0]) > 0.1 or
                details.get("peak") != actual[1]):
            raise ValueError(f"audio metric mismatch: {marker}")
    if measured["music_output"][0] <= 0 or measured["effect_output"][0] <= (
            measured["music_output"][0] * 1.5):
        raise ValueError("audio metric does not distinguish music and effect windows")

    return {"valid": True, "platform": build["platform"], "build_id": build_id,
            "run_id": run_id, "event_count": len(events),
            "input_family": derived["input_family"]}


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser()
    subparsers = parser.add_subparsers(dest="command", required=True)
    validate = subparsers.add_parser("validate")
    validate.add_argument("manifest", type=Path)
    validate.add_argument("--evidence-root", type=Path, required=True)
    validate.add_argument("--binary", type=Path, required=True)
    validate.add_argument("--source-root", type=Path, required=True)
    validate.add_argument("--rt64-receipt", type=Path, required=True)
    validate.add_argument("--platform", choices=("linux-x86_64", "windows-x86_64"),
                          default="linux-x86_64")
    args = parser.parse_args(argv)
    try:
        value = json.loads(args.manifest.read_text(encoding="utf-8"))
        source_commit = subprocess.check_output(
            ["git", "-C", str(args.source_root), "rev-parse", "HEAD"], text=True).strip()
        result = validate_evidence(value, evidence_root=args.evidence_root,
                                   binary=args.binary, source_commit=source_commit,
                                   rt64_receipt=args.rt64_receipt,
                                   expected_platform=args.platform)
    except (OSError, subprocess.SubprocessError, json.JSONDecodeError, ValueError) as error:
        print(json.dumps({"valid": False, "error": str(error)}, sort_keys=True), file=sys.stderr)
        return 2
    print(json.dumps(result, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
