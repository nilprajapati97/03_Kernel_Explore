/*
 * decoder.c — Instruction decoder + disassembler
 */

#include "decoder.h"
#include <stdio.h>
#include <string.h>

/* ── Decode ───────────────────────────────────────────────────────────────── */

Instruction decode_instruction(uint32_t raw)
{
    Instruction instr;
    instr.raw    = raw;
    instr.opcode = (Opcode)((raw >> 24) & 0xFF);
    instr.rd     = (uint8_t)((raw >> 20) & 0x0F);
    instr.rs1    = (uint8_t)((raw >> 16) & 0x0F);
    instr.rs2    = (uint8_t)((raw >> 12) & 0x0F);
    instr.imm    = sign_ext12(raw & 0xFFF);
    return instr;
}

/* ── Disassembly ──────────────────────────────────────────────────────────── */

/* Mnemonic table indexed by opcode byte (sparse — unknown → "???") */
static const char *opcode_name(Opcode op)
{
    switch (op) {
        case OP_NOP:       return "NOP";
        case OP_LOAD_IMM:  return "LOAD_IMM";
        case OP_LOAD:      return "LOAD";
        case OP_STORE:     return "STORE";
        case OP_ADD:       return "ADD";
        case OP_SUB:       return "SUB";
        case OP_MUL:       return "MUL";
        case OP_DIV:       return "DIV";
        case OP_AND:       return "AND";
        case OP_OR:        return "OR";
        case OP_XOR:       return "XOR";
        case OP_NOT:       return "NOT";
        case OP_SHL:       return "SHL";
        case OP_SHR:       return "SHR";
        case OP_MOV:       return "MOV";
        case OP_CMP:       return "CMP";
        case OP_JMP:       return "JMP";
        case OP_JEQ:       return "JEQ";
        case OP_JNE:       return "JNE";
        case OP_JGT:       return "JGT";
        case OP_JLT:       return "JLT";
        case OP_CALL:      return "CALL";
        case OP_RET:       return "RET";
        case OP_PUSH:      return "PUSH";
        case OP_POP:       return "POP";
        case OP_HALT:      return "HALT";
        default:           return "???";
    }
}

void disasm_instruction(const Instruction *instr, char *buf, int buf_size)
{
    const char *mn = opcode_name(instr->opcode);

    switch (instr->opcode) {
        /* No operands */
        case OP_NOP:
        case OP_HALT:
            snprintf(buf, (size_t)buf_size, "%-10s", mn);
            break;

        /* Rd = sign_ext(Imm) */
        case OP_LOAD_IMM:
            snprintf(buf, (size_t)buf_size,
                     "%-10s R%u, #%d", mn, instr->rd, instr->imm);
            break;

        /* Rd = MEM[Rs1 + Imm] */
        case OP_LOAD:
            snprintf(buf, (size_t)buf_size,
                     "%-10s R%u, [R%u + #%d]", mn,
                     instr->rd, instr->rs1, instr->imm);
            break;

        /* MEM[Rs1 + Imm] = Rs2 */
        case OP_STORE:
            snprintf(buf, (size_t)buf_size,
                     "%-10s [R%u + #%d], R%u", mn,
                     instr->rs1, instr->imm, instr->rs2);
            break;

        /* Rd = Rs1 OP Rs2 */
        case OP_ADD:
        case OP_SUB:
        case OP_MUL:
        case OP_DIV:
        case OP_AND:
        case OP_OR:
        case OP_XOR:
        case OP_SHL:
        case OP_SHR:
            snprintf(buf, (size_t)buf_size,
                     "%-10s R%u, R%u, R%u", mn,
                     instr->rd, instr->rs1, instr->rs2);
            break;

        /* Rd = ~Rs1 */
        case OP_NOT:
            snprintf(buf, (size_t)buf_size,
                     "%-10s R%u, R%u", mn, instr->rd, instr->rs1);
            break;

        /* Rd = Rs1 */
        case OP_MOV:
            snprintf(buf, (size_t)buf_size,
                     "%-10s R%u, R%u", mn, instr->rd, instr->rs1);
            break;

        /* CMP Rs1, Rs2 */
        case OP_CMP:
            snprintf(buf, (size_t)buf_size,
                     "%-10s R%u, R%u", mn, instr->rs1, instr->rs2);
            break;

        /* PC = PC + Imm  (PC-relative; PC is post-increment fetch_addr+4) */
        case OP_JMP:
        case OP_JEQ:
        case OP_JNE:
        case OP_JGT:
        case OP_JLT:
        case OP_CALL:
            snprintf(buf, (size_t)buf_size,
                     "%-10s PC%+d", mn, instr->imm);
            break;

        /* RET */
        case OP_RET:
            snprintf(buf, (size_t)buf_size, "%-10s", mn);
            break;

        /* PUSH Rs1 */
        case OP_PUSH:
            snprintf(buf, (size_t)buf_size,
                     "%-10s R%u", mn, instr->rs1);
            break;

        /* POP Rd */
        case OP_POP:
            snprintf(buf, (size_t)buf_size,
                     "%-10s R%u", mn, instr->rd);
            break;

        default:
            snprintf(buf, (size_t)buf_size,
                     "???        (raw=0x%08X)", instr->raw);
            break;
    }
}
