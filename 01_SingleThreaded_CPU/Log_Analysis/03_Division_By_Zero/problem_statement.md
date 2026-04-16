# Problem Statement — Scenario 03: Division By Zero

## Objective

Show how the CPU simulator handles a `DIV` instruction where the divisor
register contains zero at runtime — a value that was not known at assemble time.

## Program Description

```asm
LOAD_IMM  R1, #42    ; dividend = 42
LOAD_IMM  R2, #0     ; divisor  = 0   ← the bug
DIV       R3, R1, R2 ; R3 = R1 / R2 → division by zero!
STORE     [R4], R3   ; would store bad result
HALT
```

## Expected Outcome

The simulator's `alu_div()` detects `b == 0`, prints a warning, and returns
`0` as the result instead of crashing.  Execution **continues** — this is a
soft error, not a hard fault.

The program reaches `HALT` normally, but R3 holds `0` (a bogus value) and
the stored result at `[R4]` is `0` rather than the correct quotient.

## What to Observe

- Warning line: `[WARN] DIV by zero at cycle N addr 0x0001000C — result forced to 0`
- R3 = 0 after the DIV (not 42).
- Execution does NOT fault — `CPU_STATE_HALTED`, not `CPU_STATE_FAULT`.
- This demonstrates that silent data corruption is possible: the bug is
  behavioural, not a crash, so it could go undetected without checking outputs.
