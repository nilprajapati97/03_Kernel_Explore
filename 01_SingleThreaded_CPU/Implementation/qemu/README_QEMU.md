# QEMU Testing Guide — Single-Threaded CPU Simulator on BeagleBone Black

## Overview

This guide explains how to run the CPU simulator inside a full-system QEMU
emulation of the BeagleBone Black (AM335x SoC, ARM Cortex-A8 core).

Two modes are supported:

| Mode | Tool | Speed | Use case |
|------|------|-------|----------|
| **User-mode** | `qemu-arm` | Instant | Quick ARM binary check on Linux host |
| **Full-system** | `qemu-system-arm -M am335x-boneblack` | ~20 s boot | Real hardware emulation |

---

## Prerequisites

### Install packages (Ubuntu / Debian)

```bash
sudo apt update
sudo apt install -y \
    qemu-system-arm \
    qemu-user       \
    gcc-arm-linux-gnueabihf \
    e2fsprogs       \
    gcc python3 make
```

---

## Quick Test — User-mode ARM (no kernel, no rootfs)

This is the fastest way to validate the ARM binary.

```bash
# From Implementation/
make arm          # cross-compile to CPU_sim_arm
make assemble     # generate programs/test_fibonacci.bin
make run-arm      # run under qemu-arm

# Equivalent manual command:
qemu-arm ./cpu_sim_arm programs/test_fibonacci.bin --trace \
    --dump-mem 0x00080000 40
```

Expected output:
```
[SYS] Process pinned to CPU core 0 (single-threaded model)
[SIM] Loaded 'programs/test_fibonacci.bin': NN bytes (N instructions)
...
  [     1] 0x00010000: LOAD_IMM   R1, #0  | Z=1 N=0 C=0 O=0
  ...
[SIM] *** HALT reached normally ***
...
Memory dump at 0x00080000:
  0x00080000: 00 00 00 00  01 00 00 00  01 00 00 00  02 00 00 00 ...
              ^^^F0=0      ^^^F1=1      ^^^F2=1      ^^^F3=2
```

---

## Full-System Test — BeagleBone Black Emulation

### Step 1: Download QEMU artefacts

```bash
cd Implementation/
bash qemu/setup_qemu.sh
```

This downloads:
- `qemu/zImage` — Linux kernel compiled for AM335x (Cortex-A8)
- `qemu/am335x-boneblack.dtb` — Device tree blob for BeagleBone Black
- `qemu/rootfs.ext2` — Minimal BusyBox root filesystem (Buildroot)

If the download fails, fetch them manually:

```bash
wget -O qemu/zImage \
    https://bootlin.com/pub/demos/beaglebone/zImage
wget -O qemu/am335x-boneblack.dtb \
    https://bootlin.com/pub/demos/beaglebone/am335x-boneblack.dtb
wget -O qemu/rootfs.ext2.xz \
    https://bootlin.com/pub/demos/beaglebone/rootfs.ext2.xz
xz -dk qemu/rootfs.ext2.xz
```

### Step 2: Build the ARM binary

```bash
make arm       # produces cpu_sim_arm  (static, ARM Cortex-A8)
make assemble  # produces programs/test_fibonacci.bin
```

Verify the binary:

```bash
file cpu_sim_arm
# Expected: ELF 32-bit LSB executable, ARM, EABI5 version 1 (SYSV),
#           statically linked, ...
```

### Step 3: Launch full-system QEMU

```bash
bash qemu/run_qemu.sh
```

The script:
1. Creates a working copy of `rootfs.ext2` → `rootfs_wd.ext2`
2. Injects `cpu_sim_arm` and `test_fibonacci.bin` into `/opt/` inside the image (using `debugfs`, no root required)
3. Boots `qemu-system-arm -M am335x-boneblack` with serial console on your terminal

You will see U-Boot and Linux kernel messages. The system boots in ≈ 10–20 s.

### Step 4: Run inside the emulated BeagleBone Black

At the `buildroot login:` prompt, press **Enter** (no password needed).

```bash
# Inside the QEMU shell (BeagleBone Black virtual machine):

# Run with trace output:
/opt/cpu_sim /opt/test_fibonacci.bin --trace

# Run and dump Fibonacci results from memory:
/opt/cpu_sim /opt/test_fibonacci.bin --dump-mem 0x00080000 40

# Confirm you're running on ARM Cortex-A8 (Cortex-A8 reported by kernel):
cat /proc/cpuinfo | grep "model name"
# Hardware  : Generic AM33XX (Flat DT)
# ...
```

Press **Ctrl-A** then **X** to exit QEMU.

---

## QEMU Command Details

```
qemu-system-arm
  -M am335x-boneblack      # Board: BeagleBone Black (AM335x SoC)
  -cpu cortex-a8           # CPU: ARM Cortex-A8 (same as real BBB)
  -m 256M                  # RAM: 256 MiB
  -nographic               # Serial console on terminal (no VGA window)
  -kernel  qemu/zImage     # Linux kernel image
  -dtb     qemu/am335x-boneblack.dtb    # Device tree
  -drive   file=qemu/rootfs_wd.ext2,format=raw,if=sd,index=0
  -append  "root=/dev/mmcblk0 rw console=ttyO0,115200n8 rootwait"
```

---

## Understanding the Single-Threaded Model

When `cpu_sim_arm` starts (even inside QEMU), it calls:

```c
sched_setaffinity(0, sizeof(mask), &mask);   /* pin to core 0 */
```

This matches the hardware reality: the BeagleBone Black's AM335x SoC has a
**single ARM Cortex-A8 core**. Just as the real hardware can only run one
thread at a time without an external co-processor, the simulator is pinned
physically to that single core.

The simulated CPU model mirrors this:
- One fetch-decode-execute pipeline with **no pipeline stages** (in-order)
- No multi-issue / out-of-order execution
- Single cycle per instruction (CPI = 1 model)
- `cpu->cycle_count` gives exact instruction throughput

---

## Troubleshooting

| Problem | Solution |
|---------|----------|
| `qemu-system-arm: command not found` | `sudo apt install qemu-system-arm` |
| `arm-linux-gnueabihf-gcc: not found` | `sudo apt install gcc-arm-linux-gnueabihf` |
| `debugfs: not found` | `sudo apt install e2fsprogs` |
| Download fails (offline) | Use `make run-arm` (user-mode, no download needed) |
| QEMU hangs on boot | Press Enter; or try `Ctrl-A X` to exit and re-run |
| Binary not found inside QEMU | Ensure `debugfs` is installed; re-run `run_qemu.sh` |
| `[MEM FAULT]` in simulator | Program accesses invalid address; check `.asm` |

---

## Project Structure Reference

```
Implementation/
├── src/
│   ├── instructions.h    Custom ISA: opcodes, encoding, Instruction struct
│   ├── memory.h/.c       1 MiB flat simulated memory
│   ├── alu.h/.c          ALU: ADD/SUB/MUL/DIV/AND/OR/XOR/NOT/SHL/SHR + flags
│   ├── decoder.h/.c      32-bit word → Instruction + disassembler
│   ├── cpu.h/.c          Fetch-decode-execute engine, cpu_run(), state dump
│   └── main.c            Entry point, CLI, CPU affinity pinning
├── programs/
│   └── test_fibonacci.asm   Fibonacci(10) test program
├── tools/
│   └── assembler.py         .asm → .bin Python assembler
├── qemu/
│   ├── setup_qemu.sh        Download QEMU artefacts
│   ├── run_qemu.sh          Launch full BeagleBone Black emulation
│   └── README_QEMU.md       This file
└── Makefile
```
