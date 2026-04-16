# Interview: Design a Single-Threaded CPU
### Candidate Profile: 10 Years Linux Driver / Embedded Systems Experience

---

> **Interviewer:** "Walk me through how you would design a single-threaded CPU
> from scratch. Take as much time as you need — I want to see your thought
> process, not just the final answer."

---

## Opening: Clarify the Problem Before Designing

**Candidate:**

Before drawing anything, I'd ask a few scoping questions, because "CPU design"
spans transistors all the way up to microarchitecture:

1. What abstraction level? RTL / gate level, ISA level, or software simulator?
2. What is the target use case — embedded microcontroller, in-order CPU for
   a known workload, or a general-purpose CPU?
3. Are there area / power / performance constraints?
4. Do we need interrupt support? Privilege levels? Virtual memory?
5. What word width — 8 / 16 / 32 / 64-bit?

*For this discussion I'll design a **32-bit, single-threaded, non-pipelined
in-order CPU** at the ISA and micro-architecture level, targeting an embedded
system with no OS and no MMU — the most honest scope for a whiteboard
interview.*

---

## Part 1 — ISA Design

**Interviewer:** "Start with the ISA."

---

### 1.1 Fixed-Width 32-Bit Instructions

I would choose a **fixed-width 32-bit encoding**.  Every instruction is exactly
one word.  This simplifies the fetch unit enormously — you always read 4 bytes,
aligned, and you never need to handle variable-length boundaries.

ARM Thumb-2 and RISC-V both have variable-width extensions for code density,
but that is an optimisation I'd add only after the base design works.

### 1.2 Instruction Word Layout

```
 31      24 23   20 19   16 15   12 11          0
 +----------+-------+-------+-------+-------------+
 |  opcode  |  Rd   |  Rs1  |  Rs2  |  Imm[11:0]  |
 +----------+-------+-------+-------+-------------+
    8 bits    4 bits  4 bits  4 bits    12 bits
```

| Field | Width | Range | Reason |
|-------|-------|-------|--------|
| opcode | 8b | 256 opcodes | Room to grow; decode is a single byte switch |
| Rd | 4b | 16 registers | Enough for embedded; 32 registers would need wider fields |
| Rs1 | 4b | 16 registers | Source operand 1 |
| Rs2 | 4b | 16 registers | Source operand 2 |
| Imm12 | 12b | −2048..+2047 | Covers small struct offsets and loop counters |

**Corner case I always flag in interviews:** a 12-bit signed immediate cannot
hold a 32-bit address.  Loading addresses requires a two-step
`LOAD_IMM`+`SHL` sequence, or a dedicated `MOVW`/`MOVT` pair like ARM.
I'd document this as a **known limitation** rather than silently surprising
users.

### 1.3 Register File — 16 Registers

```
R0  – R12   General purpose
R13 = SP    Stack Pointer  (convention only; any instruction can read/write)
R14 = LR    Link Register  (CALL saves return address here)
R15 = PC    Program Counter (auto-advanced by +4 per fetch)
```

**Corner cases:**

- **Writing PC directly:** `MOV PC, R0` is a valid indirect jump.  The
  architecture must decide: does this take effect immediately (same cycle)
  or next cycle?  I choose **same cycle** — the execute stage writes the
  new PC and the fetch stage sees it on the next clock.
- **Reading PC:** `MOV R0, PC` gives `PC + 4` (the already-advanced value)
  because PC is incremented during fetch.  Compilers that generate
  PC-relative code must account for this +4 offset.  ARM has this exact
  quirk.
- **Writing SP to a misaligned address:** SP is conventionally 4-byte aligned.
  The design should either enforce alignment (trap on misaligned SP write) or
  document that misaligned PUSH/POP behaviour is undefined.

### 1.4 Status Flags

Four flags: **Z** (zero), **N** (negative), **C** (carry), **O** (overflow).

**Why four? Not two, not eight?**

- Z and N catch comparison outcomes for signed and unsigned equality/inequality.
- C distinguishes unsigned carry-out (for multi-word arithmetic).
- O catches signed overflow — essential for detecting wrap-around in signed
  arithmetic that would otherwise silently produce wrong results.

**Corner cases I always mention:**

- **SUB carry convention:** Two conventions exist.  ARM sets C when there is
  NO borrow (i.e., C = (a >= b) unsigned).  Other architectures set C on borrow.
  Choosing one and documenting it is more important than which you pick —
  mismatching between DIV/SUB and conditional branch semantics will silently
  produce wrong branch outcomes.

- **Flag-setting is selective:** NOT every instruction updates flags.  MOV,
  LOAD, STORE, PUSH, POP, NOP, branches — none of these modify flags.  This
  means flags are "live" across these instructions.  A programmer who inserts
  address arithmetic between a CMP and its conditional branch will silently
  get the wrong branch taken if the intervening instruction is a flag-modifying
  ALU op (SHL, ADD, etc.).  This is a classic **flag hazard**.

- **CMP vs SUB:** CMP is SUB with the result discarded.  Keeping them separate
  in the ISA makes the programmer's intent explicit and allows a future
  pipelined implementation to avoid the writeback stage for CMP.

---

## Part 2 — Memory Architecture

**Interviewer:** "How do you organise memory?"

---

### 2.1 Flat Linear Address Space

For an embedded single-threaded CPU with no OS:

- **No MMU, no virtual memory, no paging.**  Physical addresses only.
- **Flat 32-bit address space:** 0x00000000 – 0xFFFFFFFF (4 GiB addressable).
- In this simulator: a 1 MiB physical memory region is allocated.

```
0x000FFFFF ────────────────────────────  │
           STACK (grows downward ↓)      │  64 KiB
0x000F0000 ────────────────────────────  │
           DATA / HEAP                   │  448 KiB
0x00080000 ────────────────────────────  │
           CODE / TEXT                   │  448 KiB
0x00010000 ────────────────────────────  │
           BOOT / VECTOR TABLE           │  64 KiB
0x00000000 ────────────────────────────  │
```

**Why separate code/data regions?**  Even without an MMU, keeping them
separated makes it easier to later add write-protection (mark code pages
read-only) and detect code-into-data overwrites — the most common symptom
of a stack overflow or runaway pointer in embedded firmware.

### 2.2 Endianness — Little-Endian

I choose little-endian.  Reasons:
- Matches ARM (the dominant embedded target) and x86 (the primary development host).
- Avoids byte-swap cost when moving data between host development machine and
  the simulated CPU.
- The least-significant byte at the lowest address means a pointer to a 32-bit
  integer can be cast to a `uint8_t *` and the first byte is the LSB —
  consistent with most C toolchain assumptions.

**Corner case:** 16-bit and 8-bit accesses to a 32-bit word give the correct
sub-word value without masking only when endianness is consistent between
`mem_read8`, `mem_read16`, and `mem_read32`.  All three accessors must apply
the same byte ordering; mixing them is a source of subtle bugs.

### 2.3 Alignment

All 32-bit accesses must be **4-byte aligned** (address `& 3 == 0`).
16-bit accesses must be 2-byte aligned.

Options for unaligned accesses:
1. **Hardware alignment fault** — simplest; let the programmer fix it.
2. **Hardware fixup** — transparently handle by doing two aligned reads and
   merging.  Costs extra cycles.  ARM Cortex-M supports this.
3. **Undefined behaviour** — silently return garbage.  Never acceptable.

I'd go with option 1 (alignment fault) for a clean implementation.

### 2.4 Bounds Checking

In a simulator with a fixed physical memory size, **every single memory
access must be bounds-checked**.  The check must use `addr + (width-1)` not
just `addr`:

```c
if (addr + 3 >= mem->size) {   /* 32-bit: check last byte addr+3 */
    /* fault */
}
```

Checking only `addr` would miss a 4-byte read starting at the last valid byte.

---

## Part 3 — ALU Design

**Interviewer:** "Walk me through the ALU."

---

### 3.1 Pure Functional Unit

The ALU receives two 32-bit operands and an opcode.  It produces a 32-bit
result and updates the 4 status flags.  **It touches neither registers nor
memory** — those are the responsibility of the surrounding pipeline stages.

### 3.2 Operations

| Category | Operations |
|----------|-----------|
| Arithmetic | ADD, SUB, MUL, DIV |
| Bitwise | AND, OR, XOR, NOT |
| Shift | SHL (logical left), SHR (logical right) |
| Compare | CMP (SUB with no writeback) |

### 3.3 Corner Cases — Arithmetic

**ADD overflow detection:**

```c
uint64_t wide = (uint64_t)a + (uint64_t)b;
result = (uint32_t)wide;
C = (wide > 0xFFFFFFFF);                        // unsigned carry
O = ((~(a ^ b) & (a ^ result)) >> 31) & 1;     // signed overflow
```

Why a 64-bit intermediate?  Because `(uint32_t)a + (uint32_t)b` on a 32-bit
host can silently overflow in C, giving undefined behaviour.  Widening to 64
bits makes carry detection correct without any UB.

**SUB overflow detection:**

```c
result = a - b;
C = (a >= b);       // ARM convention: C=1 means NO borrow
O = (((a ^ b) & (a ^ result)) >> 31) & 1;
```

**MUL — upper 32 bits:**  `uint32_t × uint32_t` produces a 64-bit product.
This design discards the upper 32 bits silently.  A production CPU would
expose a `MULH` (multiply high) instruction so software can do 64-bit
multiplication without data loss.  I'd flag this as a known gap.

**DIV by zero:**  Two valid approaches:
1. **Soft error** — return 0, set a sticky flag, continue.  Simple but silent.
2. **Synchronous exception** — stop the CPU, transition to a fault handler.
   This is what ARM and x86 do.

For an embedded CPU without an exception vector table yet, option 1 is the
pragmatic choice.  I'd add a TODO to upgrade to option 2.

**Signed vs unsigned division:**  A single `DIV` opcode doing unsigned
division is insufficient for general C programs.  Some values are used as
signed integers.  A production ISA needs both `DIV` (unsigned) and
`DIVS` (signed).

### 3.4 Corner Cases — Shifts

```c
result = a << (b & 0x1F);   // mask to 5 bits — shift of 32 is UB in C
```

The C standard says shifting a 32-bit value by ≥ 32 bits is **undefined
behaviour**.  The mask `& 0x1F` prevents this.  On real hardware (x86, ARM),
the shift amount is similarly masked to 5 bits.

**Arithmetic right shift (ASR):**  A logical right shift (`SHR`) fills the
vacated high bits with 0.  An arithmetic right shift fills with the sign bit —
necessary for signed right-shift to preserve sign.  A complete ISA needs both.
This design only has `SHR` (logical) — another known gap.

---

## Part 4 — Decoder Design

**Interviewer:** "How does the decoder work?"

---

### 4.1 Decode Is Bit Extraction

```c
opcode = (raw >> 24) & 0xFF;
rd     = (raw >> 20) & 0x0F;
rs1    = (raw >> 16) & 0x0F;
rs2    = (raw >> 12) & 0x0F;
imm12  = raw & 0xFFF;
```

Then sign-extend the immediate:

```c
imm32 = (imm12 & 0x800)
          ? (int32_t)(imm12 | 0xFFFFF000u)
          : (int32_t)imm12;
```

**Corner case — sign extension:** The bit to test is bit 11 (value `0x800`,
not `0x1000`).  Off-by-one here makes all negative immediates look positive
and vice versa.  This is a common bug in first-time decoder implementations.

### 4.2 What the Decoder Does NOT Do

The decoder is strictly a bit-splitter.  It does not:
- Read registers
- Access memory
- Validate the opcode (that is the execute stage's job)
- Perform the operation

This separation means the decoder can be tested independently by feeding it
known bit patterns and checking field values.

### 4.3 Unknown Opcodes

If `opcode` has no entry in the execute dispatch table (`switch` default
case), the correct behaviour is a **hard fault** — not a no-op, not silent
continuation.  Silently treating an unknown opcode as a NOP is how you get
security vulnerabilities in real CPUs (speculative execution of "undefined"
opcodes was a source of Spectre-class issues).

---

## Part 5 — CPU Core and Pipeline Stage Design

**Interviewer:** "Now describe the execution engine itself."

---

### 5.1 Non-Pipelined: One Complete Cycle Per Instruction

```
┌────────────┐     ┌────────────┐     ┌────────────┐     ┌─────────────┐
│   FETCH    │────▶│   DECODE   │────▶│   EXECUTE  │────▶│  WRITEBACK  │
│ raw=MEM[PC]│     │ split bits │     │ ALU / MEM  │     │ Rd = result │
│ PC += 4    │     │ sign-ext   │     │ branch     │     │ flags       │
└────────────┘     └────────────┘     └────────────┘     └─────────────┘
```

The 4 stages run **sequentially within a single clock cycle**.  There is no
inter-stage hazard because there is no stage overlap.

**Interviewer:** "Why not pipeline it?"

Pipelining improves throughput (instructions per second) but introduces:
- **Data hazards:** `ADD R1, R0, R0` followed immediately by `ADD R2, R1, R1`
  — R1 is not ready yet when the second ADD reaches execute.  Requires either
  stalls or operand forwarding.
- **Control hazards:** Branch instructions — the next instruction to fetch is
  unknown until the branch is resolved in execute.  Requires branch prediction
  or pipeline flushing.
- **Structural hazards:** Memory access for LOAD/STORE conflicts with
  instruction fetch if there is only one memory bus.

For a first implementation and for a learning environment, non-pipelined is
the right choice.  You understand correctness completely before adding
performance complexity.

### 5.2 CPU State Machine

```
RESET ──────────────────▶ RUNNING
                              │
             cpu_step() = 0  ◀┘ (normal)
                              │
                    ┌─────────┴────────┐
              HALT  ▼                  ▼  FAULT
         (clean end)           (illegal op / OOB)
```

Four states because the distinctions matter:
- `RESET` vs `RUNNING`: prevents accidental execution before initialisation.
- `HALTED` vs `FAULT`: allows the calling code (main.c) to tell the difference
  between a clean stop and an error, and return different exit codes.

### 5.3 PC Management — Pre-Increment

```c
fetch_addr = PC;       // save for trace/debug
raw        = mem_read32(PC);
PC        += 4;        // pre-increment BEFORE execute
instr      = decode(raw);
execute(instr);        // branches overwrite PC as needed
```

PC is incremented **before** execute.  This means:
- When `CALL` executes, `LR = PC` already holds `(old_PC + 4)` — the correct
  return address.
- When a branch is not taken, PC is already pointing at the next instruction.
- When a branch IS taken, execute overwrites PC with the target address.

This is exactly how ARM and RISC-V work.  The alternative (post-increment) is
less natural for CALL/RET semantics.

### 5.4 The Execute Dispatch Table

```c
switch (instr->opcode) {
    case OP_ADD:     result = alu_add(Rs1, Rs2, &flags); REG(Rd) = result; break;
    case OP_LOAD:    addr = Rs1 + Imm; REG(Rd) = mem_read32(addr); break;
    case OP_JEQ:     if (flags.Z) PC = Imm; break;
    case OP_CALL:    LR = PC; PC = Imm; break;
    case OP_HALT:    state = HALTED; return 1;
    default:
        state = FAULT;
        return -1;   // hard fault
}
```

**Corner cases in execute:**

- **LOAD/STORE address calculation** must use `int32_t` arithmetic so negative
  immediates correctly subtract from the base: `(int32_t)Rs1 + Imm`, not
  `(uint32_t)Rs1 + (uint32_t)Imm`.  A wrong cast here makes negative offsets
  wrap to huge positive addresses.

- **PUSH pre-decrement / POP post-increment:**
  ```c
  case OP_PUSH:  SP -= 4; mem_write32(SP, REG(Rs1)); break;
  case OP_POP:   REG(Rd) = mem_read32(SP); SP += 4;  break;
  ```
  The order matters.  PUSH decrements first then writes.  POP reads first
  then increments.  Reversing either corrupts the stack.

- **HALT before writeback:** The HALT instruction must transition to
  `CPU_STATE_HALTED` and return before any writeback occurs.  There is no Rd
  to write — but the code must not fall through to a writeback path that
  writes a garbage result to register 0xFF.

- **Writeback for CMP:** CMP must NOT write to any register.  In the switch
  case for OP_CMP, call `alu_sub` (which sets flags) but do not assign the
  result to `REG(Rd)`.  An easy bug is copy-pasting from OP_SUB and forgetting
  to remove the writeback.

---

## Part 6 — Stack Design

**Interviewer:** "Tell me about the stack."

---

### 6.1 Conventions

```
SP = R13 (by convention, not hardware enforcement)
Stack grows DOWNWARD (toward lower addresses)
Initial SP = top of stack region - 4 = 0x000FFFFC
```

**PUSH:**  SP decrements BEFORE write (full-descending stack — ARM convention).
**POP:**   Read BEFORE SP increments.

### 6.2 Stack Overflow

Without an MMU there is **no hardware stack overflow protection**.  The stack
will silently grow into the data segment, then the code segment.  This is
catastrophic but the CPU never knows it's happening.

**Mitigation strategies:**

1. **Software stack canary:** Write a sentinel value at the stack bottom
   (`0x000F0000`) at startup.  Periodically check it is unchanged.  If
   corrupted → overflow detected.

2. **MPU (Memory Protection Unit):** An optional hardware extension that can
   mark the stack guard page as no-write.  Any write to that region traps.
   ARM Cortex-M has this.

3. **SP bounds check in PUSH:**  The simulator can optionally add a soft
   check: `if (SP < STACK_BOTTOM) { warn; }`.

**Interview answer:** In my 10 years writing Linux drivers, I've seen stack
overflows in kernel modules cause kernel panics where the only symptom was
`CONFIG_STACK_PROTECTOR` catching it — the canary was overwritten.  Same
principle applies at the hardware level.

---

## Part 7 — Assembler Design

**Interviewer:** "How does your assembler work?"

---

### 7.1 Two-Pass Design

| Pass | What It Does |
|------|-------------|
| **Pass 1** | Walk all lines.  Assign an address to every instruction (addr += 4).  Record label → absolute address.  Record constant = value. |
| **Pass 2** | Walk again.  Parse operands.  Substitute constants.  Resolve label references.  Encode each instruction as a 32-bit word. |

**Why two passes?**  A forward reference like `JGT LOOP` at the bottom of a
function references `LOOP` which was defined earlier — but a backward branch
at the top of a loop references `LOOP_END` which hasn't been seen yet.  You
can't resolve that in one pass.

**Corner case — label aliasing:**  What if a programmer names a label the same
as a register (`R0:`) or a constant (`NOP = 5`)? The assembler must check for
these conflicts and raise an error.

### 7.2 Immediate Validation

```python
if val < -2048 or val > 2047:
    raise ValueError(f"Immediate {val} does not fit in signed 12-bit [-2048..2047]")
return val & 0xFFF   # two's complement encoding
```

**Corner case — label addresses as jump targets:**  A label resolves to an
absolute address like `0x00010038`.  This is 65592, which is far outside
`[-2048, 2047]`.  The assembler must warn or error.  In this design, only the
lower 12 bits are encoded — which means only labels in the first 4096 bytes of
memory are addressable as direct jump targets.  For a real system, the fix is
a PC-relative jump (offset from current PC) rather than an absolute address.

---

## Part 8 — Single-Threaded Guarantee in Software

**Interviewer:** "You said 'single-threaded CPU' — how do you guarantee that
at the OS level when the simulator runs on a Linux host?"

---

### 8.1 CPU Affinity Pinning

```c
cpu_set_t mask;
CPU_ZERO(&mask);
CPU_SET(0, &mask);   // core 0 only
sched_setaffinity(0, sizeof(mask), &mask);
```

`sched_setaffinity()` is a Linux system call (via `<sched.h>`) that restricts
the process to a set of hardware cores.  By restricting to `{core 0}`:

- The Linux scheduler will never migrate the process to another core.
- The simulation runs on exactly one physical core — mirroring the
  single-threaded CPU it models.
- Cache state is stable on that core — memory accesses don't cause cross-core
  cache invalidations.

**Corner case:** `sched_setaffinity()` requires `_GNU_SOURCE` to be defined.
On non-Linux platforms (macOS, Windows) it doesn't exist.  The code should
`#ifdef __linux__` guard it and gracefully degrade.

**Corner case:** The call can fail if the process does not have permission to
set affinity (some container environments restrict this).  It should emit a
warning but not abort — the simulation still runs correctly, just potentially
on any core.

**Interviewer:** "Why does this matter for correctness?"

For a functional simulation at the instruction level, it doesn't.  For
**performance measurement** (cycle counting, timing) it matters enormously.
Migration between cores resets hardware performance counters and causes cache
thrashing.  Pinning to one core gives deterministic, reproducible timing.

This is exactly the same argument I'd make when writing a Linux driver that
uses `per_cpu` data structures — you pin work to a specific CPU to avoid
race conditions when accessing per-CPU state.

---

## Part 9 — Testing and Validation Strategy

**Interviewer:** "How would you test this CPU?"

---

### 9.1 Unit Test Each Subsystem

| Subsystem | Test Approach |
|-----------|--------------|
| `alu_add()` | Test with 0+0, max+max (carry), INT_MAX+1 (overflow), negative+negative |
| `alu_sub()` | Test with a-a=0 (Z), a-0=a, 0-1 (borrow/N), signed underflow |
| `decode_instruction()` | Feed known bit patterns, check all 5 fields including sign-extended imm |
| `mem_read32 / write32` | Test boundary addresses (0, MEM_SIZE-4, MEM_SIZE-3 = OOB) |
| `cpu_step()` | One instruction at a time; check register state after each |

### 9.2 Integration Tests

1. **Fibonacci test** — known correct output, easy to hand-verify.
2. **Call/return stack balance** — SP must equal initial value after balanced PUSH/POP.
3. **Flag propagation** — CMP then JEQ/JNE; verify branch taken only when correct.
4. **Boundary LOAD/STORE** — access last valid word (MEM_SIZE - 4); access
   MEM_SIZE (must fault).
5. **HALT exit code** — verify `cpu_run()` returns 0, not -1.

### 9.3 Regression Tests for Known Bugs

Every bug found in the 10 scenarios (Log_Analysis/) should become a regression
test with an assertion on the output state.  Specifically:

- Div-by-zero: result must be 0, simulator must NOT crash.
- Infinite loop: max_cycles must be respected exactly.
- Off-by-one: counter value at loop exit is N, not N-1.
- Flag hazard: test that SHL updates Z correctly.

---

## Part 10 — Design Gaps and What I'd Add Next

**Interviewer:** "What would you add if you had another week?"

---

| Gap | Priority | Reason |
|-----|----------|--------|
| Interrupt / exception vector table | P0 | Without it, div-by-zero, illegal opcode, alignment faults can't be handled gracefully — they just kill the process |
| PC-relative branches | P0 | Current absolute Imm12 jumps only reach ±2047 bytes from address 0; this breaks any program > 4 KiB |
| `MOVW` / `MOVT` (32-bit load) | P1 | Eliminates the shift trick; critical for working with 32-bit constants naturally |
| Arithmetic right shift (`ASR`) | P1 | Required for correct C signed-division and right-shift semantics |
| Signed division (`DIVS`) | P1 | `DIV` is unsigned; C programs need signed division |
| Multi-word multiply (`MULH`) | P1 | Upper 32 bits of a multiply result are currently silently discarded |
| Memory Protection Unit | P2 | Stack overflow detection, RX/RW page permissions |
| Privilege levels (user/kernel) | P2 | Required for any OS kernel support |
| Hardware timer + IRQ line | P2 | Enables preemptive scheduling and watchdog |
| Cache model | P3 | For accurate performance simulation |

---

## Part 11 — Deep Technical Corner Cases (Whiteboard Questions)

**Interviewer:** "Give me a list of the subtlest bugs you'd watch out for."

---

### 1. The `int32_t` vs `uint32_t` Address Bug

```c
// WRONG: unsigned - may not give correct negative offset
uint32_t addr = REG(rs1) + (uint32_t)instr->imm;

// RIGHT: signed offset + unsigned base
uint32_t addr = (uint32_t)((int32_t)REG(rs1) + instr->imm);
```

If `imm = -4` and `Rs1 = 0x00080008`, you want `0x00080004`.  With unsigned
arithmetic, `-4` becomes `0xFFFFFFFC` and the addition wraps to `0x00080004`
(coincidentally correct on wrap), but bounds checking then sees `0x00080004`
after a huge intermediate value, which might incorrectly fault.  Use
`int32_t` arithmetic explicitly.

### 2. The Sign-Extension Off-By-One

```c
// WRONG: tests bit 12 (the 13th bit, which is in Rs2 field)
imm32 = (imm12 & 0x1000) ? (imm12 | 0xFFFFF000u) : imm12;

// RIGHT: tests bit 11 (the MSB of a 12-bit value)
imm32 = (imm12 & 0x800) ? (imm12 | 0xFFFFF000u) : imm12;
```

### 3. PC Value Visible to the Executing Instruction

When a `LOAD R0, [PC + #0]` instruction executes, what value does `PC` hold?
Answer: `fetch_addr + 4` (the instruction after the LOAD), because PC was
pre-incremented in the fetch stage.  A programmer who writes a PC-relative
load must add 4 to their expected offset.  ARM has this exact quirk and it's
documented explicitly — "PC reads as the address of the current instruction
plus 4."

### 4. CALL Saves PC Not fetch_addr

```c
case OP_CALL:
    LR = PC;          // PC was already incremented in fetch: = fetch_addr + 4
    PC = instr->imm;  // jump
```

`LR = fetch_addr + 4` — the return address is the instruction after the CALL.
If someone wrote `LR = fetch_addr`, return would re-execute the CALL
instruction forever.

### 5. Stack Misalignment on PUSH to Odd SP

SP convention: always 4-byte aligned.  If user code does `ADD SP, SP, #2`
and then `PUSH R0`, the write `MEM32[SP-4]` = `MEM32[SP-4]` lands on an
unaligned address.  The sim either needs to enforce SP alignment on PUSH or
accept misaligned writes (which on real hardware can cross cache line
boundaries and cause subtle ordering issues in multi-core systems — irrelevant
here but a real concern in Linux driver code).

### 6. Register Index Masking

```c
#define REG(n)  (cpu->regs[(n) & 0x0F])
```

Even if the decoder guarantees 4-bit register indices, the `& 0x0F` mask
costs nothing and prevents a malformed test from causing an out-of-bounds
array access.  Defence in depth at no performance cost.

### 7. Byte Order in the Binary File

The assembler writes `struct.pack('<I', word)` — explicitly little-endian.
If the assembler ran on a big-endian host (rare today but possible), writing
`struct.pack('I', word)` (no `<`) would produce a big-endian binary.  The
simulator loads with `memcpy` + `mem_read32` which reconstructs little-endian.
The explicit `<` eliminates the host-endian dependency.

### 8. The `HALT` Opcode Value `0xFF`

Choosing `0xFF` as HALT means a region of zeroed memory generates `0x00000000`
which decodes as `OP_NOP` (0x00) — a NOP, not a HALT.  This is useful:
if the PC runs off the end of the loaded program, it will execute NOPs until
either max_cycles triggers or it reaches a HALT that was explicitly placed.
A less careful design using `0x00` as HALT would silently halt on any
uninnitalised memory, masking address bugs.

---

## Summary

**Interviewer:** "Wrap it up — what makes this a good CPU design?"

---

**Candidate:**

A good CPU design at this level has five properties:

1. **Correct semantics at every edge case** — flag conventions documented,
   sign-extension correct, PC pre-increment consistent with CALL/RET.

2. **Hard failures that are easy to see** — illegal opcode faults rather than
   silently NOPing, bounds-checked memory rather than silently reading garbage.

3. **Separation of concerns** — ALU is pure, decoder is pure bit-extraction,
   CPU core orchestrates them.  Each is independently testable.

4. **Known limitations documented, not hidden** — 12-bit immediate, no ASR,
   unsigned DIV only, no interrupt support.  A design with undocumented
   corners is a ticking bomb in production.

5. **Single-threaded guarantee is mechanical, not just by convention** —
   `sched_setaffinity` instead of "just don't create threads".

> "From my 10 years writing Linux drivers, the most dangerous code isn't the
> code that crashes — it's the code that silently does the wrong thing.  The
> same principle applies to CPU design: every ambiguous case should resolve to
> a loud failure or a documented, predictable behaviour.  Never silent
> undefined behaviour."
