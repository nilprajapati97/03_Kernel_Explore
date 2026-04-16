# Problem Statement — Scenario 02: Infinite Loop Detection

## Objective

Demonstrate that the CPU simulator's `--max-cycles` safety guard correctly
detects and aborts a program that contains an unconditional backward jump
with no exit condition.

## Program Description

```asm
LOOP:
    ADD  R0, R0, R1    ; R0 += 1  (R1 = 1)
    JMP  LOOP          ; jump back unconditionally — infinite loop
HALT                   ; unreachable
```

The program never reaches `HALT`.  Without a cycle limit the simulator
would run forever.

## Expected Outcome

- Simulator runs until `max_cycles` is reached (set to 500 in this run).
- Returns exit code `-2` (timeout).
- Log shows the same two-instruction pattern repeating.
- Final line: `[SIM] *** max_cycles (500) exceeded — execution aborted ***`

## What to Observe

- PC oscillates between `0x00010000` and `0x00010004` every 2 cycles.
- R0 increments by 1 each iteration — confirming the loop body executes.
- No `HALT` is ever reached.
- The `--max-cycles` flag is the only thing stopping execution.
