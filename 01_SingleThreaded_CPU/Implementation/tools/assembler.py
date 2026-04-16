#!/usr/bin/env python3
"""
assembler.py — Assembler for the custom single-threaded CPU simulator ISA.

Usage:
    python3 assembler.py <input.asm> <output.bin>

Produces a binary file of 32-bit little-endian instruction words that can be
loaded directly into the simulator with:
    ./cpu_sim output.bin [--trace] [--dump-mem 0x00080000 40]

Instruction encoding (32-bit word):
    [31:24] opcode (8b) | [23:20] Rd (4b) | [19:16] Rs1 (4b)
    [15:12] Rs2 (4b)    | [11:0]  Imm12 (12b, sign-extended)

Supported syntax:
    MNEMONIC  [operands,...]   ; comment
    LABEL:                     ; define a label (resolved to absolute address)
    ; full-line comment
    # full-line comment

Operands:
    Rn          — register (R0–R15)
    #N or N     — immediate (decimal or 0x hex; must fit in -2048..2047)
    [Rn + #N]   — memory reference (LOAD/STORE)
    [Rn]        — shorthand for [Rn + #0]
    LABEL       — resolved to absolute address (for jumps/calls)

Constants (defined with =):
    NAME = VALUE

MEM_PROG_BASE = 0x00010000  (where the binary is loaded in the CPU simulator)
"""

import sys
import re
import struct

# ── Constants ────────────────────────────────────────────────────────────────
MEM_PROG_BASE = 0x00010000

OPCODE_TABLE = {
    "NOP":       0x00,
    "LOAD_IMM":  0x01,
    "LOAD":      0x02,
    "STORE":     0x03,
    "ADD":       0x04,
    "SUB":       0x05,
    "MUL":       0x06,
    "DIV":       0x07,
    "AND":       0x08,
    "OR":        0x09,
    "XOR":       0x0A,
    "NOT":       0x0B,
    "SHL":       0x0C,
    "SHR":       0x0D,
    "MOV":       0x0E,
    "CMP":       0x0F,
    "JMP":       0x10,
    "JEQ":       0x11,
    "JNE":       0x12,
    "JGT":       0x13,
    "JLT":       0x14,
    "CALL":      0x15,
    "RET":       0x16,
    "PUSH":      0x17,
    "POP":       0x18,
    "HALT":      0xFF,
}


# ── Helpers ──────────────────────────────────────────────────────────────────

def parse_int(s: str) -> int:
    """Parse decimal, 0x hex, or 0b binary integer. Strips leading '#'."""
    s = s.strip().lstrip('#')
    if s.startswith('0x') or s.startswith('0X'):
        return int(s, 16)
    elif s.startswith('0b') or s.startswith('0B'):
        return int(s, 2)
    else:
        return int(s, 10)


def parse_reg(s: str) -> int:
    """Parse 'Rn' and return register index 0–15."""
    s = s.strip()
    if not re.fullmatch(r'[Rr]\d{1,2}', s):
        raise ValueError(f"Invalid register: '{s}'")
    n = int(s[1:])
    if n > 15:
        raise ValueError(f"Register index out of range: R{n}")
    return n


def parse_mem_ref(s: str) -> tuple[int, int]:
    """
    Parse '[Rn]' or '[Rn + #offset]' or '[Rn + offset]' or '[Rn - #offset]'.
    Returns (reg_index, offset_int).
    """
    s = s.strip()
    if not (s.startswith('[') and s.endswith(']')):
        raise ValueError(f"Expected memory reference '[Rn ± #N]', got: '{s}'")
    inner = s[1:-1].strip()

    # [Rn + #imm] or [Rn - #imm]
    mem_re = re.fullmatch(
        r'([Rr]\d{1,2})\s*([+\-])\s*#?(\w+)',
        inner
    )
    if mem_re:
        base   = parse_reg(mem_re.group(1))
        sign   = -1 if mem_re.group(2) == '-' else 1
        offset = sign * parse_int(mem_re.group(3))
        return base, offset

    # [Rn]
    if re.fullmatch(r'[Rr]\d{1,2}', inner):
        return parse_reg(inner), 0

    raise ValueError(f"Cannot parse memory reference: '{s}'")


def encode_imm12(val: int, lineno: int, label: str = '') -> int:
    """Clamp and encode val as a 12-bit two's-complement immediate."""
    if val < -2048 or val > 2047:
        raise ValueError(
            f"Line {lineno}: immediate {val} ('{label}') does not fit in "
            f"signed 12-bit range [-2048..2047]"
        )
    return val & 0xFFF


def pack_instr(opcode: int, rd: int = 0, rs1: int = 0,
               rs2: int = 0, imm12: int = 0) -> int:
    return (
        ((opcode & 0xFF) << 24) |
        ((rd     & 0x0F) << 20) |
        ((rs1    & 0x0F) << 16) |
        ((rs2    & 0x0F) << 12) |
        (imm12  & 0x0FFF)
    )


# ── First pass: collect labels and constants ──────────────────────────────────

def first_pass(lines: list[str]) -> tuple[dict, dict, list]:
    """
    Walk through lines, numbering real instruction lines.
    Returns:
        labels    — {name: absolute_address}
        constants — {name: int_value}
        cleaned   — list of (lineno, mnemonic_or_none, raw_body) for pass 2
    """
    labels    = {}
    constants = {}
    cleaned   = []          # (original_line_no, text_without_comment)
    addr      = MEM_PROG_BASE

    for lineno, raw in enumerate(lines, start=1):
        # Strip inline comment (semicolon only; '#' is also a full-line comment
        # marker but must NOT be stripped inline because '#N' is an immediate).
        text = raw.split(';', 1)[0].strip()
        if not text or text.startswith('#'):
            continue

        # Constant definition:  NAME = VALUE
        const_match = re.fullmatch(r'(\w+)\s*=\s*(.+)', text)
        if const_match:
            cname = const_match.group(1).upper()
            constants[cname] = parse_int(const_match.group(2).strip())
            continue

        # Label definition:  LABEL:
        # A line may be *only* a label, or a label followed by an instruction
        label_match = re.match(r'^(\w+):\s*(.*)', text)
        if label_match:
            lname = label_match.group(1).upper()
            labels[lname] = addr
            rest = label_match.group(2).strip()
            if not rest:
                continue                     # label-only line
            text = rest                      # instruction follows the label

        cleaned.append((lineno, text))
        addr += 4                            # one 32-bit word per instruction

    return labels, constants, cleaned


# ── Second pass: assemble instructions ───────────────────────────────────────

def assemble_line(lineno: int, text: str,
                  labels: dict, constants: dict,
                  cur_addr: int = MEM_PROG_BASE) -> int:
    """
    Assemble a single instruction text line into a 32-bit word.
    `labels` maps uppercase names → absolute addresses.
    `constants` maps uppercase names → integer values.
    `cur_addr` is the absolute address of this instruction (used to compute
    PC-relative branch offsets: imm = target − (cur_addr + 4)).
    """
    # Tokenise: split on first whitespace, then comma-split remaining
    parts = text.split(None, 1)
    mnemonic = parts[0].upper()
    body     = parts[1].strip() if len(parts) > 1 else ''

    if mnemonic not in OPCODE_TABLE:
        raise ValueError(f"Line {lineno}: Unknown mnemonic '{mnemonic}'")

    opcode = OPCODE_TABLE[mnemonic]

    # Helper: resolve a token that might be a label, constant, or literal int
    def resolve(tok: str) -> int:
        tok = tok.strip()
        upper = tok.lstrip('#').upper()
        if upper in labels:
            return labels[upper]
        if upper in constants:
            return constants[upper]
        return parse_int(tok)

    # ── No-operand instructions ───────────────────────────────────────────
    if mnemonic in ('NOP', 'HALT', 'RET'):
        return pack_instr(opcode)

    # ── LOAD_IMM  Rd, #imm ────────────────────────────────────────────────
    if mnemonic == 'LOAD_IMM':
        ops = [o.strip() for o in body.split(',', 1)]
        rd  = parse_reg(ops[0])
        imm = resolve(ops[1])
        return pack_instr(opcode, rd=rd, imm12=encode_imm12(imm, lineno))

    # ── LOAD  Rd, [Rs1 + #imm] ────────────────────────────────────────────
    if mnemonic == 'LOAD':
        ops = [o.strip() for o in body.split(',', 1)]
        rd       = parse_reg(ops[0])
        rs1, imm = parse_mem_ref(ops[1])
        return pack_instr(opcode, rd=rd, rs1=rs1,
                          imm12=encode_imm12(imm, lineno))

    # ── STORE  [Rs1 + #imm], Rs2 ─────────────────────────────────────────
    if mnemonic == 'STORE':
        # Find the closing ']' to split mem-ref from register
        bracket_end = body.index(']')
        mem_part = body[:bracket_end + 1].strip()
        reg_part = body[bracket_end + 1:].lstrip(',').strip()
        rs1, imm = parse_mem_ref(mem_part)
        rs2      = parse_reg(reg_part)
        return pack_instr(opcode, rs1=rs1, rs2=rs2,
                          imm12=encode_imm12(imm, lineno))

    # ── Two-register ALU: ADD Rd, Rs1, Rs2 ───────────────────────────────
    if mnemonic in ('ADD', 'SUB', 'MUL', 'DIV',
                    'AND', 'OR', 'XOR', 'SHL', 'SHR'):
        ops = [o.strip() for o in body.split(',')]
        if len(ops) != 3:
            raise ValueError(
                f"Line {lineno}: {mnemonic} expects 3 operands, got {len(ops)}"
            )
        return pack_instr(opcode,
                          rd=parse_reg(ops[0]),
                          rs1=parse_reg(ops[1]),
                          rs2=parse_reg(ops[2]))

    # ── One-register ALU: NOT Rd, Rs1 ────────────────────────────────────
    if mnemonic == 'NOT':
        ops = [o.strip() for o in body.split(',')]
        return pack_instr(opcode, rd=parse_reg(ops[0]), rs1=parse_reg(ops[1]))

    # ── MOV  Rd, Rs1 ─────────────────────────────────────────────────────
    if mnemonic == 'MOV':
        ops = [o.strip() for o in body.split(',')]
        return pack_instr(opcode,
                          rd=parse_reg(ops[0]),
                          rs1=parse_reg(ops[1]))

    # ── CMP  Rs1, Rs2 ────────────────────────────────────────────────────
    if mnemonic == 'CMP':
        ops = [o.strip() for o in body.split(',')]
        return pack_instr(opcode,
                          rs1=parse_reg(ops[0]),
                          rs2=parse_reg(ops[1]))

    # ── Branch / CALL: PC-relative offset ──────────────────────────────
    # At execute time the CPU has PC = fetch_addr + 4.  So the encoded imm
    # is:  imm = target_absolute - (cur_addr + 4)
    if mnemonic in ('JMP', 'JEQ', 'JNE', 'JGT', 'JLT', 'CALL'):
        target = resolve(body.strip())
        offset = target - (cur_addr + 4)
        label_desc = f"{body.strip()} (offset {offset})"
        return pack_instr(opcode, imm12=encode_imm12(offset, lineno, label_desc))

    # ── PUSH  Rs1 ─────────────────────────────────────────────────────────
    if mnemonic == 'PUSH':
        return pack_instr(opcode, rs1=parse_reg(body.strip()))

    # ── POP  Rd ───────────────────────────────────────────────────────────
    if mnemonic == 'POP':
        return pack_instr(opcode, rd=parse_reg(body.strip()))

    raise ValueError(f"Line {lineno}: Unhandled mnemonic '{mnemonic}'")


def assemble(src_path: str, dst_path: str) -> None:
    with open(src_path, 'r') as f:
        lines = f.readlines()

    print(f"[ASM] Assembling '{src_path}' → '{dst_path}'")

    labels, constants, cleaned = first_pass(lines)

    if labels:
        print("[ASM] Labels:")
        for name, addr in sorted(labels.items(), key=lambda x: x[1]):
            print(f"        {name}: 0x{addr:08X}")

    if constants:
        print("[ASM] Constants:")
        for name, val in sorted(constants.items()):
            print(f"        {name} = {val} (0x{val:X})")

    words = []
    for i, (lineno, text) in enumerate(cleaned):
        cur_addr = MEM_PROG_BASE + i * 4
        try:
            word = assemble_line(lineno, text, labels, constants, cur_addr)
            words.append(word)
        except ValueError as exc:
            print(f"[ASM ERROR] {exc}", file=sys.stderr)
            sys.exit(1)

    with open(dst_path, 'wb') as out:
        for w in words:
            out.write(struct.pack('<I', w))   # little-endian 32-bit

    print(f"[ASM] {len(words)} instructions → {len(words)*4} bytes written to '{dst_path}'")

    # Print listing
    print("\n[ASM] Listing:")
    print(f"  {'Addr':>10}  {'Word':>10}  Disassembly")
    print(f"  {'-'*10}  {'-'*10}  {'-'*36}")
    for i, (w, (lineno, text)) in enumerate(zip(words, cleaned)):
        addr = MEM_PROG_BASE + i * 4
        print(f"  0x{addr:08X}  0x{w:08X}  {text}")


# ── Entry point ───────────────────────────────────────────────────────────────

if __name__ == '__main__':
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} <input.asm> <output.bin>",
              file=sys.stderr)
        sys.exit(1)

    assemble(sys.argv[1], sys.argv[2])
