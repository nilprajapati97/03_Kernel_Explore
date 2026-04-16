# Module 01 — Single-Threaded CPU

> **Kernel concepts covered:**  
> CPU context, general-purpose registers, program counter, stack pointer,
> fetch-decode-execute pipeline, single-core execution model, CPU affinity
> (`sched_setaffinity`), process pinning, instruction encoding/decoding.

---

## Table of Contents

1. [Overview](#overview)
2. [What Is a CPU Context?](#what-is-a-cpu-context)
3. [The Fetch-Decode-Execute Pipeline](#the-fetch-decode-execute-pipeline)
4. [Custom ISA Design](#custom-isa-design)
5. [Implementation Walkthrough](#implementation-walkthrough)
   - [Memory (`memory.h / memory.c`)](#memory-memoryh--memoryc)
   - [ALU (`alu.h / alu.c`)](#alu-aluh--aluc)
   - [Instruction Encoding (`instructions.h`)](#instruction-encoding-instructionsh)
   - [Decoder (`decoder.h / decoder.c`)](#decoder-decoderh--decoderc)
   - [CPU Engine (`cpu.h / cpu.c`)](#cpu-engine-cpuh--cpuc)
   - [Assembler (`tools/assembler.py`)](#assembler-toolsassemblerpy)
   - [Test Program (`programs/test_fibonacci.asm`)](#test-program-programstest_fibonacciasm)
6. [Mapping to the Real Linux Kernel](#mapping-to-the-real-linux-kernel)
7. [CPU Affinity — Why It Matters](#cpu-affinity--why-it-matters)
8. [Build and Run](#build-and-run)
9. [Expected Output](#expected-output)
10. [Exercises](#exercises)
11. [What's Next](#whats-next)

---

## Overview

Before you can understand how the Linux kernel manages processes, schedules
threads, or handles interrupts, you need a solid mental model of what a CPU
actually *is* from the kernel's point of view.

This module builds a **minimal but complete CPU simulator** — a RISC processor
with:

- 16 × 32-bit general-purpose registers (R0–R15)
- A program counter (PC = R15), stack pointer (SP = R13), link register (LR = R14)
- Four status flags: **Z**ero, **N**egative, **C**arry, **O**verflow
- A flat 1 MiB simulated memory
- A single fetch-decode-execute pipeline (one instruction per "clock cycle")
- A Python assembler so you can write and run programs on it

The simulator is cross-compiled for ARM and run on a BeagleBone Black (or
inside QEMU), where it is pinned to a single hardware core — mirroring the
hardware reality of a single-core AM335x SoC.

---

## What Is a CPU Context?

In the Linux kernel, a **CPU context** (also called *processor state* or
*thread state*) is the complete set of information that describes what a CPU
is doing at any instant. When the kernel switches from one task to another, it
saves the outgoing task's context and restores the incoming task's context.

The kernel represents this as part of `struct task_struct`
([kernel: include/linux/sched.h]) and the architecture-specific
`struct thread_struct` ([kernel: arch/arm/include/asm/processor.h]).

For our simulator, the context is the `CPU` struct in `src/cpu.h`:

```c
typedef struct {
    uint32_t  regs[NUM_REGS];  /* R0–R15: GP registers + PC/SP/LR  */
    Flags     flags;           /* Z, N, C, O status flags           */
    Memory   *mem;             /* Pointer to the shared memory      */
    CpuState  state;           /* RESET / RUNNING / HALTED / FAULT  */
    uint64_t  cycle_count;     /* Instruction throughput counter    */
    uint32_t  entry_point;     /* Starting PC value                 */
    int       trace;           /* Trace-mode flag                   */
} CPU;
```

**Registers are the fast, on-chip storage that hold the CPU's live working
state.** Everything else (data, code, stack) lives in memory and must be
loaded into registers before the CPU can act on it.

### Register Map

| Register | Index | Role |
|----------|-------|------|
| R0–R12 | 0–12 | General purpose |
| R13 (SP) | 13 | Stack pointer — points to the top of the call stack |
| R14 (LR) | 14 | Link register — holds the return address for `CALL` |
| R15 (PC) | 15 | Program counter — address of the *next* instruction to fetch |

This layout directly mirrors the ARM architecture used in the Linux kernel on
32-bit ARM platforms ([kernel: arch/arm/include/asm/ptrace.h]).

---

## The Fetch-Decode-Execute Pipeline

Every CPU repeats the same three-stage loop indefinitely:

```
┌──────────────────────────────────────────────────────────────────────┐
│                  Fetch-Decode-Execute Cycle                          │
│                                                                      │
│   ┌─────────┐       ┌─────────┐       ┌─────────────────────────┐  │
│   │  FETCH  │──────▶│ DECODE  │──────▶│  EXECUTE  (+ WRITEBACK) │  │
│   └─────────┘       └─────────┘       └─────────────────────────┘  │
│                                                                      │
│  1. Read MEM32[PC]   Splits raw       Dispatches to ALU, memory     │
│  2. PC += 4          32-bit word      or branch handler; writes     │
│                      into fields      result back to Rd             │
└──────────────────────────────────────────────────────────────────────┘
```

In `cpu_step()` ([src/cpu.c]):

```c
/* Stage 1: FETCH */
uint32_t fetch_addr = PC;
uint32_t raw        = mem_read32(cpu->mem, PC);
PC += 4;

/* Stage 2: DECODE */
Instruction instr = decode_instruction(raw);

/* Stage 3+4: EXECUTE + WRITEBACK */
return execute(cpu, &instr);
```

Real processors add pipeline stages (IF, ID, EX, MEM, WB in classic 5-stage
RISC), branch prediction, out-of-order execution, and superscalar issue — but
the fundamental loop is always the same. The Linux kernel's scheduling,
interrupt handling and system-call entry all work *around* this loop.

---

## Custom ISA Design

The simulator uses a **custom 32-bit RISC ISA** (Instruction Set Architecture)
defined in `src/instructions.h`. Every instruction is exactly 32 bits wide,
which simplifies decoding enormously.

### Instruction Encoding

```
 31      24 23   20 19   16 15   12 11          0
 +----------+-------+-------+-------+-------------+
 |  opcode  |  Rd   |  Rs1  |  Rs2  |  Imm[11:0]  |
 +----------+-------+-------+-------+-------------+
    8 bits    4 bits  4 bits  4 bits    12 bits
```

- **opcode** (bits 31–24): 8 bits → 256 possible instructions (we use ~26).
- **Rd** (bits 23–20): 4 bits → destination register R0–R15.
- **Rs1** (bits 19–16): 4 bits → first source register.
- **Rs2** (bits 15–12): 4 bits → second source register.
- **Imm** (bits 11–0): 12-bit signed immediate, sign-extended to 32 bits.

This is directly comparable to RISC-V's R-type and I-type instruction formats
([kernel: arch/riscv/include/asm/]) and ARM's encoding
([kernel: arch/arm/include/asm/]).

### Opcode Table

| Mnemonic | Opcode | Operation |
|----------|--------|-----------|
| `NOP` | 0x00 | No operation |
| `LOAD_IMM Rd, #imm` | 0x01 | `Rd = sign_ext(imm)` |
| `LOAD Rd, [Rs1+#imm]` | 0x02 | `Rd = MEM32[Rs1 + imm]` |
| `STORE [Rs1+#imm], Rs2` | 0x03 | `MEM32[Rs1 + imm] = Rs2` |
| `ADD Rd, Rs1, Rs2` | 0x04 | `Rd = Rs1 + Rs2` (sets Z,N,C,O) |
| `SUB Rd, Rs1, Rs2` | 0x05 | `Rd = Rs1 - Rs2` (sets Z,N,C,O) |
| `MUL Rd, Rs1, Rs2` | 0x06 | `Rd = Rs1 * Rs2` (sets Z,N) |
| `DIV Rd, Rs1, Rs2` | 0x07 | `Rd = Rs1 / Rs2` (sets Z,N) |
| `AND/OR/XOR Rd, Rs1, Rs2` | 0x08–0x0A | Bitwise ops (set Z,N) |
| `NOT Rd, Rs1` | 0x0B | `Rd = ~Rs1` (sets Z,N) |
| `SHL/SHR Rd, Rs1, Rs2` | 0x0C–0x0D | Logical shifts (set Z,N,C) |
| `MOV Rd, Rs1` | 0x0E | `Rd = Rs1` |
| `CMP Rs1, Rs2` | 0x0F | Set flags for `Rs1 - Rs2`, discard result |
| `JMP/JEQ/JNE/JGT/JLT` | 0x10–0x14 | PC-relative branches |
| `CALL` | 0x15 | `LR = PC; PC += imm` |
| `RET` | 0x16 | `PC = LR` |
| `PUSH Rs1` | 0x17 | `SP -= 4; MEM32[SP] = Rs1` |
| `POP Rd` | 0x18 | `Rd = MEM32[SP]; SP += 4` |
| `HALT` | 0xFF | Stop execution |

---

## Implementation Walkthrough

### Memory (`memory.h / memory.c`)

The simulated memory is a heap-allocated 1 MiB byte array.

```
Address Space (0x00000000 – 0x000FFFFF):
┌─────────────────────────────────────┐ 0x00000000
│  Interrupt vector / boot area       │
│  (64 KiB, 0x0000–0xFFFF)           │
├─────────────────────────────────────┤ 0x00010000
│  Program / code segment             │
│  (448 KiB)                          │  ← MEM_PROG_BASE
├─────────────────────────────────────┤ 0x00080000
│  Data / heap segment                │
│  (448 KiB)                          │
├─────────────────────────────────────┤ 0x000F0000
│  Stack segment                      │
│  (64 KiB, grows DOWN from 0xFFFFFC) │  ← MEM_STACK_TOP
└─────────────────────────────────────┘ 0x000FFFFF
```

All 32-bit accesses use **little-endian** byte order — the same as x86 and ARM
in little-endian mode. A bounds-check helper aborts on any out-of-range access,
mimicking a hardware bus fault.

**Kernel parallel:** The kernel's physical memory map is set up by the
architecture-specific `setup_arch()` function
([kernel: arch/arm/kernel/setup.c], [kernel: arch/x86/kernel/setup.c]).
Virtual memory mappings are managed via page tables (see Module 05).

### ALU (`alu.h / alu.c`)

Pure functions: take two inputs and a `Flags *`, compute a result, update the
flags, and return. The ALU never touches registers or memory.

```c
uint32_t alu_add(uint32_t a, uint32_t b, Flags *f)
{
    uint64_t wide   = (uint64_t)a + (uint64_t)b;
    uint32_t result = (uint32_t)wide;

    f->Z = (result == 0);                     /* zero flag    */
    f->N = (result >> 31) & 1;               /* negative flag */
    f->C = (wide > UINT32_MAX);              /* carry flag    */
    /* signed overflow: both inputs same sign, result different sign */
    f->O = ((~(a ^ b) & (a ^ result)) >> 31) & 1u;

    return result;
}
```

**Flag semantics** (identical to ARM CPSR / x86 EFLAGS):

| Flag | Set when |
|------|----------|
| Z (Zero) | Result == 0 |
| N (Negative) | Result bit 31 == 1 (signed negative) |
| C (Carry) | Unsigned overflow on ADD; no borrow on SUB |
| O (Overflow) | Signed overflow (result wrong sign) |

**Kernel parallel:** On x86, the kernel tests `EFLAGS` after arithmetic
instructions. On ARM, the `CPSR` (Current Program Status Register) holds the
equivalent flags. The kernel uses flag-setting instructions like `TEST`,
`CMP`, and `SUBS` to drive conditional branches without wasting a register.

### Instruction Encoding (`instructions.h`)

Defines the `Opcode` enum, the `Instruction` decoded struct, and two inline
helpers:

```c
/* Pack a 32-bit instruction word */
static inline uint32_t instr_encode(uint8_t op, uint8_t rd,
                                    uint8_t rs1, uint8_t rs2,
                                    int16_t imm12);

/* Sign-extend a 12-bit value to 32 bits */
static inline int32_t sign_ext12(uint32_t val);
```

Sign extension is a fundamental operation: a 12-bit immediate stored in the
low bits of a 32-bit word must be extended to a full 32-bit signed integer
before use. The rule is: copy bit 11 (the sign bit of the 12-bit field) into
bits 31–12.

### Decoder (`decoder.h / decoder.c`)

Two responsibilities:

1. **`decode_instruction(raw)`** — unpack the four fields from a 32-bit word
   into an `Instruction` struct, sign-extending the immediate.
2. **`disasm_instruction(instr, buf, buf_size)`** — produce a human-readable
   string like `"ADD        R2, R0, R1"` for trace output and debugging.

The decoder has no side-effects; it is purely a bit-manipulation function.
This is important because the trace path calls the disassembler *before*
execute, which means a disassembly of an illegal instruction will still
be printed before the fault.

### CPU Engine (`cpu.h / cpu.c`)

The heart of the simulator.

**`cpu_init(cpu, mem)`** — zeroes the struct and links the memory object.

**`cpu_reset(cpu, entry_point)`** — zeroes registers and flags, sets
`PC = entry_point`, `SP = MEM_STACK_TOP`, and transitions to
`CPU_STATE_RUNNING`.

**`cpu_step(cpu)`** — one fetch-decode-execute cycle:

```
State machine:
  RESET  → (cpu_reset called) →  RUNNING
  RUNNING → (HALT instruction) → HALTED
  RUNNING → (illegal opcode)   → FAULT
  HALTED / FAULT               → returns immediately (idempotent)
```

**`cpu_run(cpu, max_cycles)`** — loops `cpu_step()` until HALT, FAULT, or
cycle limit. This is the equivalent of the "idle loop" in a bare-metal system.
In a real kernel, the idle loop is `cpu_idle()` ([kernel: kernel/sched/idle.c]).

**`cpu_dump_state(cpu)`** — prints a register dump and flags, useful for
post-execution inspection:

```
╔══════════════════════════════════════════════════╗
║              CPU STATE DUMP                      ║
╠══════════════════════════════════════════════════╣
  State      : HALTED
  Cycles     : 56
  Flags      : Z=0 N=0 C=0 O=0
  Registers:
    R0  = 0x00000001  (1)
    R1  = 0x00000015  (21)
    ...
╚══════════════════════════════════════════════════╝
```

### Assembler (`tools/assembler.py`)

A two-pass assembler:

- **Pass 1** — scan for label definitions and constant assignments; build
  a symbol table mapping each label to its absolute address.
- **Pass 2** — assemble each instruction, resolving labels to
  PC-relative offsets for branch/call instructions.

PC-relative offset encoding:

```
At execute time, PC = fetch_addr + 4 (already post-incremented)
Branch target   = fetch_addr + 4 + imm
Therefore:       imm = target_addr − (fetch_addr + 4)
```

This is the same convention used by ARM's B/BL instructions and RISC-V's
JAL/JALR instructions.

### Test Program (`programs/test_fibonacci.asm`)

Computes Fibonacci(0)–Fibonacci(9) and stores them in memory at `0x00080000`.

```asm
; Initialise: R1=F(n-2)=0, R2=F(n-1)=1, R0=loop counter=8
; R4 = data base address (0x00080000 via two-step shift)
; R5 = stride (4), R6 = decrement (1)

LOOP:
    ADD   R3, R1, R2      ; F(n) = F(n-2) + F(n-1)
    STORE [R4 + #0], R3   ; store F(n) to memory
    MOV   R1, R2           ; advance: F(n-2) = F(n-1)
    MOV   R2, R3           ; advance: F(n-1) = F(n)
    ADD   R4, R4, R5       ; pointer += 4
    SUB   R0, R0, R6       ; counter--
    CMP   R0, R6           ; compare counter with 1
    JGT   LOOP             ; if counter > 1, loop again
```

Expected memory layout after HALT:

```
0x00080000: 00 00 00 00  ← F(0) = 0
0x00080004: 01 00 00 00  ← F(1) = 1
0x00080008: 01 00 00 00  ← F(2) = 1
0x0008000C: 02 00 00 00  ← F(3) = 2
0x00080010: 03 00 00 00  ← F(4) = 3
0x00080014: 05 00 00 00  ← F(5) = 5
0x00080018: 08 00 00 00  ← F(6) = 8
0x0008001C: 0D 00 00 00  ← F(7) = 13
0x00080020: 15 00 00 00  ← F(8) = 21
0x00080024: 22 00 00 00  ← F(9) = 34
```

---

## Mapping to the Real Linux Kernel

| Simulator concept | Real kernel equivalent |
|-------------------|----------------------|
| `CPU` struct | `struct thread_info` + arch `struct pt_regs` |
| `cpu->regs[]` | `struct pt_regs` fields (e.g., `regs->ARM_r0` … `regs->ARM_pc`) |
| `cpu->flags` | ARM `CPSR` / x86 `EFLAGS` register |
| `cpu_reset()` | Architecture `start_kernel()` + `cpu_init()` |
| `cpu_step()` | The processor's microcode fetch-decode-execute loop |
| `cpu_run()` | `cpu_startup_entry()` → `do_idle()` loop |
| `cpu->state == HALTED` | Processor powered off (`cpu_die()`) |
| `MEM_STACK_TOP` | Per-thread kernel stack (`THREAD_SIZE` bytes) |
| `mem_read32 / mem_write32` | Memory-mapped I/O (`readl()` / `writel()`) |
| `MEM_PROG_BASE` | `.text` section load address (set by linker script) |

### Key kernel files to read next

| File | What it shows |
|------|--------------|
| `arch/arm/include/asm/ptrace.h` | `struct pt_regs` — saved register state |
| `arch/arm/include/asm/processor.h` | `struct thread_struct` — per-thread CPU state |
| `arch/x86/include/asm/ptrace.h` | x86 equivalent of pt_regs |
| `kernel/sched/idle.c` | The idle loop (`cpu_startup_entry`) |
| `arch/arm/kernel/entry-armv.S` | ARM exception vector table + register save/restore |
| `arch/x86/kernel/entry_64.S` | x86-64 syscall/interrupt entry, register save/restore |

---

## CPU Affinity — Why It Matters

When `cpu_sim` starts, it calls:

```c
cpu_set_t mask;
CPU_ZERO(&mask);
CPU_SET(0, &mask);
sched_setaffinity(0, sizeof(mask), &mask);
```

This pins the *host process* (running the simulator) to CPU core 0 of the
physical machine, so the simulation is genuinely running single-threaded on
one hardware core — just like the BeagleBone Black's AM335x SoC, which has
only one ARM Cortex-A8 core.

**Kernel parallel:** `sched_setaffinity()` calls into the kernel's
`kernel/sched/core.c:sched_setaffinity()`, which modifies the task's
`cpus_allowed` mask in `struct task_struct`. The scheduler then only places
the task on cores in that mask.

This is used in real systems for:

- **Real-time tasks** that must run on a dedicated core.
- **NUMA-aware placement** to keep a task near its memory.
- **Isolation** (CPU shielding) for latency-sensitive workloads.
- **Testing**: ensuring a simulation runs without OS-level preemption
  interfering with cycle counts.

---

## Build and Run

### Prerequisites

```bash
# Ubuntu / Debian
sudo apt update
sudo apt install -y gcc python3 make qemu-user gcc-arm-linux-gnueabihf
```

### Build and run (native x86/x86-64)

```bash
cd 01_SingleThreaded_CPU/Implementation

make run
# Builds cpu_sim, assembles test_fibonacci.asm, runs with --trace + --dump-mem
```

### Build and run (ARM under QEMU user-mode)

```bash
make run-arm
# Cross-compiles to cpu_sim_arm, runs under qemu-arm
```

### Build and run (full BeagleBone Black system emulation)

```bash
bash qemu/setup_qemu.sh    # downloads Alpine Linux kernel + builds initramfs
make run-fullsystem        # boots full qemu-system-arm with Alpine Linux
```

See [qemu/README_QEMU.md](Implementation/qemu/README_QEMU.md) for detailed
QEMU instructions.

### Individual make targets

```
make native        Build host binary (cpu_sim)
make arm           Cross-compile ARM binary (cpu_sim_arm)
make assemble      Assemble programs/test_fibonacci.asm → .bin
make run           native + assemble + run with trace + memory dump
make run-arm       arm + assemble + run under qemu-arm
make clean         Remove build artefacts
make help          Show all targets
```

---

## Expected Output

```
[SYS] Process pinned to CPU core 0 (single-threaded model)
[SIM] Loaded 'programs/test_fibonacci.bin': 92 bytes (23 instructions)
[SIM] Program loaded at 0x00010000 – 0x0001005B
[SIM] CPU reset. Entry point: 0x00010000
[SIM] Trace mode ON

  [     1] 0x00010000: LOAD_IMM   R1, #0              | Z=1 N=0 C=0 O=0
  [     2] 0x00010004: LOAD_IMM   R2, #1              | Z=0 N=0 C=0 O=0
  ...
  [    56] 0x00010054: HALT                           | Z=0 N=0 C=1 O=0

[SIM] *** HALT reached normally ***

╔══════════════════════════════════════════════════╗
║              CPU STATE DUMP                      ║
╠══════════════════════════════════════════════════╣
  State      : HALTED
  Cycles     : 56
  ...
╚══════════════════════════════════════════════════╝

[SIM] Memory dump at 0x00080000, 40 bytes:
  0x00080000: 00 00 00 00 01 00 00 00 01 00 00 00 02 00 00 00
  0x00080010: 03 00 00 00 05 00 00 00 08 00 00 00 0D 00 00 00
  0x00080020: 15 00 00 00 22 00 00 00
```

---

## Exercises

1. **Add a new instruction**: Add `MOD Rd, Rs1, Rs2` (modulo). You will need
   to update `instructions.h` (new opcode), `alu.h/.c` (new function),
   `cpu.c` (new case in `execute()`), `decoder.c` (new disasm case), and
   `assembler.py` (new mnemonic).

2. **Interrupt simulation**: Add an `INT` instruction that saves PC and flags
   to a fixed "exception vector" address and jumps to a handler at address
   `0x00000004`. This models how ARM's exception vectors work
   ([kernel: arch/arm/kernel/entry-armv.S]).

3. **Pipeline stages**: Add a simple 2-stage pipeline (IF + EX) with a
   pipeline register between them. Observe what happens when a branch
   instruction is in the pipeline — you will need to implement a "branch
   flush" (pipeline stall). This is the fundamental reason real CPUs have
   branch predictors.

4. **Write a second program**: Implement `bubble_sort.asm` that sorts an
   array of 8 integers stored in the data segment. Use `LOAD`, `STORE`,
   `CMP`, `JGT` and a nested loop.

---

## What's Next

- **Module 02** — Multi-Threaded CPU & Context Switch: extend this simulator
  to support two "threads" (two `CPU` structs sharing one `Memory`), and
  implement a round-robin scheduler that switches between them every N cycles.
  This directly models `switch_to()` in the kernel
  ([kernel: arch/arm/include/asm/switch_to.h]).

- Read the kernel's idle loop:
  `kernel/sched/idle.c` → `do_idle()` → `cpuidle_idle_call()`

- Read how the kernel saves CPU state on a system call entry:
  `arch/x86/kernel/entry_64.S` → `SYM_CODE_START(entry_SYSCALL_64)`
