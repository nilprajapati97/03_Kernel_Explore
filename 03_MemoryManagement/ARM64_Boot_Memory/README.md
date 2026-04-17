# ARM64 Boot Memory — Conceptual Guide

A set of 11 layered documents explaining how the Linux memory management subsystem initializes on ARM64 (AArch64), from power-on to a fully operational allocator stack.

## What's Inside

- **Docs 01–03**: Hardware entry, early assembly boot, and kernel virtual mapping  
- **Docs 04–06**: Device Tree memory discovery, memblock early allocator, and page table construction  
- **Docs 07–09**: Zone/node/page initialization, buddy allocator handoff, and slab/vmalloc runtime  
- **Doc 10**: Appendix with address map, glossary, and quick reference  

## Assumptions

- AArch64 with 4KB pages, 48-bit VA, 4-level page tables  
- SPARSEMEM_VMEMMAP memory model, SLUB slab allocator  
- Based on Linux kernel ~6.x mainline source  

Start reading from [00_Index.md](00_Index.md).
