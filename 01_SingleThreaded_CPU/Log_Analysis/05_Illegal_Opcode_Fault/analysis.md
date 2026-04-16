# Analysis — Scenario 05: Illegal Opcode Fault

## Overview

The binary contains an instruction word `0xDE000000` whose opcode byte `0xDE`
is not defined in the ISA.  The CPU's `execute()` function hits the `default`
case of the opcode `switch` statement, sets state to `CPU_STATE_FAULT`, and
returns `-1`.  Execution stops immediately.

---

## Execution Flow

```
Cycle 1: Fetch 0x04010000 at  0x00010000 → LOAD_IMM R1, #10 → OK
Cycle 2: Fetch 0xDE000000 at  0x00010004 → opcode = 0xDE
         switch(0xDE)  →  default case:
           fprintf(stderr, "[ERR] Illegal opcode 0xDE ...")
           cpu->state = CPU_STATE_FAULT
           return -1
cpu_run() sees return -1 → exits immediately
```

Instructions at `0x00010008` (ADD) and `0x0001000C` (HALT) are **never
fetched**.

---

## Where the Fault is Detected

Inside `cpu.c`, the `execute()` function:

```c
switch (instr->opcode) {
    case OP_ADD:  ...
    case OP_SUB:  ...
    /* ... all defined opcodes ... */

    default:
        fprintf(stderr,
                "[ERR] Illegal opcode 0x%02X at cycle %llu, addr 0x%08X "
                "(raw=0x%08X)\n",
                instr->opcode, cpu->cycle_count,
                fetch_addr, instr->raw);
        cpu->state = CPU_STATE_FAULT;
        return -1;    /* propagates to cpu_step() → cpu_run() → main() */
}
```

---

## Fault vs. Soft Error — The Key Distinction

| Type | Example | CPU continues? | CPU state |
|------|---------|----------------|-----------|
| **Soft error** | div-by-zero | YES | `HALTED` (eventually) |
| **Hard fault** | illegal opcode | NO | `CPU_STATE_FAULT` |
| **Hard fault** | OOB memory | NO | `CPU_STATE_FAULT` |

An illegal opcode is a **hard fault** because there is no sensible way to
continue: the CPU doesn't know what the instruction intended to do, how many
operands it takes, or what the correct result should be.

---

## PC State at Fault Time

Notice in the log:

```
[SIM] Final PC:  0x00010008  (PC advanced past faulting instruction)
```

This is because in the fetch stage, **PC is incremented before execute**:

```c
fetch_addr = PC;
raw        = mem_read32(cpu->mem, PC);
PC        += 4;              // ← PC already advanced
instr      = decode_instruction(raw);
...
execute(cpu, &instr);        // ← fault detected here, after PC+=4
```

So at the time of fault, PC already points to the instruction after the bad
one.  This is normal — it matches how real RISC CPUs report exceptions (the
exception PC is the address of the faulting instruction, which requires saving
`PC - 4`).

---

## When Does an Illegal Opcode Appear in Real Programs?

1. **Corrupted binary** — a bit flip in storage/transmission changes a valid
   opcode to an invalid one.
2. **Version mismatch** — code compiled for a newer ISA (with additional
   opcodes) running on an older CPU that doesn't know them.
3. **Executing data** — a bug in a branch/jump addresses a data word as
   code; the data byte happens to not be a valid opcode.
4. **Hand-crafted binary** — intentional, as in this test case.

---

## Key Observations

| Observation | Significance |
|-------------|-------------|
| Fault at cycle 2 | Only 1 valid instruction executed before the fault |
| PC = 0x00010008 at fault | PC pre-incremented; faulting addr was 0x00010004 |
| R2 = 0 | ADD never executed — partial execution avoided |
| Exit code -1 | Hard fault, not timeout (-2) or clean halt (0) |
| Error on stderr | Distinguishable from normal stdout trace output |

---

## Conclusion

Illegal opcode faults are **fail-fast** — the CPU stops immediately with a
clear error message identifying the faulting cycle, address, and raw word.
This is the correct behaviour: continuing past an unknown instruction would
produce undefined results.  In a real CPU, this triggers a synchronous
hardware exception (Undefined Instruction on ARM) allowing an OS to handle or
report it gracefully.
