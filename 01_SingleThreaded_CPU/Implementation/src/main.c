/*
 * main.c — Entry point for the single-threaded CPU simulator
 *
 * Usage:
 *   cpu_sim <program.bin> [--trace] [--max-cycles N]
 *           [--dump-mem <addr> <len>]
 *
 * Arguments:
 *   <program.bin>          Binary file of 32-bit instructions (little-endian).
 *                          Loaded at MEM_PROG_BASE (0x00010000).
 *   --trace                Print a disassembly trace each cycle.
 *   --max-cycles N         Limit execution to N cycles (default: unlimited).
 *   --dump-mem ADDR LEN    After HALT, hex-dump LEN bytes from ADDR.
 *
 * On BeagleBone Black / QEMU:
 *   The process is pinned to CPU core 0 via sched_setaffinity() so the
 *   simulation physically runs on a single hardware core, mirroring the
 *   single-threaded CPU model it simulates.
 */

/* _GNU_SOURCE is passed via -D_GNU_SOURCE in the Makefile */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>

/* CPU affinity (Linux-only; gracefully skipped if unavailable) */
#ifdef __linux__
#  include <sched.h>
#  include <unistd.h>
#endif

#include "memory.h"
#include "cpu.h"

/* ── Helpers ──────────────────────────────────────────────────────────────── */

static void usage(const char *prog)
{
    fprintf(stderr,
            "Usage: %s <program.bin> [--trace] [--max-cycles N]\n"
            "                        [--dump-mem ADDR LEN]\n",
            prog);
}

/* Pin this process to core 0 so we truly run single-threaded on one core */
static void pin_to_core0(void)
{
#ifdef __linux__
    cpu_set_t mask;
    CPU_ZERO(&mask);
    CPU_SET(0, &mask);   /* Core 0 */
    if (sched_setaffinity(0, sizeof(mask), &mask) != 0) {
        /* Non-fatal — just warn */
        fprintf(stderr,
                "[WARN] sched_setaffinity(core 0) failed: %s\n",
                strerror(errno));
    } else {
        printf("[SYS] Process pinned to CPU core 0 (single-threaded model)\n");
    }
#else
    printf("[SYS] CPU affinity pinning not available on this platform\n");
#endif
}

/* Read an entire binary file into a heap buffer.
 * Returns the buffer pointer (caller must free); writes size to *out_len.
 * Returns NULL on error. */
static uint8_t *read_binary_file(const char *path, size_t *out_len)
{
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "[ERR] Cannot open '%s': %s\n", path, strerror(errno));
        return NULL;
    }

    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    long sz = ftell(f);
    if (sz <= 0 || sz > (long)(MEM_SIZE - MEM_PROG_BASE)) {
        fprintf(stderr,
                "[ERR] File '%s' size %ld is invalid or too large "
                "(max %u bytes at 0x%08X)\n",
                path, sz, MEM_SIZE - MEM_PROG_BASE, MEM_PROG_BASE);
        fclose(f);
        return NULL;
    }
    rewind(f);

    uint8_t *buf = (uint8_t *)malloc((size_t)sz);
    if (!buf) { fclose(f); return NULL; }

    if (fread(buf, 1, (size_t)sz, f) != (size_t)sz) {
        fprintf(stderr, "[ERR] Short read on '%s'\n", path);
        free(buf);
        fclose(f);
        return NULL;
    }
    fclose(f);

    *out_len = (size_t)sz;
    return buf;
}

/* ── main ─────────────────────────────────────────────────────────────────── */

int main(int argc, char *argv[])
{
    /* ── Parse arguments ─────────────────────────────────────────────────── */
    if (argc < 2) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    const char *bin_path     = argv[1];
    int         trace        = 0;
    uint64_t    max_cycles   = 0;    /* 0 = unlimited */
    int         do_dump_mem  = 0;
    uint32_t    dump_addr    = 0;
    uint32_t    dump_len     = 0;

    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--trace") == 0) {
            trace = 1;
        } else if (strcmp(argv[i], "--max-cycles") == 0 && i + 1 < argc) {
            max_cycles = (uint64_t)strtoull(argv[++i], NULL, 0);
        } else if (strcmp(argv[i], "--dump-mem") == 0 && i + 2 < argc) {
            dump_addr   = (uint32_t)strtoul(argv[++i], NULL, 0);
            dump_len    = (uint32_t)strtoul(argv[++i], NULL, 0);
            do_dump_mem = 1;
        } else {
            fprintf(stderr, "[ERR] Unknown argument: %s\n", argv[i]);
            usage(argv[0]);
            return EXIT_FAILURE;
        }
    }

    /* ── Pin to core 0 ───────────────────────────────────────────────────── */
    pin_to_core0();

    /* ── Load program ─────────────────────────────────────────────────────── */
    size_t   prog_len = 0;
    uint8_t *prog_buf = read_binary_file(bin_path, &prog_len);
    if (!prog_buf)
        return EXIT_FAILURE;

    printf("[SIM] Loaded '%s': %zu bytes (%zu instructions)\n",
           bin_path, prog_len, prog_len / 4);

    /* ── Initialise memory ───────────────────────────────────────────────── */
    Memory mem;
    if (mem_init(&mem) != 0) {
        free(prog_buf);
        return EXIT_FAILURE;
    }

    if (mem_load_binary(&mem, MEM_PROG_BASE, prog_buf, prog_len) != 0) {
        free(prog_buf);
        mem_free(&mem);
        return EXIT_FAILURE;
    }
    free(prog_buf);

    printf("[SIM] Program loaded at 0x%08X – 0x%08X\n",
           MEM_PROG_BASE, (uint32_t)(MEM_PROG_BASE + prog_len - 1));

    /* ── Initialise CPU ──────────────────────────────────────────────────── */
    CPU cpu;
    cpu_init(&cpu, &mem);
    cpu_reset(&cpu, MEM_PROG_BASE);
    cpu.trace = trace;

    printf("[SIM] CPU reset. Entry point: 0x%08X\n", MEM_PROG_BASE);
    if (trace)
        printf("[SIM] Trace mode ON\n");
    if (max_cycles)
        printf("[SIM] Max cycles: %llu\n", (unsigned long long)max_cycles);

    printf("\n");

    /* ── Run ─────────────────────────────────────────────────────────────── */
    int rc = cpu_run(&cpu, max_cycles);

    /* ── Post-execution report ───────────────────────────────────────────── */
    printf("\n");

    if (rc == 0) {
        printf("[SIM] *** HALT reached normally ***\n");
    } else if (rc == -1) {
        printf("[SIM] *** CPU FAULT — execution stopped ***\n");
    } else if (rc == -2) {
        printf("[SIM] *** Max cycle limit exceeded ***\n");
    }

    cpu_dump_state(&cpu);

    if (do_dump_mem) {
        printf("\n[SIM] Memory dump at 0x%08X, %u bytes:\n",
               dump_addr, dump_len);
        mem_dump(&mem, dump_addr, dump_len);
    }

    /* ── Cleanup ─────────────────────────────────────────────────────────── */
    mem_free(&mem);

    return (rc == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
