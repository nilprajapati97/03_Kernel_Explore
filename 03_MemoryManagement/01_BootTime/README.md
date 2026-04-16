# ARMv8 Linux Memory Subsystem Boot — Master Overview

## Why is Memory Initialization Special?
At reset, the ARMv8 CPU has no memory allocator, no virtual memory, and no usable heap. The Linux kernel must build the entire memory subsystem from scratch, in a strict sequence, before any dynamic memory can be used.

---

## Five Phases of Memory Initialization

| Phase | Allocator Active | What You Can Do |
|-------|------------------|-----------------|
| 1. Assembly Bootstrap | Static .bss only | Set up page tables, enable MMU |
| 2. Early C (setup_arch) | Fixmap, memblock | Reserve/allocate early memory, parse DTB |
| 3. Paging Init | Memblock | Map all RAM, build kernel linear map |
| 4. Buddy Allocator | Buddy system | Full page allocation/free |
| 5. Slab Allocator | SLUB/SLAB | `kmalloc`, kernel object caches |

---

## Master Flowchart

```mermaid
flowchart TD
    A[Reset: CPU starts] --> B[head.S: primary_entry]
    B --> C[Create early page tables]
    C --> D[Enable MMU]
    D --> E[start_kernel (C)]
    E --> F[setup_arch]
    F --> G[Parse DTB, memblock init]
    G --> H[paging_init: map all RAM]
    H --> I[bootmem_init: buddy up]
    I --> J[kmem_cache_init: slab up]
    J --> K[Kernel ready for allocations]
```

---

## Resource Availability Timeline

| Phase | Allocator | API |
|-------|-----------|-----|
| 1 | Static | None |
| 2 | Memblock | memblock_alloc |
| 3 | Memblock | memblock_alloc |
| 4 | Buddy | alloc_pages/free_pages |
| 5 | Slab | kmalloc/kfree |

---

## See Also
- [07_CallSequence_Master.md](07_CallSequence_Master.md) — Full call sequence
- [01_EarlyBoot_ASM.md](01_EarlyBoot_ASM.md) — Assembly bootstrap
- [02_EarlyC_SetupArch.md](02_EarlyC_SetupArch.md) — Early C setup
- [03_Memblock_Allocator.md](03_Memblock_Allocator.md) — Memblock details
- [04_Paging_Init.md](04_Paging_Init.md) — Paging setup
- [05_BuddyAllocator_Init.md](05_BuddyAllocator_Init.md) — Buddy system
- [06_Slab_Init.md](06_Slab_Init.md) — Slab/SLUB
