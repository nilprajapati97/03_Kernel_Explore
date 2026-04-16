# Analysis — Scenario 10: Flag Dependency Chain

## Overview

A `CMP` instruction sets `Z=1` (equal), but three instructions later a `SHL`
instruction updates the flags and clears `Z` to 0.  The subsequent `JEQ`
tests the stale flag value and takes the wrong branch.  The CPU is working
correctly — the bug is in the programmer's mental model of flag lifetime.

---

## The Flag Hazard

```
Cycle 3: CMP R0, R1   → 5 - 5 = 0  → Z=1  (programmer's intent: branch equals)
Cycle 4: LOAD_IMM R4, #128           Z unchanged (LOAD_IMM does NOT update flags)
Cycle 5: LOAD_IMM R7, #12            Z unchanged (LOAD_IMM does NOT update flags)
Cycle 6: SHL R4, R4, R7  → 0x80<<12 = 0x00080000 ≠ 0  → Z=0  ← FLAGS CLOBBERED
Cycle 7: JEQ ...          → checks Z == 1?  Z = 0 (from SHL, not CMP)
          Branch NOT taken.
```

The `Z` flag survived `LOAD_IMM` (which doesn't modify flags) but was
destroyed by `SHL` (which does).

---

## Which Instructions Update Flags?

| Updates flags | Does NOT update flags |
|---------------|----------------------|
| ADD, SUB, MUL, DIV | MOV |
| AND, OR, XOR, NOT | LOAD_IMM |
| SHL, SHR | LOAD, STORE |
| CMP | PUSH, POP |
| | NOP, HALT |
| | JMP, JEQ, JNE, JGT, JLT |
| | CALL, RET |

The programmer correctly assumed `LOAD_IMM` doesn't clobber flags (it
doesn't), but forgot that `SHL` does.

---

## Flag Value at Each Cycle

| Cycle | Instruction | Z | N | C | O | Comment |
|-------|-------------|---|---|---|---|---------|
| 1 | LOAD_IMM R0, #5 | 0 | 0 | 0 | 0 | No flag update |
| 2 | LOAD_IMM R1, #5 | 0 | 0 | 0 | 0 | No flag update |
| 3 | CMP R0, R1 | **1** | 0 | 0 | 0 | **Z=1 — equal!** |
| 4 | LOAD_IMM R4, #128 | 1 | 0 | 0 | 0 | Preserved (no flag update) |
| 5 | LOAD_IMM R7, #12 | 1 | 0 | 0 | 0 | Preserved |
| 6 | SHL R4, R4, R7 | **0** | 0 | 0 | 0 | **Z=0 — clobbered!** |
| 7 | JEQ ... | 0 | 0 | 0 | 0 | Tests Z=0 → branch not taken |
| 8 | ADD R2, R0, R1 | 0 | 0 | 0 | 0 | Wrong path: R2 = 10 |

---

## The Correct Fix: Move CMP Immediately Before Branch

```asm
LOAD_IMM  R0, #5
LOAD_IMM  R1, #5

; Do all non-flag-critical work BEFORE the comparison
LOAD_IMM  R4, #128
LOAD_IMM  R7, #12
SHL       R4, R4, R7       ; address setup done

CMP       R0, R1            ; NOW compare — Z=1 set here
JEQ       EQUAL_BRANCH      ; immediately after CMP — no intervening flag-clobbers
```

**Rule:** Place `CMP` (or the flag-setting instruction) immediately before
the conditional branch that depends on its result.

---

## Connection to Real CPU Hazards

In a **pipelined CPU**, this type of problem is called a **data hazard** and
is handled by stalling, forwarding, or the compiler inserting NOPs.

In this **non-pipelined CPU**, there are no pipeline hazards — but there is
still the logical hazard that the programmer must manage: "which instruction
last updated the flags I'm about to test?"

Real ISAs handle this differently:
- **ARM**: Every flag-setting instruction has an `S` suffix (`ADDS`, `SUBS`).
  Non-suffixed instructions leave flags unchanged.  This gives explicit control.
- **RISC-V**: Has no flags at all.  Branches compare registers directly:
  `BEQ R0, R1, label` — no flag intermediary.
- **x86**: Most arithmetic instructions implicitly update EFLAGS, making the
  hazard even more common.

---

## Key Observations

| Observation | Significance |
|-------------|-------------|
| Z=1 after CMP (cycle 3) | Visibly correct in the trace |
| Z=1 preserved through LOAD_IMM ×2 | Confirms LOAD_IMM does not modify flags |
| Z=0 after SHL (cycle 6) | The flag clobber is clearly visible one cycle before the branch |
| JEQ not taken (cycle 7) | The incorrect outcome is directly readable in the trace |
| R2 = 10, not 99 | Silent wrong result — no crash, no fault |
| 3 instructions between CMP and JEQ | Distance is small enough to catch in code review |

---

## Conclusion

Flag dependency chains are a subtle class of assembly bug where the programmer
must track flag lifetime manually.  In this single-threaded, non-pipelined CPU
the trace makes the clobber trivially visible: you can read the Z flag column
line-by-line and see exactly where it transitions from 1 to 0.  The fix is
architectural discipline: always place the comparison immediately before the
branch, or use registers directly for conditional logic.
