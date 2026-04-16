/*
 * init.c – Minimal PID-1 init for CPU Simulator full-system QEMU demo.
 *
 * This program is the ONLY process in the initramfs.  It:
 *   1. Mounts /proc, /sys, /dev
 *   2. Forks a child that runs the CPU simulator
 *   3. Waits for the child to exit
 *   4. Powers off the virtual machine via LINUX_REBOOT_CMD_POWER_OFF
 *
 * Using fork+waitpid instead of execv so that when cpu_sim finishes,
 * PID 1 calls reboot() rather than causing a kernel panic.
 *
 * Compile as a fully-static ARM binary:
 *   arm-none-linux-gnueabihf-gcc -static -O2 -o init init.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mount.h>
#include <sys/reboot.h>
#include <sys/wait.h>
#include <linux/reboot.h>

#define DIVIDER \
    "============================================================\n"

int main(void)
{
    /* Mount virtual filesystems required by the simulator */
    mount("proc",     "/proc", "proc",    0, "");
    mount("sysfs",    "/sys",  "sysfs",   0, "");
    mount("devtmpfs", "/dev",  "devtmpfs",0, "");

    write(STDOUT_FILENO, "\n" DIVIDER, sizeof(DIVIDER));
    write(STDOUT_FILENO,
          "  Single-Threaded CPU Simulator — QEMU Full System Test\n"
          "  Target: ARM Cortex-A8 (BeagleBone Black equivalent)\n",
          100);
    write(STDOUT_FILENO, DIVIDER "\n", sizeof(DIVIDER) + 1);

    /* Fork a child to run the CPU simulator */
    pid_t pid = fork();
    if (pid == 0) {
        /* Child: exec the simulator */
        char *const args[] = {
            "/cpu_sim",
            "/test_fibonacci.bin",
            "--dump-mem", "0x00080000", "40",
            NULL
        };
        execv("/cpu_sim", args);
        perror("execv /cpu_sim");
        _exit(1);
    }

    /* Parent (PID 1): wait for child to finish */
    int status = 0;
    waitpid(pid, &status, 0);

    /* Flush all output */
    fflush(stdout);
    fflush(stderr);

    /* Power the virtual machine off cleanly */
    reboot(LINUX_REBOOT_CMD_POWER_OFF);
    return 0;
}
