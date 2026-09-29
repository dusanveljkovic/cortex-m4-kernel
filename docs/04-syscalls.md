# Syscalls

**Files:** `source/kernel/syscall.c`, `source/kernel/sv_call_handler.s`,
`include/syscall.h`

## Why syscalls exist here

Every task runs unprivileged by default (see
[Memory Protection (MPU)](07-memory-protection-mpu.md)), and the MPU only
grants unprivileged code access to flash, the heap, and its own stack.
Anything else - touching kernel data structures (the ready queue, a
semaphore's wait queue), talking to hardware, or creating/exiting a task —
has to go through a syscall, which executes in **privileged Handler mode**
regardless of who called it.

## The call path

A syscall wrapper (e.g. `sys_yield()`) loads a syscall number into `r0` and
executes `svc #0`:

```c
void *syscall(uint32_t number, uintptr_t arg2, uintptr_t arg3, uintptr_t arg4) {
  register uint32_t r0 asm("r0") = number;
  asm volatile("svc #0" ::"r"(r0) : "memory");
  return (void *)r0;
}
```

`sv_call_handler.s` is the actual SVC exception entry. It determines whether
the SVC was taken from the main stack or the process stack (`tst lr, #4`,
same technique as the MemManage handler), retrieves the stacked arguments,
and calls `svc_dispatch()` - the single C function that handles every syscall
number via a big `switch`.

## Available syscalls

| Syscall | Wrapper | What it does |
|---|---|---|
| `SYS_YIELD` | `sys_yield()` | Re-queues the current task and forces a reschedule |
| `SYS_MALLOC` / `SYS_CALLOC` / `SYS_FREE` | `sys_malloc` / `sys_calloc` / `sys_free` | Passes straight through to the heap allocator |
| `SYS_TASK_CREATE` | `sys_task_create(priority, func, args)` | Allocates a TCB and queues it (only if allocation succeeded) |
| `SYS_TASK_EXIT` | `sys_task_exit()` | Marks the current task `TASK_FINISHED` and yields |
| `SYS_TASK_SET_NAME` | `sys_task_set_name(task, name)` | Sets `task->name`, used by `ps` |
| `SYS_SEM_CREATE` / `WAIT` / `POST` / `CLOSE` | `sys_sem_*` | Heap-allocates/operates a semaphore; see [Synchronization](05-synchronization.md) |
| `SYS_MUTEX_CREATE` / `LOCK` / `UNLOCK` | `sys_mutex_*` | Same, for priority-inheriting mutexes |
| `SYS_SLEEP` | `sys_sleep(ticks)` | Calls `sleep_for_ticks()`, always yields |
| `SYS_PUTC` / `SYS_GETC` | `sys_putc(c)` / `sys_getc(&c)` | One character to/from the console |
| `SYS_CONSOLE_WRITE` | `sys_console_write(buf, len)` | A whole buffer to the console in one call — what `uprintf` uses |

`svc_dispatch` sets a local `should_yield` flag rather than yielding
immediately inside each case; every path that can wake a higher-priority task
(a semaphore post, a mutex unlock, creating a task, sleeping) sets it, and a
single block at the end of the function re-queues `current_task` and pends
`PendSV` if it's set 

