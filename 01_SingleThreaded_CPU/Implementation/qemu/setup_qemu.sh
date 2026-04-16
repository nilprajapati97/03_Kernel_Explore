#!/usr/bin/env bash
# =============================================================================
# setup_qemu.sh — Set up the full-system QEMU ARM environment
#
# What this script does:
#   1. Checks for qemu-system-arm, qemu-arm, and the ARM cross-compiler.
#   2. Downloads the Alpine Linux 6.12.81-virt ARM kernel (vmlinuz-virt).
#   3. Builds the minimal initramfs (init.c → compiled init → cpio.gz).
#
# Machine target : qemu-system-arm -M virt -cpu cortex-a15
# Kernel source  : Alpine Linux 3.21 armv7 linux-virt package (~24 MB APK)
#
# No root required.  No ext2 rootfs image needed.
#
# Usage:
#   cd Implementation/
#   make arm                  # cross-compile cpu_sim_arm
#   make assemble             # generate test_fibonacci.bin
#   bash qemu/setup_qemu.sh   # download kernel + build initramfs
#   bash qemu/run_qemu.sh     # run full-system test
# =============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
QEMU_DIR="${SCRIPT_DIR}"
ROOT_DIR="${SCRIPT_DIR}/.."   # Implementation/

KERNEL="${QEMU_DIR}/vmlinuz-virt"
INITRAMFS="${QEMU_DIR}/initramfs.cpio.gz"
INIT_C="${QEMU_DIR}/init.c"
INIT_BIN="${QEMU_DIR}/init"

ALPINE_VERSION="3.21"
ALPINE_PKG="linux-virt-6.12.81-r0.apk"
ALPINE_URL="https://dl-cdn.alpinelinux.org/alpine/v${ALPINE_VERSION}/main/armv7/${ALPINE_PKG}"

# ── Helpers ───────────────────────────────────────────────────────────────────
check() {
    if command -v "$1" >/dev/null 2>&1; then
        echo "  [OK]  $1 found at $(command -v "$1")"
        return 0
    else
        echo "  [!!]  $1 NOT found"
        return 1
    fi
}

# ── Header ────────────────────────────────────────────────────────────────────
echo ""
echo "╔═══════════════════════════════════════════════════════╗"
echo "║   Single-Threaded CPU Simulator — QEMU Setup          ║"
echo "║   Machine: qemu-system-arm -M virt (ARMv7 Cortex-A15)║"
echo "╚═══════════════════════════════════════════════════════╝"
echo ""

# ── Step 1: Check required tools ─────────────────────────────────────────────
echo "── Step 1: Checking required tools ─────────────────────────────────────"
MISSING=0

# Accept system qemu-system-arm or extracted fallback
if ! check qemu-system-arm && ! [[ -x "/tmp/toolchain/usr/bin/qemu-system-arm" ]]; then
    echo "  [FALLBACK] Will use /tmp/toolchain/usr/bin/qemu-system-arm if present"
    MISSING=$((MISSING+1))
fi
check qemu-arm          2>/dev/null || check /tmp/toolchain/usr/bin/qemu-arm 2>/dev/null || MISSING=$((MISSING+1))
check python3           || MISSING=$((MISSING+1))
check gcc               || MISSING=$((MISSING+1))
check cpio              || MISSING=$((MISSING+1))

# Cross-compiler: official ARM GNU toolchain or system
if command -v arm-linux-gnueabihf-gcc >/dev/null 2>&1; then
    CC_ARM="arm-linux-gnueabihf-gcc"
    echo "  [OK]  arm-linux-gnueabihf-gcc found"
elif [[ -x "/tmp/arm-gnu-toolchain/bin/arm-none-linux-gnueabihf-gcc" ]]; then
    CC_ARM="/tmp/arm-gnu-toolchain/bin/arm-none-linux-gnueabihf-gcc"
    echo "  [OK]  ARM toolchain found at ${CC_ARM}"
else
    echo "  [!!]  ARM cross-compiler not found"
    MISSING=$((MISSING+1))
fi

if [[ ${MISSING} -gt 0 ]]; then
    echo ""
    echo "  Some tools are missing. Install with:"
    echo "    sudo apt install qemu-system-arm qemu-user gcc-arm-linux-gnueabihf gcc python3 make"
fi

# ── Step 2: Download Alpine ARM virt kernel ───────────────────────────────────
echo ""
echo "── Step 2: Downloading Alpine Linux ARM virt kernel ────────────────────"

if [[ -f "${KERNEL}" ]]; then
    echo "  [OK]  vmlinuz-virt already present ($(du -h "${KERNEL}" | cut -f1))"
else
    echo "  [DL]  Downloading Alpine ${ALPINE_PKG} from dl-cdn.alpinelinux.org ..."
    TMPAPK="/tmp/${ALPINE_PKG}"
    wget --no-check-certificate -q --show-progress -O "${TMPAPK}" "${ALPINE_URL}"
    echo "  [..] Extracting vmlinuz-virt from APK ..."
    TMPDIR_KERNEL="/tmp/alpine_virt_kernel_setup"
    rm -rf "${TMPDIR_KERNEL}" && mkdir -p "${TMPDIR_KERNEL}"
    tar -xzf "${TMPAPK}" -C "${TMPDIR_KERNEL}" 2>/dev/null || true
    cp "${TMPDIR_KERNEL}/boot/vmlinuz-virt" "${KERNEL}"
    rm -rf "${TMPDIR_KERNEL}" "${TMPAPK}"
    echo "  [OK]  vmlinuz-virt installed ($(du -h "${KERNEL}" | cut -f1))"
fi

# ── Step 3: Compile init + build initramfs ────────────────────────────────────
echo ""
echo "── Step 3: Building initramfs ──────────────────────────────────────────"

CPU_SIM="${ROOT_DIR}/cpu_sim_arm"
TEST_BIN="${ROOT_DIR}/programs/test_fibonacci.bin"

[[ -f "${CPU_SIM}" ]]  || { echo "  [!!] Build cpu_sim_arm first: make arm";   exit 1; }
[[ -f "${TEST_BIN}" ]] || { echo "  [!!] Assemble test program:  make assemble"; exit 1; }

echo "  [..] Compiling init.c (ARM static) ..."
"${CC_ARM}" -static -O2 -o "${INIT_BIN}" "${INIT_C}"
echo "  [OK]  init compiled ($(du -h "${INIT_BIN}" | cut -f1))"

echo "  [..] Packing initramfs ..."
ROOTFS_DIR="${QEMU_DIR}/initramfs_root"
rm -rf "${ROOTFS_DIR}" && mkdir -p "${ROOTFS_DIR}"/{proc,sys,dev}
cp "${INIT_BIN}"   "${ROOTFS_DIR}/init"
cp "${CPU_SIM}"    "${ROOTFS_DIR}/cpu_sim"
cp "${TEST_BIN}"   "${ROOTFS_DIR}/test_fibonacci.bin"
(cd "${ROOTFS_DIR}" && find . | cpio -o --format=newc | gzip -9 > "${INITRAMFS}")
echo "  [OK]  initramfs.cpio.gz ready ($(du -h "${INITRAMFS}" | cut -f1))"

# ── Step 4: Instructions ──────────────────────────────────────────────────────
echo ""
echo "── Step 4: Done — Next steps ───────────────────────────────────────────"
echo ""
echo "  Run the full-system QEMU test:"
echo "    bash qemu/run_qemu.sh"
echo ""
echo "  Or via Makefile:"
echo "    make run-fullsystem"
echo ""
echo "  Quick ARM user-mode test (no kernel required):"
echo "    make run-arm"
echo ""

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
QEMU_DIR="${SCRIPT_DIR}"

# Buildroot BeagleBone Black artefacts (publicly available pre-built images)
# These are the Buildroot 2024.02 artefacts for am335x (Cortex-A8).
BUILDROOT_BASE="https://bootlin.com/pub/demos/beaglebone"
ZIMAGE_URL="${BUILDROOT_BASE}/zImage"
DTB_URL="${BUILDROOT_BASE}/am335x-boneblack.dtb"
ROOTFS_URL="${BUILDROOT_BASE}/rootfs.ext2.xz"

ZIMAGE="${QEMU_DIR}/zImage"
DTB="${QEMU_DIR}/am335x-boneblack.dtb"
ROOTFS_XZ="${QEMU_DIR}/rootfs.ext2.xz"
ROOTFS="${QEMU_DIR}/rootfs.ext2"

# ── Helpers ───────────────────────────────────────────────────────────────────
check() {
    if command -v "$1" >/dev/null 2>&1; then
        echo "  [OK]  $1 found at $(command -v "$1")"
        return 0
    else
        echo "  [!!]  $1 NOT found"
        return 1
    fi
}

download_if_missing() {
    local path="$1"
    local url="$2"
    local label="$3"
    if [[ -f "${path}" ]]; then
        echo "  [OK]  ${label} already present: ${path}"
        return 0
    fi
    echo "  [DL]  Downloading ${label} ..."
    echo "        URL: ${url}"
    if command -v wget >/dev/null 2>&1; then
        wget -q --show-progress -O "${path}" "${url}" || {
            echo "  [!!]  Download failed. See manual instructions below."
            return 1
        }
    elif command -v curl >/dev/null 2>&1; then
        curl -L --progress-bar -o "${path}" "${url}" || {
            echo "  [!!]  Download failed. See manual instructions below."
            return 1
        }
    else
        echo "  [!!]  Neither wget nor curl found. Cannot download automatically."
        return 1
    fi
    echo "  [OK]  Downloaded: ${path}"
}

# ── Header ────────────────────────────────────────────────────────────────────
echo ""
echo "╔═══════════════════════════════════════════════════════╗"
echo "║   Single-Threaded CPU Simulator — QEMU Setup          ║"
echo "║   Target: BeagleBone Black (AM335x / Cortex-A8)       ║"
echo "╚═══════════════════════════════════════════════════════╝"
echo ""

# ── Step 1: Check required tools ─────────────────────────────────────────────
echo "── Step 1: Checking required tools ─────────────────────────────────────"
MISSING=0

check qemu-system-arm   || MISSING=$((MISSING+1))
check qemu-arm          || MISSING=$((MISSING+1))
check arm-linux-gnueabihf-gcc || MISSING=$((MISSING+1))
check python3           || MISSING=$((MISSING+1))
check gcc               || MISSING=$((MISSING+1))

if [[ ${MISSING} -gt 0 ]]; then
    echo ""
    echo "  To install missing packages on Ubuntu/Debian:"
    echo "    sudo apt update"
    echo "    sudo apt install -y \\"
    echo "        qemu-system-arm qemu-user \\"
    echo "        gcc-arm-linux-gnueabihf \\"
    echo "        gcc python3 make"
    echo ""
fi

# ── Step 2: Download QEMU artefacts ──────────────────────────────────────────
echo ""
echo "── Step 2: Downloading BeagleBone Black QEMU artefacts ─────────────────"
echo "   Directory: ${QEMU_DIR}"
echo ""

DOWNLOAD_OK=1

download_if_missing "${ZIMAGE}" "${ZIMAGE_URL}" "Linux zImage (Cortex-A8)" \
    || DOWNLOAD_OK=0
download_if_missing "${DTB}"    "${DTB_URL}"    "am335x-boneblack.dtb"      \
    || DOWNLOAD_OK=0

# Download + decompress rootfs
if [[ ! -f "${ROOTFS}" ]]; then
    download_if_missing "${ROOTFS_XZ}" "${ROOTFS_URL}" "rootfs.ext2.xz" \
        || DOWNLOAD_OK=0
    if [[ -f "${ROOTFS_XZ}" ]]; then
        echo "  [..] Decompressing rootfs.ext2.xz ..."
        xz -dk "${ROOTFS_XZ}" && echo "  [OK] rootfs.ext2 ready"
    fi
else
    echo "  [OK]  rootfs.ext2 already present"
fi

echo ""
if [[ ${DOWNLOAD_OK} -eq 0 ]]; then
    echo "  ⚠  Some downloads failed. Manual download instructions:"
    echo ""
    echo "     wget -O qemu/zImage         ${ZIMAGE_URL}"
    echo "     wget -O qemu/am335x-boneblack.dtb ${DTB_URL}"
    echo "     wget -O qemu/rootfs.ext2.xz  ${ROOTFS_URL}"
    echo "     xz -dk qemu/rootfs.ext2.xz"
    echo ""
    echo "  Alternatively use the user-mode QEMU test (no download needed):"
    echo "     make run-arm"
else
    echo "  [OK] All QEMU artefacts are ready."
fi

# ── Step 3: Instructions ──────────────────────────────────────────────────────
echo ""
echo "── Step 3: Next steps ───────────────────────────────────────────────────"
echo ""
echo "  1. Build the ARM binary:"
echo "       make arm"
echo ""
echo "  2. Assemble the test program:"
echo "       make assemble"
echo ""
echo "  3. Launch the full BeagleBone Black QEMU simulation:"
echo "       bash qemu/run_qemu.sh"
echo ""
echo "  4. Or run the quick user-mode ARM test (no kernel needed):"
echo "       make run-arm"
echo ""
echo "  See qemu/README_QEMU.md for full documentation."
echo ""
