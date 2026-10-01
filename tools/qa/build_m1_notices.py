#!/usr/bin/env python3
"""Build a deterministic full-text license bundle from pinned checkouts."""
from __future__ import annotations

import argparse
from pathlib import Path
from typing import Mapping


def build_notice_bundle(dependencies: Mapping[str, Path]) -> str:
    sections = [
        "M1_LICENSE_BUNDLE_COMPLETE=1",
        "# Tetrisphere M1 third-party license bundle",
        "",
        "Generated from the pinned private dependency checkouts. Paths below are",
        "component-relative and contain no workstation information.",
    ]
    count = 0
    for name, root_value in sorted(dependencies.items()):
        root = Path(root_value).resolve()
        if not root.is_dir():
            raise ValueError(f"dependency checkout is missing: {name}")
        candidates = {
            path for pattern in ("LICENSE*", "COPYING*")
            for path in root.rglob(pattern)
            if path.is_file() and ".git" not in path.relative_to(root).parts
        }
        for path in sorted(candidates, key=lambda value: value.relative_to(root).as_posix()):
            try:
                license_text = path.read_text(encoding="utf-8-sig")
            except UnicodeDecodeError:
                continue
            relative = path.relative_to(root).as_posix()
            sections.extend(("", f"## {name}/{relative}", "", license_text.rstrip()))
            count += 1
    if count == 0:
        raise ValueError("no complete license texts were found")
    return "\n".join(sections) + "\n"


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--dependency", action="append", required=True,
                        help="NAME=CHECKOUT (repeat for each pinned dependency)")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args(argv)
    dependencies: dict[str, Path] = {}
    for value in args.dependency:
        name, separator, raw_path = value.partition("=")
        if not separator or not name or name in dependencies:
            parser.error("--dependency must be a unique NAME=CHECKOUT")
        dependencies[name] = Path(raw_path)
    try:
        payload = build_notice_bundle(dependencies)
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(payload, encoding="utf-8", newline="\n")
    except (OSError, ValueError) as error:
        parser.error(str(error))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
