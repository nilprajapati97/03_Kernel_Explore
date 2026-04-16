# Problem Statement — Scenario 06: Memory Out-Of-Bounds Access

## Objective

Demonstrate the simulator's bounds-checking mechanism when a program attempts
to read from an address beyond the 1 MiB simulated memory limit (`0x00100000`).

## Program Description

```asm
; Load a deliberately out-of-bounds address
LOAD_IMM  R4, #0x7FF    ; R4 = 0x7FF = 2047 (max positive imm12)
LOAD_IMM  R7, #12
SHL       R4, R4, R7    ; R4 = 0x7FF << 12 = 0x7FF000  (still in range)
LOAD_IMM  R5, #0x7FF
SHL       R5, R5, R7    ; R5 = 0x7FF000
ADD       R4, R4, R5    ; R4 = 0xFFE000  (close to limit)
LOAD_IMM  R6, #0x7FF
ADD       R4, R4, R6    ; R4 = 0x001007FF + extras → 0x00100400 (OUT OF BOUNDS)
LOAD      R0, [R4+0]    ; ACCESS BEYOND 0x000FFFFF → FAULT
HALT
```

## Expected Outcome

- The `LOAD` instruction computes address `0x00100400`.
- `mem_read32()` performs a bounds check: `addr >= MEM_SIZE (0x00100000)`.
- The check fails → prints error → terminates the simulator.
- CPU state = `CPU_STATE_FAULT`.

## What to Observe

- The fault happens during the **execute stage** of the LOAD instruction.
- The error message shows the faulting address and the valid range.
- No data is read; R0 remains 0.
