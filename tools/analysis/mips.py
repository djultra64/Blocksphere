"""Small MIPS III control-flow decoder, not a code/data classifier."""

from dataclasses import dataclass


def signed16(value: int) -> int:
    value &= 0xFFFF
    return value - 0x10000 if value & 0x8000 else value


@dataclass(frozen=True)
class Flow:
    kind: str
    target: int | None
    delay_slot: int
    register: int | None = None
    annul_not_taken: bool = False


def decode_flow(word: int, pc: int) -> Flow | None:
    """Decode delayed transfers, including REGIMM and coprocessor branches."""
    op, rs, rt = word >> 26, (word >> 21) & 31, (word >> 16) & 31
    next_pc = (pc + 4) & 0xFFFFFFFF
    if op in (2, 3):
        return Flow("call" if op == 3 else "jump",
                    (next_pc & 0xF0000000) | ((word & 0x3FFFFFF) << 2), next_pc)
    if op == 0 and word & 63 in (8, 9):
        kind = "return" if (word & 63) == 8 and rs == 31 else "indirect_jump"
        if (word & 63) == 9 and (word >> 11) & 31:
            kind = "indirect_call"
        return Flow(kind, None, next_pc, rs)
    branch = op in (4, 5, 6, 7, 20, 21, 22, 23)
    regimm = op == 1 and rt in (0, 1, 2, 3, 16, 17, 18, 19)
    copbranch = op in (16, 17, 18) and rs == 8
    if branch or regimm or copbranch:
        return Flow("conditional_call" if regimm and rt >= 16 else "branch",
                    (next_pc + signed16(word) * 4) & 0xFFFFFFFF, next_pc,
                    annul_not_taken=op in (20, 21, 22, 23) or
                    (regimm and rt in (2, 3, 18, 19)) or (copbranch and bool(rt & 2)))
    return None
