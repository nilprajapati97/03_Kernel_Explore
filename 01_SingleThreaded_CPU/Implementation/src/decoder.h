/*
 * decoder.h — Instruction decoder for the custom CPU simulator
 *
 * Splits a raw 32-bit instruction word into its constituent fields
 * and returns a populated Instruction struct.
 *
 * Encoding reminder:
 *   [31:24] opcode (8b) | [23:20] Rd (4b) | [19:16] Rs1 (4b)
 *   [15:12] Rs2 (4b)    | [11:0]  Imm12 (12b, sign-extended)
 */

#ifndef DECODER_H
#define DECODER_H

#include "instructions.h"

/**
 * decode_instruction() — Decode a 32-bit raw instruction word.
 *
 * Populates and returns an Instruction struct.  The `imm` field is
 * already sign-extended to 32 bits.
 */
Instruction decode_instruction(uint32_t raw);

/**
 * disasm_instruction() — Write a human-readable disassembly of `instr`
 * into `buf` (at most `buf_size` bytes).
 *
 * Example output:  "ADD  R2, R0, R1"
 *                  "LOAD_IMM R3, #42"
 *                  "JEQ  0x00010014"
 */
void disasm_instruction(const Instruction *instr, char *buf, int buf_size);

#endif /* DECODER_H */
