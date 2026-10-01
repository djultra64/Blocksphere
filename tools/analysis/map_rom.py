"""Deterministic, deliberately provisional static analysis of a normalized N64 ROM.

Only candidate metadata is emitted. A linear instruction sweep can interpret data
as instructions. Neither it nor pointer runs establish executable function bounds.
Private output is never merged automatically with checked-in human rulings.
"""

import argparse
from dataclasses import asdict, dataclass
import json
from pathlib import Path
import struct

from tools.analysis.mips import decode_flow, signed16
from tools.rom.validate import MANIFEST, validate_bytes


ENTRY_POINT = 0x80025C50
SCAN_SIZE = 0x100000


def detected(provenance: str, confidence: str = "low", **fields) -> dict:
    return {"status": "detected", "confidence": confidence,
            "provenance": provenance, **fields}


def validate_sections(sections: list[dict]) -> None:
    """Reject ambiguous static mappings; overlays require separate evidence."""
    for section in sections:
        if section["size"] <= 0 or any(section[key] < 0 for key in ("rom_start", "ram_start")):
            raise ValueError("Sections must have nonnegative addresses and positive size")
        for key in ("rom_start", "ram_start"):
            if section[key] + section["size"] > 0x100000000:
                raise ValueError("Section address wraps")
    for key in ("rom_start", "ram_start"):
        ordered = sorted(sections, key=lambda section: section[key])
        for left, right in zip(ordered, ordered[1:]):
            if left[key] + left["size"] > right[key]:
                raise ValueError(f"Overlapping sections in {key}")


@dataclass
class AnalysisResult:
    schema_version: int
    entry_point: int
    rom_size: int
    detections: dict
    human_rulings: list
    open_questions: list

    def to_json(self) -> str:
        return json.dumps(asdict(self), sort_keys=True, indent=2) + "\n"


def _constants(word: int, regs: dict[int, int]) -> None:
    """Single basic-block constants; unknown writes kill their destination."""
    op, rs, rt, rd = word >> 26, (word >> 21) & 31, (word >> 16) & 31, (word >> 11) & 31
    value = None
    dest = rt
    if op == 15:
        value = (word & 0xFFFF) << 16
    elif op in (8, 9, 13) and rs in regs:
        value = (regs[rs] | (word & 0xFFFF)) if op == 13 else regs[rs] + signed16(word)
    elif op == 0:
        dest = rd
        funct = word & 63
        if funct in (33, 37) and rs in regs and rt in regs:
            value = regs[rs] + regs[rt] if funct == 33 else regs[rs] | regs[rt]
        elif funct == 0 and rt in regs:
            value = regs[rt] << ((word >> 6) & 31)
        elif funct in (8, 12, 13, 17, 19, 24, 25, 26, 27):
            dest = 0
    elif op in (2, 4, 5, 6, 7, 20, 21, 22, 23, 40, 41, 42, 43, 44, 45, 46, 47, 49, 53, 57, 61, 63):
        dest = 0
    elif op == 3:
        dest = 31
    elif op == 1:
        dest = 31 if rt in (16, 17, 18, 19) else 0
    elif op in (16, 17, 18):
        dest = rt if rs in (0, 1, 2) else 0
    if dest:
        regs.pop(dest, None)
        if value is not None:
            regs[dest] = value & 0xFFFFFFFF
    regs[0] = 0


def _copy_loop(words: list[int], i: int, regs: dict[int, int], pc: int) -> dict | None:
    """Recognize a narrow word-copy loop with two ascending pointers and bound."""
    if i + 5 >= len(words):
        return None
    load, store, src_step, dst_step, branch, slot = words[i:i + 6]
    src, value = (load >> 21) & 31, (load >> 16) & 31
    dst = (store >> 21) & 31
    bound = (branch >> 16) & 31
    if not (load >> 26 == 35 and load & 0xFFFF == 0 and
            store >> 26 == 43 and store & 0xFFFF == 0 and (store >> 16) & 31 == value and
            src_step == (9 << 26 | src << 21 | src << 16 | 4) and
            dst_step == (9 << 26 | dst << 21 | dst << 16 | 4) and
            branch >> 26 == 5 and (branch >> 21) & 31 == src and signed16(branch) == -5 and
            slot == 0 and src in regs and dst in regs and bound in regs):
        return None
    size = regs[bound] - regs[src]
    if size <= 0 or size % 4 or len({src, dst, value, bound}) != 4:
        return None
    return detected("constant pointers and six-instruction ascending word-copy loop",
                    "medium", kind="cpu_copy_loop", address=pc, source_start=regs[src],
                    ram_start=regs[dst], size=size, execution_scope="unproven")


def _zero_loop(words: list[int], i: int, regs: dict[int, int], pc: int) -> dict | None:
    """Recognize count -= 8; zero two words; branch; pointer += 8 in slot."""
    if i + 4 >= len(words):
        return None
    decrement, first, second, branch, slot = words[i:i + 5]
    count, ptr = (decrement >> 16) & 31, (first >> 21) & 31
    if not (decrement >> 26 in (8, 9) and (decrement >> 21) & 31 == count and
            signed16(decrement) == -8 and first == (43 << 26 | ptr << 21) and
            second == first + 4 and branch == (5 << 26 | count << 21 | 0xFFFC) and
            slot >> 26 in (8, 9) and (slot & 0x3FFFFFF) == (ptr << 21 | ptr << 16 | 8) and
            ptr != count and ptr in regs and count in regs):
        return None
    size = regs[count]
    if size == 0 or size % 8 or regs[ptr] + size > 0x100000000:
        return None
    return detected("constant pointer/count and two-word zero loop including delayed increment",
                    "medium", kind="cpu_zero_loop", address=pc, ram_start=regs[ptr], size=size,
                    execution_scope="unproven")


def analyze_rom(data: bytes) -> AnalysisResult:
    """Analyze a normalized image; the scan extent is a hypothesis, not a load map."""
    if len(data) < 0x1004 or len(data) % 4 or data[:4] != bytes.fromhex("80371240"):
        raise ValueError("Expected a word-aligned normalized z64 image with a ROM body")
    entry = struct.unpack_from(">I", data, 8)[0]
    if entry != ENTRY_POINT:
        raise ValueError("Expected NTPE rev0 entry point 0x80025c50; validate revision separately")
    size = min(SCAN_SIZE, len(data) - 0x1000)
    sections = [detected("header entry plus ROM body at 0x1000; 1 MiB scan cap, not a proven load extent",
                         name="initial_linear_scan", rom_start=0x1000, ram_start=entry, size=size)]
    validate_sections(sections)
    words = list(struct.unpack(f">{size // 4}I", data[0x1000:0x1000 + size]))
    findings = {key: [] for key in ("functions", "direct_calls", "branches", "indirect_calls",
                                    "indirect_jumps", "jump_tables", "boot_transfers",
                                    "boot_decompression", "compression", "overlays")}
    findings["sections"] = sections
    flow_items = [(i, decode_flow(word, entry + i * 4)) for i, word in enumerate(words)]
    targets = {flow.target for _, flow in flow_items if flow is not None and flow.target is not None}
    functions = {entry: detected("ROM header entry; bounds require manual inspection",
                                 "medium", address=entry, size=None)}
    regs, pi = {0: 0}, {}
    reset_at = -1
    for i, flow in flow_items:
        pc, word = entry + i * 4, words[i]
        if i == reset_at:
            regs, pi = {0: 0}, {}
        # Preserve the fallthrough initial state of a bounded copy-loop entry;
        # _copy_loop below is explicitly a first-iteration hypothesis.
        copy = _copy_loop(words, i, regs, pc) or _zero_loop(words, i, regs, pc)
        if pc in targets:
            regs, pi = {0: 0}, {}
        if copy is not None:
            findings["boot_transfers"].append(copy)
        if flow is not None:
            item = detected("linear MIPS III decode in provisional mapping; code/data unclassified",
                            address=pc, rom_offset=0x1000 + i * 4, **asdict(flow))
            key = {"call": "direct_calls", "conditional_call": "direct_calls",
                   "indirect_call": "indirect_calls", "indirect_jump": "indirect_jumps"}.get(flow.kind, "branches")
            findings[key].append(item)
            if flow.kind in ("call", "conditional_call") and entry <= flow.target < entry + size:
                functions.setdefault(flow.target, detected("direct call target in provisional mapping",
                                                            address=flow.target, size=None))
            reset_at = i + 2  # The delay slot executes before the transfer.
        op, rs, rt = word >> 26, (word >> 21) & 31, (word >> 16) & 31
        if op == 43 and rs in regs:
            address = (regs[rs] + signed16(word)) & 0x1FFFFFFF
            if address in (0x04600000, 0x04600004, 0x04600008, 0x0460000C):
                pi[address] = regs.get(rt)
                if address == 0x0460000C and all(pi.get(a) is not None for a in
                                                              (0x04600000, 0x04600004, 0x0460000C)):
                    cart = pi[0x04600004] & 0x1FFFFFFF
                    if 0x10000000 <= cart < 0x1FC00000:
                        findings["boot_transfers"].append(detected(
                            "constant PI DRAM_ADDR/CART_ADDR/WR_LEN stores in one linear basic block",
                            "medium", kind="pi_dma_read", address=pc, rom_start=cart - 0x10000000,
                            ram_start=0x80000000 | (pi[0x04600000] & 0xFFFFFF),
                            size=(pi[0x0460000C] & 0xFFFFFF) + 1, execution_scope="unproven"))
                    pi = {}
        _constants(word, regs)
    findings["functions"] = [functions[key] for key in sorted(functions)]
    i = 0
    while i < len(words):
        start = i
        while i < len(words) and entry <= words[i] < entry + size and words[i] % 4 == 0:
            i += 1
        if i - start >= 3:
            findings["jump_tables"].append(detected(
                "three or more aligned pointers into provisional mapping; no dispatch xref required",
                rom_start=0x1000 + start * 4, ram_start=entry + start * 4,
                entry_count=i - start, targets=words[start:i]))
        i = max(i, start + 1)
    for magic, name in ((b"Yay0", "Yay0"), (b"MIO0", "MIO0"), (b"Yaz0", "Yaz0"),
                        (b"RNC\x01", "RNC1"), (b"RNC\x02", "RNC2")):
        offset = data.find(magic)
        while offset >= 0:
            findings["compression"].append(detected("literal signature only; no stream validation",
                                                     rom_offset=offset, format=name, validated_stream=False))
            offset = data.find(magic, offset + 1)
    findings["compression"].sort(key=lambda item: (item["rom_offset"], item["format"]))
    transfers = [t for t in findings["boot_transfers"] if "rom_start" in t]
    for index, left in enumerate(transfers):
        for right in transfers[index + 1:]:
            if (left["rom_start"] != right["rom_start"] and
                    max(left["ram_start"], right["ram_start"]) <
                    min(left["ram_start"] + left["size"], right["ram_start"] + right["size"])):
                findings["overlays"].append(detected(
                    "distinct candidate ROM DMA ranges overlap in RAM; executable use unproven",
                    transfer_sites=[left["address"], right["address"]]))
    return AnalysisResult(1, entry, len(data), findings, [], [
        "Provisional linear mapping and 1 MiB scan cap do not establish load or code boundaries.",
        "Linear decode includes data; call targets and pointer runs are not confirmed functions or jump tables.",
        "Transfer recognition is limited to constant PI stores, one word-copy pattern and one zero-loop pattern.",
        "No IPL3 execution, decompressor recognition, stream validation, relocation recovery or runtime load trace.",
        "Empty decompression/overlay lists mean no supported detection, not proof of absence.",
    ])


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("rom", type=Path)
    parser.add_argument("--output", type=Path, default=Path(".local/m1/analysis/static-map.json"))
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    output = args.output.absolute()
    try:
        # A symlinked worktree .local is intentional; resolve before checking.
        output.resolve().relative_to((root / ".local").resolve())
    except ValueError:
        parser.error("Analysis output must remain under this workspace's private .local directory")
    try:
        if output.resolve() == args.rom.resolve() or (output.exists() and output.samefile(args.rom)):
            parser.error("Output must not refer to the input ROM")
        manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
        with args.rom.open("rb") as source:
            data = source.read(manifest["size_bytes"] + 1)
        identity = validate_bytes(data, manifest)
        if not identity["recognized"]:
            parser.error(f"Unsupported normalized NTPE rev0 ROM: {identity['reason']}")
        if identity["format"] != "z64":
            parser.error("Unsupported normalized NTPE rev0 ROM: not_normalized_z64")
        result = analyze_rom(data)
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(result.to_json(), encoding="utf-8")
    except (OSError, ValueError) as error:
        parser.error(str(error))
    print("Static candidate map written under .local; human rulings remain separate.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
