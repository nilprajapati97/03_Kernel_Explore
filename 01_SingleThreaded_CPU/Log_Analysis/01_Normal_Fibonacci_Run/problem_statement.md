# Problem Statement — Scenario 01: Normal Fibonacci Run

## Objective

Execute a correct Fibonacci program on the single-threaded CPU simulator
and verify that:
1. The program assembles without errors.
2. The simulator fetches, decodes, and executes all instructions in order.
3. The 10 Fibonacci numbers are written correctly to the data segment.
4. Execution terminates cleanly with `HALT`.

## Program Description

`test_fibonacci.asm` computes F0–F9 using two running variables and a
loop counter.  No recursion, no function calls — pure iterative arithmetic
using `ADD`, `MOV`, `STORE`, `CMP`, and `JGT`.

## Expected Outcome

- CPU halts after ~87 cycles with `CPU_STATE_HALTED`.
- Memory at `0x00080000` contains: `0, 1, 1, 2, 3, 5, 8, 13, 21, 34`.
- No warnings, no faults, exit code 0.

## What to Look For in the Log

- Every `STORE` instruction writes to an address 4 bytes beyond the previous.
- The `JGT LOOP` jumps backward exactly 7 times.
- On the 8th test, `JGT` falls through (counter = 1, not > 1).
- Final two instructions execute the last Fibonacci value before `HALT`.
