# Design decisions

Every mechanism stated in this document had at least two other design alternatives. This page records my choises and explains them.

| # | Decision | Main cost |
|---|---|---|
| 1 | No vendor HAL, no CMSIS, no libc | slower bring-up; `memcpy`, `crc32` and `kvsnprintf` are mine |
| 2 | Static task and socket pools | a hard limit of 8 tasks; `create_task` can fail |
| 3 | Unprivileged by default, privilege opt-in | every hardware access becomes a syscall |
| 4 | Four MPU regions plus one dynamic task region | task stacks must be powers of two and aligned |
| 5 | Exception order: USART2, SVCall, SysTick, PendSV | syscall latency is bounded below by the UART handler |
| 6 | 84 MHz rather than 180 MHz | half the peak performance is unused |
| 7 | Fixed-priority preemptive, never EDF | a task cannot borrow priority from its deadline |
| 8 | Priority inheritance rather than priority ceiling | no deadlock prevention, only inversion bounding |
| 9 | Networking as one privileged service task | all network I/O is serialised |
| 10 | The FPU frame is saved only when the FPU was used | an extra branch in the switch path |
| 11 | First-fit heap with immediate coalescing | worst-case allocation time is unbounded in the number of blocks |

## 1. No vendor HAL, no CMSIS, no libc

Peripheral registers are structs over fixed addresses, as in
`SPI1 ((volatile SPI_RegisterMap *)0x40013000)`. Bit fields are `#define`s. The runtime
routines the compiler would otherwise emit live in `source/utils.c` and
`source/kernel/kprintf.c`.

The alternatives were CMSIS with the ST LL drivers, which is about 100 KB of source you did
not write, and FreeRTOS with the HAL, which gives a working system in a day and nothing to
explain.
The alternatives were CMSIS, which abstracts a lot of the CPU internals and FreeRTOS with HAL, which gives a working system but doesn't teach or explain anything.

The main point was to understand the machine, but a vendor library skips that process.

## 2. Static task and socket pools

`N_TASKS` control blocks and their stacks are static arrays, and `alloc_task()` scans for an
unused slot.

The alternative was to allocate control blocks and stacks from the heap on `create_task`,
which allows dynamic lifetimes and arbitrary stack sizes.

The scheduler therefore never allocates, so scheduling cannot fail at run time. Task
creation can fail, and returns `NULL`. Stack sizes are uniform, which wastes memory on small
tasks and bounds what a task can do. Tasks must be uniform because of the [MPU](06-memory-management.md).

## 3. Unprivileged tasks by default

Tasks are unprivileged by default because it creates a clear boundary between code that can touch system resources and the code which can not.

The alternative was to run everything privileged and rely on discipline

This choice forces the syscall surface to be complete enough to build a DHCP server on, and
it forces every pointer crossing it to be validated. The CLI being privileged is a
deliberate choice, becuse a debugger need to see inside the system to be able to report its state.

## 4. Four static MPU regions plus one that changes on every switch

| Region | Content | Access for unprivileged code |
|---|---|---|
| 0 | flash `0x08000000` + 512K | read, execute |
| 1 | the running task's stack | read, write, no execute |
| 2 | heap `0x20010000` + 64K | read, write, no execute |
| 7 | kernel stack `0x2001F000` + 4K | no access |

`PRIVDEFENA` is set, so privileged code falls back to the flat memory map for anything
these regions do not cover, which includes peripherals and kernel globals, without spending
a region on them. Region 1 is re-pointed in `pend_sv_handler.s` on every context switch,
through `mpu_switch_to_task(next)`.

The alternative was static per-task regions. The Cortex-M4 has 8 regions, three are already
used, and that would cap the system at five tasks. The other alternative was no MPU.

Task stacks must be exactly a power of two and naturally aligned, which is why
`TASK_STACK_SIZE` is 2,048 and `task_stacks` is aligned to it. The switch path carries one
more register write per switch. In exchange, the number of regions does not depend on the
number of tasks.

## 5. Exception order: USART2, SVCall, SysTick, PendSV

The priorities are `0xC0`, `0xD0`, `0xE0` and `0xF0`, and lower numbers are higher
priority.

A byte arriving on the UART is the only event that is lost if it is late, because the
receive register holds exactly one byte. A syscall must not be preempted by the tick,
because the dispatcher may be manipulating the queue the scheduler would touch. PendSV is
last so a switch can never happen in the middle of another exception.

The consequence is that syscall latency has a lower bound equal to the UART handler's
duration. That handler is small, a ring-buffer push and a semaphore post, but it is not
zero.

## 6. 84 MHz rather than 180 MHz

HSI at 16 MHz feeds a PLL with M=8, N=84 and P=2, giving 84 MHz. APB1 divides by 2, APB2
by 1, and flash runs with 2 wait states and the instruction and data caches enabled.

The alternative was 180 MHz with over-drive and voltage scale 1. On the F446 that requires
`PWR` voltage scaling, over-drive mode and keep-alive handling in the PLL configuration,
which is three subsystems' worth of failure modes in exchange for twice the core clock.

## 7. Fixed-priority preemptive scheduling

One priority byte per task, where a lower number means a higher priority. Ready queue for tasks is priority-ordered, FIFO among equal priorities, and preemption on the tick and on wake-up.

The alternatives were round-robin, which gives no timing guarantees and EDF, which needs deadlines known at run time and allows priority to change per job.

## 8. Priority inheritance rather than priority ceiling

On a contended `mutex_lock` the holder is boosted to the waiter's effective priority, and
the boost propagates along a chain of waiters. On unlock the owner's priority is recomputed
from the mutexes it still holds. 

The alternatives were priority ceiling, where each mutex carries the highest priority that
can ever lock it and both inversion and deadlock are bounded, and doing nothing.

The system prevents a task from locking the same mutex twice, but relies on the programmer for cycles across two mutexes. 

## 9. Networking as a service task

One privileged task owns the W5500. Clients build a `net_request_t`, post a semaphore and
block in a syscall until the reply slot is filled. Details are in
[Network stack](12-network-stack.md).

The alternatives were a mutex around a W5500 library, which makes every task a driver
author and deadlocks when a blocking wait happens inside a critical section, and
interrupt-driven socket handling, which the chip does not offer.

The price of this decision is serialisation: one client stuck in a timeout can stall other clients.

## 10. FPU state is saved only when it was used

The context switch checks `EXC_RETURN[4]` and saves `s16` to `s31` only for tasks that have
used the FPU, on top of the hardware's lazy stacking of `s0` to `s15` and `FPSCR`.

A full FPU frame is 26 words, or 104 bytes, per switch. Most tasks never touch the FPU, and
the hardware already reserves the caller-saved half without writing it, through lazy
stacking with `LSPEN` and `ASPEN`.

The alternative was to always stack FPU registers, which would cost time but would not cost space because the space is already allocated.

## 11. First-fit heap with immediate coalescing

One doubly linked block list, where `kmalloc` takes the first block that fits and splits it and 
`kfree` coalesces with both neighbours. Every entry point runs under `irq_save`.

The alternatives were a buddy allocator, which bounds fragmentation and wastes memory on
powers of two, and TLSF, which bounds allocation time at the cost of more code.

