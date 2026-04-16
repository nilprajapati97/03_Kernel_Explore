# Problem Statement — Scenario 10: Flag Dependency Chain

## Objective

Demonstrate that status flags set by one instruction can be **stale** when
read many instructions later, leading to a conditional branch taking the
wrong path.

## Background

On this CPU, flags are only updated by ALU operations (`ADD`, `SUB`, `CMP`,
etc.).  Instructions like `MOV`, `LOAD`, `STORE`, `PUSH`, `POP`, and `NOP`
do **not** touch flags.

A programmer who forgets this can write code where a `CMP` is placed several
instructions before a conditional jump, and one of the intervening instructions
(perhaps an `ADD` needed for address arithmetic) overwrites the flags before
the branch is tested.

## The Buggy Program

```asm
LOAD_IMM  R0, #5
LOAD_IMM  R1, #5
CMP       R0, R1       ; sets Z=1 (5==5, equal)
; ---- BUG: developer adds an address-prep ADD here ----
LOAD_IMM  R4, #128
LOAD_IMM  R7, #12
SHL       R4, R4, R7   ; SHL clears/sets Z based on result → Z=0 now (0x80000 != 0)
; ---- branch uses stale flags from SHL, not from CMP ----
JEQ       EQUAL_BRANCH ; should jump (CMP said equal), but WON'T because Z was cleared by SHL
ADD       R2, R0, R1   ; falls through to here instead
HALT
EQUAL_BRANCH:
LOAD_IMM  R2, #99
HALT
```

## Expected Outcome

- Programmer intended: JEQ taken (R0 == R1).
- Actual: JEQ NOT taken (flags were overwritten by `SHL`).
- R2 = 10 (wrong path), not 99 (intended path).

## What to Observe

- The Z flag after `CMP` (Z=1) is visible in the trace.
- Each subsequent instruction changes the flag state.
- By the time `JEQ` executes, Z=0 due to an intervening arithmetic instruction.
