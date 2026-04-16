/*
 * memory.h — Simulated flat 1MB memory for the CPU simulator
 *
 * The simulated memory is a heap-allocated byte array.
 * It is byte-addressable; 32-bit accesses are little-endian.
 *
 * Address space: 0x00000000 – 0x000FFFFF  (1 MiB)
 *
 * Layout convention (mirroring a real embedded system):
 *   0x00000000 – 0x0000FFFF   Interrupt vector / boot code area
 *   0x00010000 – 0x0007FFFF   Program / code segment
 *   0x00080000 – 0x000EFFFF   Data / heap segment
 *   0x000F0000 – 0x000FFFFF   Stack segment (grows downward from 0x000FFFFF)
 */

#ifndef MEMORY_H
#define MEMORY_H

#include <stdint.h>
#include <stddef.h>

/* Total simulated memory size (1 MiB) */
#define MEM_SIZE        (1024u * 1024u)

/* Default initial stack pointer value */
#define MEM_STACK_TOP   0x000FFFFCu

/* Program load base address */
#define MEM_PROG_BASE   0x00010000u

/* ── Simulated memory context ─────────────────────────────────────────────── */
typedef struct {
    uint8_t *data;    /* Pointer to the 1MiB byte array */
    uint32_t size;    /* Always MEM_SIZE; kept here for bounds checking */
} Memory;

/* ── Lifecycle ────────────────────────────────────────────────────────────── */

/**
 * mem_init() — Allocate and zero-initialise the simulated memory.
 * Returns 0 on success, -1 on allocation failure.
 */
int  mem_init(Memory *m);

/**
 * mem_free() — Release the underlying memory buffer.
 */
void mem_free(Memory *m);

/**
 * mem_reset() — Zero the entire memory contents (keeps allocation).
 */
void mem_reset(Memory *m);

/* ── Read accessors ───────────────────────────────────────────────────────── */

/**
 * mem_read8() — Read one byte from address `addr`.
 * Terminates with an error message on out-of-bounds access.
 */
uint8_t  mem_read8 (const Memory *m, uint32_t addr);

/**
 * mem_read16() — Read a little-endian 16-bit value from `addr`.
 */
uint16_t mem_read16(const Memory *m, uint32_t addr);

/**
 * mem_read32() — Read a little-endian 32-bit value from `addr`.
 */
uint32_t mem_read32(const Memory *m, uint32_t addr);

/* ── Write accessors ──────────────────────────────────────────────────────── */

/**
 * mem_write8() — Write one byte to address `addr`.
 */
void mem_write8 (Memory *m, uint32_t addr, uint8_t  val);

/**
 * mem_write16() — Write a little-endian 16-bit value to `addr`.
 */
void mem_write16(Memory *m, uint32_t addr, uint16_t val);

/**
 * mem_write32() — Write a little-endian 32-bit value to `addr`.
 */
void mem_write32(Memory *m, uint32_t addr, uint32_t val);

/* ── Utility ──────────────────────────────────────────────────────────────── */

/**
 * mem_load_binary() — Copy `len` bytes from host buffer `src` into the
 * simulated memory starting at `base_addr`.
 * Returns 0 on success, -1 if the region overflows MEM_SIZE.
 */
int mem_load_binary(Memory *m, uint32_t base_addr,
                    const uint8_t *src, size_t len);

/**
 * mem_dump() — Hex-dump `len` bytes starting at `addr` to stdout.
 */
void mem_dump(const Memory *m, uint32_t addr, uint32_t len);

#endif /* MEMORY_H */
