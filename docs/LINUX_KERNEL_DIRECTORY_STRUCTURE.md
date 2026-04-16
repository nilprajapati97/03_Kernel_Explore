# Linux Kernel Directory Structure — Deep Explanation

> **Reference kernel version:** Linux 6.x (stable)  
> **Online browser:** https://elixir.bootlin.com/linux/latest/source  
> **GitHub mirror:** https://github.com/torvalds/linux

This document walks through every top-level directory in the Linux kernel
source tree, explains what it contains, why it exists, and which files are
most important for understanding core kernel behaviour.

---

## Top-Level Tree at a Glance

```
linux/
├── arch/           Architecture-specific code (x86, ARM, RISC-V, …)
├── block/          Block device I/O layer (request queues, elevators)
├── certs/          Kernel module signing certificates
├── crypto/         Cryptographic algorithm library
├── Documentation/  Kernel documentation (RST, plain text)
├── drivers/        Device drivers (the largest directory)
├── fs/             File system implementations (ext4, btrfs, vfs core, …)
├── include/        Architecture-independent header files
├── init/           Kernel initialisation entry point
├── io_uring/       io_uring async I/O subsystem
├── ipc/            Inter-process communication (SysV SHM, semaphores, …)
├── kernel/         Core kernel: scheduler, signals, timers, tracing
├── lib/            Library routines (radix tree, rbtree, string, …)
├── mm/             Memory management (buddy, slab, paging, OOM)
├── net/            Networking (TCP/IP stack, sockets, netfilter, …)
├── rust/           Rust-language kernel modules infrastructure
├── samples/        Sample kernel code (BPF, kprobes, …)
├── scripts/        Build scripts, kernel config helpers, checkpatch.pl
├── security/       Security frameworks (SELinux, AppArmor, LSM hooks)
├── sound/          Audio subsystem (ALSA, ASoC)
├── tools/          User-space tools (perf, bpf, testing harness)
├── usr/            Initramfs/initrd generation support
├── virt/           Virtualization (KVM paravirt)
├── Kconfig         Top-level kernel configuration
├── Kbuild          Top-level kernel build rules
├── Makefile        Top-level build entry point
└── MAINTAINERS     Per-subsystem maintainer list (thousands of entries)
```

---

## `arch/` — Architecture-Specific Code

**Size:** ~40% of the entire kernel source tree  
**Purpose:** Everything that must be implemented differently for each
processor architecture.

```
arch/
├── arm/            32-bit ARM (Cortex-A, older SoCs)
├── arm64/          64-bit ARM (AArch64, Cortex-A53/A72/A76, Apple M1/M2)
├── x86/            x86 and x86-64 (most desktops/servers)
├── riscv/          RISC-V (open ISA)
├── mips/           MIPS (embedded, some routers)
├── powerpc/        PowerPC / POWER (IBM servers, old Macs)
├── s390/           IBM mainframe (Z series)
├── sparc/          SPARC (Sun/Oracle legacy)
└── ... (20+ architectures total)
```

### Inside each architecture directory

Every architecture follows a common layout:

```
arch/arm/
├── boot/           Compressed kernel image, device tree blobs (zImage, DTBs)
│   └── dts/        Device Tree Source files (.dts / .dtsi)
├── configs/        Default Kconfig files for known boards
├── include/asm/    Architecture-specific header files (processor.h, ptrace.h)
├── kernel/         Core arch code: entry points, SMP, CPU init
│   ├── entry-armv.S    Exception vector table, syscall entry/exit
│   ├── process.c       copy_thread(), start_thread()
│   ├── setup.c         setup_arch(): parse DTB, init memory map
│   ├── signal.c        Signal delivery, sigreturn
│   └── smp.c           SMP boot, CPU hotplug
├── lib/            Optimised string/memcpy routines in assembly
├── mm/             Page table setup, fault handling, TLB management
│   ├── fault.c         do_page_fault()
│   ├── mmu.c           Page table setup, TTBR management
│   └── pgalloc.h       Page table allocation helpers
└── mach-*/         Board / SoC-specific code (legacy; modern uses DTB)
```

### Key files to know

| File | What it does |
|------|-------------|
| `arch/x86/kernel/entry_64.S` | x86-64 syscall entry (`entry_SYSCALL_64`), interrupt entry, register save/restore via `SAVE_REGS` macros |
| `arch/arm/kernel/entry-armv.S` | ARM exception vector table, `SVC` (syscall) entry, `IRQ` entry, context save macros |
| `arch/x86/include/asm/ptrace.h` | `struct pt_regs` — the layout of saved registers on the kernel stack |
| `arch/arm/include/asm/processor.h` | `struct thread_struct` — per-task CPU state for context switch |
| `arch/x86/kernel/cpu/common.c` | `cpu_init()` — per-CPU TSS, LDT, GDT setup |
| `arch/arm64/mm/fault.c` | `do_mem_abort()` — MMU fault handler (maps to Linux `do_page_fault`) |

---

## `kernel/` — Core Kernel Subsystems

**Purpose:** The architecture-independent "heart" of the kernel: the scheduler,
signal delivery, timers, workqueues, locking, tracing, RCU, namespaces, and more.

```
kernel/
├── sched/          Process scheduler
│   ├── core.c          schedule(), try_to_wake_up(), context_switch()
│   ├── fair.c          Completely Fair Scheduler (CFS) — the default class
│   ├── rt.c            Real-time scheduler (SCHED_FIFO, SCHED_RR)
│   ├── deadline.c      EDF/CBS deadline scheduling (SCHED_DEADLINE)
│   ├── idle.c          Idle loop (cpu_startup_entry, do_idle)
│   ├── wait.c          Wait queues (wait_event, wake_up)
│   └── topology.c      CPU topology, NUMA-awareness
├── fork.c          do_fork() / copy_process() — process/thread creation
├── exit.c          do_exit(), do_wait() — process termination
├── signal.c        Signal delivery, sigaction, kill()
├── sys.c           Generic syscall implementations (uname, sysinfo, …)
├── time/           Timekeeping, hrtimers, clocksource
│   ├── hrtimer.c       High-resolution timers
│   ├── timekeeping.c   Wall clock (CLOCK_REALTIME), monotonic clock
│   └── clockevents.c   Per-CPU tick device abstraction
├── irq/            IRQ descriptor management, softirqs, tasklets
│   ├── irqdesc.c       struct irq_desc allocation and lookup
│   ├── chip.c          irqchip operations (enable/disable/ack)
│   └── manage.c        request_irq(), free_irq()
├── locking/        Spinlocks, mutexes, RW-semaphores, lockdep
│   ├── spinlock.c
│   ├── mutex.c
│   └── rwsem.c
├── rcu/            Read-Copy-Update (lock-free read concurrency)
│   ├── tree.c          Hierarchical RCU (the default)
│   └── tree_plugin.h   SRCU, TASKS RCU
├── workqueue.c     Deferred/async work (schedule_work, CMWQ)
├── kthread.c       Kernel threads (kthread_create, kthread_run)
├── printk/         printk(), log buffer, console drivers
├── trace/          ftrace, kprobes, tracepoints, perf events
├── bpf/            BPF verifier, interpreter, JIT, syscall
└── pid.c           PID allocation (idr-based), pidfd
```

### Scheduler in depth (`kernel/sched/`)

The scheduler is the most complex subsystem in this directory.

```
Scheduling classes (highest → lowest priority):
  stop_sched_class      — stop-machine tasks (migration, hotplug)
  dl_sched_class        — SCHED_DEADLINE (earliest-deadline-first)
  rt_sched_class        — SCHED_FIFO / SCHED_RR (real-time)
  fair_sched_class      — SCHED_NORMAL / SCHED_BATCH (CFS — default)
  idle_sched_class      — SCHED_IDLE (background, lowest priority)
```

**Completely Fair Scheduler (CFS)** — `kernel/sched/fair.c`:

- Every runnable task has a **virtual runtime** (`vruntime`) measured in
  nanoseconds.
- The task with the smallest `vruntime` runs next (red-black tree ordered by
  `vruntime`).
- Each tick, the running task's `vruntime` increases proportionally to its
  nice value (weight).
- When a task's `vruntime` exceeds the minimum by more than the scheduling
  latency target, it is preempted.

Key data structures:
- `struct task_struct` ([kernel: include/linux/sched.h]) — the process
  descriptor (PID, state, mm, files, signals, creds, …)
- `struct sched_entity` — embedded in `task_struct`; holds `vruntime`
- `struct cfs_rq` — the per-CPU run queue (a red-black tree)
- `struct rq` — the per-CPU run queue container (holds cfs_rq, rt_rq, dl_rq)

---

## `mm/` — Memory Management

**Purpose:** Everything related to how the kernel manages physical and
virtual memory.

```
mm/
├── memory.c        Core page fault handler, page table walking/installation
├── mmap.c          mmap(), munmap(), mremap() — virtual address space management
├── fault.c         handle_mm_fault() — the page fault entry point
├── page_alloc.c    Buddy allocator — physical page allocation (alloc_pages)
├── slub.c          SLUB allocator — small object allocator (kmalloc, kmem_cache)
├── slab.c          SLAB allocator (older; SLUB is now the default)
├── vmalloc.c       vmalloc() — virtually contiguous but physically scattered
├── swap.c          Swap space management, page eviction
├── vmscan.c        Page reclaim (kswapd, direct reclaim), LRU lists
├── oom_kill.c      Out-of-memory killer: select and kill a process
├── hugetlb.c       Huge pages (2 MiB / 1 GiB pages on x86-64)
├── mprotect.c      mprotect() — change page permissions
├── mremap.c        mremap() — remap pages in virtual address space
├── highmem.c       Mapping high memory on 32-bit kernels (legacy)
├── memcontrol.c    Memory cgroups (memory.limit, memory.stat)
├── percpu.c        Per-CPU memory allocation (get_cpu_var, this_cpu_ptr)
└── util.c          Utility functions (page colouring, etc.)
```

### Physical memory: Buddy Allocator (`page_alloc.c`)

The kernel tracks free physical pages in **free lists of power-of-2 sizes**
(order 0 = 1 page = 4 KiB, order 1 = 2 pages, …, order 11 = 2048 pages = 8 MiB).

```
alloc_pages(gfp_flags, order)
  → __alloc_pages()
    → get_page_from_freelist()     try NORMAL zone first
    → __alloc_pages_slowpath()     reclaim / compaction / OOM if needed
```

When a block of order N is needed but only order N+1 is free, the buddy
allocator **splits** the larger block, giving one half to the caller and
placing the other half ("buddy") back onto the order-N free list.
Freeing reverses this: if the freed page's buddy is also free, they are
**merged** back into an order-N+1 block.

### Virtual memory: Page Tables

On x86-64 Linux uses a **5-level page table** (since kernel 4.12+):

```
Virtual address (48 or 57 bits):
  PGD → P4D → PUD → PMD → PTE → physical page offset
  (9)   (9)   (9)   (9)   (9)       (12 bits)
```

Key files:
- `arch/x86/mm/pgtable.c` — page table allocation helpers
- `mm/memory.c` — `handle_pte_fault()`, `do_anonymous_page()`, COW logic
- `arch/x86/mm/fault.c` — `do_page_fault()` → `handle_mm_fault()`

### Object allocator: SLUB (`slub.c`)

`kmalloc(size, GFP_KERNEL)` → `__kmalloc()` → SLUB.

SLUB organises objects into **slabs** (groups of physically contiguous pages).
Each slab cache (`struct kmem_cache`) holds objects of one fixed size.
Objects are allocated from a per-CPU **partial slab** (a slab with some free
slots); when a slab is full it is moved to the full list; when empty it is
returned to the buddy allocator.

---

## `fs/` — File System Implementations

**Purpose:** The VFS (Virtual File System) abstraction layer plus concrete
file system implementations.

```
fs/
├── namei.c         Path resolution (path_lookup, filename_lookup, link traversal)
├── open.c          open(), creat(), close() system calls
├── read_write.c    read(), write(), pread(), pwrite()
├── stat.c          stat(), fstat(), lstat(), statx()
├── inode.c         Inode lifecycle (iget, iput, evict_inode)
├── dcache.c        Dentry cache (path component → inode cache)
├── super.c         Superblock management (mount/umount)
├── file.c          struct file, file descriptor table
├── pipe.c          Anonymous pipes (pipe(), pipe2())
├── eventpoll.c     epoll (epoll_create, epoll_ctl, epoll_wait)
├── select.c        select() and poll()
├── aio.c           POSIX AIO
├── io_uring/       io_uring (modern async I/O)
├── proc/           /proc virtual filesystem
├── sysfs/          /sys virtual filesystem (kernel object model)
├── debugfs/        /sys/kernel/debug (driver debug exports)
├── tmpfs/          In-memory file system (tmpfs/shmem)
├── ext2/           ext2 file system
├── ext4/           ext4 file system (journalling, extents)
├── btrfs/          Btrfs (B-tree based, snapshots, checksums)
├── xfs/            XFS (high-performance journalling FS)
├── fat/            FAT12/16/32 (USB drives, boot partitions)
├── nfs/            NFS client
└── overlayfs/      OverlayFS (used by Docker/container layers)
```

### VFS Layer: The Four Core Objects

The VFS defines four abstract objects that all concrete file systems implement:

```
struct super_block   — represents a mounted file system
struct inode         — represents a file (metadata: size, owner, permissions)
struct dentry        — represents a path component (directory entry)
struct file          — represents an open file (per-process handle)
```

Each has an associated **operations table** (function pointer struct):

```c
struct inode_operations {
    int (*create)(struct mnt_idmap *, struct inode *, struct dentry *, ...);
    struct dentry *(*lookup)(struct inode *, struct dentry *, ...);
    int (*link)(struct dentry *, struct inode *, struct dentry *);
    int (*unlink)(struct inode *, struct dentry *);
    int (*mkdir)(struct mnt_idmap *, struct inode *, struct dentry *, umode_t);
    ...
};

struct file_operations {
    ssize_t (*read)(struct file *, char __user *, size_t, loff_t *);
    ssize_t (*write)(struct file *, const char __user *, size_t, loff_t *);
    int (*mmap)(struct file *, struct vm_area_struct *);
    long (*unlocked_ioctl)(struct file *, unsigned int, unsigned long);
    ...
};
```

When you call `read()` on a file, the kernel:
1. Looks up the `struct file` in the process's file descriptor table.
2. Calls `file->f_op->read()` — which points to the concrete FS implementation.
3. The concrete FS reads blocks from the block device via the page cache.

---

## `drivers/` — Device Drivers

**Purpose:** Drivers for every class of hardware the kernel supports.
This is the *largest* directory (~50% of the kernel's line count).

```
drivers/
├── base/           Driver model core (kobject, device, bus, class)
├── block/          Block device layer (virtio-blk, loop, nbd)
├── char/           Character devices (tty, random, nvram)
├── clk/            Clock framework (clock gating, frequency scaling)
├── dma/            DMA engine API
├── gpio/           GPIO subsystem
├── gpu/            GPU drivers (DRM/KMS)
│   └── drm/        Direct Rendering Manager (display + 3D)
├── i2c/            I2C bus master + device drivers
├── input/          Input devices (keyboard, mouse, touchscreen)
├── iommu/          IOMMU (memory protection for DMA)
├── irqchip/        Interrupt controller drivers (GIC, APIC)
├── md/             Software RAID (md, dm)
├── mmc/            SD/eMMC host controllers
├── net/            Network interface card drivers
│   ├── ethernet/   Ethernet NICs (Intel e1000e, Broadcom bnxt, …)
│   └── wireless/   Wi-Fi drivers (ath9k, iwlwifi, mac80211)
├── nvme/           NVMe (PCIe SSD) driver
├── pci/            PCI bus enumeration, MSI/MSI-X
├── phy/            USB/MIPI/PCIe PHY drivers
├── pinctrl/        Pin multiplexing and configuration
├── platform/       Platform device drivers (SoC peripherals)
├── power/          Power management (runtime PM, suspend/resume)
├── regulator/      Voltage/current regulators
├── scsi/           SCSI/SATA/SAS host adapters and disk drivers
├── spi/            SPI bus controller + device drivers
├── thermal/        Thermal management, cooling devices
├── tty/            Serial/terminal subsystem (UART, pty, vt)
│   └── serial/     UART drivers (8250, PL011, …)
├── usb/            USB host controllers + gadget + device classes
│   ├── host/       EHCI, OHCI, XHCI drivers
│   └── gadget/     USB device (gadget) framework
├── virt/           Paravirtual drivers (virtio-net, virtio-blk)
│   └── virtio/     VirtIO transport layer
└── watchdog/       Hardware watchdog drivers
```

### Driver Model: `drivers/base/`

Every device in the kernel is represented by a `struct device`
([kernel: include/linux/device.h]). Devices are organised into a hierarchy:

```
bus → device → driver
```

- `bus_type` (e.g., PCI, USB, I2C) manages device enumeration and
  driver binding.
- `device_driver` holds the `probe()` function called when a matching device
  is found.
- `kobject` ([kernel: include/linux/kobject.h]) is the base object; it
  provides reference counting and sysfs integration.

---

## `include/` — Header Files

```
include/
├── linux/          Core kernel API headers (used everywhere)
│   ├── sched.h         struct task_struct (the process descriptor)
│   ├── mm_types.h      struct mm_struct, vm_area_struct, page
│   ├── fs.h            struct file, inode, super_block, dentry
│   ├── device.h        struct device, device_driver, bus_type
│   ├── interrupt.h     request_irq(), free_irq(), IRQ flags
│   ├── spinlock.h      spinlock_t, spin_lock(), spin_unlock()
│   ├── mutex.h         struct mutex, mutex_lock(), mutex_unlock()
│   ├── list.h          Doubly-linked list (list_head, list_for_each)
│   ├── rbtree.h        Red-black tree (rb_root, rb_node, rb_insert_color)
│   ├── radix-tree.h    Radix tree (used by page cache)
│   ├── kref.h          Reference counting (kref_init, kref_get, kref_put)
│   ├── gfp.h           GFP (Get Free Pages) flags for memory allocation
│   ├── slab.h          kmalloc, kfree, kmem_cache
│   ├── workqueue.h     Workqueue API (INIT_WORK, schedule_work)
│   ├── kthread.h       Kernel thread API (kthread_create, kthread_run)
│   ├── uaccess.h       User-space memory access (copy_to_user, get_user)
│   ├── syscalls.h      SYSCALL_DEFINE macros
│   ├── net/            Network layer headers (sk_buff, socket, …)
│   └── ... (thousands of headers)
├── uapi/           User-space API headers (exported via usr/include)
│   └── linux/          Headers visible to user-space programs
│       ├── syscall.h
│       ├── ioctl.h
│       └── ...
└── asm-generic/    Architecture-independent asm header templates
```

### The Most Important Headers

**`include/linux/sched.h`** — `struct task_struct` is the single most
important data structure in the kernel. At ~800 fields, it describes everything
about a process: PID, state, scheduling class, memory descriptor, open files,
signal handlers, namespaces, credentials, and much more.

**`include/linux/mm_types.h`** — defines `struct page` (one per physical
page frame), `struct mm_struct` (one per process address space), and
`struct vm_area_struct` (one per virtual memory region / VMA).

**`include/linux/list.h`** — the kernel's intrusive doubly-linked list, used
throughout the kernel. Every `struct list_head` embedded in a larger struct
allows that struct to be linked into lists without extra heap allocation.

---

## `init/` — Kernel Initialisation

**Purpose:** The entry point for kernel startup after the boot loader hands
control to the decompressed kernel.

```
init/
├── main.c          start_kernel() — the C entry point
├── initramfs.c     Populate the initial RAM filesystem
└── Kconfig         Init-related configuration options
```

### `start_kernel()` call graph

```c
start_kernel()                          /* init/main.c */
  ├── setup_arch()                      /* arch/x86/kernel/setup.c */
  │     Parse command line, init memory map, set up paging
  ├── trap_init()                       /* arch/x86/kernel/traps.c */
  │     Set up IDT (interrupt descriptor table)
  ├── mm_init()                         /* init/main.c */
  │     mem_init() → free_all_bootmem()
  │     kmem_cache_init() → SLUB initialisation
  ├── sched_init()                      /* kernel/sched/core.c */
  │     Initialise per-CPU run queues, set up idle task
  ├── time_init()                       /* arch/x86/kernel/time.c */
  │     Calibrate TSC, set up timer interrupt
  ├── rcu_init()                        /* kernel/rcu/tree.c */
  ├── vfs_caches_init()                 /* fs/dcache.c */
  │     dcache, inode cache, filp cache
  ├── signals_init()                    /* kernel/signal.c */
  ├── rest_init()                       /* init/main.c */
  │     ├── kernel_thread(kernel_init)  /* PID 1 */
  │     └── cpu_startup_entry()         /* CPU0 idle loop */
  └── (never returns)

kernel_init() [PID 1]
  ├── kernel_init_freeable()
  │     do_initcalls()    → run all __initcall functions
  │     prepare_namespace() → mount rootfs
  └── run_init_process("/sbin/init")  → exec user-space init
```

---

## `ipc/` — Inter-Process Communication

```
ipc/
├── msg.c           SysV message queues (msgget, msgsnd, msgrcv)
├── sem.c           SysV semaphores (semget, semop, semctl)
├── shm.c           SysV shared memory (shmget, shmat, shmdt)
├── mqueue.c        POSIX message queues (mq_open, mq_send, mq_receive)
└── namespace.c     IPC namespaces (isolation for containers)
```

The kernel also supports:
- **Pipes** (`fs/pipe.c`) — anonymous byte streams between related processes
- **FIFOs** — named pipes in the file system
- **Signals** (`kernel/signal.c`) — asynchronous notifications
- **Sockets** (`net/socket.c`) — general-purpose communication (including
  Unix domain sockets for local IPC)
- **Futexes** (`kernel/futex/`) — fast user-space mutexes (underpins
  `pthread_mutex_lock`)

---

## `net/` — Networking

```
net/
├── socket.c        sock_create(), bind(), connect(), sendmsg(), recvmsg()
├── core/           Core socket layer (sk_buff management, protocol demux)
├── ipv4/           IPv4: IP input/output, TCP, UDP, ICMP, ARP
│   ├── tcp_input.c     TCP receive path (ack, data, window)
│   ├── tcp_output.c    TCP send path (segmentation, retransmit)
│   ├── ip_input.c      IP receive: ip_rcv() → ip_local_deliver()
│   └── udp.c           UDP protocol
├── ipv6/           IPv6 stack
├── netfilter/      iptables / nftables hooks (PREROUTING, INPUT, FORWARD, …)
├── bridge/         Ethernet bridging (Linux software switch)
├── wireless/       mac80211 (Wi-Fi stack above the driver)
├── unix/           Unix domain sockets (AF_UNIX)
├── packet/         AF_PACKET raw sockets (tcpdump, Wireshark)
└── bpf/            BPF socket filters, XDP
```

### sk_buff — The Network Buffer

Every network packet is represented by `struct sk_buff`
([kernel: include/linux/skbuff.h]). It holds:

- **Data pointers:** `head`, `data`, `tail`, `end` — allow headers to be
  prepended/appended without copying data.
- **Protocol metadata:** incoming interface, protocol type, priority, checksum.
- **Layer state:** IP header offset, transport header offset, etc.

The sk_buff is passed up and down the network stack, with each layer
adjusting the data pointers to add or remove headers.

---

## `security/` — Security Frameworks

```
security/
├── security.c      LSM (Linux Security Module) hook dispatch
├── selinux/        SELinux — mandatory access control (policy, contexts)
├── apparmor/       AppArmor — profile-based MAC (used by Ubuntu, Snap)
├── smack/          Simplified Mandatory Access Control Kernel
├── tomoyo/         TOMOYO Linux — pathname-based MAC
├── integrity/      IMA/EVM — file integrity measurement
└── keys/           Kernel keyring (symmetric/asymmetric keys, TLS)
```

The LSM framework inserts security hooks at hundreds of points in the kernel
(open, exec, network connect, etc.). When a security-sensitive operation is
performed, the kernel calls the active LSM's hook to check whether the
operation is permitted, based on the security policy.

---

## `block/` — Block Device Layer

```
block/
├── blk-core.c      Block I/O core: bio submission, request queues
├── blk-mq.c        Multi-queue block layer (modern, NUMA-aware)
├── blk-sysfs.c     /sys/block/ entries
├── elevator.c      I/O scheduler framework (mq-deadline, kyber, BFQ)
├── bio.c           struct bio — the unit of block I/O
└── partitions/     Partition table parsers (MBR, GPT, …)
```

The block layer sits between file systems and disk drivers:

```
fs/ext4  →  buffer_head / bio  →  block layer  →  NVMe/SCSI driver  →  hardware
```

`struct bio` ([kernel: include/linux/bio.h]) represents one I/O operation
(read or write) as a list of page/offset/length tuples. The block layer may
merge multiple bios into a single request for efficiency.

---

## `crypto/` — Cryptographic Library

```
crypto/
├── api.c           Crypto API: algorithm registration and lookup
├── aead.c          Authenticated Encryption (AES-GCM, ChaCha20-Poly1305)
├── hash.c          Hash operations (SHA-256, SHA-3, BLAKE2)
├── skcipher.c      Symmetric ciphers (AES-CBC, AES-CTR)
├── rng.c           Random number generation (DRBG)
└── ... hardware acceleration drivers in drivers/crypto/
```

Used by: IPsec, TLS (in-kernel), dm-crypt, ecryptfs, module signing, keys.

---

## `Documentation/` — Kernel Documentation

```
Documentation/
├── admin-guide/    For system administrators (sysctl, cgroups, hugepages)
├── core-api/       Core kernel API reference (memory, locking, workqueues)
├── driver-api/     Driver writing guides (DMA, regulator, clk, GPIO)
├── filesystems/    Per-filesystem documentation
├── networking/     Networking subsystem docs
├── scheduler/      Scheduler design docs (CFS design, NUMA, RT)
├── memory-barriers.txt   Memory ordering guarantees (CRITICAL reading)
├── locking/        Locking primer, lockdep, RCU explained
└── process/        How to contribute patches, coding style
```

**Start here:** `Documentation/process/howto.rst` and
`Documentation/core-api/kernel-api.rst`.

---

## `scripts/` — Build Infrastructure

```
scripts/
├── Makefile.*      Recursive Make rules for building kernel objects
├── Kconfig         Kconfig language parser
├── checkpatch.pl   Coding style checker (run before submitting patches)
├── get_maintainer.pl  Find the right maintainer from MAINTAINERS
├── kernel-doc      Kernel-doc comment extractor (generates HTML/PDF docs)
├── recordmcount.*  ftrace function tracer instrumentation helper
└── mod/            Module signing and symbol versioning tools
```

---

## `tools/` — User-Space Kernel Tools

```
tools/
├── perf/           Performance analysis tool (perf stat, record, report, trace)
├── bpf/            BPF program examples and helpers
├── testing/
│   ├── selftests/  Kernel self-tests (run with `make kselftest`)
│   └── kunit/      KUnit in-kernel unit testing framework
├── trace/          trace-cmd front-end for ftrace
└── memory-model/   Linux memory model litmus tests (LKMM)
```

---

## Summary Table

| Directory | Lines (approx.) | Key role |
|-----------|----------------|----------|
| `drivers/` | ~18 M | Hardware support |
| `arch/` | ~5 M | Architecture portability |
| `net/` | ~3 M | Network stack |
| `fs/` | ~2.5 M | File systems |
| `sound/` | ~1.5 M | Audio |
| `kernel/` | ~1 M | Scheduler, signals, IPC |
| `mm/` | ~600 K | Memory management |
| `include/` | ~500 K | Public kernel headers |
| `Documentation/` | ~400 K | Docs |
| `crypto/` | ~300 K | Cryptography |
| `block/` | ~200 K | Block I/O |
| `security/` | ~200 K | LSM / SELinux |
| `init/` | ~10 K | Boot entry point |

---

## Where to Go Next

- [Kernel Concepts Overview](KERNEL_CONCEPTS_OVERVIEW.md) — high-level map of
  how all subsystems interact.
- [Process Management Deep Dive](KERNEL_PROCESS_MANAGEMENT.md) — `task_struct`,
  `fork`, context switch, signals.
- [Memory Management Deep Dive](KERNEL_MEMORY_MANAGEMENT.md) — physical memory,
  virtual memory, page faults, allocators.
- Return to [Module 01](../01_SingleThreaded_CPU/README.md) to see how the
  CPU simulator maps to `arch/` and `kernel/sched/`.
