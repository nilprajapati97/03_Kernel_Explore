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

Here’s a **high-quality, NVIDIA-style OS interview question bank**—not fluff, but the kind of depth they actually probe for (especially for systems, CUDA, driver, or low-level roles).

---

# 🧠 Core OS Fundamentals (Very Important)

These are almost guaranteed.

### Process vs Thread

* Difference between process and thread?
* When would you prefer threads over processes?
* What is context switching? Cost?
* User threads vs kernel threads?
* What is a zombie process? Orphan process?

---

### Scheduling

* What are scheduling algorithms? Compare:

  * FCFS, SJF, Round Robin, Priority
* What is starvation? Aging?
* What is preemption?
* Real-time scheduling vs general-purpose?

---

### Synchronization & Concurrency 🔥 (NVIDIA LOVES THIS)

* What is a race condition?
* Mutex vs semaphore?
* Binary semaphore vs counting semaphore?
* What is a spinlock?
* What is deadlock?

Deadlock deep dive:

* 4 necessary conditions
* Prevention vs avoidance vs detection
* Banker’s algorithm (sometimes asked)

Advanced:

* Lock-free vs wait-free programming
* Memory ordering / memory model (very important for GPU + CPU)

---

### Memory Management (CRITICAL)

* What is virtual memory?
* Paging vs segmentation
* What is fragmentation? Internal vs external
* What is TLB?
* Page replacement algorithms:

  * LRU, FIFO, Optimal

Advanced NVIDIA-style:

* What is NUMA?
* What is cache coherence?
* What is false sharing?

---

### System Calls & Kernel

* What is a system call?
* User mode vs kernel mode
* What happens when you call `malloc()` internally?
* What happens during a context switch?

---

### File Systems

* How does a file system work?
* What is inode?
* What is journaling?

---

# ⚡ Advanced OS (NVIDIA FOCUS AREA)

These differentiate candidates.

---

### Multithreading & Parallelism

* What is thread affinity?
* What is work stealing?
* What is lock contention?

---

### CPU + GPU Interaction (VERY NVIDIA-SPECIFIC)

* How does CPU communicate with GPU?
* What is DMA?
* What is pinned memory?
* What is unified memory (CUDA)?

---

### Memory Hierarchy

* What is cache coherence protocol?
* MESI protocol basics
* What is cache line?
* What is false sharing?

---

### Performance & Optimization

* What is latency vs throughput?
* What causes cache misses?
* How to optimize memory access?

---

### OS + Hardware Interface

* Interrupt vs polling?
* What is an interrupt handler?
* What is a device driver?

---

# 🔥 Coding + Design Questions (Common in NVIDIA)

### Implementations

* Design your own malloc (you just asked this 👍)
* Implement a thread-safe queue
* Producer-consumer problem
* Reader-writer lock

---

### Design Questions

* Design a memory allocator
* Design a scheduler
* Design a logging system (multi-threaded)
* Design a high-performance cache

---

# 🧩 Trick / Deep Thinking Questions

These test real understanding.

* Why is `volatile` not enough for synchronization?
* Why are locks expensive?
* What happens if you disable interrupts?
* Can deadlock occur with one thread?
* Why is malloc not constant time?

---

# 🧪 Debugging / Scenario Questions

Very common in NVIDIA interviews.

* Program runs slower on multi-core — why?
* Threads not scaling — what could be wrong?
* Memory leak detection?
* High CPU usage debugging?

---

# 🎯 Must-Know Topics for NVIDIA (Priority Order)

1. Concurrency + Synchronization 🔥🔥🔥
2. Memory Management 🔥🔥🔥
3. CPU–GPU Interaction 🔥🔥
4. Cache & Performance 🔥🔥
5. Scheduling 🔥
6. File Systems (light)

---

# 💡 How NVIDIA Differs from Typical Companies

They focus less on definitions and more on:

* **Performance**
* **Parallelism**
* **Hardware-aware thinking**
* **Real-world debugging**

---

For a candidate with 10 years of experience, a Linux Kernel interview shifts from "how do things work" to "why are they designed this way" and "how do they perform at scale." 

Here are 50 system design and internal architecture questions categorized by subsystem.

---

## 1. Process Management & Scheduling
1. **Explain the Completely Fair Scheduler (CFS) design.** *Answer:* CFS uses a Red-Black tree to manage runnable processes based on `vruntime`. It aims to give each process an equal share of the processor.
2. **How does the kernel handle Load Balancing in multi-core systems?** *Answer:* It uses scheduling domains to represent the CPU hierarchy (SMT, Cores, Sockets) and pulls tasks from busier queues to idle ones.
3. **What is the difference between `TASK_INTERRUPTIBLE` and `TASK_UNINTERRUPTIBLE`?** *Answer:* Both are sleeping states, but `UNINTERRUPTIBLE` ignores signals (used for critical I/O), preventing the process from being killed until the event occurs.
4. **How does `copy-on-write` (COW) work during a `fork()`?** *Answer:* Pages aren't copied initially; the child shares the parent's pages marked as read-only. A copy is made only when one process attempts a write.
5. **Describe the implementation of Namespaces for Containers.** *Answer:* Namespaces wrap global system resources (PID, Net, Mount) in an abstraction that makes processes within a namespace think they have their own isolated instance.
6. **What is a "Zombie" process and why does it exist?** *Answer:* A process that has finished execution but still has an entry in the process table so the parent can read its exit code via `wait()`.
7. **Explain the `preempt_count` variable.** *Answer:* An atomic counter used to track if the current context can be preempted (e.g., it’s 0 when in a safe user-space/kernel-thread state).
8. **How does the Real-Time (`PREEMPT_RT`) patch change the kernel?** *Answer:* It turns spinlocks into sleepable mutexes and forces almost all interrupt handlers into threaded contexts to reduce latency.
9. **Explain "Priority Inversion" and how the kernel solves it.** *Answer:* A low-priority task holds a resource needed by a high-priority task. Solved via Priority Inheritance (temporarily boosting the low-priority task).
10. **What is the `sched_yield()` syscall and why is it often discouraged?** *Answer:* It gives up the CPU voluntarily, but in modern CFS, it might just put the process back at the end of the tree, leading to "ping-pong" overhead.

---

## 2. Memory Management
11. **Explain the Buddy Allocator.** *Answer:* It splits memory into power-of-two sized blocks to minimize external fragmentation and allows quick merging of "buddy" blocks when freed.
12. **What is the Slab Allocator (SLUB/SLAB)?** *Answer:* A cache of frequently used objects (like `task_struct`) to avoid the overhead of full allocation/deallocation and reduce internal fragmentation.
13. **How does the OOM (Out of Memory) Killer choose a victim?** *Answer:* It calculates an `oom_score` based on memory usage, process importance, and `oom_score_adj` values.
14. **Describe the difference between Physical and Virtual addresses.** *Answer:* Physical is the actual RAM location; Virtual is what the CPU sees, translated via the MMU and Page Tables.
15. **What is a TLB (Translation Lookaside Buffer)?** *Answer:* A hardware cache in the CPU that stores recent virtual-to-physical address translations to speed up memory access.
16. **How does "Memory Compaction" work?** *Answer:* The kernel moves used pages to one end of a memory zone to create larger contiguous free blocks for high-order allocations.
17. **What is the role of `kswapd`?** *Answer:* A kernel thread that runs asynchronously to reclaim pages when free memory drops below a certain "low" watermark.
18. **Explain "HugePages" and why they are used.** *Answer:* Using larger page sizes (e.g., 2MB vs 4KB) to reduce the number of TLB misses for memory-intensive applications like databases.
19. **What is "Memory Overcommit"?** *Answer:* The kernel allowing processes to allocate more memory than physically available, betting that not all processes will use their full allocation at once.
20. **Explain the "Dirty Ratio" in the Page Cache.** *Answer:* The percentage of system memory that can be filled with "dirty" (unwritten to disk) pages before the kernel starts forcing writes.

---

## 3. I/O and File Systems
21. **What is the VFS (Virtual File System) layer?** *Answer:* An abstraction layer that allows different filesystems (ext4, XFS, NFS) to appear uniform to user-space via standard syscalls.
22. **Explain the `inode` structure.** *Answer:* A data structure that stores metadata about a file (permissions, size, block locations) but not the filename or the actual data.
23. **What is a "Dentry" (Directory Entry) cache?** *Answer:* A cache that links filenames to their corresponding inodes to speed up file path lookups.
24. **How does "Asynchronous I/O" (AIO) differ from `io_uring`?** *Answer:* AIO is older and often blocks on O_DIRECT; `io_uring` is a modern, ring-buffer based interface that minimizes syscall overhead and context switching.
25. **Explain the Page Cache.** *Answer:* A transparent cache of disk blocks in RAM, allowing the kernel to satisfy read requests without hitting the disk.
26. **What is "Direct I/O" (O_DIRECT)?** *Answer:* Bypassing the Page Cache to let the application manage its own caching (common in high-performance databases).
27. **Describe the "I/O Scheduler" (e.g., BFQ, Kyber).** *Answer:* It decides the order in which I/O requests are sent to the storage device to optimize throughput or minimize latency.
28. **How do Hard Links and Soft Links differ at the kernel level?** *Answer:* Hard links point to the same inode; soft links are separate files containing a path string to another file.
29. **What is the purpose of the `writeback` mechanism?** *Answer:* Periodically flushing "dirty" pages from RAM to disk to ensure data persistence while maintaining performance.
30. **Explain the "Bio" structure in the Block I/O layer.** *Answer:* The primary unit of I/O in the kernel, representing an I/O operation in progress, containing target blocks and memory segments.

---

## 4. Interrupts & Synchronization
31. **What are "Top Halves" and "Bottom Halves"?** *Answer:* Top half is the immediate ISR (Interrupt Service Routine) that runs with interrupts disabled; Bottom half (Tasklets, Workqueues) handles the heavy lifting later.
32. **Explain the difference between Spinlocks and Mutexes.** *Answer:* Spinlocks busy-wait (consume CPU) and cannot sleep; Mutexes put the thread to sleep and are used for longer-held locks.
33. **What is RCU (Read-Copy-Update)?** *Answer:* A synchronization mechanism that allows multiple readers to access data simultaneously with a single writer by creating a copy for updates.
34. **Why shouldn't you sleep in an Interrupt Context?** *Answer:* Because the interrupt handler doesn't have a task context; there is no way for the scheduler to "wake up" an ISR.
35. **What is an "Atomic Operation"?** *Answer:* An operation that completes in a single step without being interrupted, essential for managing counters in SMP systems.
36. **Explain "Softirqs" vs "Tasklets".** *Answer:* Softirqs are statically allocated and can run on multiple CPUs simultaneously; Tasklets are built on softirqs but are dynamically allocated and pinned to one CPU.
37. **What is "IRQ Affinity"?** *Answer:* The ability to bind specific hardware interrupts to specific CPU cores to improve cache locality and performance.
38. **Describe "Sequence Locks" (seqlocks).** *Answer:* Optimized for frequent reads and infrequent writes; readers check a sequence counter to see if data was modified during their read.
39. **What is a "Memory Barrier"?** *Answer:* An instruction that forces the CPU/Compiler to maintain a specific order of memory operations to prevent race conditions.
40. **How does the kernel handle "Spurious Interrupts"?** *Answer:* By checking hardware status registers within the ISR to verify if the device actually requested attention.

---

## 5. Device Drivers & Networking
41. **What is the "Device Model" (kobject, kset)?** *Answer:* A framework used to track all devices in the system, their hierarchy, and their relationship to drivers.
42. **Explain "Character Devices" vs "Block Devices".** *Answer:* Char devices are accessed as a stream of bytes (keyboard); Block devices are accessed in fixed-size chunks and support seeking (SSD).
43. **How does DMA (Direct Memory Access) work?** *Answer:* Hardware transfers data directly to/from RAM without CPU intervention, notifying the CPU via interrupt when finished.
44. **What is the "NAPI" (New API) in networking?** *Answer:* A technique that switches from interrupt-driven to polling-driven packet processing under high load to prevent "interrupt storms."
45. **Explain the "sk_buff" (Socket Buffer) structure.** *Answer:* The core data structure for network packets, designed to avoid copying data as it moves through the protocol stack.
46. **What is "Zero-Copy" networking?** *Answer:* Using `sendfile()` or `splice()` to move data from disk to network card without copying it into user-space memory.
47. **Describe "Netfilter" and its role.** *Answer:* A framework that provides hooks at various points in the networking stack for packet filtering (iptables), NAT, and mangling.
48. **What is a "Loadable Kernel Module" (LKM)?** *Answer:* Code that can be added to or removed from the kernel at runtime without rebooting, typically used for drivers.
49. **How does the kernel handle "Hotplugging" (e.g., USB)?** *Answer:* The kernel detects the hardware change, notifies `udev` in user-space, which then loads the appropriate module.
50. **What is the purpose of `/proc` and `/sys`?** *Answer:* Pseudo-filesystems that provide an interface to kernel internals (`/proc` for processes/system info, `/sys` for device/driver hierarchy).

---

[Linux System Design and Architecture](https://www.youtube.com/watch?v=MxfLlmSofNc)

This video provides a solid overview of Linux system design and the core components of the kernel, which is essential for understanding the high-level architecture expected at a senior level.

