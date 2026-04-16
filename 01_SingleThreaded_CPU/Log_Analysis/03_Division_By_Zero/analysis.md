# Analysis — Scenario 03: Division By Zero

## Overview

The program loads the value `42` into R1 and `0` into R2, then attempts
`DIV R3, R1, R2`.  The ALU detects the zero divisor, prints a warning,
forces the result to `0`, and continues execution.  The program reaches
HALT normally — but with **wrong data in memory**.

---

## Execution Trace Breakdown

| Cycle | Instruction | Key State Change |
|-------|-------------|-----------------|
| 1–3 | Address setup for R4 = 0x00080000 | R4 = 0x00080000 |
| 4 | `LOAD_IMM R1, #42` | R1 = 42 |
| 5 | `LOAD_IMM R2, #0` | R2 = 0; **Z flag = 1** |
| 6 | `DIV R3, R1, R2` | ⚠️ Warning emitted; R3 = 0 (forced) |
| 7 | `STORE [R4+0], R3` | MEM[0x00080000] = 0 (wrong!) |
| 8 | `HALT` | CPU halts normally |

---

## What Happens Inside `alu_div()`

```c
uint32_t alu_div(uint32_t a, uint32_t b, Flags *f) {
    if (b == 0) {
        fprintf(stderr, "[WARN] DIV by zero ... result forced to 0\n");
        set_zn(0, f);
        clear_co(f);
        return 0;          // ← silent corruption
    }
    ...
}
```

The function does three things when `b == 0`:
1. Prints a warning to stderr.
2. Updates flags as if the result were 0.
3. Returns 0 to the CPU, which writes it into Rd.

Execution continues on the **next instruction** — no fault, no exception.

---

## Why This Is Dangerous

### The problem: soft error with silent data corruption

```
Expected result: MEM[0x00080000] = 42
Actual result:   MEM[0x00080000] = 0
```

The CPU did not crash.  The exit code is 0 (clean halt).  Only by inspecting
the memory dump can you see the wrong answer.  In a real application:

- A quotient of 0 passed to downstream code could cause further wrong results.
- If the computed quotient is used as an array index, it could silently access
  the wrong memory location.
- The bug would only be caught by testing the final output — not by observing
  the CPU state.

### Comparison: fault vs. soft error

| Behaviour | Impact |
|-----------|--------|
| **Fault** (e.g. illegal opcode) | CPU stops, state = FAULT, easy to detect |
| **Soft error** (div-by-zero here) | CPU continues, result = 0, hard to detect |

The simulator deliberately chose the soft-error approach to match embedded
RISC behaviour where division traps are optional.

---

## The Zero Flag Clue

At cycle 5, `LOAD_IMM R2, #0` sets `Z = 1`.  This is a hint visible in the
trace **before** the division instruction:

```
  [     5] 0x00010010: LOAD_IMM R2, #0    | Z=1 N=0 C=0 O=0
                                              ^^^
                             Z=1 here means R2 = 0
                             Next instruction divides BY R2 → danger!
```

A static analyser or runtime checker could inspect this flag pattern to
predict division by zero before it occurs.

---

## How to Fix the Bug

**In assembly — add a guard:**

```asm
    LOAD_IMM  R2, #0
    CMP       R2, R2         ; sets Z=1 if R2==0
    JEQ       SKIP_DIV       ; skip division if divisor is zero
    DIV       R3, R1, R2
SKIP_DIV:
    STORE     [R4+0], R3
    HALT
```

**In the ALU — make it a hard fault:**

Instead of returning 0, `alu_div()` could set `cpu->state = CPU_STATE_FAULT`,
just like an illegal opcode would.  This is a design choice: safe-but-wrong
vs. fail-fast.

---

## Key Observations

| Observation | Significance |
|-------------|-------------|
| HALT reached, exit code 0 | Bug is invisible at the process level |
| R3 = 0, not 42 | The dividend is lost |
| Z=1 before DIV | Flag in trace signals zero register ahead of the division |
| Warning only on stderr | If stderr is redirected, the warning vanishes too |
| CPU state = HALTED (not FAULT) | Soft error — execution continued past the bug |

---

## Conclusion

Division by zero is a **silent data corruption** in this simulator.  The CPU
does not crash; it produces a wrong answer and keeps running.  Detection
requires either a defensive guard in the assembly code, inspecting the warning
on stderr, or verifying output values.  This scenario illustrates why input
validation is essential even at the lowest hardware abstraction level.
