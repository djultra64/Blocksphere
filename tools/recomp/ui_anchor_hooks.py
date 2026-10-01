"""Validate and generate semantic UI producer hooks for NTPE rev0.

The committed manifest contains only call addresses and semantic labels. The
source ROM is checked before hooks can be emitted; no ROM bytes enter Git.
"""

from __future__ import annotations

import json
import struct
from typing import Any


MAIN_START = 0x80075CD8
MAIN_END = 0x80078514
IMAGE_START = 0x80025C50
UI_TARGETS = {0x8003396C, 0x80071DE8, 0x8007240C, 0x80038478}
ANCHORS = {"native": 0, "left": 1, "right": 2, "center": 3, "pause_text": 7}
OPTIONAL_GATES = {"player_count_one", "single_player_not_mode_three"}
HEART_START, HEART_END = 0x80078BA8, 0x80078D60
HEART_TARGET = 0x8003396C
MODEL_START, MODEL_END = 0x800C3CEC, 0x800C3DE4
MODEL_TARGET = 0x8002D93C
# Only reviewed helper JALs may enclose an entire callee's display-list output.
# Extend this table as further producer callsites are identified in NTPE rev0.
# Entries are (containing function, ROM JAL target, anchor, runtime gate).
HELPER_CALLS = {
    # Pause labels and goal values share the frame; exclude frame and robot calls.
    0x800408BC: ("func_8003FE3C", 0x80038478, "pause_text", "always"),
    0x80040954: ("func_8003FE3C", 0x80038478, "pause_text", "always"),
    0x800409F0: ("func_8003FE3C", 0x80038478, "pause_text", "always"),
    0x80040A84: ("func_8003FE3C", 0x80038478, "pause_text", "always"),

    0x80076ED8: ("func_80075CD8", 0x80078DA0, "right", "single_player_not_mode_three"),
    0x80076F64: ("func_80075CD8", 0x80078514, "right", "single_player_not_mode_three"),
    0x80077470: ("func_80075CD8", 0x8007570C, "right", "player_count_one"),
    0x800774E0: ("func_80075CD8", 0x8007570C, "right", "player_count_one"),
    0x800CF8A8: ("func_800CD3B8", 0x800A5BB4, "left", "single_player_not_mode_three"),
    # Enclose the combo number and its surrounding animated graphics together.
    0x800D003C: ("func_800CD3B8", 0x800699B4, "right", "single_player_not_mode_three"),
}


class UiAnchorManifestError(ValueError):
    """Manifest does not match the supported ROM or hook contract."""


def _jal_target(image: bytes, pc: int) -> int | None:
    index = pc - IMAGE_START
    if index < 0 or index + 4 > len(image):
        return None
    instruction = struct.unpack_from(">I", image, index)[0]
    if instruction >> 26 != 3:
        return None
    return ((pc + 4) & 0xF0000000) | ((instruction & 0x03FFFFFF) << 2)


def validate_ui_anchor_manifest(manifest: dict[str, Any], image: bytes) -> None:
    if (manifest.get("schema_version") != 1 or
            manifest.get("rom_revision") != "NTPE rev0" or
            manifest.get("function") != "func_80075CD8"):
        raise UiAnchorManifestError("unsupported UI anchor manifest identity")
    calls = manifest.get("calls")
    if not isinstance(calls, list) or len(calls) != manifest.get("expected_direct_calls"):
        raise UiAnchorManifestError("UI call count differs from the reviewed manifest")
    reviewed: set[int] = set()
    for call in calls:
        try:
            pc, expected = int(call["pc"], 16), int(call["target"], 16)
            anchor = call["anchor"]
        except (KeyError, TypeError, ValueError) as error:
            raise UiAnchorManifestError("malformed UI callsite") from error
        if (pc in reviewed or pc < MAIN_START or pc >= MAIN_END or pc % 4 or
                expected not in UI_TARGETS or anchor not in ANCHORS):
            raise UiAnchorManifestError(f"invalid UI callsite 0x{pc:08X}")
        when = call.get("when")
        if when is not None and (when not in OPTIONAL_GATES or anchor == "native"):
            raise UiAnchorManifestError(f"invalid UI gate at 0x{pc:08X}")
        reviewed.add(pc)
        actual = _jal_target(image, pc)
        if actual != expected:
            raise UiAnchorManifestError(
                f"UI callsite 0x{pc:08X} targets {actual!r}, expected 0x{expected:08X}")
    discovered = {
        pc for pc in range(MAIN_START, MAIN_END, 4)
        if _jal_target(image, pc) in UI_TARGETS
    }
    if discovered != reviewed:
        missing = sorted(discovered - reviewed)
        extra = sorted(reviewed - discovered)
        raise UiAnchorManifestError(
            f"UI callsite coverage differs: missing={missing}, extra={extra}")
    if "extra_calls" in manifest:
        extra = manifest["extra_calls"]
        if not isinstance(extra, list) or len(extra) != 3:
            raise UiAnchorManifestError("heart/skull call count differs")
        extra_reviewed: set[int] = set()
        for call in extra:
            try:
                pc, target = int(call["pc"], 16), int(call["target"], 16)
            except (KeyError, TypeError, ValueError) as error:
                raise UiAnchorManifestError("malformed heart/skull callsite") from error
            if (pc in extra_reviewed or pc < HEART_START or pc >= HEART_END or
                    pc % 4 or target != HEART_TARGET or
                    call.get("anchor") != "left" or
                    call.get("when") != "single_player_not_mode_three" or
                    _jal_target(image, pc) != HEART_TARGET):
                raise UiAnchorManifestError(f"invalid heart/skull callsite 0x{pc:08X}")
            extra_reviewed.add(pc)
        discovered_extra = {
            pc for pc in range(HEART_START, HEART_END, 4)
            if _jal_target(image, pc) == HEART_TARGET
        }
        if discovered_extra != extra_reviewed:
            raise UiAnchorManifestError("heart/skull callsite coverage differs")
    if "model_calls" in manifest:
        model = manifest["model_calls"]
        if not isinstance(model, list) or len(model) != 1:
            raise UiAnchorManifestError("heart-loss model call count differs")
        call = model[0]
        try:
            pc, target = int(call["pc"], 16), int(call["target"], 16)
        except (KeyError, TypeError, ValueError) as error:
            raise UiAnchorManifestError("malformed heart-loss model callsite") from error
        discovered_model = {
            address for address in range(MODEL_START, MODEL_END, 4)
            if _jal_target(image, address) == MODEL_TARGET
        }
        if (pc not in discovered_model or len(discovered_model) != 1 or
                target != MODEL_TARGET or call.get("anchor") != "left" or
                call.get("when") != "single_player_not_mode_three"):
            raise UiAnchorManifestError(f"invalid heart-loss model callsite 0x{pc:08X}")
    if "helper_calls" in manifest:
        helpers = manifest["helper_calls"]
        if not isinstance(helpers, list):
            raise UiAnchorManifestError("helper calls must be a list")
        helper_reviewed: set[int] = set()
        for call in helpers:
            try:
                pc, target = int(call["pc"], 16), int(call["target"], 16)
            except (KeyError, TypeError, ValueError) as error:
                raise UiAnchorManifestError("malformed helper callsite") from error
            approved = HELPER_CALLS.get(pc)
            if (pc in helper_reviewed or pc in reviewed or approved is None or
                    (call.get("function"), target, call.get("anchor"),
                     call.get("when")) != approved or
                    _jal_target(image, pc) != target):
                raise UiAnchorManifestError(f"invalid helper callsite 0x{pc:08X}")
            helper_reviewed.add(pc)


def render_ui_anchor_hooks(manifest: dict[str, Any]) -> str:
    """Append TOML hooks only for calls whose placement is known."""
    lines: list[str] = []
    for call, function in (
        [(call, "func_80075CD8") for call in manifest["calls"]] +
        [(call, "func_80078BA8") for call in manifest.get("extra_calls", [])] +
        [(call, "func_800C3CEC") for call in manifest.get("model_calls", [])] +
        [(call, call["function"]) for call in manifest.get("helper_calls", [])]
    ):
        anchor = ANCHORS[call["anchor"]]
        if anchor == 0:
            continue
        pc = int(call["pc"], 16)
        begin = f"tetrisphere_ui_span_begin(rdram, ctx, 0x{pc:08X}u, {anchor}u);"
        if call.get("when") == "player_count_one":
            begin = f"if (MEM_H(0, S32(0x8015D980u)) == 1) {begin}"
        elif call.get("when") == "single_player_not_mode_three":
            begin = ("if (MEM_H(0, S32(0x8015D980u)) == 1 && "
                     "MEM_H(0, S32(0x800E44A8u)) != 3) " + begin)
        for address, text in (
            (pc, begin),
            (pc + 8, f"tetrisphere_ui_span_end(rdram, ctx, 0x{pc:08X}u);"),
        ):
            lines.extend(("", "[[patches.hook]]", f'func = "{function}"',
                          f"before_vram = 0x{address:08X}",
                          f"text = {json.dumps(text)}"))
    return "\n".join(lines) + ("\n" if lines else "")
