# ARM64 MemInit Code Walkthrough

A set of 11 documents tracing the **actual kernel source code** for ARM64 boot-time memory initialization — every function signature, argument value, register assignment, and call chain from `_text` through `memblock_free_all()`.

## What's Inside

- **Docs 01–02**: `head.S` entry, MMU enable sequence, identity map and kernel mapping via `map_range()`  
- **Docs 03–05**: `start_kernel()` → `setup_arch()`, memblock internals, page table construction chain  
- **Docs 06–08**: Zone/page init (`free_area_init`), buddy merge algorithm (`__free_one_page`), SLUB bootstrap trick  
- **Docs 09–10**: vmalloc init, completion timeline, and master call graph with file:line references  

## Assumptions

- AArch64 with 4KB pages, 48-bit VA, 4-level page tables  
- SPARSEMEM_VMEMMAP, SLUB allocator, Linux ~6.x mainline  
- Code snippets show actual function prototypes and logic from kernel source  

Start reading from [00_Index.md](00_Index.md).
