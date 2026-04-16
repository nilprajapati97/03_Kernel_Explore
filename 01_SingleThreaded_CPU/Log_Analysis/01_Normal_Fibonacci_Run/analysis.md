# Analysis — Scenario 01: Normal Fibonacci Run

## Overview

This is the baseline "golden" execution.  Every stage of the CPU pipeline
behaves correctly and the program terminates cleanly.

---

## Phase 1: Initialisation (Cycles 1–14)

```
Cycle  1-6:  Load constants into R0–R6
Cycle  7-9:  Two-step address construction → R4 = 0x00080000
             LOAD_IMM R4, #128 → R4 = 0x80
             LOAD_IMM R7, #12  → R7 = 12 (shift amount)
             SHL R4, R4, R7    → R4 = 0x80 << 12 = 0x00080000
Cycle 10-11: Pre-store F0 and F1 directly
Cycle 12-13: Advance R4 by 8 → R4 = 0x00080008
Cycle 14:    Reset counter R0 = 8
```

### Why the two-step shift for the address?

The 12-bit immediate field can hold values only in `[-2048, +2047]`.
`0x00080000 = 524288` is far outside that range.  The shift trick decomposes
the constant:

```
0x00080000 = 128 × 4096 = 128 × 2^12
→ LOAD_IMM R4, #128   (fits in 12 bits: 128 < 2047 ✓)
→ SHL R4, R4, R7      (R7 = 12)
```

This pattern is the canonical workaround for the Imm12 limitation.

---

## Phase 2: Loop Body (Cycles 15–70)

Each of the 7 loop iterations executes exactly 8 instructions:

| # | Instruction | Purpose |
|---|-------------|---------|
| 1 | `ADD R3, R1, R2` | Compute next Fibonacci: F(n) = F(n-1) + F(n-2) |
| 2 | `STORE [R4+0], R3` | Write result to memory |
| 3 | `MOV R1, R2` | Slide the window: R1 ← R2 |
| 4 | `MOV R2, R3` | Slide the window: R2 ← R3 |
| 5 | `ADD R4, R4, R5` | Advance memory pointer (+4 bytes) |
| 6 | `SUB R0, R0, R6` | Decrement counter |
| 7 | `CMP R0, R6` | Compare counter with 1 |
| 8 | `JGT LOOP` | Jump if counter > 1 |

### Flag analysis at loop exit (Cycle 68–70)

```
Cycle 68: SUB R0(=2), R6(=1)  → R0 = 1;  Z=1, N=0, C=1, O=0
Cycle 69: CMP R0(=1), R6(=1)  → 1-1=0;   Z=1, N=0, C=0, O=0
Cycle 70: JGT checks !Z && !N → !1 && !0 = 0 && 1 = FALSE
          → Branch NOT taken → fall through
```

The loop exits correctly after exactly 7 iterations.

---

## Phase 3: Final Two Instructions (Cycles 71–72)

After the loop falls through, the last Fibonacci pair (F8=21, F9=34) is
handled by two explicit instructions outside the loop:

```
Cycle 71: ADD R3, R1(=13), R2(=21) → R3 = 34 (F9)
Cycle 72: STORE [R4+0], R3         → MEM[0x00080024] = 34
```

---

## Memory Dump Breakdown

```
0x00080000:  00 00 00 00  → 0   = F0
0x00080004:  01 00 00 00  → 1   = F1
0x00080008:  01 00 00 00  → 1   = F2
0x0008000C:  02 00 00 00  → 2   = F3
0x00080010:  03 00 00 00  → 3   = F4
0x00080014:  05 00 00 00  → 5   = F5
0x00080018:  08 00 00 00  → 8   = F6
0x0008001C:  0d 00 00 00  → 13  = F7
0x00080020:  15 00 00 00  → 21  = F8
0x00080024:  22 00 00 00  → 34  = F9
```

All values confirmed correct.  Little-endian byte order: `22 00 00 00` reads
as `0x00000022 = 34` in decimal.

---

## Key Observations

| Observation | Significance |
|-------------|-------------|
| 73 total cycles | Non-pipelined: 24 instructions + 49 loop-repeat + overhead |
| JGT taken 7 times, skipped once | Correct loop exit condition |
| SP unchanged at `0x000FFFFC` | No PUSH/POP used — stack untouched |
| Flags reset between phases | CMP flags are stale after non-flag-setting instructions — trace shows flags preserved between MOV/STORE/JMP |
| Zero flag set after LOAD_IMM R4, #0 (cycle 6) | Immediately overwritten by cycle 7 |

---

## Conclusion

The program is correct.  All 10 Fibonacci numbers are present and accurate in
memory.  The single-threaded, non-pipelined CPU executes each instruction
completely before starting the next — the trace shows a perfectly sequential
stream with no out-of-order or parallel operations.
