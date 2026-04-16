# Problem Statement — Scenario 09: Off-By-One Loop Bug

## Objective

Trace a classic **off-by-one error** in a loop: the program intends to
compute and store 10 Fibonacci numbers but stores only 9 due to a wrong
initial loop counter.

## The Bug

In the correct Fibonacci program, the loop counter is initialized to `8`
for the 8 remaining iterations (after pre-storing F0 and F1).

In this buggy version, the counter is mistakenly initialized to `7`:

```asm
; BUGGY: should be #8 but programmer wrote #7
LOAD_IMM R0, #7          ; ← BUG: one iteration too few
```

## Expected Outcome (Correct)

Memory at `0x00080000`: `0, 1, 1, 2, 3, 5, 8, 13, 21, 34` (10 values)

## Actual Outcome (With Bug)

Memory at `0x00080000`: `0, 1, 1, 2, 3, 5, 8, 13, 21, 0`

**F9 (34) is never computed or stored.** `MEM[0x00080024] = 0` (uninitialised).

## What to Observe

- The loop executes only 6 iterations instead of 7 (counter 7→1 with the
  JGT condition).
- The final two instructions outside the loop produce F8 (21) correctly,
  but F9 is the final instruction — which was meant to run after the loop.
- The trace shows the JGT falling through one iteration too early.
- The memory dump clearly shows a trailing `00 00 00 00` at offset 36.
