# Architecture Overview

This documents presents the map of all the other docs. Each subsystem has its 
own detailed doc explaining its workings in detail. This one explaines how 
they fit together.

## Boot sequence

1. **Reset.** First the CPU loads the initial stack pointer and reset vector table 
    from the start of flash (`source/kernel/startup_code.s`), then executes 
    to `reset_handler`
2. **`reset_handler`** Copies `.data` from flash to RAM and zeroes `.bss` before 
    calling main
3. **`main()`** (`source/kernel/main.c`):
   - `clock_init()` - brings the core up to 84 MHz (see
     [Clock & Power](02-clock-and-power.md)).
   - Registers the USART2 driver and attaches it to the console, so `kprintf`
     works from this point on (see
     [Driver Framework & Console](08-driver-framework-and-console.md)).
   - `static_memory_init()`, `heap_init()`, `scheduler_init()`, `mpu_init()` -
     bring up the fixed-size task pool, the heap, the scheduler's ready queue,
     and the three initial MPU regions (see the respective docs).
   - Sets exception priorities for USART2, SVCall, SysTick and PendSV.
   - Creates three tasks - `CLI` (privileged), `IDLE`, and `user` (which calls
     into `source/user/user_main.c`) - and queues all three.
   - `systick_init()` starts the 1 ms tick, and `main()` falls into an empty
     `while(1)`, never returning: from here on, everything runs as tasks
     scheduled by the kernel.
4. **Context switch** First context switch is trigger by first SysTick 
    and runs the highest priority task from the scheduler which is `user`.

## Privilege and memory isolation

Every task by default runs **unprivileged** (Thread mode, `CONTROL.nPRIV=1`).
Unprivileged tasks are restricted in what they can read/write by the MPU. An 
unprivileged task can touch its own stack and the shared heap. Anything else 
that a task needs has to go through a **syscall**, which always runs in privileged 
Handler mode. See [Memory Protection (MPU)](07-memory-protection-mpu.md) and
[Syscalls](04-syscalls.md).

## Where things live

```
include/                       Shared headers
  arm.h, scb.h, nvic.h, rcc.h,   Peripheral register maps
  flash.h, gpio.h
  tcb.h, scheduler.h,            Kernel data structures + APIs
  semaphore.h, heap.h,
  static_memory.h, syscall.h,
  mpu.h, sleep.h, clock.h,
  systick.h
  driver.h, console.h,           Driver/console/printf layer
  kprintf.h, ring_buffer.h
  cli.h, utils.h

source/kernel/                 Kernel implementation
  startup_code.s                Vector table + reset handler
  task_stack_init.s              Builds a new task's initial stack frame
  pend_sv_handler.s              Context switch
  sv_call_handler.s              Syscall entry (SVC exception)
  handlers.s + debug_handlers.c  MemManage fault entry + decode/report
  clock.c, systick.c              Clock config, 1 ms tick
  scheduler.c, tcb.c              Ready queue, task lifecycle
  syscall.c                       Syscall dispatcher
  semaphore.c                     Semaphores + priority-inheriting mutexes
  heap.c, static_memory.c          Heap allocator, fixed task/stack pools
  sleep.c                          Tick-based sleeping
  mpu.c                            MPU region setup, per-task stack guard
  driver.c, console.c              Driver registry, console fan-out
  kprintf.c                        Kernel-side printf engine
  cli.c                           Debug shell over the console
  ring_buffer.c                   Lock-free single-producer/consumer buffer

source/drivers/                Peripheral drivers
  usart.c/.h, usart_driver.c      USART2 (interrupt-driven, ring-buffered)
  spi.c/.h                        SPI1 (blocking, software chip-select)
  w5500.c/.h                      W5500 Ethernet controller, over spi.c

source/user/                   Your code
  user_main.c                    Entry point for the "user" task
  uprintf.c/.h                    printf for unprivileged tasks

source/utils.c                  strcmp/strlen (no libc is linked)
linker_script.ld                 Flash/SRAM layout, heap region reservation
```
