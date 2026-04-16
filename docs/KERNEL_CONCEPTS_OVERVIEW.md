# Linux Kernel — Concepts Overview

> A map of how all major kernel subsystems fit together, with pointers to
> the relevant source directories and key data structures.

---

## The Big Picture

```
╔══════════════════════════════════════════════════════════════════════════╗
║                        User Space                                        ║
║  Applications   Shell   Libraries (glibc)   Daemons   Containers        ║
╠══════════════════════════════════════════════════════════════════════════╣
║                     System Call Interface                                ║
║              (arch/x86/kernel/entry_64.S, kernel/sys.c)                 ║
╠══════════════╦══════════════╦═══════════════╦════════════════════════════╣
║   Process    ║    Memory    ║  File System  ║    Device Drivers          ║
║  Management  ║  Management  ║     (VFS)     ║    & Hardware              ║
║  kernel/     ║  mm/         ║  fs/          ║    drivers/                ║
║  sched/      ║              ║               ║                            ║
╠══════════════╩══════════════╩═══════════════╩════════════════════════════╣
║             Networking          IPC          Security (LSM)              ║
║             net/                ipc/         security/                   ║
╠══════════════════════════════════════════════════════════════════════════╣
║                  Architecture-Specific Layer (arch/)                     ║
║     CPU init   Paging   Interrupt handling   Context switch              ║
╠══════════════════════════════════════════════════════════════════════════╣
║                         Hardware                                         ║
║   CPU   RAM   Disk   NIC   GPU   USB   Serial   Timers   …              ║
╚══════════════════════════════════════════════════════════════════════════╝
```

---

## 1. Process Management

**Where:** `kernel/` (especially `kernel/sched/`), `arch/*/kernel/`

The kernel must manage the lifecycle of every process and thread: creation,
execution, suspension, waking, and termination.

### Key concepts

| Concept | Description | Kernel location |
|---------|-------------|-----------------|
| `task_struct` | The process descriptor — holds everything about a task | `include/linux/sched.h` |
| `fork()` | Create a copy of a process | `kernel/fork.c` → `copy_process()` |
| `exec()` | Replace process image with a new program | `fs/exec.c` → `do_execveat_common()` |
| `exit()` | Terminate a process | `kernel/exit.c` → `do_exit()` |
| Context switch | Save current task state; restore next task state | `kernel/sched/core.c` → `context_switch()` |
| CFS scheduler | Pick which task runs next | `kernel/sched/fair.c` |
| Wait queues | Sleep until a condition is true | `kernel/sched/wait.c` |
| Signals | Asynchronous notifications between processes | `kernel/signal.c` |

### Process states

```
TASK_RUNNING         — on a run queue, or currently executing on a CPU
TASK_INTERRUPTIBLE   — sleeping, woken by signal or event
TASK_UNINTERRUPTIBLE — sleeping, NOT woken by signal (disk I/O wait)
TASK_ZOMBIE          — exited, parent has not called wait() yet
TASK_STOPPED         — stopped by SIGSTOP / SIGTSTP
```

### Thread vs process

In the Linux kernel, both processes and threads are `task_struct` instances.
The difference: threads created with `clone(CLONE_VM | CLONE_FILES | …)` share
the memory descriptor (`mm_struct`) and file descriptor table; processes created
with `fork()` get private copies.

---

## 2. Memory Management

**Where:** `mm/`, `arch/*/mm/`

The kernel manages two distinct kinds of memory:

1. **Physical memory** — RAM pages allocated by the buddy allocator.
2. **Virtual memory** — the address space seen by each process, mapped
   through page tables.

### Key concepts

| Concept | Description | Kernel location |
|---------|-------------|-----------------|
| `mm_struct` | Per-process virtual address space descriptor | `include/linux/mm_types.h` |
| `vm_area_struct` | One contiguous virtual memory region (e.g., stack, heap, mmap) | `include/linux/mm_types.h` |
| `struct page` | One physical page frame (4 KiB) | `include/linux/mm_types.h` |
| Buddy allocator | Physical page allocator | `mm/page_alloc.c` |
| SLUB allocator | Small object allocator (`kmalloc`) | `mm/slub.c` |
| Page fault | Handle access to unmapped/swapped-out page | `mm/memory.c` → `handle_mm_fault()` |
| `mmap()` | Map files or anonymous memory into address space | `mm/mmap.c` |
| Page cache | Cache file data in RAM | `mm/filemap.c` |
| OOM killer | Kill a process when memory is exhausted | `mm/oom_kill.c` |
| Huge pages | 2 MiB / 1 GiB pages for reduced TLB pressure | `mm/hugetlb.c` |
| cgroups memory | Per-cgroup memory limits | `mm/memcontrol.c` |

### Virtual address space layout (x86-64)

```
0xFFFFFFFFFFFFFFFF  ─────────────────────────────── top of address space
                     Kernel direct mapping (physmem mapped here)
                     Kernel code, data, modules
0xFFFF800000000000  ─────────────────────────────── kernel space boundary
                     (unmapped / non-canonical)
0x00007FFFFFFFFFFF  ─────────────────────────────── user space top
                     Stack (grows down)
                     ...
                     Shared libraries (.so files, mmap)
                     ...
                     Heap (grows up via brk/mmap)
                     BSS / data / text segments
0x0000000000400000  ─────────────────────────────── typical ELF load address
0x0000000000000000  ─────────────────────────────── NULL (always unmapped)
```

---

## 3. File System (VFS)

**Where:** `fs/`

The VFS provides a uniform interface for all file systems. An application calls
`read()` without knowing whether the file is on ext4, btrfs, NFS, or tmpfs.

### The four VFS objects

```
super_block ──── inode ──── dentry ──── file
     │               │           │         │
 per-mount       per-file    per-path   per-open-fd
 metadata        metadata    component  state (offset,
 (FS type,       (size,      (name →    mode, flags)
 block device)   perms,      inode)
                 times)
```

### I/O path for `read(fd, buf, len)`

```
syscall read()
  → vfs_read()                     fs/read_write.c
    → file->f_op->read_iter()      concrete FS (e.g., ext4_file_read_iter)
      → generic_file_read_iter()   fs/generic_file.c
        → page_cache_sync_readahead()  read pages into page cache
          → ext4_readpages()       mm/readahead.c + fs/ext4/
            → submit_bio()         block layer
              → NVMe/SCSI driver   drivers/nvme/ or drivers/scsi/
                → hardware
```

### Page cache

File data is cached in the **page cache** (also called "buffer cache" in older
kernels). When a process reads a file block:

1. The kernel checks the page cache (indexed by `inode + page_offset`).
2. **Cache hit:** data is copied directly from the cached page.
3. **Cache miss:** the kernel issues a block I/O request, waits, then caches
   and returns the data.

Write-back: modified ("dirty") pages are written back to disk asynchronously
by `pdflush` / `writeback` kernel threads.

---

## 4. System Calls

**Where:** `arch/*/kernel/entry*.S`, `kernel/sys.c`, `fs/`, `mm/`, `net/`

A system call is the controlled way a user-space program asks the kernel to
do something privileged (open a file, allocate memory, create a process, …).

### System call flow (x86-64)

```
User space                          Kernel space
──────────                          ─────────────
libc wrapper                        entry_SYSCALL_64 (arch/x86/kernel/entry_64.S)
  SYSCALL instruction   ──────────▶  Save user registers to struct pt_regs on stack
                                     Look up syscall number in sys_call_table
                                     Call sys_xxx() handler
                                     Restore registers
                        ◀──────────  SYSRET back to user space
return value in rax
```

### How a syscall is defined

```c
/* Example: sys_getpid() */
SYSCALL_DEFINE0(getpid)             /* kernel/sys.c */
{
    return task_tgid_vnr(current);
}
```

The `SYSCALL_DEFINE` macro generates a function named `__x64_sys_getpid` and
registers it in the syscall table at the correct index.

### Syscall numbers

```
x86-64: arch/x86/entry/syscalls/syscall_64.tbl
ARM:    arch/arm/tools/syscall.tbl
ARM64:  include/uapi/asm-generic/unistd.h
```

---

## 5. Interrupts & Exception Handling

**Where:** `arch/*/kernel/entry*.S`, `kernel/irq/`

Hardware events (keyboard press, network packet arrival, timer tick) signal
the CPU via **interrupts**. Software errors (page fault, divide-by-zero) raise
**exceptions** (also called faults or traps).

### Interrupt handling flow

```
Hardware raises IRQ
  → CPU jumps to interrupt vector (IDT on x86 / exception vector on ARM)
    → Save registers (push to kernel stack)
      → do_IRQ() / handle_irq()       kernel/irq/handle.c
        → handle_irq_event()
          → action->handler()          the registered IRQ handler
            (e.g., NIC driver, timer)
        → irq_exit()                   check for softirq/tasklet work
          → __do_softirq()             run pending softirqs
    → Restore registers
  → Return from interrupt (iret / eret)
```

### Deferred processing: softirqs and tasklets

Some IRQ work is too slow for the interrupt context (e.g., processing a full
TCP segment). The kernel defers it to a **softirq** or **tasklet**:

- **Softirq** (static, run on each CPU): `NET_RX_SOFTIRQ`, `NET_TX_SOFTIRQ`,
  `BLOCK_SOFTIRQ`, `TIMER_SOFTIRQ`, …
- **Tasklet** (dynamic, per-CPU serialised): simpler softirq wrapper.
- **Workqueue** (`kernel/workqueue.c`): runs in a kernel thread context;
  can sleep. Best for I/O-heavy deferred work.

---

## 6. Locking and Synchronisation

**Where:** `kernel/locking/`, `include/linux/`

The kernel runs on many CPUs simultaneously. Any shared data structure must
be protected.

### Locking primitives

| Primitive | Use case | Can sleep? |
|-----------|----------|-----------|
| `spinlock_t` | Short critical sections; IRQ context | No |
| `mutex` | Longer critical sections; process context | Yes |
| `rwlock_t` | Multiple readers, one writer; no sleep | No |
| `rw_semaphore` | Readers/writers with sleep | Yes |
| `seqlock_t` | Fast reads, infrequent writes (e.g., jiffies) | No |
| `RCU` | Read-mostly data (routing tables, inodes) | Read: No |

### RCU (Read-Copy-Update)

RCU allows multiple readers to access a data structure **without any locking**
while a writer atomically replaces a pointer to the data. Readers run in a
**read-side critical section** (`rcu_read_lock()` / `rcu_read_unlock()`).
The writer waits for all current readers to finish before freeing the old
data (`synchronize_rcu()`).

Used heavily in: routing tables (`net/ipv4/route.c`), task list traversal,
file system dcache.

---

## 7. Timers and Timekeeping

**Where:** `kernel/time/`, `arch/*/kernel/time*`

| Concept | Description |
|---------|-------------|
| `jiffies` | Kernel tick counter (CONFIG_HZ ticks/s, typically 250 or 1000) |
| `hrtimer` | High-resolution timer (nanosecond precision) |
| `clocksource` | Hardware clock source (TSC, HPET, ARM generic timer) |
| `clockevent` | Per-CPU timer event device (drives scheduling ticks) |
| `CLOCK_REALTIME` | Wall clock (can jump via NTP/settimeofday) |
| `CLOCK_MONOTONIC` | Monotonically increasing clock (boot time base) |

The scheduler relies on `hrtimer` for its preemption timer (the "tick" that
triggers `scheduler_tick()` → possible `schedule()` call).

---

## 8. Networking

**Where:** `net/`

The network stack is layered:

```
Application (socket API)
  ↕ AF_INET/AF_INET6 socket
  ↕ TCP / UDP / RAW
  ↕ IP (routing, fragmentation)
  ↕ Netfilter hooks (iptables/nftables)
  ↕ Network device (struct net_device)
  ↕ NIC driver (drivers/net/)
  ↕ Hardware
```

### Key data structures

- `struct socket` — the user-facing socket
- `struct sock` — the kernel-internal socket state (per-socket buffers, state machine)
- `struct sk_buff` — a network packet (buffer + metadata)
- `struct net_device` — a network interface (eth0, wlan0, …)

### TCP state machine

The kernel implements the full TCP state machine in `net/ipv4/tcp_input.c`
and `net/ipv4/tcp_output.c`:

```
LISTEN → SYN_RECV → ESTABLISHED → FIN_WAIT1 → FIN_WAIT2 → TIME_WAIT → CLOSED
```

---

## 9. Security: LSM Framework

**Where:** `security/`

The Linux Security Module (LSM) framework inserts **hooks** at security-
sensitive points. An LSM can allow or deny the operation.

```c
/* Example: LSM hook on file open (security/security.c) */
int security_file_open(struct file *file)
{
    return call_int_hook(file_open, 0, file);
    /* → selinux_file_open() if SELinux is active */
    /* → apparmor_file_open() if AppArmor is active */
}
```

LSMs available: SELinux, AppArmor, Smack, TOMOYO, Yama, Lockdown.

---

## 10. Power Management

**Where:** `drivers/power/`, `kernel/power/`

```
Runtime PM      — suspend individual devices when idle (device_runtime_suspend)
System suspend  — suspend the whole system (S3 sleep / S2idle)
Hibernate       — save RAM to disk and power off (S4)
CPU frequency   — scale CPU frequency based on load (CPUfreq: drivers/cpufreq/)
CPU idle        — idle states (C-states) when CPU has no work (CPUidle)
```

---

## 11. Namespaces and Containers

**Where:** `kernel/nsproxy.c`, individual namespace files

Linux containers (Docker, LXC, Kubernetes pods) are built on **namespaces**:

| Namespace | Isolates | File |
|-----------|----------|------|
| PID | Process IDs | `kernel/pid_namespace.c` |
| NET | Network stack, interfaces, routing | `net/core/net_namespace.c` |
| MNT | Mount points (file system view) | `fs/namespace.c` |
| UTS | Hostname, domain name | `kernel/utsname.c` |
| IPC | SysV IPC, POSIX message queues | `ipc/namespace.c` |
| USER | User and group IDs | `kernel/user_namespace.c` |
| CGROUP | Cgroup root | `kernel/cgroup/namespace.c` |
| TIME | Clock offsets per container | `kernel/time_namespace.c` |

Combined with **cgroups** (resource limits: CPU, memory, I/O) and
**seccomp** (syscall filtering), namespaces provide full container isolation.

---

## Subsystem Interaction Diagram

```
fork() system call
  │
  ├─ copy_process()          [kernel/fork.c]
  │    ├─ dup_task_struct()  alloc new task_struct + kernel stack
  │    ├─ copy_mm()          copy/share mm_struct  [mm/]
  │    ├─ copy_files()       copy/share fdtable    [fs/]
  │    ├─ copy_sighand()     copy signal handlers  [kernel/signal.c]
  │    └─ copy_namespaces()  copy namespace refs   [kernel/nsproxy.c]
  │
  └─ wake_up_new_task()      [kernel/sched/core.c]
       └─ enqueue_task()     put on CFS run queue  [kernel/sched/fair.c]
```

```
Page fault  (process accesses unmapped address)
  │
  └─ do_page_fault()         [arch/x86/mm/fault.c]
       └─ handle_mm_fault()  [mm/memory.c]
            ├─ handle_pte_fault()
            │    ├─ do_anonymous_page()   alloc new page (anonymous mmap)
            │    ├─ do_read_fault()       read from file-backed VMA
            │    └─ do_cow_fault()        copy-on-write (fork shared page)
            └─ (return to user space with page now mapped)
```

---

## Further Reading

| Resource | URL |
|----------|-----|
| Kernel documentation | https://www.kernel.org/doc/html/latest/ |
| Elixir cross-reference | https://elixir.bootlin.com/linux/latest/source |
| The Linux Kernel (book) | https://www.tldp.org/LDP/tlk/tlk.html |
| Linux Kernel Development (Bovet & Cesati) | ISBN 978-0596005658 |
| Linux Device Drivers (Rubini et al.) | https://lwn.net/Kernel/LDD3/ |
| LWN.net kernel articles | https://lwn.net/ |

---

## Module Cross-Reference

| Kernel concept | This repo module |
|---------------|-----------------|
| CPU fetch-decode-execute | [01 — Single-Threaded CPU](../01_SingleThreaded_CPU/README.md) |
| Context switch, task_struct | Module 02 (planned) |
| Fork, exec, wait | Module 03 (planned) |
| CFS scheduler | Module 04 (planned) |
| Virtual memory, page tables | Module 05 (planned) |
| kmalloc, buddy, slab | Module 06 (planned) |
| System calls | Module 07 (planned) |
| VFS, inode, dentry | Module 08 (planned) |
| Pipes, signals, IPC | Module 09 (planned) |
| IRQ, device drivers | Module 10 (planned) |
