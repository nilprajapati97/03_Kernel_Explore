# Linux Kernel — Process Management Deep Dive

> **Kernel source references:**  
> `include/linux/sched.h`, `kernel/fork.c`, `kernel/exit.c`,
> `kernel/sched/core.c`, `kernel/sched/fair.c`, `kernel/signal.c`

---

## Table of Contents

1. [What Is a Process in the Linux Kernel?](#1-what-is-a-process-in-the-linux-kernel)
2. [struct task_struct — The Process Descriptor](#2-struct-task_struct--the-process-descriptor)
3. [Process Lifecycle](#3-process-lifecycle)
   - [Creation: fork() and clone()](#creation-fork-and-clone)
   - [Program Loading: exec()](#program-loading-exec)
   - [Termination: exit() and wait()](#termination-exit-and-wait)
4. [Threads in the Linux Kernel](#4-threads-in-the-linux-kernel)
5. [Process States](#5-process-states)
6. [The Scheduler](#6-the-scheduler)
   - [Scheduling Classes](#scheduling-classes)
   - [Completely Fair Scheduler (CFS)](#completely-fair-scheduler-cfs)
   - [Real-Time Scheduling](#real-time-scheduling)
   - [The schedule() Function](#the-schedule-function)
   - [Context Switch](#context-switch)
7. [Signals](#7-signals)
8. [Namespaces and PIDs](#8-namespaces-and-pids)
9. [CPU Affinity and NUMA Scheduling](#9-cpu-affinity-and-numa-scheduling)
10. [Mapping to Module 01](#10-mapping-to-module-01)

---

## 1. What Is a Process in the Linux Kernel?

A **process** is an executing program with its own:

- Virtual address space (`mm_struct`)
- File descriptor table
- Signal handlers
- Credentials (UID, GID, capabilities)
- Namespace memberships

A **thread** is a process that shares its address space and file table with
one or more other threads — but still has its own stack, registers, and
scheduling state.

In the Linux kernel, **there is no fundamental distinction between a thread
and a process at the kernel level**. Both are represented by `task_struct`.
The difference is which resources are shared (controlled by flags passed to
the `clone()` syscall).

---

## 2. struct task_struct — The Process Descriptor

`struct task_struct` ([kernel: include/linux/sched.h]) is the single most
important data structure in the kernel. Each process/thread has one.

Simplified layout (selected fields):

```c
struct task_struct {
    /* ── Scheduling ──────────────────────────────────────────────── */
    volatile long           state;           /* TASK_RUNNING, TASK_INTERRUPTIBLE, … */
    unsigned int            flags;           /* PF_IDLE, PF_KTHREAD, … */
    int                     prio;            /* effective scheduling priority */
    int                     static_prio;     /* nice-based static priority */
    int                     normal_prio;     /* priority without RT boost */
    unsigned int            rt_priority;     /* RT priority (1–99) */
    const struct sched_class *sched_class;   /* &fair_sched_class, &rt_sched_class, … */
    struct sched_entity     se;              /* CFS scheduling entity (vruntime) */
    struct sched_rt_entity  rt;              /* RT scheduling entity */
    struct sched_dl_entity  dl;              /* Deadline scheduling entity */
    struct rq              *rq;              /* current run queue */
    cpumask_t               cpus_allowed;    /* CPUs this task may run on */
    unsigned int            policy;          /* SCHED_NORMAL, SCHED_FIFO, … */

    /* ── PID ─────────────────────────────────────────────────────── */
    pid_t                   pid;             /* thread ID */
    pid_t                   tgid;            /* thread group ID (= PID of group leader) */
    struct pid             *thread_pid;      /* PID object */
    struct task_struct     *group_leader;    /* thread group leader */

    /* ── Family ─────────────────────────────────────────────────── */
    struct task_struct     *real_parent;     /* biological parent */
    struct task_struct     *parent;          /* current parent (may differ via ptrace) */
    struct list_head        children;        /* list of children */
    struct list_head        sibling;         /* sibling list node */

    /* ── Memory ─────────────────────────────────────────────────── */
    struct mm_struct       *mm;              /* virtual address space (NULL for kthreads) */
    struct mm_struct       *active_mm;       /* active mm (borrowed by kthreads) */

    /* ── Files ──────────────────────────────────────────────────── */
    struct files_struct    *files;           /* open file descriptor table */
    struct fs_struct       *fs;              /* root and current working directory */

    /* ── Signals ────────────────────────────────────────────────── */
    struct signal_struct   *signal;          /* shared signal state (per thread group) */
    struct sighand_struct  *sighand;         /* signal handlers */
    sigset_t                blocked;         /* blocked signal mask */
    sigset_t                pending;         /* pending signals */

    /* ── Credentials ────────────────────────────────────────────── */
    const struct cred      *real_cred;       /* objective UID/GID */
    const struct cred      *cred;            /* effective UID/GID */

    /* ── Namespaces ─────────────────────────────────────────────── */
    struct nsproxy         *nsproxy;         /* namespace descriptors */

    /* ── CPU context ────────────────────────────────────────────── */
    struct thread_struct    thread;          /* arch-specific registers/state */

    /* ── Timekeeping ────────────────────────────────────────────── */
    u64                     utime;           /* user time (nanoseconds) */
    u64                     stime;           /* system (kernel) time */
    u64                     start_time;      /* process start time */

    /* ── Stack ──────────────────────────────────────────────────── */
    void                   *stack;           /* pointer to kernel stack */
    /* … hundreds more fields … */
};
```

### Accessing the current task

From anywhere in the kernel, `current` is a macro that returns a pointer to
the `task_struct` of the currently executing process:

```c
/* Architecture-specific magic — typically reads from a per-CPU variable */
#define current  get_current()

/* Usage examples */
printk("Current PID: %d\n", current->pid);
printk("Current comm: %s\n", current->comm);
```

---

## 3. Process Lifecycle

### Creation: fork() and clone()

```
User space                          Kernel
──────────                          ──────
pid_t pid = fork();      ─────────▶ sys_fork()              kernel/fork.c
                                      └─ kernel_clone()
                                           └─ copy_process()
                                                ├─ dup_task_struct()
                                                │    alloc task_struct + kernel stack
                                                ├─ copy_creds()
                                                ├─ copy_mm()         (COW: shares page tables)
                                                ├─ copy_files()      (copies fdtable)
                                                ├─ copy_sighand()
                                                ├─ copy_signal()
                                                ├─ copy_thread()     (arch: save registers)
                                                └─ alloc_pid()
                                           └─ wake_up_new_task()
                                                └─ enqueue_task_fair() (add to CFS rq)
```

**Copy-on-Write (COW):** When `fork()` copies the page tables, it marks all
pages read-only. The first write to a shared page triggers a page fault, which
allocates a private copy for the writing process. This makes `fork()` very
fast even for large processes.

### Program Loading: exec()

After `fork()`, the child typically calls `exec()` to replace its image:

```
execve("/bin/ls", argv, envp)
  → do_execveat_common()        fs/exec.c
      ├─ open_exec()            open the binary file
      ├─ bprm_fill_uid()        set UID/GID from file permissions (setuid)
      ├─ exec_binprm()
      │    └─ search_binary_handler()
      │         → load_elf_binary()   fs/binfmt_elf.c
      │              ├─ Read ELF headers
      │              ├─ mmap() PT_LOAD segments into address space
      │              ├─ Set up stack (argc, argv, envp, auxvec)
      │              └─ start_thread(regs, elf_entry, sp)
      │                   Sets PC = elf_entry, SP = user stack top
      └─ (returns to user space at the new program's entry point)
```

### Termination: exit() and wait()

```
exit(0)
  → do_exit()                   kernel/exit.c
      ├─ exit_signals()         wake up threads waiting for this task
      ├─ exit_mm()              drop reference to mm_struct
      ├─ exit_files()           close all file descriptors
      ├─ exit_fs()              release FS context
      ├─ exit_task_namespaces() release namespace references
      ├─ task->state = TASK_ZOMBIE
      └─ schedule()             give up the CPU

Parent calls wait()
  → sys_wait4() → do_wait()     kernel/exit.c
      ├─ Find a TASK_ZOMBIE child
      ├─ release_task()         free task_struct
      └─ Return child PID + status to parent
```

**Zombie process:** A process that has exited but whose `task_struct` has not
yet been reaped by its parent. The zombie holds the exit status until `wait()`
is called. If the parent exits without calling `wait()`, the child is adopted
by PID 1 (init/systemd), which calls `wait()` periodically.

---

## 4. Threads in the Linux Kernel

POSIX threads (`pthreads`) are created using the `clone()` syscall with
sharing flags:

```c
/* pthread_create() eventually calls: */
clone(child_func,
      CLONE_VM       |   /* share address space (mm_struct) */
      CLONE_FILES    |   /* share file descriptor table */
      CLONE_FS       |   /* share root/cwd */
      CLONE_SIGHAND  |   /* share signal handlers */
      CLONE_THREAD   |   /* same thread group (same TGID) */
      CLONE_SETTLS   |   /* set TLS descriptor */
      CLONE_PARENT_SETTID | CLONE_CHILD_CLEARTID,
      &thread_attr);
```

Each thread has its **own**:
- `pid` (kernel thread ID)
- Kernel stack
- `thread_struct` (saved registers)
- Signal mask (`blocked`)
- CPU affinity

All threads in a group share:
- `tgid` (same as the group leader's `pid`)
- `mm_struct` (virtual address space)
- `files_struct` (open file descriptors)
- `signal_struct` (signal handlers)

---

## 5. Process States

```c
/* include/linux/sched.h */
#define TASK_RUNNING            0x00000000  /* runnable or running */
#define TASK_INTERRUPTIBLE      0x00000001  /* sleeping; woken by signal */
#define TASK_UNINTERRUPTIBLE    0x00000002  /* sleeping; NOT woken by signal */
#define __TASK_STOPPED          0x00000004  /* stopped by SIGSTOP/SIGTSTP */
#define __TASK_TRACED           0x00000008  /* being ptraced */
#define TASK_PARKED             0x00000040  /* kthread parked */
#define TASK_DEAD               0x00000080  /* about to die */
#define TASK_ZOMBIE             (TASK_DEAD | EXIT_ZOMBIE)
```

State transitions:

```
              wake_up()             schedule()
    ┌──────────────────────────┐  ┌──────────────────────┐
    │                          ▼  │                       ▼
INTERRUPTIBLE ─────────────▶ RUNNING ──────────────▶ INTERRUPTIBLE
                              │  ▲                   (sleep/wait)
                     preempt  │  │  wake_up_process()
                              │  │
                              ▼  │
                           RUNNING (on another CPU)
                              │
                         SIGSTOP│
                              ▼
                           STOPPED
                              │
                         SIGCONT│
                              ▼
                           RUNNING
```

---

## 6. The Scheduler

### Scheduling Classes

The Linux scheduler is modular: each **scheduling class** implements a set of
operations (`struct sched_class`):

```c
struct sched_class {
    void (*enqueue_task)(struct rq *rq, struct task_struct *p, int flags);
    void (*dequeue_task)(struct rq *rq, struct task_struct *p, int flags);
    void (*yield_task)(struct rq *rq);
    bool (*yield_to_task)(struct rq *rq, struct task_struct *p);
    void (*check_preempt_curr)(struct rq *rq, struct task_struct *p, int flags);
    struct task_struct *(*pick_next_task)(struct rq *rq);
    void (*put_prev_task)(struct rq *rq, struct task_struct *p);
    void (*task_tick)(struct rq *rq, struct task_struct *p, int queued);
    /* … more ops … */
};
```

Classes in priority order:

```
stop_sched_class   — highest priority; used by migration/stop-machine
dl_sched_class     — SCHED_DEADLINE (EDF)
rt_sched_class     — SCHED_FIFO, SCHED_RR
fair_sched_class   — SCHED_NORMAL, SCHED_BATCH (CFS — used by most processes)
idle_sched_class   — SCHED_IDLE (lower priority than nice +19)
```

### Completely Fair Scheduler (CFS)

**Key idea:** give every runnable task a fair share of CPU time, proportional
to its weight (derived from its `nice` value).

**Data structure:** A per-CPU red-black tree (`struct cfs_rq.tasks_timeline`)
ordered by `vruntime`. The leftmost node always has the smallest `vruntime`
(the task that has received the least CPU time) and is picked next.

```
vruntime updates:
  Every timer tick: current->se.vruntime += delta_exec * NICE_0_WEIGHT / weight
  (Tasks with lower nice/higher weight accumulate vruntime more slowly.)

Pick next task:
  pick_next_entity() → leftmost node in the red-black tree

Preemption check (after task wakes up):
  waking task's vruntime < (current vruntime - sched_latency / nrRunning)
  → preempt current task
```

**Scheduling latency:** The target time within which every runnable task gets
one CPU turn. Default: 6 ms (tunable via `/proc/sys/kernel/sched_latency_ns`).
If more than `sched_nr_latency` tasks are runnable, the minimum granularity
per task applies instead.

### Real-Time Scheduling

`SCHED_FIFO`: once a task starts running, it runs until it blocks, yields,
or is preempted by a higher-priority RT task. No time-slicing.

`SCHED_RR`: like FIFO but with time slices. When a task exhausts its slice,
it is moved to the end of the queue at the same priority.

RT tasks preempt any CFS task immediately.

**Priority inversion and priority inheritance:** If a high-priority RT task
blocks waiting for a mutex held by a low-priority CFS task, the CFS task
temporarily inherits the RT priority until it releases the mutex
(`CONFIG_RT_MUTEXES` priority inheritance protocol).

### The schedule() Function

`schedule()` ([kernel: kernel/sched/core.c]) is called when:

1. A task calls `sleep()`, `wait()`, `read()`, `mutex_lock()`, etc. and blocks.
2. A timer tick fires and the scheduler decides to preempt the current task.
3. A task voluntarily yields (`sched_yield()`).

```c
void schedule(void)
{
    struct task_struct *prev = current;
    struct rq *rq = this_rq();

    /* Dequeue prev from run queue if it's going to sleep */
    if (prev->state != TASK_RUNNING)
        deactivate_task(rq, prev, DEQUEUE_SLEEP);

    /* Pick the next task */
    next = pick_next_task(rq, prev, &rf);

    /* Perform context switch if a different task was selected */
    if (prev != next)
        rq = context_switch(rq, prev, next, &rf);
}
```

### Context Switch

`context_switch()` ([kernel: kernel/sched/core.c]) does two things:

1. **Switch the memory context** (`switch_mm()`): load the new task's page
   table base register (CR3 on x86, TTBR on ARM). This flushes the TLB
   (or uses ASID to avoid full flush on ARM64).

2. **Switch the CPU registers** (`switch_to()`): save the current task's
   general-purpose registers + stack pointer + PC to `thread_struct`; load
   the new task's `thread_struct`.

```c
/* arch/x86/include/asm/switch_to.h */
#define switch_to(prev, next, last)                 \
do {                                                \
    ((last) = __switch_to_asm((prev), (next)));     \
} while (0)

/* arch/x86/kernel/switch_to.S — assembly */
__switch_to_asm:
    /* Save callee-saved registers of 'prev' onto its kernel stack */
    pushq %rbp
    pushq %rbx
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15
    /* Save stack pointer into prev->thread.sp */
    movq %rsp, TASK_threadsp(%rdi)
    /* Load stack pointer from next->thread.sp */
    movq TASK_threadsp(%rsi), %rsp
    /* Restore callee-saved registers of 'next' from its kernel stack */
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %rbx
    popq %rbp
    ret                     /* returns to wherever 'next' was last interrupted */
```

After `switch_to()`, the CPU is now executing in the context of `next`.
The `prev` task is suspended on its kernel stack, precisely where it called
`schedule()`. When `prev` is scheduled again, it resumes from that point.

---

## 7. Signals

Signals are asynchronous notifications sent to a process (e.g., `SIGKILL`,
`SIGSEGV`, `SIGCHLD`).

### Delivery flow

```
kill(pid, SIGTERM)           [User space]
  → sys_kill()               kernel/signal.c
      → send_signal()
          → __send_signal()  Append to task->pending.list

... (later, when the target task is about to return to user space) ...

  entry_SYSCALL_64 / ret_from_intr:
    → do_signal()            arch/x86/kernel/signal.c
        → get_signal()       kernel/signal.c — dequeue one pending signal
        → handle_signal()    set up signal frame on user stack
            → setup_rt_frame()
               Writes siginfo, ucontext to user stack
               Sets PC = signal handler address
        → (returns to user space at the signal handler)

  Signal handler executes in user space
  Handler calls sigreturn()
    → sys_rt_sigreturn()     arch/x86/kernel/signal.c
        → restore_sigcontext() — restore original registers from ucontext
        → (returns to original code that was interrupted)
```

### Real-time signals

Linux supports both standard signals (1–31) and POSIX real-time signals
(32–64). RT signals are queued (not coalesced), ordered by signal number,
and can carry a `siginfo_t` payload.

---

## 8. Namespaces and PIDs

A **PID namespace** creates an isolated PID numbering space. The first process
in a namespace is PID 1. From outside, it has a different (higher) PID.

```
Host namespace:
  PID 1 = systemd
  PID 1234 = container "entrypoint" (view from host)

Inside container's PID namespace:
  PID 1 = container "entrypoint" (same task, different namespace view)
  PID 2 = container daemon
```

`task->thread_pid` points to a `struct pid` which contains one PID number per
active namespace level. `task_pid_nr_ns(task, ns)` retrieves the PID seen
from namespace `ns`.

---

## 9. CPU Affinity and NUMA Scheduling

**CPU affinity** (`task->cpus_allowed`) restricts which CPUs a task may run on.
Set by `sched_setaffinity()` ([kernel: kernel/sched/core.c]).

**NUMA** (Non-Uniform Memory Access): on multi-socket servers, memory attached
to one socket is faster for CPUs on that socket. The scheduler tries to keep
tasks and their memory on the same NUMA node (`kernel/sched/topology.c`,
`mm/mempolicy.c`).

**Load balancing:** `kernel/sched/fair.c` → `load_balance()`: periodically,
each idle CPU tries to pull tasks from overloaded CPUs (within the constraints
of CPU affinity and NUMA policy).

---

## 10. Mapping to Module 01

| Module 01 concept | Process management equivalent |
|-------------------|-------------------------------|
| `CPU` struct | `struct task_struct` + `struct thread_struct` |
| `cpu->regs[]` | `struct pt_regs` (saved on kernel stack during syscall/interrupt) |
| `cpu->state` | `task->state` (RUNNING / INTERRUPTIBLE / HALTED / FAULT) |
| `cpu_reset()` | `copy_thread()` — set up initial register state for a new task |
| `cpu_run()` | The kernel's `schedule()` loop |
| `cpu->cycle_count` | `task->utime + task->stime` — CPU time accounting |
| `MEM_STACK_TOP` | `THREAD_SIZE` kernel stack, `task->stack` pointer |
| Single-core affinity | `sched_setaffinity(0, sizeof(mask), &mask)` |
| `CPU_STATE_HALTED` | `TASK_DEAD` / `do_exit()` |
| `CPU_STATE_FAULT` | `SIGSEGV` / kernel oops / `do_exit(SIGSEGV)` |

---

## Key Source Files Summary

| File | Role |
|------|------|
| `include/linux/sched.h` | `struct task_struct` — the central data structure |
| `kernel/fork.c` | `copy_process()`, `wake_up_new_task()` |
| `kernel/exit.c` | `do_exit()`, `do_wait()`, `release_task()` |
| `kernel/sched/core.c` | `schedule()`, `context_switch()`, `try_to_wake_up()` |
| `kernel/sched/fair.c` | CFS: `enqueue_task_fair()`, `pick_next_task_fair()` |
| `kernel/sched/rt.c` | Real-time scheduler |
| `kernel/sched/idle.c` | `cpu_startup_entry()`, `do_idle()` |
| `kernel/signal.c` | `send_signal()`, `get_signal()` |
| `arch/x86/kernel/process.c` | `copy_thread()`, `__switch_to()` |
| `arch/arm/kernel/process.c` | ARM equivalent of the above |
| `arch/x86/kernel/entry_64.S` | syscall entry, register save/restore |
| `fs/exec.c` | `do_execveat_common()`, `load_elf_binary()` |
