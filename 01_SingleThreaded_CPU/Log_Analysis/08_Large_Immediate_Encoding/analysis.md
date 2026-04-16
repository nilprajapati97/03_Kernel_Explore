# Analysis — Scenario 08: Large Immediate Encoding

## Overview

Three different large constants are constructed using the shift trick.
This analysis breaks down why the limitation exists, how each constant
is built, and what the cost is compared to a hypothetical 32-bit load.

---

## Why 12 Bits?

The 32-bit instruction word is divided as:

```
[31:24] opcode (8b) | [23:20] Rd (4b) | [19:16] Rs1 (4b) | [15:12] Rs2 (4b) | [11:0] Imm12 (12b)
```

With 8 bits for opcode and 12 bits for registers, only 12 bits remain for
the immediate.  Signed 12-bit covers `[-2048, +2047]`.  Any absolute address
like `0x00080000 = 524,288` is far outside this range.

---

## Example 1: Building `0x00080000`

```
0x00080000 = 128 × 4096 = 0x80 × 2^12

Step 1: LOAD_IMM R4, #128   → R4 = 0x00000080  (128 fits: 128 < 2047 ✓)
Step 2: LOAD_IMM R7, #12    → R7 = 12
Step 3: SHL R4, R4, R7      → R4 = 0x00000080 << 12 = 0x00080000 ✓
```

**Instruction cost:** 3 instructions instead of 1.

---

## Example 2: Building `0x00100000`

```
0x00100000 = 256 × 4096 = 0x100 × 2^12

Step 1: LOAD_IMM R1, #256   → R1 = 0x00000100  (256 < 2047 ✓)
Step 2: SHL R1, R1, R7      → R1 = 0x00000100 << 12 = 0x00100000 ✓
          (R7 = 12 already set from Example 1)
```

**Instruction cost:** 2 instructions (R7 reused).

---

## Example 3: Building `0x0000FFFF`

```
0x0000FFFF has bits in two 8-bit groups: 0xFF00 | 0x00FF

Step 1: LOAD_IMM R2, #0xFF  → R2 = 0x000000FF
Step 2: LOAD_IMM R8, #8     → R8 = 8
Step 3: SHL R2, R2, R8      → R2 = 0x0000FF00
Step 4: LOAD_IMM R9, #0xFF  → R9 = 0x000000FF
Step 5: OR  R2, R2, R9      → R2 = 0x0000FF00 | 0x000000FF = 0x0000FFFF ✓
```

**Instruction cost:** 5 instructions instead of 1.

---

## Cost vs. Hypothetical 32-bit Load

| Constant | Actual instructions | If LOAD32 existed |
|----------|--------------------|--------------------|
| 0x00080000 | 3 | 1 |
| 0x00100000 | 2 (R7 reused) | 1 |
| 0x0000FFFF | 5 | 1 |

### ARM Thumb-2 Comparison

ARM Thumb-2 solves this with the `MOVW` / `MOVT` pair:
- `MOVW Rd, #imm16` — loads 16-bit immediate into lower half of Rd
- `MOVT Rd, #imm16` — loads 16-bit immediate into upper half of Rd

Together, they provide a 2-instruction 32-bit load, always costing exactly 2
cycles regardless of the constant value.

---

## Common Mistakes When Building Large Constants

| Mistake | Symptom |
|---------|---------|
| Wrong shift amount | Address points to wrong segment |
| Using signed shift on 2's complement negative intermediate | Sign-extension corrupts upper bits |
| Forgetting to zero R4 before OR-ing | Previous R4 bits bleed into result |
| Shift amount > 31 | Undefined in C; masked to 5 bits by SHR/SHL implementation |

---

## Key Observations

| Observation | Significance |
|-------------|-------------|
| R7 = 12 shared across examples 1 and 2 | Constants with same shift amount share setup overhead |
| SHL updates flags | Z=0 for any non-zero result — these flag updates are side effects |
| 0x0000FFFF required 5 instructions | Non-power-of-two constants need more steps; OR/AND needed for bit merging |
| None of these use STORE | Pure register arithmetic — demonstrates that large constants live only in registers |

---

## Conclusion

The 12-bit immediate limitation is a direct consequence of the fixed 32-bit
instruction encoding.  It forces programmers to decompose large constants into
multiple shift-and-or steps.  While this is workable, it is costly: 2–5
instructions per constant, with each instruction updating flags as a side
effect.  Real ISAs (ARM, RISC-V) provide dedicated wide-immediate instructions
(`MOVN`/`MOVZ`, `LUI`/`ADDI`) to address this.
