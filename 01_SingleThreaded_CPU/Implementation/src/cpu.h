/*
 * cpu.h — CPU context and execution engine for the custom CPU simulator
 *
 * The CPU models a single-core, single-threaded RISC processor.
 * External model (BeagleBone Black context): when cross-compiled to ARM and
 * run on a BeagleBone Black (or inside qemu-system-arm), this process is
 * pinned to CPU core 0 so the simulator itself runs truly single-threaded on
 * the physical hardware.
 *
 *  State per cycle (fetch → decode → execute → writeback):
 *   1. Fetch:   Read MEM32[PC]
 *   2. Decode:  decode_instruction() → Instruction
 *   3. Execute: dispatch to ALU / memory / branch handler
 *   4. Writeback: write result to Rd; advance PC
 *
 * Trace mode: when enabled, each cycle prints a disassembly + register dump.
 */

#ifndef CPU_H
#define CPU_H

#include <stdint.h>
#include "instructions.h"
#include "memory.h"
#include "alu.h"

/* ── CPU run-state ────────────────────────────────────────────────────────── */
typedef enum {
    CPU_STATE_RESET   = 0,  /* Not yet initialised or held in reset        */
    CPU_STATE_RUNNING = 1,  /* Executing instructions                      */
    CPU_STATE_HALTED  = 2,  /* HALT instruction encountered                */
    CPU_STATE_FAULT   = 3,  /* Unrecoverable error (illegal opcode, etc.)  */
} CpuState;

/* ── CPU context ──────────────────────────────────────────────────────────── */
typedef struct {
    uint32_t  regs[NUM_REGS];  /* General-purpose registers R0–R15         */
    Flags     flags;           /* Status flags: Z, N, C, O                 */
    Memory   *mem;             /* Pointer to the shared simulated memory   */
    CpuState  state;           /* Current run-state                        */
    uint64_t  cycle_count;     /* Total number of fetch-decode-execute cycles */
    uint32_t  entry_point;     /* PC value set by cpu_reset()              */
    int       trace;           /* Non-zero → print trace each cycle        */
} CPU;

/* ── Lifecycle ────────────────────────────────────────────────────────────── */

/**
 * cpu_init() — Attach the CPU to a Memory object and zero all state.
 */
void cpu_init(CPU *cpu, Memory *mem);

/**
 * cpu_reset() — Reset registers, flags, cycle counter to a clean state.
 * Sets PC to `entry_point`.  Does not modify memory.
 */
void cpu_reset(CPU *cpu, uint32_t entry_point);

/* ── Execution ────────────────────────────────────────────────────────────── */

/**
 * cpu_step() — Execute exactly one fetch-decode-execute cycle.
 *
 * Returns:
 *   0  — instruction executed normally; CPU still running.
 *   1  — HALT instruction reached; CPU transitions to CPU_STATE_HALTED.
 *  -1  — Fault (illegal opcode, memory fault, etc.); CPU → CPU_STATE_FAULT.
 */
int cpu_step(CPU *cpu);

/**
 * cpu_run() — Execute instructions until HALT or FAULT.
 * If `max_cycles` > 0 the loop stops after that many cycles even if the
 * program has not halted (returns -2 on timeout).
 *
 * Returns:
 *   0   — HALT reached normally.
 *  -1   — FAULT encountered.
 *  -2   — max_cycles exceeded.
 */
int cpu_run(CPU *cpu, uint64_t max_cycles);

/* ── Introspection ────────────────────────────────────────────────────────── */

/**
 * cpu_dump_state() — Print the full register file, flags, PC, and
 * cycle counter to stdout.
 */
void cpu_dump_state(const CPU *cpu);

/**
 * cpu_dump_registers() — Print only the register file.
 */
void cpu_dump_registers(const CPU *cpu);

#endif /* CPU_H */
