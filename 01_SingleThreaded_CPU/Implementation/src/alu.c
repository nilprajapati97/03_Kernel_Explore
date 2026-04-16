/*
 * alu.c — ALU implementation for the custom CPU simulator
 */

#include "alu.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>

/* ── Internal helpers ─────────────────────────────────────────────────────── */

/* Update Z and N flags from a 32-bit result */
static inline void set_zn(uint32_t result, Flags *f)
{
    f->Z = (result == 0)             ? 1 : 0;
    f->N = ((result >> 31) & 1u)     ? 1 : 0;
}

/* Clear C and O */
static inline void clear_co(Flags *f)
{
    f->C = 0;
    f->O = 0;
}

/* ── Arithmetic ───────────────────────────────────────────────────────────── */

uint32_t alu_add(uint32_t a, uint32_t b, Flags *f)
{
    uint64_t wide   = (uint64_t)a + (uint64_t)b;
    uint32_t result = (uint32_t)wide;

    set_zn(result, f);

    /* Unsigned carry */
    f->C = (wide > UINT32_MAX) ? 1 : 0;

    /* Signed overflow: both operands same sign, result different sign */
    f->O = ((~(a ^ b) & (a ^ result)) >> 31) & 1u;

    return result;
}

uint32_t alu_sub(uint32_t a, uint32_t b, Flags *f)
{
    uint32_t result = a - b;

    set_zn(result, f);

    /* C is set when there is NO borrow (unsigned a >= b) */
    f->C = (a >= b) ? 1 : 0;

    /* Signed overflow: operands different signs, result sign differs from a */
    f->O = (((a ^ b) & (a ^ result)) >> 31) & 1u;

    return result;
}

uint32_t alu_mul(uint32_t a, uint32_t b, Flags *f)
{
    uint32_t result = a * b;   /* Lower 32 bits only */
    set_zn(result, f);
    clear_co(f);
    return result;
}

uint32_t alu_div(uint32_t a, uint32_t b, Flags *f)
{
    if (b == 0) {
        fprintf(stderr, "[ALU] Division by zero (a=0x%08X / b=0)\n", a);
        /* Return 0; do not abort — let CPU decide to trap or continue */
        f->Z = 1; f->N = 0;
        clear_co(f);
        return 0;
    }
    uint32_t result = a / b;
    set_zn(result, f);
    clear_co(f);
    return result;
}

/* ── Bitwise ──────────────────────────────────────────────────────────────── */

uint32_t alu_and(uint32_t a, uint32_t b, Flags *f)
{
    uint32_t result = a & b;
    set_zn(result, f);
    clear_co(f);
    return result;
}

uint32_t alu_or(uint32_t a, uint32_t b, Flags *f)
{
    uint32_t result = a | b;
    set_zn(result, f);
    clear_co(f);
    return result;
}

uint32_t alu_xor(uint32_t a, uint32_t b, Flags *f)
{
    uint32_t result = a ^ b;
    set_zn(result, f);
    clear_co(f);
    return result;
}

uint32_t alu_not(uint32_t a, Flags *f)
{
    uint32_t result = ~a;
    set_zn(result, f);
    clear_co(f);
    return result;
}

/* ── Shift ────────────────────────────────────────────────────────────────── */

uint32_t alu_shl(uint32_t a, uint32_t shift, Flags *f)
{
    if (shift >= 32) {
        /* C = LSB of shift=32 case is 0; result is 0 */
        f->C = (shift == 32) ? (a & 1u) : 0u;
        f->O = 0;
        f->Z = 1; f->N = 0;
        return 0;
    }
    /* Carry = the bit that will be shifted out (bit 31 of partially shifted) */
    f->C = (shift > 0) ? ((a >> (32u - shift)) & 1u) : 0u;
    uint32_t result = a << shift;
    set_zn(result, f);
    f->O = 0;
    return result;
}

uint32_t alu_shr(uint32_t a, uint32_t shift, Flags *f)
{
    if (shift >= 32) {
        f->Z = 1; f->N = 0; f->C = 0; f->O = 0;
        return 0;
    }
    uint32_t result = a >> shift;   /* logical (unsigned) right shift */
    set_zn(result, f);
    clear_co(f);
    return result;
}

/* ── Comparison ───────────────────────────────────────────────────────────── */

void alu_cmp(uint32_t a, uint32_t b, Flags *f)
{
    alu_sub(a, b, f);   /* Compute a - b; flags updated, result discarded */
}

/* ── Utility ──────────────────────────────────────────────────────────────── */

void alu_flags_str(const Flags *f, char *buf, int buf_size)
{
    snprintf(buf, (size_t)buf_size,
             "Z=%u N=%u C=%u O=%u", f->Z, f->N, f->C, f->O);
}
