# Problem Statement — Scenario 04: Stack Overflow

## Objective

Demonstrate what happens when a program executes too many `PUSH` instructions,
causing the Stack Pointer (SP / R13) to underflow past the bottom of the
reserved stack segment (`0x000F0000`) and into the data region.

## Program Description

```asm
LOAD_IMM  R1, #0xAA    ; arbitrary value to push
LOOP:
    PUSH  R1           ; SP -= 4; MEM[SP] = R1  (infinite push)
    JMP   LOOP         ; jump back — push forever
HALT                   ; unreachable
```

Each `PUSH` decrements SP by 4.  The stack segment is 64 KiB (16,384 words).
After 16,384 `PUSH` instructions, SP = `0x000EFFFC` — now inside the data
segment.  After ~131,072 more pushes, SP hits `0x00000000` and the next
`PUSH` would require writing to `0xFFFFFC` — out of the simulated 1 MiB.

## Expected Outcome

- SP starts at `0x000FFFFC`.
- After 16,384 iterations, SP has crossed `0x000F0000` into the data region.
- The bounds-checking in `mem_write32()` will terminate the simulator with an
  out-of-bounds error when SP wraps below `0x00000000`.
- This run uses `--max-cycles 20000` to stop before full corruption.

## What to Observe

- SP decreases by 4 every iteration.
- After enough pushes, SP enters the data segment — memory corruption begins
  silently before any hardware error.
- The run is terminated by max-cycles, showing the problem at the boundary.
