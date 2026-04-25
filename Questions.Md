Below is a high-coverage OS interview question bank for NVIDIA-style roles (systems software, kernel/driver, platform, performance).  
“all possible” is impossible, but this is close to what you should prepare.

**Core OS Fundamentals**
1. What happens from power-on to `main()` in a Linux system?
2. User mode vs kernel mode: why do we need both?
3. What is a system call, and what is the syscall path?
4. Difference between process, thread, and task.
5. What data structures does OS keep per process?
6. What is context switching cost, and what contributes to it?
7. What is reentrancy? Is a syscall handler reentrant?
8. What is interrupt latency vs scheduling latency?
9. What are traps, faults, and aborts?
10. What is the difference between monolithic kernel and microkernel?

**Processes, Threads, and Scheduling**
1. Explain Linux CFS scheduling in simple terms.
2. Difference between preemptive and cooperative scheduling.
3. What is priority inversion? How do you solve it?
4. What are real-time scheduling classes in Linux?
5. What is CPU affinity and why use it?
6. What is load balancing across cores?
7. How does NUMA affect thread placement?
8. What is a zombie process? orphan process?
9. Difference between `fork`, `vfork`, `clone`.
10. How would you debug high context-switch rate?

**Synchronization and Concurrency**
1. Mutex vs spinlock: when to use each?
2. Semaphore vs condition variable.
3. What is a memory barrier and why needed on ARM?
4. Explain happens-before relationship.
5. What is lock-free vs wait-free?
6. ABA problem: what is it and mitigation?
7. What causes deadlock? list Coffman conditions.
8. How to detect and avoid deadlocks in design?
9. Readers-writer lock tradeoffs.
10. How do you design a thread-safe queue with high throughput?

**Memory Management**
1. Virtual memory: why not direct physical addressing?
2. What is paging? page table hierarchy?
3. TLB: what is it and why is miss expensive?
4. What is copy-on-write?
5. Demand paging vs pre-paging.
6. Page fault handling steps.
7. Internal vs external fragmentation.
8. What is slab/slub allocator in Linux?
9. `malloc` vs `mmap` allocation paths.
10. How does `brk` differ from `mmap` for heap growth?
11. What is huge page / transparent huge page?
12. What is memory compaction and reclaim?
13. How do you diagnose memory leaks in C/C++ on Linux?
14. What is OOM killer and how it selects victim?
15. Explain cache line, false sharing, and padding.

**File Systems and Storage**
1. What is a VFS layer and why important?
2. Inode, dentry, superblock roles.
3. Journaling vs copy-on-write file systems.
4. What is fsync and when is data actually durable?
5. Buffered I/O vs direct I/O.
6. Memory-mapped files pros/cons.
7. What is page cache and write-back?
8. Explain RAID levels briefly.
9. What is I/O scheduler and when to tune it?
10. How would you debug sudden I/O latency spikes?

**IPC and Networking**
1. Compare pipe, FIFO, message queue, shared memory, socket.
2. Why is shared memory fastest IPC and what are risks?
3. What is zero-copy I/O?
4. `select` vs `poll` vs `epoll`.
5. What is NAPI in Linux networking?
6. What are softirq and tasklets?
7. TCP 3-way and 4-way handshakes.
8. What causes TCP retransmissions and tail latency?
9. What is socket backlog and SYN queue?
10. How to debug packet drops in Linux?

**Linux Kernel / Driver Focus (Very NVIDIA-Relevant)**
1. User-space vs kernel-space driver split.
2. What is DMA and why pin pages?
3. What is IOMMU and why needed for device isolation?
4. Interrupt-driven I/O vs polling.
5. MSI/MSI-X vs legacy interrupts.
6. Top half vs bottom half in interrupt handling.
7. What is a character device vs block device?
8. How does `ioctl` work and what are design pitfalls?
9. What is scatter-gather DMA?
10. How to handle race conditions between ISR and process context?
11. What is reference counting in kernel objects?
12. What is RCU and where is it useful?
13. How would you debug a kernel panic from a driver?
14. What tools: `dmesg`, `ftrace`, `perf`, `bpftrace`, `kgdb`?
15. How to maintain ABI compatibility between kernel/user components?

**Architecture-Specific (x86 + ARM)**
1. Key memory model differences between x86 and ARM.
2. Why does code that works on x86 fail on ARM without barriers?
3. What is endianess? where does it matter?
4. What is cache coherency protocol (MESI-like)?
5. What are atomic operations at hardware level?
6. Why alignment matters more on some ARM variants?
7. TSO vs weaker ordering models impact on lock-free code.
8. What is the role of `volatile` and why it is not synchronization?
9. How do you write portable low-level C across architectures?
10. How would you validate behavior on both x86 and ARM?

**Virtualization and Containers**
1. Type-1 vs Type-2 hypervisor.
2. What is paravirtualization?
3. How do cgroups and namespaces isolate containers?
4. What is container vs VM tradeoff for performance?
5. What is SR-IOV and why useful for high-performance devices?
6. How do you diagnose noisy-neighbor CPU/memory interference?

**Performance and Debugging Scenarios**
1. System is 100% CPU but throughput dropped. How debug?
2. Latency spikes every few seconds. Where to look first?
3. Multithreaded app slows when scaling from 8 to 64 cores. Why?
4. Memory usage stable but OOM still happens. Possible causes?
5. Driver occasionally hangs under heavy load. Debug strategy?
6. Frequent page faults despite enough RAM. Why?
7. High syscall overhead in a tight loop. Optimization options?
8. How do you build a reproducible perf test for kernel/user boundary?
9. What metrics would you monitor for scheduler issues?
10. How would you triage a production crash dump?

**Common Design Questions**
1. Design a custom memory allocator for multithreaded workloads.
2. Design a bounded lock-free ring buffer.
3. Design fair reader-writer synchronization for mixed workloads.
4. Design a scheduler for latency-sensitive + throughput tasks.
5. Design a userspace service that minimizes syscall overhead.
6. Design a logging pipeline that avoids blocking hot path.

If you want, I can give you:
1. A 7-day preparation plan for these OS topics.
2. Top 30 most likely NVIDIA OS questions with model answers.
3. A mock interview (I ask, you answer, I evaluate).
