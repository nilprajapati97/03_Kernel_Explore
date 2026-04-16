# Analysis — Scenario 09: Off-By-One Loop Bug

## Overview

A single digit change — `LOAD_IMM R0, #7` instead of `#8` — causes the
loop to execute one fewer iteration than intended.  The 10th Fibonacci number
(F9 = 34) is never computed, leaving `MEM[0x00080024] = 0`.

---

## The Off-By-One in Detail

The correct logic:
- F0 and F1 are pre-stored before the loop.
- The loop needs to compute **F2 through F8** → **7 iterations**.
- After the loop, 2 explicit final instructions compute and store **F9**.
- Initial counter for the loop: **R0 = 8** (loop exits when R0 == 1, so it runs for 7 iterations)

Wait — let's re-examine:

```
Counter starts at 8.
Each iteration: R0 -= 1, then CMP R0, R6 (R6=1), then JGT if R0 > 1.

Iteration 1: R0 = 8 → 7;  CMP 7,1 → !Z && !N → JGT taken
Iteration 2: R0 = 7 → 6;  CMP 6,1 → JGT taken
...
Iteration 7: R0 = 2 → 1;  CMP 1,1 → Z=1 → JGT NOT taken (exit)
```

So with R0=8, the loop runs exactly 7 times, then falls through to the 2
final instructions.  Total stored: 2 (pre) + 7 (loop) + 1 (final) = 10. ✓

**Buggy version (R0 = 7):**
```
Iteration 1: R0 = 7 → 6;  JGT taken
...
Iteration 6: R0 = 2 → 1;  Z=1 → JGT NOT taken (exit ONE iteration early)
Final instructions store only F8; F9 never computed.
Total stored: 2 (pre) + 6 (loop) + 1 (final) = 9. ✗
```

---

## Iteration Count Comparison

| Counter init | Loop iterations | Values stored in loop | Final stores | Total |
|--------------|-----------------|----------------------|--------------|-------|
| R0 = 8 (correct) | 7 | F2..F8 | F9 | **10** |
| R0 = 7 (buggy) | 6 | F2..F7 | F8 | **9** (F9 missing) |
| R0 = 9 | 8 | F2..F9 | F9 again | **10 + 1 duplicate** (F9 stored twice) |

---

## Flag Trace at Loop Exit (Buggy)

```
Buggy iteration 6:
  Cycle N:   SUB R0(=2), R6(=1) → R0 = 1;  Z=0 (1 ≠ 0), N=0, C=1
  Cycle N+1: CMP R0(=1), R6(=1) → 1-1=0;   Z=1, N=0, C=0
  Cycle N+2: JGT: !Z && !N = !1 && !0 = FALSE → NOT taken

Correct iteration 7:
  Same flag pattern — the distinction is that this was supposed to be
  iteration 7 (of 7), not iteration 6 (of 7).
```

The log at cycle 57-58 shows the exact same flag pattern as the correct
program at cycles 68-70 — but it fires one iteration early.

---

## Memory Dump Difference

| Address | Correct output | Buggy output |
|---------|---------------|-------------|
| 0x00080000 | `00 00 00 00` (F0=0) | `00 00 00 00` ✓ |
| 0x00080004 | `01 00 00 00` (F1=1) | `01 00 00 00` ✓ |
| ... | ... | ... |
| 0x00080020 | `15 00 00 00` (F8=21) | `15 00 00 00` ✓ |
| **0x00080024** | **`22 00 00 00` (F9=34)** | **`00 00 00 00` ← MISSING** |

The bug is invisible at runtime — no error, no fault, clean HALT.  It only
shows up in the final memory dump.

---

## Cycle Count Difference

| Version | Total cycles |
|---------|-------------|
| Correct (R0=8) | 73 |
| Buggy (R0=7) | 61 |

The 12-cycle reduction (8 instructions × 1 missing iteration + the extra
final instruction pair = 8+2 = 10 cycles fewer... close to 12 with overhead)
could be a subtle performance difference that hints something changed.

---

## How to Detect This Bug

1. **Memory dump inspection** — compare the 10th word against expected value.
2. **Cycle count mismatch** — 61 vs expected 73 cycles.
3. **Boundary value test** — always test the last iteration explicitly.
4. **Assertion in assembly:**
   ```asm
   LOAD     R9, [R4+0]     ; read last stored value
   LOAD_IMM R10, #34       ; expected F9
   CMP      R9, R10
   JEQ      OK
   HALT                    ; wrong result — trap here
   ```

---

## Key Observations

| Observation | Significance |
|-------------|-------------|
| Single digit change (#7 vs #8) | Most dangerous bugs are tiny and easy to miss in review |
| No CPU error of any kind | Off-by-one is a logic bug, invisible to the hardware |
| Loop ran 6 iterations not 7 | Verifiable by counting JGT-taken events in the trace |
| Final pair computes F8 not F9 | The final 2 instructions are always 1 step behind expected |

---

## Conclusion

Off-by-one errors in loop counters are among the most common assembly bugs.
The CPU executes the program perfectly correctly — the logic error exists
entirely in the programmer's intent vs their code.  The single-threaded,
non-pipelined trace makes these bugs easy to root-cause: every cycle is
visible, and the exact loop exit condition can be read directly from the flag
column of the trace.
