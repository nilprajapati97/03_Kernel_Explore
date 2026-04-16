# Kernel_Explore — Linux Kernel: Real Scenario, Deep Understanding

A hands-on, bottom-up journey through the Linux kernel — built module by module,
from bare-metal CPU simulation all the way up to scheduling, memory management,
file systems and device drivers.

Each module has a working **C implementation** you can build and run, plus
complete documentation that maps the implementation back to the real Linux
kernel source tree.

---

## Why This Repository Exists

Reading the Linux kernel source cold is overwhelming. `arch/x86/kernel/entry_64.S`,
`mm/slub.c`, `fs/namei.c` — thousands of files, millions of lines. Where do you
start?

This repository answers that question by building understanding **incrementally**:

1. Start with the most fundamental abstraction — a CPU.
2. Add one concept at a time (scheduling, virtual memory, system calls, …).
3. At every step, read the *matching* kernel source code with a map in hand.

---

## Repository Structure

```
Kernel_Explore/
│
├── ReadMe.Md                          ← You are here
│
├── docs/                              ← Deep-dive reference documents
│   ├── LINUX_KERNEL_DIRECTORY_STRUCTURE.md   Linux kernel source tree, dir by dir
│   ├── KERNEL_CONCEPTS_OVERVIEW.md           All major kernel subsystems at a glance
│   ├── KERNEL_PROCESS_MANAGEMENT.md          Processes, threads, scheduling
│   └── KERNEL_MEMORY_MANAGEMENT.md           Virtual memory, paging, allocators
│
└── 01_SingleThreaded_CPU/             ← Module 1: CPU simulation + context
    ├── README.md                      ← Module concepts + implementation guide
    └── Implementation/                ← Buildable C code + QEMU scripts
        ├── src/
        │   ├── instructions.h         Custom RISC ISA definition
        │   ├── alu.h / alu.c          Arithmetic-Logic Unit
        │   ├── memory.h / memory.c    1 MiB simulated flat memory
        │   ├── decoder.h / decoder.c  32-bit word → Instruction + disassembler
        │   ├── cpu.h / cpu.c          Fetch-Decode-Execute engine
        │   └── main.c                 Entry point, CLI, CPU affinity pinning
        ├── programs/
        │   └── test_fibonacci.asm     Fibonacci(10) test program
        ├── tools/
        │   └── assembler.py           .asm → .bin Python assembler
        ├── qemu/
        │   ├── README_QEMU.md         QEMU full-system testing guide
        │   ├── setup_qemu.sh          Download kernel + build initramfs
        │   └── run_qemu.sh            Boot full-system ARM QEMU
        └── Makefile
```

---

## Learning Modules

| # | Module | Kernel Concepts Covered | Status |
|---|--------|------------------------|--------|
| 01 | [Single-Threaded CPU](01_SingleThreaded_CPU/README.md) | CPU context, registers, fetch-decode-execute, single-core scheduling | ✅ Complete |
| 02 | Multi-Threaded CPU & Context Switch | `task_struct`, context switch (`switch_to`), register save/restore | 🔜 Planned |
| 03 | Process & Thread Lifecycle | `fork`, `exec`, `exit`, `wait`, zombie/orphan processes | 🔜 Planned |
| 04 | Scheduler (CFS) | Completely Fair Scheduler, run queues, `schedule()`, priority | 🔜 Planned |
| 05 | Virtual Memory & Paging | Page tables, TLB, `mmap`, demand paging, page faults | 🔜 Planned |
| 06 | Memory Allocators | `kmalloc`/`kfree`, slab/slub allocator, buddy system | 🔜 Planned |
| 07 | System Calls | syscall entry, `sys_call_table`, user↔kernel boundary | 🔜 Planned |
| 08 | Virtual File System (VFS) | `inode`, `dentry`, `file`, `super_block`, VFS layer | 🔜 Planned |
| 09 | Inter-Process Communication | Pipes, signals, shared memory, semaphores, futexes | 🔜 Planned |
| 10 | Device Drivers & Interrupts | IRQ handling, `request_irq`, character/block drivers | 🔜 Planned |

---

## Quick Start — Module 01

```bash
cd 01_SingleThreaded_CPU/Implementation

# Build and run a Fibonacci program on the simulated CPU:
make run

# Cross-compile for ARM and run under QEMU (user-mode):
make run-arm

# Full BeagleBone Black system emulation:
bash qemu/setup_qemu.sh
make run-fullsystem
```

See [01_SingleThreaded_CPU/README.md](01_SingleThreaded_CPU/README.md) for the
full walkthrough.

---

## Reference Documentation

| Document | Contents |
|----------|----------|
| [Linux Kernel Directory Structure](docs/LINUX_KERNEL_DIRECTORY_STRUCTURE.md) | Every top-level directory in the kernel source explained |
| [Kernel Concepts Overview](docs/KERNEL_CONCEPTS_OVERVIEW.md) | All major subsystems: scheduling, MM, FS, networking, IPC, drivers |
| [Process Management Deep Dive](docs/KERNEL_PROCESS_MANAGEMENT.md) | `task_struct`, `fork`, CFS, signals, namespaces |
| [Memory Management Deep Dive](docs/KERNEL_MEMORY_MANAGEMENT.md) | Physical/virtual memory, page tables, allocators, OOM |

---

## How to Read the Kernel Source Alongside This Repo

Every module README points to specific files in the Linux kernel source.
The recommended way to browse them:

```bash
# Clone a recent stable kernel (shallow for speed):
git clone --depth=1 --branch v6.9 \
    https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git

# Or browse online:
# https://elixir.bootlin.com/linux/latest/source
# https://github.com/torvalds/linux
```

Cross-reference tags used in this repo:

- **`[kernel: path/to/file.c]`** — points to the real kernel file
- **`[kernel: function_name()]`** — points to a specific function
- **`[kernel: struct name]`** — points to a data structure definition

---

## Contributing

Contributions are welcome! To add a new module:

1. Create a directory `NN_ModuleName/` (e.g., `02_ContextSwitch/`).
2. Add a `README.md` following the same structure as module 01.
3. Add a working `Implementation/` with `Makefile`, `src/`, etc.
4. Update the modules table above and the `docs/` reference documents.

---

## Licence

MIT — see individual files for details.
