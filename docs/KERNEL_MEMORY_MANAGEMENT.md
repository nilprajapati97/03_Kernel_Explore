# Linux Kernel — Memory Management Deep Dive

> **Kernel source references:**  
> `include/linux/mm_types.h`, `mm/page_alloc.c`, `mm/slub.c`,
> `mm/memory.c`, `mm/mmap.c`, `mm/vmscan.c`, `mm/oom_kill.c`,
> `arch/x86/mm/fault.c`

---

## Table of Contents

1. [The Two Worlds: Physical vs Virtual Memory](#1-the-two-worlds-physical-vs-virtual-memory)
2. [Key Data Structures](#2-key-data-structures)
   - [struct page](#struct-page)
   - [struct mm_struct](#struct-mm_struct)
   - [struct vm_area_struct](#struct-vm_area_struct)
3. [Physical Memory Layout](#3-physical-memory-layout)
4. [Buddy Allocator](#4-buddy-allocator)
5. [SLUB Allocator (kmalloc)](#5-slub-allocator-kmalloc)
6. [Page Tables and Virtual Memory](#6-page-tables-and-virtual-memory)
7. [Page Faults](#7-page-faults)
   - [Anonymous Pages (heap/stack)](#anonymous-pages-heapstack)
   - [File-Backed Pages](#file-backed-pages)
   - [Copy-on-Write (COW)](#copy-on-write-cow)
8. [The Page Cache](#8-the-page-cache)
9. [Memory Zones (ZONE_DMA, ZONE_NORMAL, ZONE_HIGHMEM)](#9-memory-zones)
10. [Page Reclaim and Swap](#10-page-reclaim-and-swap)
11. [Out-of-Memory Killer](#11-out-of-memory-killer)
12. [vmalloc vs kmalloc vs get_free_pages](#12-vmalloc-vs-kmalloc-vs-get_free_pages)
13. [Memory-Mapped I/O](#13-memory-mapped-io)
14. [Huge Pages](#14-huge-pages)
15. [Memory Cgroups](#15-memory-cgroups)
16. [Mapping to Module 01](#16-mapping-to-module-01)
17. [Key Source Files Summary](#17-key-source-files-summary)

---

## 1. The Two Worlds: Physical vs Virtual Memory

```
┌──────────────────────────────────────────────────────────────────────┐
│             VIRTUAL ADDRESS SPACE (per process)                       │
│                                                                        │
│   0xFFFF...   kernel space  (shared by all processes)                 │
│   0x00007FFF  ─────────────────────────────────── user space top      │
│               stack (grows ↓)                                          │
│               mmap region (shared libs, anonymous, file maps)          │
│               heap  (grows ↑)                                          │
│               bss / data / text                                        │
│   0x00400000  ─────────────────────────────────── typical ELF base    │
│   0x00000000  NULL (unmapped)                                          │
└─────────────────────────────────┬────────────────────────────────────┘
                                   │ page table (per process)
                                   ▼
┌──────────────────────────────────────────────────────────────────────┐
│                  PHYSICAL ADDRESS SPACE (RAM)                          │
│                                                                        │
│   Managed by the buddy allocator as an array of struct page objects   │
│   Each 4 KiB page frame has one struct page in the mem_map[] array    │
└──────────────────────────────────────────────────────────────────────┘
```

The kernel translates between the two using **page tables**. Each process has
its own set of page tables (pointed to by `mm->pgd`). The hardware MMU
(Memory Management Unit) performs the translation on every memory access,
using the page table loaded into a special register (CR3 on x86, TTBR on ARM).

---

## 2. Key Data Structures

### struct page

`struct page` ([kernel: include/linux/mm_types.h]) represents one physical
page frame (4 KiB). There is one `struct page` per page in RAM, stored in the
`mem_map[]` array.

Selected fields:

```c
struct page {
    unsigned long   flags;          /* PG_locked, PG_dirty, PG_referenced, … */
    union {
        /* For page cache pages */
        struct {
            struct address_space *mapping;   /* inode's address_space */
            pgoff_t               index;     /* offset within mapping  */
        };
        /* For slab/slub pages */
        struct {
            struct kmem_cache *slab_cache;
            void              *freelist;     /* first free object */
        };
        /* For anonymous pages */
        struct {
            unsigned long private;  /* swap entry when page is swapped out */
        };
    };
    atomic_t        _refcount;      /* reference count */
    atomic_t        _mapcount;      /* number of PTEs pointing to this page */
    /* … */
};
```

### struct mm_struct

`struct mm_struct` ([kernel: include/linux/mm_types.h]) describes the complete
virtual address space of one process (or thread group).

Selected fields:

```c
struct mm_struct {
    struct vm_area_struct *mmap;           /* list of VMAs (sorted by address) */
    struct maple_tree      mm_mt;          /* VMA tree (replaces rb_tree in 6.1+) */
    pgd_t                 *pgd;            /* page table root (loaded into CR3/TTBR) */

    atomic_t               mm_users;       /* how many threads use this mm */
    atomic_t               mm_count;       /* reference count (mm_users + kthreads) */

    unsigned long          mmap_base;      /* where mmap() allocations start */
    unsigned long          task_size;      /* user space top */
    unsigned long          start_code;     /* start of .text segment */
    unsigned long          end_code;       /* end of .text segment */
    unsigned long          start_data;     /* start of .data segment */
    unsigned long          end_data;       /* end of .data segment */
    unsigned long          start_brk;      /* start of heap */
    unsigned long          brk;            /* current top of heap */
    unsigned long          start_stack;    /* stack base */

    spinlock_t             page_table_lock;
    /* … */
};
```

### struct vm_area_struct

`struct vm_area_struct` (VMA) represents one contiguous virtual memory region:

```c
struct vm_area_struct {
    unsigned long       vm_start;      /* virtual address start (inclusive) */
    unsigned long       vm_end;        /* virtual address end (exclusive) */
    struct mm_struct   *vm_mm;         /* owning mm_struct */
    pgprot_t            vm_page_prot;  /* page protection flags */
    unsigned long       vm_flags;      /* VM_READ, VM_WRITE, VM_EXEC, VM_SHARED */

    /* For file-backed regions: */
    struct file        *vm_file;       /* mapped file (NULL for anonymous) */
    unsigned long       vm_pgoff;      /* file offset (in pages) */
    const struct vm_operations_struct *vm_ops;  /* fault, open, close handlers */
    /* … */
};
```

Every `mmap()` call creates (or modifies) one or more VMAs.
`/proc/PID/maps` shows all VMAs for a process:

```
00400000-004a0000 r-xp 00000000 fd:01 1234567  /bin/bash   ← .text
004a0000-004a2000 r--p 000a0000 fd:01 1234567  /bin/bash   ← .rodata
004a2000-004ad000 rw-p 000a2000 fd:01 1234567  /bin/bash   ← .data
...
7fffce000000-7fffce400000 rw-p 00000000 00:00 0            ← heap (anonymous)
7ffff7a00000-7ffff7d00000 r-xp 00000000 fd:01 999          /lib/libc.so.6
...
7ffffffde000-7ffffffff000 rw-p 00000000 00:00 0            ← stack
```

---

## 3. Physical Memory Layout

When the kernel boots, it learns the physical RAM layout from the firmware
(BIOS/EFI memory map, Device Tree, or ACPI).

It stores this as an array of `struct memblock_region` entries during early
boot, then converts to the zone-based `struct pglist_data` / `struct zone` /
`struct page` model before enabling the full allocator.

```
Physical RAM:
  ZONE_DMA      0 – 16 MiB        (ISA DMA constraints; legacy)
  ZONE_DMA32    0 – 4 GiB         (32-bit PCI DMA on x86-64)
  ZONE_NORMAL   above ZONE_DMA32  (directly mapped into kernel virtual space)
  ZONE_HIGHMEM  (32-bit only)     (not directly mapped; requires kmap())
  ZONE_MOVABLE  (for hotplug/balloon; pages can be moved)
```

---

## 4. Buddy Allocator

**File:** `mm/page_alloc.c`  
**Purpose:** Allocate and free **physically contiguous page frames**.

### Free lists

The buddy allocator maintains **free lists at each order** (power-of-2 sizes):

```
Order 0:  free 4 KiB blocks     (1 page)
Order 1:  free 8 KiB blocks     (2 pages)
Order 2:  free 16 KiB blocks    (4 pages)
…
Order 11: free 8 MiB blocks     (2048 pages)
```

### Allocation

```c
struct page *alloc_pages(gfp_t gfp, unsigned int order);
/* Allocates 2^order contiguous pages */

void *__get_free_pages(gfp_t gfp, unsigned int order);
/* Same, returns kernel virtual address */
```

1. Look for a free block of exactly the requested order.
2. If not found, go to the next higher order, take a block, and **split** it:
   one half satisfies the request; the other half ("buddy") is placed on the
   lower-order free list.
3. Repeat until a suitable block is found or all orders are exhausted.

### Freeing (Merging)

When a page is freed, the allocator checks whether its **buddy** (the
complementary block that forms a higher-order aligned pair) is also free.
If so, the two are **merged** into one higher-order block. This recurses up
the order chain.

```
Buddy identification: buddy_addr = addr XOR (1 << (order * PAGE_SHIFT))
(Both buddies must have the same alignment bit XOR'd out.)
```

### GFP flags

GFP (Get Free Pages) flags control allocation behaviour:

| Flag | Meaning |
|------|---------|
| `GFP_KERNEL` | Normal allocation; may sleep/reclaim (process context) |
| `GFP_ATOMIC` | Cannot sleep (interrupt context); may fail |
| `GFP_USER` | User-space allocation; may sleep |
| `GFP_HIGHUSER` | Like GFP_USER but prefers ZONE_HIGHMEM |
| `GFP_DMA` | Must be in ZONE_DMA (legacy 16 MiB DMA) |
| `GFP_NOWAIT` | Don't sleep; return NULL if unavailable |
| `__GFP_ZERO` | Zero-fill the allocated pages |

---

## 5. SLUB Allocator (kmalloc)

**File:** `mm/slub.c`  
**Purpose:** Allocate small objects (< 1 page) efficiently.

```c
void *kmalloc(size_t size, gfp_t flags);
void  kfree(const void *ptr);
```

### How SLUB works

1. **Slab caches:** Each fixed size has its own `struct kmem_cache`
   (e.g., `kmalloc-32`, `kmalloc-64`, `kmalloc-128`, …, up to `kmalloc-8192`).
   Custom caches can be created with `kmem_cache_create()`.

2. **Slabs:** Each cache manages a set of **slabs** — groups of pages holding
   multiple objects. One slab = `oo_order` pages = enough for many objects.

3. **Per-CPU partial slabs:** Each CPU has a pointer to a partial slab (some
   free, some used). Allocating from a per-CPU slab is lock-free and very fast.

4. **When the per-CPU slab is full:** Get a new partial slab from the node's
   partial list. If none, allocate fresh pages from the buddy allocator.

5. **When a slab is empty:** Return its pages to the buddy allocator.

### Object sizes and slab caches

```
Size     Cache name
4        kmalloc-4
8        kmalloc-8
16       kmalloc-16
32       kmalloc-32
64       kmalloc-64
96       kmalloc-96
128      kmalloc-128
192      kmalloc-192
256      kmalloc-256
512      kmalloc-512
1024     kmalloc-1k
2048     kmalloc-2k
4096     kmalloc-4k
8192     kmalloc-8k
```

For sizes > 8192, `kmalloc` falls back to `__get_free_pages` (buddy allocator).

---

## 6. Page Tables and Virtual Memory

### Page table structure (x86-64, 4-level)

```
CR3 register → PGD (Page Global Directory)
                 └── PUD (Page Upper Directory)
                       └── PMD (Page Middle Directory)
                             └── PTE (Page Table Entry)
                                   └── Physical page + offset
```

Each level is a 4 KiB table of 512 × 8-byte entries. A virtual address is
decomposed:

```
Bits:  63–48    47–39   38–30   29–21   20–12   11–0
       (sign)   PGD     PUD     PMD     PTE     Offset
```

Linux 6.x on most x86-64 systems uses 5-level page tables (PGD → P4D → PUD →
PMD → PTE) when LA57 is available, supporting 57-bit virtual addresses (128 PiB).

### PTE flags

Each PTE contains the physical page frame number (PFN) and permission bits:

```
Bit  Meaning
  0  Present (P) — page is mapped in RAM
  1  Read/write — 0 = read-only, 1 = writable
  2  User/supervisor — 0 = kernel only, 1 = user accessible
  5  Accessed — set by CPU on first access
  6  Dirty — set by CPU on first write
  7  Page Size (PS) — 1 = large page (2 MiB PMD or 1 GiB PUD)
 63  Execute Disable (NX) — if set, no instruction fetch from this page
```

Linux wraps PTE construction in arch-specific helpers like `mk_pte()`,
`pte_mkwrite()`, `pte_mkdirty()`, etc. ([kernel: arch/x86/include/asm/pgtable.h]).

---

## 7. Page Faults

When a process accesses a virtual address that has no valid PTE (or a PTE
with wrong permissions), the MMU raises a **page fault** exception.

```
CPU raises #PF exception
  → do_page_fault()              arch/x86/mm/fault.c (or arch/arm64/mm/fault.c)
      Check: kernel fault? → handle_kernel_fault() or oops
      Check: user fault?
        → handle_mm_fault()      mm/memory.c
            find_vma(mm, address)   find the VMA covering this address
            ├─ No VMA found       → SIGSEGV (segfault) to user process
            ├─ VMA found, check permissions
            │    wrong perm       → SIGSEGV
            └─ handle_pte_fault()
                 ├─ Page not present, VMA anonymous  → do_anonymous_page()
                 ├─ Page not present, VMA file-backed → do_read_fault() / do_shared_fault()
                 └─ Page present but read-only, VMA writable → do_wp_fault() (COW)
```

### Anonymous Pages (heap/stack)

When `malloc()` allocates memory (via `brk()` or `mmap(MAP_ANONYMOUS)`),
the kernel creates a VMA but does **not** allocate any physical pages yet.
The first access to a page in that VMA triggers a fault:

```
do_anonymous_page():
  1. Allocate a new zero-filled page (alloc_zeroed_user_highpage_movable())
  2. Create a PTE mapping the virtual address to the new physical page
  3. Return — the faulting instruction is re-executed and succeeds
```

This is **demand paging**: memory is only allocated when actually used.

### File-Backed Pages

When a process reads a file through `mmap()` or via the page cache:

```
do_read_fault():
  1. Call vma->vm_ops->fault() → filemap_fault()
  2. Find or allocate a page cache entry for (inode, page_index)
  3. If the page is not in cache, issue a block I/O read
  4. Map the cache page into the process's page table
  5. Return — re-execute the faulting instruction
```

### Copy-on-Write (COW)

After `fork()`, parent and child share all physical pages mapped read-only.
The first write to a shared page triggers a COW fault:

```
do_wp_fault():
  1. Allocate a new physical page
  2. Copy the shared page's contents to the new page
  3. Update the faulting process's PTE to point to the new (private) page
  4. Mark the original page read/write again if only one user remains
  5. Return — re-execute the write instruction
```

This is why `fork()` is fast even for processes with gigabytes of memory:
no actual copying happens until a write occurs.

---

## 8. The Page Cache

The **page cache** (`struct address_space`, `mm/filemap.c`) caches file data
in RAM. It is indexed by `(inode, page_offset)` using an XArray (formerly a
radix tree).

```
read(fd, buf, len)
  → vfs_read() → file->f_op->read_iter()
      → filemap_read()              mm/filemap.c
          For each page of the requested range:
            page = find_get_page(mapping, index)
            if (!page) {
                /* Cache miss */
                page = __page_cache_alloc()
                add_to_page_cache(page, mapping, index)
                readpage(file, page)   → block layer → disk
                wait_on_page_locked(page)
            }
            copy_page_to_user(buf, page, offset, len)
```

### Write-back

Modified ("dirty") pages are not immediately written to disk. The kernel's
`writeback` threads (`bdi_writeback`, `pdflush`) periodically flush dirty
pages to disk. Behaviour is controlled by:

- `/proc/sys/vm/dirty_ratio` — max % of RAM that can be dirty before sync writes
- `/proc/sys/vm/dirty_background_ratio` — when writeback starts in the background
- `fsync()` / `fdatasync()` — explicit flush to disk

---

## 9. Memory Zones

The buddy allocator divides physical memory into **zones** based on DMA and
addressing constraints:

```c
enum zone_type {
    ZONE_DMA,       /* 0 – 16 MiB: ISA DMA legacy */
    ZONE_DMA32,     /* 0 – 4 GiB: 32-bit DMA devices */
    ZONE_NORMAL,    /* Directly mapped kernel virtual memory */
    ZONE_HIGHMEM,   /* 32-bit kernels: memory above ~896 MiB */
    ZONE_MOVABLE,   /* Movable pages for hotplug / ballooning */
    __MAX_NR_ZONES
};
```

On x86-64 there is no HIGHMEM (all RAM is directly mapped). HIGHMEM only
affects 32-bit kernels (PAE or non-PAE).

**NUMA nodes (`struct pglist_data`):** On multi-socket (NUMA) systems, each
socket has its own node descriptor containing its own zones and free lists.
Allocations try to come from the node local to the requesting CPU.

---

## 10. Page Reclaim and Swap

When free memory is scarce, the kernel must **reclaim** pages:

1. **LRU lists:** The kernel maintains per-zone inactive/active LRU lists for
   file-backed and anonymous pages. Recently accessed pages are moved to the
   active list; less recently accessed pages stay on the inactive list.

2. **kswapd:** A kernel thread (`mm/vmscan.c` → `kswapd()`) wakes up when
   free pages fall below `pages_low`. It runs `shrink_lruvec()` to reclaim
   pages from the inactive list.

3. **Direct reclaim:** If `kswapd` cannot keep up, allocations that cannot
   sleep anyway wait for `kswapd`; allocations that can sleep reclaim pages
   directly (`__alloc_pages_slowpath()` → `__alloc_pages_direct_reclaim()`).

4. **Swap:** Anonymous pages that are not in use can be written to swap
   (`mm/swap_state.c`). Their PTEs are replaced with swap entries encoding
   the swap slot. On next access, a page fault reads the page back from swap.

Reclaim policies are tunable via `/proc/sys/vm/`:
- `swappiness` — how aggressively to swap out anonymous pages (0–200, default 60)
- `vfs_cache_pressure` — how aggressively to reclaim dentry/inode caches

---

## 11. Out-of-Memory Killer

**File:** `mm/oom_kill.c`

When the kernel cannot reclaim enough memory and an allocation is needed,
the **OOM killer** selects a process to kill.

```
out_of_memory()
  → select_bad_process()   — score each process by oom_score_adj + RSS
  → oom_kill_process()     — send SIGKILL to the selected process (and its thread group)
```

**OOM score:** Each process has an `oom_score` (0–1000) based on its RSS
(resident set size) relative to total RAM. A higher score = more likely to
be killed.

**User control:** Set `/proc/PID/oom_score_adj` (-1000 to +1000):
- `-1000`: never kill this process (systemd uses this)
- `+1000`: kill this process first

---

## 12. vmalloc vs kmalloc vs get_free_pages

| Function | Physical contiguity | Size | When to use |
|----------|--------------------|----|-------------|
| `alloc_pages(order)` | Contiguous, 2^order pages | Up to 8 MiB | When you need raw page frames |
| `kmalloc(size, gfp)` | Contiguous (up to 8 KiB typical) | Bytes | Small objects in any context |
| `kzalloc(size, gfp)` | Same as kmalloc, zero-filled | Bytes | Same + need zeroed memory |
| `vmalloc(size)` | Virtually contiguous, NOT physically | Bytes, large | Large allocations not needing DMA |
| `vfree(ptr)` | — | — | Free vmalloc memory |
| `dma_alloc_coherent()` | Contiguous, DMA-accessible | Bytes | DMA buffer allocation |

**vmalloc** maps non-contiguous physical pages into a contiguous virtual
range using the `VMALLOC_START`–`VMALLOC_END` kernel virtual address range.
This avoids the "order 11 contiguous pages not available" problem, but adds
TLB entries and cannot be used for DMA.

---

## 13. Memory-Mapped I/O

Hardware registers are accessed by mapping their physical addresses into the
kernel virtual address space:

```c
void __iomem *base = ioremap(phys_addr, size);
/* Read a 32-bit register */
u32 val = readl(base + OFFSET);
/* Write a 32-bit register */
writel(val, base + OFFSET);
/* Unmap when done */
iounmap(base);
```

`ioremap()` ([kernel: arch/x86/mm/ioremap.c]) creates a page table mapping
with caching disabled (or write-combining for framebuffers).

**Mapping to Module 01:** `mem_read32()` and `mem_write32()` in the simulator
are the software equivalent of `readl()` and `writel()` — both access a
flat address space through a base pointer.

---

## 14. Huge Pages

On x86-64, the PMD level can map a **2 MiB huge page** directly, skipping
the PTE level. This reduces TLB pressure for large memory regions (databases,
JVMs).

**Transparent Huge Pages (THP):** The kernel automatically promotes 2 MiB
aligned anonymous regions to huge pages (`mm/huge_memory.c`). Controlled by:
```
/sys/kernel/mm/transparent_hugepage/enabled   (always|madvise|never)
```

**HugeTLB pages:** Explicitly pre-allocated huge pages (2 MiB or 1 GiB)
available via `mmap(MAP_HUGETLB)` or `hugetlbfs`. Used by databases
(Oracle, PostgreSQL) for guaranteed huge-page backing.

---

## 15. Memory Cgroups

`mm/memcontrol.c` implements memory cgroups, which limit the amount of
memory a group of processes can use.

```
/sys/fs/cgroup/memory/<group>/
    memory.limit_in_bytes    ← set memory limit
    memory.usage_in_bytes    ← current usage
    memory.stat              ← detailed statistics
    memory.swappiness        ← per-cgroup swappiness
```

When a cgroup exceeds its limit, the kernel runs the per-cgroup OOM killer
or throttles the processes (configurable via `memory.oom_control`).

Used extensively by containers (Docker, Kubernetes, systemd services).

---

## 16. Mapping to Module 01

| Simulator concept | Memory management equivalent |
|-------------------|------------------------------|
| `Memory` struct | `struct mm_struct` |
| `mem->data` (1 MiB byte array) | Physical RAM managed by the buddy allocator |
| `MEM_PROG_BASE` (0x00010000) | `.text` VMA; file-backed mapping of the ELF binary |
| `MEM_STACK_TOP` (0x000FFFFC) | Top of the user stack VMA |
| `mem_read32 / mem_write32` | `readl() / writel()` for MMIO; or page cache read for files |
| `bounds_check()` + `abort()` | MMU page fault → SIGSEGV signal |
| `mem_init()` / `calloc()` | `mm_alloc()` + page table setup in `copy_mm()` |
| `mem_free()` | `mmput()` → `exit_mmap()` → free all VMAs and page tables |
| `mem_load_binary()` | `load_elf_binary()` → `elf_map()` → create VMA + mmap() |

---

## 17. Key Source Files Summary

| File | Role |
|------|------|
| `include/linux/mm_types.h` | `struct page`, `mm_struct`, `vm_area_struct` |
| `include/linux/gfp.h` | GFP flags for memory allocation |
| `mm/page_alloc.c` | Buddy allocator: `alloc_pages()`, `free_pages()` |
| `mm/slub.c` | SLUB object allocator: `kmalloc()`, `kfree()` |
| `mm/memory.c` | `handle_mm_fault()`, `do_anonymous_page()`, COW |
| `mm/mmap.c` | `do_mmap()`, `munmap()`, VMA management |
| `mm/filemap.c` | Page cache: `filemap_read()`, `filemap_write_and_wait()` |
| `mm/vmscan.c` | `kswapd()`, page reclaim, LRU lists |
| `mm/oom_kill.c` | OOM killer: `out_of_memory()`, `select_bad_process()` |
| `mm/vmalloc.c` | `vmalloc()`, `vfree()` — virtually contiguous allocations |
| `mm/huge_memory.c` | Transparent Huge Pages |
| `mm/memcontrol.c` | Memory cgroups |
| `arch/x86/mm/fault.c` | `do_page_fault()` — x86 page fault entry point |
| `arch/x86/mm/pgtable.c` | Page table allocation and management |
| `arch/arm64/mm/fault.c` | ARM64 page fault entry point |
