# ARM64 Memory Initialization — Bit-by-Bit QEMU Validation

## Target Hardware
- **SDM660** (Qualcomm Snapdragon 660) — Kryo 260 (Cortex-A73 + Cortex-A53)
- **QEMU proxy**: `virt` machine with `cortex-a72` CPU (closest to Kryo 260 big cores)
- **Config**: 4GB RAM, 8 CPUs, GICv3, 4KB pages, 48-bit VA

## Quick Start

```bash
# 1. Install prerequisites
sudo apt install -y gcc-aarch64-linux-gnu qemu-system-aarch64 \
    build-essential flex bison libssl-dev bc libelf-dev cpio gdb-multiarch

# 2. Build kernel (downloads Linux 6.12 if needed)
cd Implementation
./scripts/build_kernel.sh

# 3. Build kernel modules
make modules

# 4. Create root filesystem
./scripts/create_rootfs.sh

# 5. Run on QEMU (boots, loads modules, prints MM state)
./scripts/run_qemu.sh

# 6. Run with GDB debugging (for assembly phases 1-6)
./scripts/run_qemu_gdb.sh
# In another terminal:
gdb-multiarch -x gdb/early_boot.gdb
```

## What Gets Validated

### Assembly Phases (via GDB — before modules can load)
| Phase | What | Register/State Decoded |
|-------|------|----------------------|
| 1 | Primary Entry | SCTLR_EL1 bits (M,C,EE), x19/x20/x21 |
| 2 | Identity Map | idmap_pg_dir entries, VA==PA verification |
| 3 | CPU Setup | TCR_EL1 (20+ fields), MAIR_EL1 (8 attrs), SCTLR_EL1 |
| 4 | Enable MMU | TTBR0/TTBR1 BADDR, SCTLR M=1 transition |
| 5 | Map Kernel | TTBR1 transitions: reserved→init→swapper |
| 6 | Primary Switched | SP, VBAR_EL1, SP_EL0, TPIDR_EL1 |

### Kernel Module Phases (loaded at runtime)
| Module | Phase | Bit-by-Bit Inspection |
|--------|-------|-----------------------|
| 01_mmu_state_inspect | 1,3,4 | SCTLR/TCR/MAIR/TTBR/MMFR0 full field decode |
| 02_idmap_walk | 2 | Walk idmap_pg_dir, decode every PTE bit |
| 03_pagetable_walk | 5,9 | Walk swapper_pg_dir for key addresses |
| 04_ttbr_translate | 4,5 | Manual VA→PA translation, index extraction |
| 05_kernel_segments | 5 | Segment boundaries, PA/VA/prot verification |
| 06_boot_info | 6 | Boot mode, FDT, kimage_voffset, MIDR_EL1 |
| 07_fixmap_inspect | 7 | Fixmap slot enumeration |
| 08_memblock_dump | 8 | memblock.memory/reserved walk |
| 09_linear_map_verify | 9 | phys↔virt round-trip, mapping type counts |
| 10_zone_inspect | 10 | Per-node/zone PFN ranges, watermarks |
| 11_buddy_inspect | 11 | Per-order/migtype free counts, alloc test |
| 12_slab_inspect | 12 | kmem_cache list, allocation test |
| 13_vmalloc_inspect | 13 | vmap_area walk, vmalloc alloc/free test |

## Directory Structure

```
Implementation/
├── Makefile                  # Master build
├── configs/
│   ├── kernel.config         # Kernel config fragment (MM debug)
│   └── kernel_cmdline.txt    # Boot command line reference
├── scripts/
│   ├── build_kernel.sh       # Download + build kernel
│   ├── create_rootfs.sh      # BusyBox initramfs
│   ├── run_qemu.sh           # Launch QEMU
│   ├── run_qemu_gdb.sh       # Launch QEMU with GDB
│   ├── extract_mm_logs.sh    # Parse dmesg for MM phases
│   └── extract_early_boot.sh # Parse early boot dmesg
├── gdb/
│   ├── early_boot.gdb        # GDB breakpoint script
│   └── register_decode.py    # Python GDB register decoder
├── modules/
│   ├── Kbuild                # Kernel build integration
│   ├── Makefile              # Module Makefile
│   ├── common.h              # Shared macros
│   └── 01-13_*.c             # 13 inspection modules
└── tests/
    ├── run_all_tests.sh      # Master test runner
    ├── test_phase_order.sh   # Init order validation
    ├── test_registers.sh     # Register bit validation
    └── init_script.sh        # Initramfs /init
```
