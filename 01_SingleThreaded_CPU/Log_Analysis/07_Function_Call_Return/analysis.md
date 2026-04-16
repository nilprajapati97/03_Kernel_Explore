# Analysis — Scenario 07: Function Call and Return

## Overview

The program calls a 3-word multiply subroutine twice.  This exercises the
complete `CALL` → `PUSH LR` → `MUL` → `POP LR` → `RET` sequence and verifies
that the stack is balanced after each call.

---

## The CALL / RET Mechanism

### CALL instruction

```
CALL imm:
  LR = PC          (PC already advanced by +4 in fetch stage)
  PC = imm         (jump to subroutine)
```

So `LR` (R14) receives the address of the **instruction immediately after the
CALL** — the correct return address.

### RET instruction

```
RET:
  PC = LR
```

Simple: restore PC from LR.

### Why PUSH / POP LR?

If the subroutine itself calls another function (`CALL` inside the subroutine),
the `CALL` would overwrite `LR` with the inner return address, destroying the
outer return address.  The pattern `PUSH LR` / `POP LR` saves and restores it
on the stack, enabling **nested calls**.

In this example, the subroutine does not itself call anything (it's a leaf
function), but using PUSH/POP LR is shown as the correct defensive pattern.

---

## Stack Trace — First Call

```
Before CALL:   SP = 0x000FFFFC
During CALL:   LR = 0x00010014  (return addr to main)
               PC = 0x00010024  (subroutine)

Cycle 6: PUSH R14
  SP = 0x000FFFFC - 4 = 0x000FFFF8
  MEM[0x000FFFF8] = 0x00010014

Cycle 9: POP R14
  R14 = MEM[0x000FFFF8] = 0x00010014
  SP = 0x000FFFF8 + 4 = 0x000FFFFC   ← stack restored

Cycle 10: RET
  PC = R14 = 0x00010014   ← returns to correct location
```

---

## Stack Trace — Second Call

```
Before CALL:   SP = 0x000FFFFC  (balanced from first call)
During CALL:   LR = 0x00010020
               PC = 0x00010024

Cycle 14: PUSH R14
  SP = 0x000FFFF8
  MEM[0x000FFFF8] = 0x00010020

Cycle 17: POP R14
  R14 = 0x00010020
  SP = 0x000FFFFC   ← balanced again

Cycle 18: RET → PC = 0x00010020
```

---

## Register State Across Both Calls

| Register | After Call 1 | After Call 2 |
|----------|-------------|-------------|
| R0 | 15 (5 × 3) | 21 (7 × 3) |
| R1 | 3 (set in subroutine — caller-clobbered) | 3 |
| R4 | 0x00080000 (base addr) | 0x00080000 |
| LR (R14) | 0x00010014 | 0x00010020 |
| SP (R13) | 0x000FFFFC (balanced) | 0x000FFFFC (balanced) |

---

## Memory Results

```
MEM[0x00080000] = 15  (5 × 3)  ← stored after first call
MEM[0x00080004] = 21  (7 × 3)  ← stored after second call
```

---

## Key Observations

| Observation | Significance |
|-------------|-------------|
| SP returns to 0x000FFFFC after each call | Stack is balanced — PUSH and POP are matched |
| LR correctly set to instruction after CALL | PC pre-increment in fetch makes this automatic |
| R1 = 3 visible to caller after return | This CPU has no calling convention — callee-saved vs caller-saved is programmer responsibility |
| 20 total cycles for 2 calls | Each call costs 5 cycles (CALL + PUSH + LOAD_IMM + MUL/LD + POP + RET = 6 cycles in subroutine + return path) |
| HALT at 0x00010024 | Reused the subroutine's address by accident (shows labels can alias code boundaries) |

---

## Calling Convention Note

This CPU has no enforced calling convention.  The subroutine modifies R1
(the constant `3`) without saving it.  In a larger program, a proper ABI
would designate which registers a subroutine may clobber (caller-saved) and
which it must preserve (callee-saved).

---

## Conclusion

The CALL/RET/PUSH/POP mechanism works correctly.  LR holds the precise return
address because PC is pre-incremented in the fetch stage before CALL executes.
The stack is balanced after each call because every PUSH has a matching POP.
This pattern is the foundation of all function calls in assembly.
