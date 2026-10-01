"""Compare opt-in NTPE canonical traces and report the first changed guest field."""

from __future__ import annotations

import argparse
from dataclasses import asdict, dataclass
from itertools import zip_longest
import json
from pathlib import Path
import re
import sys
from typing import Iterator


TRACE_HEADER = "tetrisphere-guest-trace-v1"
FRAME_HEADER = "tetrisphere-canonical-v1"
FRAME_END = "end-canonical-frame"
FIELD = re.compile(r"([A-Za-z0-9_.\[\]]+)=([0-9a-f]{16})\Z")


@dataclass(frozen=True)
class Difference:
    frame: int
    field: str
    expected: int | str | None
    actual: int | str | None


def read_trace(path: Path) -> Iterator[tuple[tuple[str, int], ...]]:
    """Read complete frames, rejecting partial or ambiguous evidence."""
    with path.open("r", encoding="ascii", newline=None) as source:
        if source.readline().rstrip("\r\n") != TRACE_HEADER:
            raise ValueError(f"{path}: missing {TRACE_HEADER}")
        frame_index = 0
        while (line := source.readline()) != "":
            if line.rstrip("\r\n") != FRAME_HEADER:
                raise ValueError(f"{path}: frame {frame_index}: missing {FRAME_HEADER}")
            fields: list[tuple[str, int]] = []
            seen: set[str] = set()
            while True:
                line = source.readline()
                if line == "":
                    raise ValueError(f"{path}: frame {frame_index}: missing {FRAME_END}")
                line = line.rstrip("\r\n")
                if line == FRAME_END:
                    break
                match = FIELD.fullmatch(line)
                if match is None:
                    raise ValueError(f"{path}: frame {frame_index}: invalid field {line!r}")
                name, encoded = match.groups()
                if name in seen:
                    raise ValueError(f"{path}: frame {frame_index}: duplicate field {name}")
                seen.add(name)
                fields.append((name, int(encoded, 16)))
            if not fields or fields[0][0] != "tick" or fields[0][1] != frame_index:
                raise ValueError(f"{path}: frame {frame_index}: invalid tick")
            yield tuple(fields)
            frame_index += 1


def compare_trace_files(expected: Path, actual: Path) -> Difference | None:
    missing = object()
    for frame_index, (left, right) in enumerate(zip_longest(
            read_trace(expected), read_trace(actual), fillvalue=missing)):
        if left is missing or right is missing:
            return Difference(frame_index, "frame.count",
                              None if left is missing else frame_index + 1,
                              None if right is missing else frame_index + 1)
        for field_index, (left_field, right_field) in enumerate(zip_longest(
                left, right, fillvalue=missing)):
            if left_field is missing or right_field is missing:
                return Difference(frame_index, "field.count", len(left), len(right))
            if left_field[0] != right_field[0]:
                return Difference(frame_index, f"field.order[{field_index}]",
                                  left_field[0], right_field[0])
            if left_field[1] != right_field[1]:
                return Difference(frame_index, left_field[0],
                                  left_field[1], right_field[1])
    return None


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("expected", type=Path)
    parser.add_argument("actual", type=Path)
    args = parser.parse_args()
    try:
        difference = compare_trace_files(args.expected, args.actual)
    except (OSError, UnicodeError, ValueError) as exc:
        print(str(exc), file=sys.stderr)
        return 2
    if difference is None:
        print(json.dumps({"equal": True}, sort_keys=True))
        return 0
    print(json.dumps({"equal": False, **asdict(difference)}, sort_keys=True))
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
