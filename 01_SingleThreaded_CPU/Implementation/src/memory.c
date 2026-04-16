/*
 * memory.c — Simulated 1MiB flat memory implementation
 */

#include "memory.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ── Internal bounds-check helper ────────────────────────────────────────── */

static void bounds_check(const Memory *m, uint32_t addr, uint32_t width,
                          const char *op)
{
    if ((uint64_t)addr + width > (uint64_t)m->size) {
        fprintf(stderr,
                "[MEM FAULT] %s: address 0x%08X (width=%u) out of range "
                "[0x00000000 – 0x%08X]\n",
                op, addr, width, m->size - 1);
        abort();
    }
}

/* ── Lifecycle ────────────────────────────────────────────────────────────── */

int mem_init(Memory *m)
{
    m->size = MEM_SIZE;
    m->data = (uint8_t *)calloc(1, MEM_SIZE);
    if (!m->data) {
        fprintf(stderr, "[MEM] calloc(%u) failed\n", MEM_SIZE);
        return -1;
    }
    return 0;
}

void mem_free(Memory *m)
{
    free(m->data);
    m->data = NULL;
    m->size = 0;
}

void mem_reset(Memory *m)
{
    if (m->data)
        memset(m->data, 0, m->size);
}

/* ── Read accessors ───────────────────────────────────────────────────────── */

uint8_t mem_read8(const Memory *m, uint32_t addr)
{
    bounds_check(m, addr, 1, "READ8");
    return m->data[addr];
}

uint16_t mem_read16(const Memory *m, uint32_t addr)
{
    bounds_check(m, addr, 2, "READ16");
    /* Little-endian */
    return (uint16_t)(m->data[addr])
         | (uint16_t)(m->data[addr + 1] << 8);
}

uint32_t mem_read32(const Memory *m, uint32_t addr)
{
    bounds_check(m, addr, 4, "READ32");
    /* Little-endian */
    return (uint32_t)(m->data[addr])
         | (uint32_t)(m->data[addr + 1] <<  8)
         | (uint32_t)(m->data[addr + 2] << 16)
         | (uint32_t)(m->data[addr + 3] << 24);
}

/* ── Write accessors ──────────────────────────────────────────────────────── */

void mem_write8(Memory *m, uint32_t addr, uint8_t val)
{
    bounds_check(m, addr, 1, "WRITE8");
    m->data[addr] = val;
}

void mem_write16(Memory *m, uint32_t addr, uint16_t val)
{
    bounds_check(m, addr, 2, "WRITE16");
    m->data[addr]     = (uint8_t)(val & 0xFF);
    m->data[addr + 1] = (uint8_t)(val >> 8);
}

void mem_write32(Memory *m, uint32_t addr, uint32_t val)
{
    bounds_check(m, addr, 4, "WRITE32");
    m->data[addr]     = (uint8_t)(val        & 0xFF);
    m->data[addr + 1] = (uint8_t)((val >>  8) & 0xFF);
    m->data[addr + 2] = (uint8_t)((val >> 16) & 0xFF);
    m->data[addr + 3] = (uint8_t)((val >> 24) & 0xFF);
}

/* ── Utility ──────────────────────────────────────────────────────────────── */

int mem_load_binary(Memory *m, uint32_t base_addr,
                    const uint8_t *src, size_t len)
{
    if ((uint64_t)base_addr + len > (uint64_t)m->size) {
        fprintf(stderr,
                "[MEM] mem_load_binary: program size %zu at 0x%08X "
                "overflows memory (limit 0x%08X)\n",
                len, base_addr, m->size);
        return -1;
    }
    memcpy(m->data + base_addr, src, len);
    return 0;
}

void mem_dump(const Memory *m, uint32_t addr, uint32_t len)
{
    uint32_t i;
    for (i = 0; i < len; i++) {
        if (i % 16 == 0) {
            printf("  0x%08X: ", addr + i);
        }
        bounds_check(m, addr + i, 1, "DUMP");
        printf("%02X ", m->data[addr + i]);
        if (i % 16 == 15) {
            putchar('\n');
        }
    }
    if (len % 16 != 0)
        putchar('\n');
}
