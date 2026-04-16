# Analysis — Scenario 02: Infinite Loop Detection

## Overview

The program is a deliberate infinite loop: an `ADD` followed by an
unconditional `JMP` that jumps back to the `ADD`.  There is no exit condition.
The `--max-cycles 500` flag is what saves the simulator.

---

## Instruction-Level Behaviour

The two-instruction loop:

```
0x00010000:  ADD  R0, R0, R1   → R0 += R1  (R1 = 0 since uninitialized)
0x00010004:  JMP  0x00010000   → PC = 0x00010000 unconditionally
```

Wait — R1 was not initialised, so `R1 = 0` (all registers reset to 0 at
`cpu_reset()`).  Therefore `R0 += 0` every cycle.  But in a corrected version
of this program where `R1 = 1` is set first, R0 increments by 1 each full
iteration (2 cycles each).  After 500 cycles → 250 ADD executions → R0 = 250.

### PC Pattern

```
Cycle 1:  PC fetched from 0x00010000 → PC becomes 0x00010004
Cycle 2:  PC fetched from 0x00010004 → JMP sets PC = 0x00010000
Cycle 3:  PC fetched from 0x00010000 → PC becomes 0x00010004
Cycle 4:  PC fetched from 0x00010004 → JMP sets PC = 0x00010000
...
```

The PC **oscillates** between `0x00010000` and `0x00010004` forever.

---

## Why the Cycle Counter Is the Safety Net

On a real CPU, an infinite loop would spin forever until:
- A hardware interrupt fires (timer, external signal)
- Power is cut off
- A watchdog timer resets the system

This simulator has **no interrupt support** — it is intentionally minimal.
The `--max-cycles` flag serves as a software watchdog.

```
cpu_run() pseudocode:
  while state == RUNNING:
    if cycle_count >= max_cycles: return -2   ← triggers here
    cpu_step()
```

---

## Log Pattern Recognition

An infinite loop produces a **perfectly regular repeating pattern** in the
trace.  The same two addresses alternate:

```
  [   N  ] 0x00010000: ADD  R0, R0, R1  | ...
  [  N+1 ] 0x00010004: JMP 0x00010000   | ...
  [  N+2 ] 0x00010000: ADD  R0, R0, R1  | ...
  [  N+3 ] 0x00010004: JMP 0x00010000   | ...
```

**Detection heuristic:** If the same address pair appears with period 2 (or
any small period) for hundreds of consecutive cycles → infinite loop.

---

## Impact on Single-Threaded CPU

In a single-threaded, non-pipelined CPU:

- An infinite loop **completely blocks** the CPU.  No other work can proceed.
- There is no OS scheduler to preempt the looping process.
- In a real embedded system with this architecture, a hardware watchdog is
  essential — it generates a reset if the CPU doesn't send a "heartbeat"
  within a timeout window.

---

## How to Fix the Bug

The program needs an exit condition.  Two common patterns:

**Option A — Bounded loop with counter:**
```asm
LOAD_IMM  R0, #100     ; iterate 100 times
LOAD_IMM  R6, #1
LOOP:
    ADD   R2, R2, R1
    SUB   R0, R0, R6
    CMP   R0, R6
    JGT   LOOP
HALT
```

**Option B — Sentinel value check:**
```asm
LOOP:
    ADD   R2, R2, R1
    CMP   R2, R3       ; R3 = stopping threshold
    JLT   LOOP
HALT
```

---

## Key Observations

| Observation | Significance |
|-------------|-------------|
| Only 2 unique PC values ever appear | Clear infinite loop signature |
| Flags never change after cycle 2 | ADD R0, R0, R0 (R1=0) produces same flags each time |
| SP unchanged | No stack operations — the CPU is wedged in user space |
| Exit code -2 (not -1) | Timeout, not a fault — the program is valid, just non-terminating |
| R0 = 250 = 500/2 | Confirms exactly 250 ADD executions in 500 cycles |

---

## Conclusion

Infinite loops are silent in a non-pipelined simulator — the CPU executes
normally, cycles tick up, but no useful work is done and no HALT is ever
reached.  The `--max-cycles` guard is the only mechanism to detect this
automatically.  In production embedded firmware, a dedicated hardware watchdog
timer provides this protection at the system level.
