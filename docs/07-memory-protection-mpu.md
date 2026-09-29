# Memory Protection (MPU)

**Files:** `source/kernel/mpu.c`, `include/mpu.h`, plus `tcb.c`'s
`task_set_privilege` and the calls into `mpu_switch_to_task` from
`pend_sv_handler.s`

## The three static regions

Set up once in `mpu_init()`:

| Region | Base | Size | Access | Execute |
|---|---|---|---|---|
| `MPU_REGION_FLASH` (0) | `0x08000000` | 512 KB | RO / RO | allowed |
| `MPU_REGION_HEAP` (2) | `0x20010000` | 64 KB | RW / RW | **no** |
| `MPU_REGION_KERNEL_STACK` (7) | `0x2001F000` | 4 KB | privileged RW, unprivileged **none** | **no** |

Region sizes are encoded as `2^(SIZE+1)` bytes (so `SIZE=18` → 512 KB,
`SIZE=15` → 64 KB, `SIZE=11` → 4 KB), and
`MPU->CTRL` is set with `PRIVDEFENA` on, meaning **privileged** code (the
kernel, running in Handler mode for syscalls, or the `CLI` task) still falls
back to the normal flat memory map for anything these three regions don't
cover. Unprivileged code gets no such fallback.

## The fourth region: per-task stack

`MPU_REGION_TASK_STACK` (region 1) isn't set once - it's **reconfigured on
every context switch**. `pend_sv_handler.s` calls `mpu_switch_to_task(next)`
before restoring the incoming task's registers, which calls
`mpu_configure_task_stack(task->stack_base, task->stack_size)`, pointing
region 1 at whichever task is about to run, read/write, no-execute.

This is why task stacks needed to be power-of-two sized and naturally
aligned (see [Memory Management](06-memory-management.md)) - an MPU region's
base address must be aligned to its own size.

## What this leaves uncovered

Peripheral registers (USART2, RCC, GPIO, etc.) and general SRAM outside the
heap and the current task's stack - including every kernel global (the ready
queue, `current_task`, the driver registry, `task_slots`) aren't covered by
any region. When an unprivileged task need to access anything outside its 
designated memory it need to call the respective syscall. By using this 
we can successfully separate the task memory from the kernel.

