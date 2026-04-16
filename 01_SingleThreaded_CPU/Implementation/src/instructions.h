/*
 * instructions.h — Custom RISC-like ISA definitions
 *
 * 32-bit fixed-width instruction encoding:
 *
 *  31      24 23   20 19   16 15   12 11          0
 *  +----------+-------+-------+-------+-------------+
 *  |  opcode  |  Rd   |  Rs1  |  Rs2  |  Imm[11:0]  |
 *  +----------+-------+-------+-------+-------------+
 *     8 bits    4 bits  4 bits  4 bits    12 bits
 *
 * Registers: R0–R15 (32-bit each)
 *   R14 = Link Register (LR)  — convention, not enforced in HW
 *   R15 = Program Counter (PC) — auto-advanced by cpu_step()
 *
 * Imm is sign-extended to 32 bits where applicable.
 *
 * Status flags: Z (Zero), N (Negative), C (Carry), O (Overflow)
 */

#ifndef INSTRUCTIONS_H
#define INSTRUCTIONS_H

#include <stdint.h>

/* ── Opcode definitions ───────────────────────────────────────────────────── */
typedef enum {
    OP_NOP       = 0x00,  /* No operation                                    */
    OP_LOAD_IMM  = 0x01,  /* Rd = sign_ext(Imm)                              */
    OP_LOAD      = 0x02,  /* Rd = MEM32[Rs1 + sign_ext(Imm)]                 */
    OP_STORE     = 0x03,  /* MEM32[Rs1 + sign_ext(Imm)] = Rs2                */
    OP_ADD       = 0x04,  /* Rd = Rs1 + Rs2  (sets Z,N,C,O)                  */
    OP_SUB       = 0x05,  /* Rd = Rs1 - Rs2  (sets Z,N,C,O)                  */
    OP_MUL       = 0x06,  /* Rd = Rs1 * Rs2  (sets Z,N)                      */
    OP_DIV       = 0x07,  /* Rd = Rs1 / Rs2  (sets Z,N; div-by-zero → trap)  */
    OP_AND       = 0x08,  /* Rd = Rs1 & Rs2  (sets Z,N)                      */
    OP_OR        = 0x09,  /* Rd = Rs1 | Rs2  (sets Z,N)                      */
    OP_XOR       = 0x0A,  /* Rd = Rs1 ^ Rs2  (sets Z,N)                      */
    OP_NOT       = 0x0B,  /* Rd = ~Rs1       (sets Z,N)                      */
    OP_SHL       = 0x0C,  /* Rd = Rs1 << Rs2 (logical; sets Z,N,C)           */
    OP_SHR       = 0x0D,  /* Rd = Rs1 >> Rs2 (logical; sets Z,N)             */
    OP_MOV       = 0x0E,  /* Rd = Rs1                                        */
    OP_CMP       = 0x0F,  /* flags = Rs1 - Rs2  (result discarded)           */
    OP_JMP       = 0x10,  /* PC = sign_ext(Imm)  (absolute)                  */
    OP_JEQ       = 0x11,  /* if  Z : PC = sign_ext(Imm)                      */
    OP_JNE       = 0x12,  /* if !Z : PC = sign_ext(Imm)                      */
    OP_JGT       = 0x13,  /* if !Z && !N : PC = sign_ext(Imm)                */
    OP_JLT       = 0x14,  /* if  N : PC = sign_ext(Imm)                      */
    OP_CALL      = 0x15,  /* LR = PC+4; PC = sign_ext(Imm)                   */
    OP_RET       = 0x16,  /* PC = LR                                         */
    OP_PUSH      = 0x17,  /* SP -= 4; MEM32[SP] = Rs1  (SP = R13)            */
    OP_POP       = 0x18,  /* Rd = MEM32[SP];  SP += 4  (SP = R13)            */
    OP_HALT      = 0xFF,  /* Stop execution                                  */
} Opcode;

/* ── Register aliases ─────────────────────────────────────────────────────── */
#define REG_SP   13   /* Stack Pointer */
#define REG_LR   14   /* Link Register */
#define REG_PC   15   /* Program Counter */
#define NUM_REGS 16

/* ── Decoded instruction fields ───────────────────────────────────────────── */
typedef struct {
    Opcode   opcode;   /* 8-bit opcode                */
    uint8_t  rd;       /* Destination register [3:0]  */
    uint8_t  rs1;      /* Source register 1   [3:0]   */
    uint8_t  rs2;      /* Source register 2   [3:0]   */
    int32_t  imm;      /* Sign-extended 12-bit immediate */
    uint32_t raw;      /* Original 32-bit word (for disasm / trace) */
} Instruction;

/* ── Instruction encoding helpers ────────────────────────────────────────── */

/* Pack a 32-bit instruction word */
static inline uint32_t instr_encode(uint8_t op, uint8_t rd,
                                    uint8_t rs1, uint8_t rs2,
                                    int16_t imm12)
{
    return ((uint32_t)(op  & 0xFF) << 24)
         | ((uint32_t)(rd  & 0x0F) << 20)
         | ((uint32_t)(rs1 & 0x0F) << 16)
         | ((uint32_t)(rs2 & 0x0F) << 12)
         | ((uint32_t)(imm12 & 0x0FFF));
}

/* Sign-extend a 12-bit value to 32 bits */
static inline int32_t sign_ext12(uint32_t val)
{
    val &= 0xFFF;
    return (val & 0x800) ? (int32_t)(val | 0xFFFFF000u) : (int32_t)val;
}

#endif /* INSTRUCTIONS_H */
