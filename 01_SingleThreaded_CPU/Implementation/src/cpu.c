/*
 * cpu.c — Single-threaded CPU fetch-decode-execute engine
 *
 * Each call to cpu_step() models one clock cycle:
 *
 *   Stage 1 — FETCH:     raw = MEM32[PC]; PC += 4
 *   Stage 2 — DECODE:    instr = decode_instruction(raw)
 *   Stage 3 — EXECUTE:   dispatch by opcode; invoke ALU / mem helpers
 *   Stage 4 — WRITEBACK: result written to Rd (handled inside execute)
 *
 * The CPU is deliberately single-threaded and has no pipeline; one
 * instruction completes fully before the next is fetched.
 */

#include "cpu.h"
#include "decoder.h"
#include "alu.h"

#include <stdio.h>
#include <string.h>

/* ── Convenience macros ───────────────────────────────────────────────────── */

#define PC   (cpu->regs[REG_PC])
#define SP   (cpu->regs[REG_SP])
#define LR   (cpu->regs[REG_LR])
#define REG(n) (cpu->regs[(n) & 0x0F])

/* ── Lifecycle ────────────────────────────────────────────────────────────── */

void cpu_init(CPU *cpu, Memory *mem)
{
    memset(cpu, 0, sizeof(*cpu));
    cpu->mem   = mem;
    cpu->state = CPU_STATE_RESET;
    cpu->trace = 0;
}

void cpu_reset(CPU *cpu, uint32_t entry_point)
{
    memset(cpu->regs, 0, sizeof(cpu->regs));
    memset(&cpu->flags, 0, sizeof(cpu->flags));
    cpu->cycle_count = 0;
    cpu->entry_point = entry_point;
    cpu->state       = CPU_STATE_RUNNING;

    PC = entry_point;
    SP = MEM_STACK_TOP;   /* Stack grows down from top of memory */
}

/* ── Trace helper ─────────────────────────────────────────────────────────── */

static void trace_print(const CPU *cpu, uint32_t fetch_addr,
                         const Instruction *instr)
{
    char disasm[64];
    char flagstr[32];

    disasm_instruction(instr, disasm, sizeof(disasm));
    alu_flags_str(&cpu->flags, flagstr, sizeof(flagstr));

    printf("  [%6llu] 0x%08X: %-36s | %s\n",
           (unsigned long long)cpu->cycle_count,
           fetch_addr,
           disasm,
           flagstr);
}

/* ── Execute dispatch ─────────────────────────────────────────────────────── */

/*
 * Returns:
 *   0  normal
 *   1  HALT
 *  -1  FAULT
 */
static int execute(CPU *cpu, const Instruction *instr)
{
    uint32_t result = 0;

    switch (instr->opcode) {

        /* ── NOP ─────────────────────────────────────────────────────── */
        case OP_NOP:
            break;

        /* ── HALT ────────────────────────────────────────────────────── */
        case OP_HALT:
            cpu->state = CPU_STATE_HALTED;
            return 1;

        /* ── Immediate load ──────────────────────────────────────────── */
        case OP_LOAD_IMM:
            REG(instr->rd) = (uint32_t)instr->imm;
            break;

        /* ── Memory: LOAD  Rd, [Rs1 + Imm] ──────────────────────────── */
        case OP_LOAD: {
            uint32_t addr = (uint32_t)((int32_t)REG(instr->rs1) + instr->imm);
            REG(instr->rd) = mem_read32(cpu->mem, addr);
            break;
        }

        /* ── Memory: STORE [Rs1 + Imm], Rs2 ─────────────────────────── */
        case OP_STORE: {
            uint32_t addr = (uint32_t)((int32_t)REG(instr->rs1) + instr->imm);
            mem_write32(cpu->mem, addr, REG(instr->rs2));
            break;
        }

        /* ── ALU: two-register operations ───────────────────────────── */
        case OP_ADD:
            result = alu_add(REG(instr->rs1), REG(instr->rs2), &cpu->flags);
            REG(instr->rd) = result;
            break;

        case OP_SUB:
            result = alu_sub(REG(instr->rs1), REG(instr->rs2), &cpu->flags);
            REG(instr->rd) = result;
            break;

        case OP_MUL:
            result = alu_mul(REG(instr->rs1), REG(instr->rs2), &cpu->flags);
            REG(instr->rd) = result;
            break;

        case OP_DIV:
            result = alu_div(REG(instr->rs1), REG(instr->rs2), &cpu->flags);
            REG(instr->rd) = result;
            break;

        case OP_AND:
            result = alu_and(REG(instr->rs1), REG(instr->rs2), &cpu->flags);
            REG(instr->rd) = result;
            break;

        case OP_OR:
            result = alu_or(REG(instr->rs1), REG(instr->rs2), &cpu->flags);
            REG(instr->rd) = result;
            break;

        case OP_XOR:
            result = alu_xor(REG(instr->rs1), REG(instr->rs2), &cpu->flags);
            REG(instr->rd) = result;
            break;

        case OP_NOT:
            result = alu_not(REG(instr->rs1), &cpu->flags);
            REG(instr->rd) = result;
            break;

        case OP_SHL:
            result = alu_shl(REG(instr->rs1), REG(instr->rs2), &cpu->flags);
            REG(instr->rd) = result;
            break;

        case OP_SHR:
            result = alu_shr(REG(instr->rs1), REG(instr->rs2), &cpu->flags);
            REG(instr->rd) = result;
            break;

        /* ── MOV  Rd, Rs1 ────────────────────────────────────────────── */
        case OP_MOV:
            REG(instr->rd) = REG(instr->rs1);
            break;

        /* ── CMP  Rs1, Rs2  (flags only) ─────────────────────────────── */
        case OP_CMP:
            alu_cmp(REG(instr->rs1), REG(instr->rs2), &cpu->flags);
            break;

        /* ── Branch: PC-relative offset from sign_ext(Imm) ────────────
         * At execute time PC has already been pre-incremented by the fetch
         * stage (PC = fetch_addr + 4).  The branch target is therefore:
         *   PC_new = PC + imm   (imm = target − (fetch_addr + 4))
         * ─────────────────────────────────────────────────────────────── */
        case OP_JMP:
            PC = (uint32_t)((int32_t)PC + instr->imm);
            return 0;

        case OP_JEQ:
            if (cpu->flags.Z)  { PC = (uint32_t)((int32_t)PC + instr->imm); return 0; }
            break;

        case OP_JNE:
            if (!cpu->flags.Z) { PC = (uint32_t)((int32_t)PC + instr->imm); return 0; }
            break;

        case OP_JGT:
            if (!cpu->flags.Z && !cpu->flags.N) {
                PC = (uint32_t)((int32_t)PC + instr->imm); return 0;
            }
            break;

        case OP_JLT:
            if (cpu->flags.N)  { PC = (uint32_t)((int32_t)PC + instr->imm); return 0; }
            break;

        /* ── CALL / RET ───────────────────────────────────────────────── */
        case OP_CALL:
            /* LR = return address (PC already post-incremented by fetch) */
            LR = PC;
            PC = (uint32_t)((int32_t)PC + instr->imm);
            return 0;

        case OP_RET:
            PC = LR;
            return 0;

        /* ── Stack: PUSH Rs1 / POP Rd ────────────────────────────────── */
        case OP_PUSH:
            SP -= 4;
            mem_write32(cpu->mem, SP, REG(instr->rs1));
            break;

        case OP_POP:
            REG(instr->rd) = mem_read32(cpu->mem, SP);
            SP += 4;
            break;

        /* ── Unknown opcode ───────────────────────────────────────────── */
        default:
            fprintf(stderr,
                    "[CPU FAULT] Illegal opcode 0x%02X at PC=0x%08X\n",
                    (unsigned)instr->opcode,
                    (unsigned)(PC - 4));
            cpu->state = CPU_STATE_FAULT;
            return -1;
    }

    return 0;
}

/* ── Public step / run ────────────────────────────────────────────────────── */

int cpu_step(CPU *cpu)
{
    if (cpu->state != CPU_STATE_RUNNING)
        return (cpu->state == CPU_STATE_HALTED) ? 1 : -1;

    /* Stage 1: FETCH */
    uint32_t fetch_addr = PC;
    uint32_t raw        = mem_read32(cpu->mem, PC);
    PC += 4;   /* Pre-increment before decode/execute (like most RISC CPUs) */

    /* Stage 2: DECODE */
    Instruction instr = decode_instruction(raw);

    /* Increment cycle counter */
    cpu->cycle_count++;

    /* Trace (if enabled) */
    if (cpu->trace)
        trace_print(cpu, fetch_addr, &instr);

    /* Stage 3+4: EXECUTE + WRITEBACK */
    return execute(cpu, &instr);
}

int cpu_run(CPU *cpu, uint64_t max_cycles)
{
    for (;;) {
        int rc = cpu_step(cpu);

        if (rc == 1)   return 0;   /* HALT */
        if (rc == -1)  return -1;  /* FAULT */

        if (max_cycles > 0 && cpu->cycle_count >= max_cycles) {
            fprintf(stderr,
                    "[CPU] Max cycle limit (%llu) reached. Stopping.\n",
                    (unsigned long long)max_cycles);
            return -2;
        }
    }
}

/* ── Dump helpers ─────────────────────────────────────────────────────────── */

void cpu_dump_registers(const CPU *cpu)
{
    int i;
    printf("  Registers:\n");
    for (i = 0; i < NUM_REGS; i++) {
        const char *alias = "";
        if (i == REG_SP) alias = " (SP)";
        else if (i == REG_LR) alias = " (LR)";
        else if (i == REG_PC) alias = " (PC)";
        printf("    R%-2d%s = 0x%08X  (%d)\n",
               i, alias, cpu->regs[i], (int32_t)cpu->regs[i]);
    }
}

void cpu_dump_state(const CPU *cpu)
{
    const char *state_name[] = { "RESET", "RUNNING", "HALTED", "FAULT" };
    char flagstr[32];
    alu_flags_str(&cpu->flags, flagstr, sizeof(flagstr));

    printf("\n╔══════════════════════════════════════════════════╗\n");
    printf("║              CPU STATE DUMP                      ║\n");
    printf("╠══════════════════════════════════════════════════╣\n");
    printf("  State      : %s\n", state_name[cpu->state]);
    printf("  Cycles     : %llu\n", (unsigned long long)cpu->cycle_count);
    printf("  Flags      : %s\n", flagstr);
    cpu_dump_registers(cpu);
    printf("╚══════════════════════════════════════════════════╝\n");
}
