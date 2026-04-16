# Problem Statement — Scenario 05: Illegal Opcode Fault

## Objective

Show how the CPU simulator responds to an **unknown opcode** — a raw 32-bit
word whose top 8 bits do not correspond to any defined instruction in the ISA.

## Program Description

The binary contains a hand-crafted invalid instruction word `0xDE000000`
(opcode `0xDE`) inserted between valid instructions:

```asm
LOAD_IMM  R1, #10
; raw bytes 0xDE 0x00 0x00 0x00 injected here  ← illegal opcode
ADD       R2, R1, R1
HALT
```

## Expected Outcome

- Execution proceeds normally through Cycle 1 (`LOAD_IMM R1, #10`).
- At Cycle 2, the CPU fetches opcode `0xDE` from the switch statement's
  `default` case.
- The simulator prints an error, sets `state = CPU_STATE_FAULT`, and returns
  `-1` from `cpu_step()`.
- `cpu_run()` returns `-1` and main exits with `EXIT_FAILURE`.
- Cycles 3 and 4 (`ADD` and `HALT`) are **never reached**.

## What to Observe

- The fault is a **hard error** — unlike div-by-zero, the CPU state becomes
  `CPU_STATE_FAULT` and execution stops immediately.
- The error message includes the cycle number and the raw instruction word.
- R2 stays at 0 — the ADD never executed.
