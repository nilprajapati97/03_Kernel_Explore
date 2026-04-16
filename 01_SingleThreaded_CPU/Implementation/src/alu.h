/*
 * alu.h — Arithmetic-Logic Unit for the custom CPU simulator
 *
 * All ALU operations are pure functions that:
 *   1. Compute the result.
 *   2. Update the CPU status flags (Z, N, C, O).
 *   3. Return the 32-bit result.
 *
 * The ALU does not touch registers or memory directly.
 */

#ifndef ALU_H
#define ALU_H

#include <stdint.h>

/* ── Status flags ─────────────────────────────────────────────────────────── */
typedef struct {
    uint8_t Z;   /* Zero     — result == 0               */
    uint8_t N;   /* Negative — result bit 31 is set       */
    uint8_t C;   /* Carry    — unsigned overflow / borrow */
    uint8_t O;   /* Overflow — signed overflow            */
} Flags;

/* ── ALU function declarations ────────────────────────────────────────────── */

/**
 * alu_add() — Rd = a + b  (sets Z, N, C, O)
 */
uint32_t alu_add(uint32_t a, uint32_t b, Flags *f);

/**
 * alu_sub() — Rd = a - b  (sets Z, N, C, O)
 * C is set when there is NO borrow (i.e., a >= b unsigned).
 */
uint32_t alu_sub(uint32_t a, uint32_t b, Flags *f);

/**
 * alu_mul() — Rd = a * b  (sets Z, N; C and O cleared)
 */
uint32_t alu_mul(uint32_t a, uint32_t b, Flags *f);

/**
 * alu_div() — Rd = a / b  (sets Z, N; C and O cleared)
 * Returns 0 and prints an error if b == 0 (no exception in this sim).
 */
uint32_t alu_div(uint32_t a, uint32_t b, Flags *f);

/**
 * alu_and() — Rd = a & b  (sets Z, N; C and O cleared)
 */
uint32_t alu_and(uint32_t a, uint32_t b, Flags *f);

/**
 * alu_or()  — Rd = a | b  (sets Z, N; C and O cleared)
 */
uint32_t alu_or (uint32_t a, uint32_t b, Flags *f);

/**
 * alu_xor() — Rd = a ^ b  (sets Z, N; C and O cleared)
 */
uint32_t alu_xor(uint32_t a, uint32_t b, Flags *f);

/**
 * alu_not() — Rd = ~a     (sets Z, N; C and O cleared)
 */
uint32_t alu_not(uint32_t a, Flags *f);

/**
 * alu_shl() — Rd = a << shift  (sets Z, N; C = last bit shifted out)
 * Shift amount is clamped to [0, 31].
 */
uint32_t alu_shl(uint32_t a, uint32_t shift, Flags *f);

/**
 * alu_shr() — Rd = a >> shift  (logical; sets Z, N; C and O cleared)
 * Shift amount is clamped to [0, 31].
 */
uint32_t alu_shr(uint32_t a, uint32_t shift, Flags *f);

/**
 * alu_cmp() — Sets flags as if computing a - b, discards result.
 */
void alu_cmp(uint32_t a, uint32_t b, Flags *f);

/* ── Utility ──────────────────────────────────────────────────────────────── */

/**
 * alu_flags_str() — Write flags as a human-readable string into `buf`.
 * buf must be at least 9 bytes.  Format: "Z=x N=x C=x O=x"
 */
void alu_flags_str(const Flags *f, char *buf, int buf_size);

#endif /* ALU_H */
