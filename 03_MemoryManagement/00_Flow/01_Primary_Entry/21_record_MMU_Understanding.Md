## `record_mmu_state` — Detailed ARMv8 Explanation

This function detects the MMU/cache state at boot and records it in `x19` for later use. It also fixes endianness mismatches early.

---

### Phase 1: Determine Exception Level (lines 135–138)

```asm
mrs   x19, CurrentEL       // read CurrentEL system register
cmp   x19, #CurrentEL_EL2  // compare against EL2 (value 0x8)
mrs   x19, sctlr_el1       // speculatively read SCTLR_EL1
b.ne  0f                    // if not EL2, skip ahead (keep sctlr_el1)
mrs   x19, sctlr_el2       // we ARE at EL2, override with SCTLR_EL2
```

- **`CurrentEL`** (bits [3:2]): encodes the current exception level. EL1 = `0x4`, EL2 = `0x8`.
- **`SCTLR_ELx`** (System Control Register): controls MMU enable, caches, endianness, etc. The code reads the SCTLR for *whichever EL we're running at*, since that's the one governing current execution.

After this block, **`x19` = the active SCTLR** and the **condition flags still reflect the EL2 comparison** (`Z=1` if EL2, `Z=0` if EL1). This is critical — `b.ne` later reuses these flags.

---

### Phase 2: Check Endianness (lines 141–142)

```asm
CPU_LE( tbnz  x19, #SCTLR_ELx_EE_SHIFT, 1f )
CPU_BE( tbz   x19, #SCTLR_ELx_EE_SHIFT, 1f )
```

- **`SCTLR_ELx.EE`** (bit 25): Exception Endianness. `0` = little-endian, `1` = big-endian.
- `CPU_LE()` / `CPU_BE()` are compile-time macros — only one emits code depending on the kernel's configured endianness.
- **If kernel is LE**: branch to `1f` if EE=1 (hardware is BE, mismatch → needs fixing).
- **If kernel is BE**: branch to `1f` if EE=0 (hardware is LE, mismatch → needs fixing).

If endianness **matches**, fall through to Phase 3. If **mismatched**, jump to Phase 4.

---

### Phase 3: Record MMU + Cache State (lines 143–146)

```asm
tst   x19, #SCTLR_ELx_C    // test C bit (data cache enable), sets Z if C==0
and   x19, x19, #SCTLR_ELx_M  // isolate M bit (MMU enable)
csel  x19, xzr, x19, eq    // if Z==1 (caches off): x19 = 0; else: x19 = M bit
ret
```

- **`SCTLR_ELx.M`** (bit 0): MMU enable. `1` = MMU on.
- **`SCTLR_ELx.C`** (bit 2): Data cache enable. `1` = D-cache on.
- The logic encodes three states into `x19`:

| MMU (M) | D-Cache (C) | Result in x19 | Meaning                                    |
|---------|-------------|---------------|--------------------------------------------|
| 0       | 0           | `0`           | MMU off (normal boot)                      |
| 1       | 0           | `0`           | MMU on but caches off — treated as MMU-off |
| 1       | 1           | `1` (M bit)   |  MMU and caches on (e.g., EFI boot)        |
| 0       | 1           | `0`           | Impossible in practice                     |

The caller (`primary_entry`) uses `x19 != 0` to decide whether cache maintenance uses **invalidate** (MMU was off) or **clean** (MMU was on).

---

### Phase 4: Fix Endianness Mismatch (lines 154–165)

```asm
1:  eor   x19, x19, #SCTLR_ELx_EE  // toggle EE bit to match kernel endianness
    bic   x19, x19, #SCTLR_ELx_M   // clear M bit (disable MMU)
    b.ne  2f                         // branch uses flags from the EL2 cmp above!
    pre_disable_mmu_workaround       // Cortex-A errata workaround
    msr   sctlr_el2, x19            // we're at EL2: write corrected SCTLR_EL2
    b     3f
2:  pre_disable_mmu_workaround
    msr   sctlr_el1, x19            // we're at EL1: write corrected SCTLR_EL1
3:  isb                              // instruction synchronization barrier
    mov   x19, xzr                   // x19 = 0 (MMU is now off)
    ret
```

Key points:

- **`EOR` toggles EE**: flips the endianness to match what the kernel expects.
- **`BIC` clears M**: the MMU **must** be disabled when changing endianness, because the page tables were built for the original byte order — interpreting them with flipped endianness would cause immediate faults.
- **`b.ne 2f`**: the condition flags are *still* from the `cmp x19, #CurrentEL_EL2` at line 136. If we were at EL1 (NE), write `sctlr_el1`; if EL2 (EQ), write `sctlr_el2`.
- **`ISB`**: required after writing SCTLR to ensure the pipeline sees the new endianness/MMU settings before any subsequent instruction fetch.
- **`x19 = 0`**: since we just disabled the MMU, report MMU-off state to the caller.

---

### Summary

The function answers two questions for the boot path:
1. **Was the MMU+cache on at entry?** → stored in `x19` (nonzero = yes)
2. **Is the CPU endianness wrong?** → if so, fix it immediately by toggling `SCTLR.EE`, disable the MMU, and report MMU-off
