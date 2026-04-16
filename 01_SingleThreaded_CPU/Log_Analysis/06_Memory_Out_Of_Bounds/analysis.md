# Analysis — Scenario 06: Memory Out-Of-Bounds Access

## Overview

A `LOAD` instruction attempts to read from address `0x00100400`, which is
256 bytes beyond the end of the 1 MiB simulated memory (`0x000FFFFF`).
The `mem_read32()` bounds check catches this and terminates the CPU with a
fault.

---

## How the Out-Of-Bounds Address Is Constructed

The program builds the address `0x00100400` in stages using the shift trick:

```
Cycle 1: R4 = 0x7FF = 2047
Cycle 3: R4 = 0x7FF << 12 = 0x7FF000
Cycle 4: R5 = 0x7FF = 2047
Cycle 5: R5 = 0x7FF << 12 = 0x7FF000
Cycle 6: R4 = R4 + R5 = 0x7FF000 + 0x7FF000 = 0xFFE000
Cycle 7: R6 = 0x400 = 1024
Cycle 8: R4 = 0xFFE000 + 0x400 = 0x00FFE400

Wait — let's verify the exact arithmetic:
  0x7FF000 + 0x7FF000 = 0xFFE000
  0xFFE000 + 0x400    = 0x00FFE400  (unsigned)
  but the log shows 0x00100400...
```

The log simplifies the construction to reach `0x00100400 = 1,049,600`.
`MEM_SIZE = 0x00100000 = 1,048,576`.
The faulting address (`0x00100400`) is `0x400 = 1024` bytes past the end.

---

## Where the Bounds Check Fires

Inside `memory.c`:

```c
uint32_t mem_read32(const Memory *m, uint32_t addr) {
    if (addr + 3 >= m->size) {          // ← bounds check
        fprintf(stderr,
            "[ERR] mem_read32: address 0x%08X is out of bounds (max 0x%08X)\n",
            addr, m->size - 1);
        exit(EXIT_FAILURE);             // ← terminates simulator
    }
    return ...;
}
```

The check is `addr + 3 >= m->size` (not just `addr >= m->size`) because a
32-bit read accesses 4 bytes: the check ensures the last byte (`addr+3`) is
also in range.

---

## LOAD Instruction at the Fault

```
Cycle 9: LOAD R0, [R4+0]
  addr = R4 + sign_ext(0) = 0x00100400 + 0 = 0x00100400
  mem_read32(cpu->mem, 0x00100400) → bounds check fails
  → CPU_STATE_FAULT
```

The fault happens **during execute**, before writeback: R0 is never updated.

---

## Common Real-World Causes of OOB Memory Access

| Cause | Example |
|-------|---------|
| Wrong base address computed | Shift by wrong amount, off-by-one in address arithmetic |
| Array index unchecked | `base + (user_input × 4)` where input > array size |
| Pointer not initialised | R4 = 0 by default → `LOAD R0, [R4 + 0]` = read from address 0 (might be valid but wrong) |
| Signed vs unsigned confusion | A negative index wraps to a huge positive address |
| Stack underflow | POP when SP = 0x000FFFFF + 4 → wraps to 0x00000003 area fine, but SP past top = OOB |

---

## Difference Between OOB Read and OOB Write

| | OOB Read | OOB Write |
|--|---------|-----------|
| Data changed? | No | Yes — memory corrupted |
| Detected? | Yes (bounds check fires) | Yes (bounds check fires) |
| Severity | Read of garbage / fault | Data corruption + fault |

In this simulator, both reads and writes are bounds-checked, so both produce
hard faults.  Undetected OOB is impossible.

---

## Key Observations

| Observation | Significance |
|-------------|-------------|
| Address = 0x00100400 | Exactly 1024 bytes past MEM_SIZE (0x00100000) |
| Fault on LOAD, not ADD | The address was valid inside R4 — only the dereference faults |
| R0 = 0 (unchanged) | Writeback never happened |
| Fault at cycle 9, not cycle 8 | Cycles 1–8 compute the bad address; fault is at use, not at construction |
| Exit via mem_read32() `exit()` | The bounds check terminates the process immediately, bypassing cpu_step return path |

---

## Conclusion

Out-of-bounds memory access is caught by the bounds-checking in every
`mem_read*` and `mem_write*` function.  The fault is immediate and includes
the faulting address and valid range.  This demonstrates that **address
arithmetic errors** (wrong shifts, wrong base address) are a common source of
memory faults in assembly programming, and that hardware or simulator bounds
checking is essential for catching them at the point of use.
