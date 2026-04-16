# =============================================================================
# Makefile — Single-Threaded CPU Simulator
#
# Targets:
#   native          Build with native GCC for local testing (default)
#   arm             Cross-compile for ARM Cortex-A8 (BeagleBone Black / QEMU)
#   assemble        Run the Python assembler to generate programs/*.bin
#   run             Build (native) then run the Fibonacci test
#   run-arm         Run the ARM binary under qemu-arm (user-mode, quick test)
#   clean           Remove build artefacts
#   help            Show this help
#
# Requirements:
#   native:  gcc, python3
#   arm:     arm-linux-gnueabihf-gcc, qemu-arm (for run-arm)
#            Full-system QEMU: see qemu/README_QEMU.md
# =============================================================================

# ── Directories ──────────────────────────────────────────────────────────────
SRC_DIR      := src
PROG_DIR     := programs
TOOLS_DIR    := tools

# ── Sources ───────────────────────────────────────────────────────────────────
SRCS := \
	$(SRC_DIR)/main.c     \
	$(SRC_DIR)/cpu.c      \
	$(SRC_DIR)/alu.c      \
	$(SRC_DIR)/decoder.c  \
	$(SRC_DIR)/memory.c

# ── qemu-arm: fall back to extracted binary ───────────────────────────────────
ifeq ($(shell command -v qemu-arm 2>/dev/null),)
  QEMU_ARM := /tmp/toolchain/usr/bin/qemu-arm
else
  QEMU_ARM := qemu-arm
endif
BIN_NATIVE   := cpu_sim
BIN_ARM      := cpu_sim_arm

# ── Native (host) compiler settings ──────────────────────────────────────────
CC_NATIVE    := gcc
CFLAGS_NATIVE := \
	-std=c11        \
	-Wall           \
	-Wextra         \
	-Wpedantic      \
	-Wshadow        \
	-O2             \
	-I$(SRC_DIR)    \
	-D_GNU_SOURCE

# ── ARM cross-compiler settings ───────────────────────────────────────────────
# Target: ARM Cortex-A8 (BeagleBone Black, AM335x)
# -static: produce a self-contained binary for minimal rootfs (no shared libs)
# Fall back to official ARM GNU toolchain (self-contained, no root needed)
ifeq ($(shell command -v arm-linux-gnueabihf-gcc 2>/dev/null),)
  CC_ARM := /tmp/arm-gnu-toolchain/bin/arm-none-linux-gnueabihf-gcc
else
  CC_ARM := arm-linux-gnueabihf-gcc
endif
CFLAGS_ARM   := \
	-std=c11                \
	-Wall                   \
	-Wextra                 \
	-Wpedantic              \
	-Wshadow                \
	-O2                     \
	-I$(SRC_DIR)            \
	-D_GNU_SOURCE           \
	-march=armv7-a          \
	-mtune=cortex-a8        \
	-mfpu=neon              \
	-mfloat-abi=hard        \
	-static

# ── Assembler settings ────────────────────────────────────────────────────────
ASSEMBLER    := python3 $(TOOLS_DIR)/assembler.py
ASM_SRC      := $(PROG_DIR)/test_fibonacci.asm
ASM_BIN      := $(PROG_DIR)/test_fibonacci.bin

# ── Default target ────────────────────────────────────────────────────────────
.DEFAULT_GOAL := native

.PHONY: native arm assemble run run-arm clean help

# ── native: build with host GCC ───────────────────────────────────────────────
native: $(BIN_NATIVE)

$(BIN_NATIVE): $(SRCS)
	@echo "[BUILD] Compiling native binary: $(BIN_NATIVE)"
	$(CC_NATIVE) $(CFLAGS_NATIVE) -o $@ $^
	@echo "[BUILD] Done: $@"
	@file $@

# ── arm: cross-compile for BeagleBone Black ───────────────────────────────────
arm: $(BIN_ARM)

$(BIN_ARM): $(SRCS)
	@echo "[BUILD] Cross-compiling ARM binary: $(BIN_ARM)"
	@test -x $(CC_ARM) || { \
	  echo "[ERROR] $(CC_ARM) not found."; \
	  echo "        Install with: sudo apt install gcc-arm-linux-gnueabihf"; \
	  exit 1; }
	$(CC_ARM) $(CFLAGS_ARM) -o $@ $^
	@echo "[BUILD] Done: $@"
	@file $@

# ── assemble: .asm → .bin via Python assembler ────────────────────────────────
assemble: $(ASM_BIN)

$(ASM_BIN): $(ASM_SRC) $(TOOLS_DIR)/assembler.py
	@echo "[ASM]   Assembling: $(ASM_SRC) → $(ASM_BIN)"
	$(ASSEMBLER) $(ASM_SRC) $(ASM_BIN)

# ── qemu-system-arm: fall back to extracted binary ───────────────────────────
ifeq ($(shell command -v qemu-system-arm 2>/dev/null),)
  QEMU_SYS     := /tmp/toolchain/usr/bin/qemu-system-arm
  QEMU_SYS_ENV := env LD_LIBRARY_PATH=/tmp/toolchain/usr/lib/x86_64-linux-gnu
else
  QEMU_SYS     := qemu-system-arm
  QEMU_SYS_ENV :=
endif

# ── run: build native + assemble + execute under simulator ────────────────────
run: native assemble
	@echo ""
	@echo "════════════════════════════════════════════════════"
	@echo "  Running Fibonacci test on native CPU simulator"
	@echo "════════════════════════════════════════════════════"
	./$(BIN_NATIVE) $(ASM_BIN) --trace \
	    --dump-mem 0x00080000 40
	@echo ""
	@echo "Expected Fibonacci values at 0x00080000:"
	@echo "  F0=0  F1=1  F2=1  F3=2  F4=3  F5=5  F6=8  F7=13  F8=21  F9=34"

# ── run-arm: run ARM binary under qemu-arm (user-mode; quick test) ────────────
run-arm: arm assemble
	@echo ""
	@echo "════════════════════════════════════════════════════"
	@echo "  Running ARM binary under qemu-arm (user-mode)"
	@echo "════════════════════════════════════════════════════"
	@test -x $(QEMU_ARM) || { \
	  echo "[ERROR] qemu-arm not found."; \
	  echo "        Install with: sudo apt install qemu-user"; \
	  exit 1; }
	$(QEMU_ARM) ./$(BIN_ARM) $(ASM_BIN) --trace \
	    --dump-mem 0x00080000 40

# ── run-fullsystem: full-system QEMU with Alpine Linux virt kernel ─────────────
run-fullsystem: arm assemble
	@echo ""
	@echo "════════════════════════════════════════════════════"
	@echo "  Full-system QEMU: qemu-system-arm -M virt"
	@echo "  Kernel: Alpine Linux 6.12.81-virt (armv7)"
	@echo "════════════════════════════════════════════════════"
	@[[ -f qemu/vmlinuz-virt ]] || { \
	  echo "[INFO] Kernel not found. Running setup..."; \
	  bash qemu/setup_qemu.sh; }
	@[[ -f qemu/initramfs.cpio.gz ]] || bash qemu/setup_qemu.sh
	@test -x $(QEMU_SYS) || { \
	  echo "[ERROR] qemu-system-arm not found. Install: sudo apt install qemu-system-arm"; \
	  exit 1; }
	$(QEMU_SYS_ENV) $(QEMU_SYS) \
	    -M virt -cpu cortex-a15 -m 256M \
	    -kernel    qemu/vmlinuz-virt    \
	    -initrd    qemu/initramfs.cpio.gz \
	    -append    "console=ttyAMA0,115200 rdinit=/init" \
	    -nographic -nic none -no-reboot

# ── clean ─────────────────────────────────────────────────────────────────────
clean:
	@echo "[CLEAN] Removing build artefacts"
	rm -f $(BIN_NATIVE) $(BIN_ARM) $(ASM_BIN)
	rm -f qemu/init qemu/initramfs.cpio.gz
	rm -rf qemu/initramfs_root
	@echo "[CLEAN] Done"

# ── help ──────────────────────────────────────────────────────────────────────
help:
	@echo ""
	@echo "Single-Threaded CPU Simulator — Make targets"
	@echo "─────────────────────────────────────────────"
	@echo "  native          Build with native GCC              (default)"
	@echo "  arm             Cross-compile for ARM Cortex-A8"
	@echo "  assemble        Assemble programs/test_fibonacci.asm → .bin"
	@echo "  run             Build + assemble + run native simulation"
	@echo "  run-arm         Build ARM + run under qemu-arm (user-mode)"
	@echo "  run-fullsystem  Full-system QEMU boot (Alpine Linux virt kernel)"
	@echo "  clean           Remove cpu_sim, cpu_sim_arm, *.bin, initramfs"
	@echo "  help            Show this help"
	@echo ""
	@echo "Full-system QEMU setup:"
	@echo "  bash qemu/setup_qemu.sh   (downloads kernel + builds initramfs)"
	@echo "  bash qemu/run_qemu.sh     (runs full-system QEMU)"
	@echo ""
