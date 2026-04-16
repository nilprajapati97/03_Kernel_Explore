#!/usr/bin/env bash
# =============================================================================
# run_qemu.sh — Launch the full-system QEMU ARM simulation of the CPU Simulator
#
# Boots a minimal Linux (Alpine 6.12.81-virt) inside qemu-system-arm (-M virt)
# using a tiny custom initramfs that IS the CPU simulator.  No rootfs image,
# no U-Boot, no network required.
#
# Prerequisites:
#   bash qemu/setup_qemu.sh     (downloads kernel, builds initramfs)
#   make arm                    (cross-compile cpu_sim_arm)
#   make assemble               (generate programs/test_fibonacci.bin)
#
# Usage:
#   cd Implementation/
#   bash qemu/run_qemu.sh               # full run; serial → this terminal
#   bash qemu/run_qemu.sh --serial-file # capture output in qemu/serial_out.txt
#
# What happens:
#   1. qemu-system-arm -M virt boots vmlinuz-virt (ARMv7 kernel).
#   2. Our custom init mounts /proc + /sys, then forks cpu_sim_arm.
#   3. Simulator loads test_fibonacci.bin and executes 73 clock cycles.
#   4. Output appears on serial console; QEMU powers off automatically.
# =============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="${SCRIPT_DIR}/.."          # Implementation/
QEMU_DIR="${SCRIPT_DIR}"

KERNEL="${QEMU_DIR}/vmlinuz-virt"
INITRAMFS="${QEMU_DIR}/initramfs.cpio.gz"
CPU_SIM="${ROOT_DIR}/cpu_sim_arm"
TEST_BIN="${ROOT_DIR}/programs/test_fibonacci.bin"

# Fall back to extracted qemu-system-arm if not on PATH
if command -v qemu-system-arm >/dev/null 2>&1; then
    QEMU_SYS="qemu-system-arm"
elif [[ -x "/tmp/toolchain/usr/bin/qemu-system-arm" ]]; then
    QEMU_SYS="/tmp/toolchain/usr/bin/qemu-system-arm"
    export LD_LIBRARY_PATH="/tmp/toolchain/usr/lib/x86_64-linux-gnu${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
else
    echo "[ERROR] qemu-system-arm not found. Install with: sudo apt install qemu-system-arm"; exit 1
fi

SERIAL_FILE=0
for arg in "$@"; do
    [[ "$arg" == "--serial-file" ]] && SERIAL_FILE=1
done

# ── Sanity checks ─────────────────────────────────────────────────────────────
echo ""
echo "╔══════════════════════════════════════════════════════════════╗"
echo "║  Single-Threaded CPU Simulator — QEMU Full-System Test       ║"
echo "║  Machine: qemu-system-arm -M virt (ARMv7 Cortex-A15)        ║"
echo "║  Kernel : Alpine Linux 6.12.81-virt (armv7)                  ║"
echo "╚══════════════════════════════════════════════════════════════╝"
echo ""

die() { echo "[ERROR] $*"; exit 1; }

[[ -f "${KERNEL}" ]]    || die "Kernel not found: ${KERNEL}       (run qemu/setup_qemu.sh)"
[[ -f "${INITRAMFS}" ]] || die "Initramfs not found: ${INITRAMFS}  (run qemu/setup_qemu.sh)"
[[ -f "${CPU_SIM}" ]]   || die "ARM binary not found: ${CPU_SIM}   (run: make arm)"
[[ -f "${TEST_BIN}" ]]  || die "Test program not found: ${TEST_BIN} (run: make assemble)"

# ── Step 1: Rebuild initramfs with current binaries ───────────────────────────
echo "[QEMU] Rebuilding initramfs with current cpu_sim_arm + test_fibonacci.bin ..."
rm -rf "${QEMU_DIR}/initramfs_root"
mkdir -p "${QEMU_DIR}/initramfs_root"/{proc,sys,dev}
cp "${QEMU_DIR}/init"  "${QEMU_DIR}/initramfs_root/init"
cp "${CPU_SIM}"        "${QEMU_DIR}/initramfs_root/cpu_sim"
cp "${TEST_BIN}"       "${QEMU_DIR}/initramfs_root/test_fibonacci.bin"
(cd "${QEMU_DIR}/initramfs_root" && find . | cpio -o --format=newc | gzip -9 > "${INITRAMFS}")
echo "[QEMU] Initramfs ready: ${INITRAMFS} ($(du -h "${INITRAMFS}" | cut -f1))"

# ── Step 2: Launch QEMU ───────────────────────────────────────────────────────
echo ""
echo "[QEMU] Launching: ${QEMU_SYS} -M virt -cpu cortex-a15 -m 256M"
echo "[QEMU] The simulator will run automatically and QEMU will power off."
echo "       (no user interaction required)"
echo ""
echo "══════════════════════════════════════════════════════════════"

if [[ "${SERIAL_FILE}" -eq 1 ]]; then
    OUT="${QEMU_DIR}/serial_out.txt"
    echo "[QEMU] Capturing output to: ${OUT}"
    "${QEMU_SYS}" \
        -M virt -cpu cortex-a15 -m 256M \
        -kernel    "${KERNEL}"           \
        -initrd    "${INITRAMFS}"        \
        -append    "console=ttyAMA0,115200 rdinit=/init" \
        -serial    "file:${OUT}"         \
        -nic none -display none -no-reboot
    echo "[QEMU] Done. Output:"
    cat "${OUT}"
else
    "${QEMU_SYS}" \
        -M virt -cpu cortex-a15 -m 256M \
        -kernel    "${KERNEL}"           \
        -initrd    "${INITRAMFS}"        \
        -append    "console=ttyAMA0,115200 rdinit=/init" \
        -nographic -nic none -no-reboot
fi
