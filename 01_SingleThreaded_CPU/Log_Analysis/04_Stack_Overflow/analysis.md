# Analysis — Scenario 04: Stack Overflow

## Overview

The program pushes a value onto the stack in an infinite loop.  There is no
corresponding `POP` or `RET`.  The Stack Pointer (`R13 / SP`) decrements by 4
on every iteration and eventually crosses the stack segment boundary,
overwriting first the data segment, then the code segment.

---

## The Stack Mechanism

```
PUSH Rx:
  SP = SP - 4             ← SP decrements first (pre-decrement)
  MEM32[SP] = Rx          ← then write
```

Starting SP = `0x000FFFFC`.  After N push operations:

```
SP = 0x000FFFFC - (N × 4)
```

---

## Segment Boundary Crossings

| Push count | SP value | Segment crossed |
|------------|----------|-----------------|
| 0 | 0x000FFFFC | Stack top (initial) |
| 1 | 0x000FFFF8 | First push — normal |
| 4,096 | 0x000EFFFC | Stack bottom (`0x000F0000`) |
| **4,097** | **0x000EFFFC** | **Entered DATA SEGMENT** ← corruption begins |
| 8,192 | 0x000DFFFC | Middle of data segment |
| 32,768 | 0x000FFFFC - 131072 = 0x000EFFFC | (continued descent) |
| ~49,152 | 0x00080000 | Data segment start — Fibonacci results overwritten |
| ~81,920 | 0x00010000 | CODE SEGMENT — program self-corrupts! |
| ~262,143 | 0x00000004 | Near address zero |
| 262,144 | 0xFFFFFFFC | Underflow past zero — mem_write32 → FAULT |

In this run, `--max-cycles 20000` stops at cycle ~20,000 (SP ≈ `0x00057AEC`),
still inside the code segment.

---

## Why This Is a Critical Bug

### Silent Data Corruption

The simulator does NOT stop when the stack crosses into the data segment.
`mem_write32()` only rejects addresses `>= MEM_SIZE`.  Addresses `0x000EFFFF`
through `0x00000001` are all valid memory — the bounds check passes.

This means:
- Fibonacci results at `0x00080000–0x00080024` get overwritten with `0xAA` (the pushed value).
- Code instructions at `0x00010000–0x0007FFFF` get overwritten, changing the program itself.
- The corruption is **invisible** until the corrupted region is read back.

### On a Real CPU

A real CPU with an MMU would raise a **stack overflow exception** (page fault
or MPU region violation) when SP leaves the designated stack region.  This
simulator has no MMU — it uses a flat memory model.

---

## SP Progression in the Log

The log shows SP decreasing by 4 every two cycles (one PUSH + one JMP):

```
Cycle  2: SP 0x000FFFFC → 0x000FFFF8
Cycle  4: SP 0x000FFFF8 → 0x000FFFF4
Cycle  6: SP 0x000FFFF4 → 0x000FFFF0
...
Cycle 8194: SP 0x000F0004 → 0x000F0000  (stack-data boundary)
Cycle 8196: SP 0x000F0000 → 0x000EFFFC  (DATA SEGMENT — corruption starts)
```

---

## Detection Strategy

To detect stack overflow **before** it causes corruption:

**Option 1 — Software stack guard (in assembly):**
```asm
LOAD_IMM  R9, #0x80   ; load 0x000F0000 (stack bottom) into R9
SHL       R9, R9, #12
CMP       R13, R9     ; compare SP with stack bottom
JLT       OVERFLOW_HANDLER
PUSH      R1
```

**Option 2 — Simulator enhancement:**
Add a `stack_base` field to the `CPU` struct and check `SP < stack_base`
before each PUSH.

---

## Key Observations

| Observation | Significance |
|-------------|-------------|
| SP decreases by 4 every iteration | Confirms pre-decrement semantics |
| No fault when SP crosses 0x000F0000 | Flat memory model has no segment protection |
| Data segment overwritten at cycle ~32,768 | Silent corruption — CPU still running |
| Terminated by max-cycles, not a hardware error | The bug itself doesn't crash the cpu until SP underflows past 0 |
| JMP back means SP never recovers | Without RET or SP restoration, every call/push is permanent |

---

## Conclusion

Stack overflow in a flat-memory, non-MMU CPU is a **silent, progressive data
corruption** bug.  The CPU happily writes into data and code regions.  The
only protection is software-side stack guard checks.  This scenario highlights
why embedded systems need either an MMU/MPU for stack guard pages, or explicit
SP boundary checks in critical firmware loops.
