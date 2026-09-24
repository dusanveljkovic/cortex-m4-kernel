#include "../../include/syscall.h"
#include "../../include/heap.h"
#include "../../include/scb.h"
#include "../../include/scheduler.h"
#include "../../include/semaphore.h"
#include "../../include/sleep.h"
#include "../../include/static_memory.h"
#include "../../include/tcb.h"
#include "../../include/usart.h"
#include "stdint.h"
#include <stdint.h>

void *syscall(uint32_t number, uintptr_t arg2, uintptr_t arg3, uintptr_t arg4) {
  register uint32_t r0 asm("r0") = number;
  asm volatile("svc #0" ::"r"(r0) : "memory");
  return (void *)r0;
}

inline void sys_yield(void) { syscall(SYS_YIELD, 0, 0, 0); }

inline void *sys_malloc(uint32_t size) {
  return syscall(SYS_MALLOC, (uintptr_t)size, 0, 0);
}
inline void *sys_calloc(uint32_t count, uint32_t size) {
  return syscall(SYS_CALLOC, (uintptr_t)count, (uintptr_t)size, 0);
}
inline void sys_free(void *ptr) { syscall(SYS_FREE, (uintptr_t)ptr, 0, 0); }

inline tcb_t *sys_task_create(uint8_t priority, void (*func)(void *),
                              void *args) {
  return (tcb_t *)syscall(SYS_TASK_CREATE, (uintptr_t)priority, (uintptr_t)func,
                          (uintptr_t)args);
}

inline semaphore_t *sys_sem_create(uint32_t count) {
  return (semaphore_t *)syscall(SYS_SEM_CREATE, (uintptr_t)count, 0, 0);
}

inline void sys_sem_wait(semaphore_t *sem) {
  syscall(SYS_SEM_WAIT, (uintptr_t)sem, 0, 0);
}
inline void sys_sem_post(semaphore_t *sem) {
  syscall(SYS_SEM_POST, (uintptr_t)sem, 0, 0);
}

inline mutex_t *sys_mutex_create() {
  return (mutex_t *)syscall(SYS_MUTEX_CREATE, 0, 0, 0);
}
inline void sys_mutex_lock(mutex_t *m) {
  syscall(SYS_MUTEX_LOCK, (uintptr_t)m, 0, 0);
}
inline void sys_mutex_unlock(mutex_t *m) {
  syscall(SYS_MUTEX_UNLOCK, (uintptr_t)m, 0, 0);
}

inline void sys_task_exit(void) { syscall(SYS_TASK_EXIT, 0, 0, 0); }

inline void sys_sleep(uint32_t ticks) { syscall(SYS_SLEEP, ticks, 0, 0); }

inline void sys_putc(char c) { syscall(SYS_PUTC, (uintptr_t)c, 0, 0); }
inline bool sys_getc(char *c) { syscall(SYS_GETC, (uintptr_t)c, 0, 0); }

void svc_dispatch(uintptr_t arg1, uintptr_t arg2, uintptr_t arg3,
                  uintptr_t arg4) {
  uint32_t syscall_number = *(uint32_t *)arg1;
  uint8_t should_yield = 0;
  uint32_t *ret = 0;

  switch (syscall_number) {
  case SYS_YIELD:
    should_yield = 1;
    break;
  case SYS_MALLOC: {
    ret = kmalloc((uint32_t)arg2);
    break;
  }
  case SYS_CALLOC: {
    ret = kcalloc((uint32_t)arg2, (uint32_t)arg3);
    break;
  }
  case SYS_FREE: {
    kfree((void *)arg2);
    break;
  }
  case SYS_SEM_CREATE: {
    uint32_t count = (uint32_t)arg2;
    ret = (uint32_t *)kmalloc(sizeof(semaphore_t));
    semaphore_init((semaphore_t *)ret, count);
    break;
  }
  case SYS_SEM_WAIT: {
    semaphore_t *sem = (semaphore_t *)arg2;
    sem_result_t result = semaphore_wait(sem);
    if (result == SEM_OK_YIELD)
      should_yield = 1;
    ret = (uint32_t *)result;
    break;
  }
  case SYS_SEM_POST: {
    semaphore_t *sem = (semaphore_t *)arg2;
    sem_result_t result = semaphore_post(sem);
    if (result == SEM_OK_YIELD)
      should_yield = 1;
    ret = (uint32_t *)result;
    break;
  }
  case SYS_SEM_CLOSE: {
    semaphore_t *sem = (semaphore_t *)arg2;
    semaphore_close(sem);
    kfree(sem);
    break;
  }

  case SYS_MUTEX_CREATE: {
    ret = (uint32_t *)kmalloc(sizeof(mutex_t));
    mutex_init((mutex_t *)ret);
    break;
  }
  case SYS_MUTEX_LOCK: {
    mutex_t *m = (mutex_t *)arg2;
    sem_result_t result = mutex_lock(m);
    if (result == SEM_OK_YIELD)
      should_yield = 1;
    ret = (uint32_t *)result;
    break;
  }
  case SYS_MUTEX_UNLOCK: {
    mutex_t *m = (mutex_t *)arg2;
    sem_result_t result = mutex_unlock(m);
    if (result == SEM_OK_YIELD)
      should_yield = 1;
    ret = (uint32_t *)result;
    break;
  }
  case SYS_TASK_CREATE: {
    uint8_t priority = (uint8_t)arg2;
    void (*func)(void *) = (void (*)(void *))arg3;
    void *args = (void *)arg4;
    ret = (uint32_t *)create_task(priority, func, args);
    if (ret != 0)
      scheduler_put_task((tcb_t *)ret);
    break;
  }
  case SYS_TASK_EXIT:
    current_task->state = TASK_FINISHED;
    should_yield = 1;
    break;
  case SYS_SLEEP:
    sleep_for_ticks((uint32_t)arg2);
    should_yield = 1;
    break;

  case SYS_PUTC: {
    usart2_putc((char)arg2);
    break;
  }
  case SYS_GETC: {
    ret = (uint32_t *)usart2_getc((char *)arg2);
    break;
  }
  }

  if (should_yield == 1) {
    scheduler_put_task(current_task);
    SCB->ICSR |= ICSR_PENDSV_SET;
    asm volatile("dsb");
    asm volatile("isb");
  }

  *(uint32_t *)arg1 = (uint32_t)ret;

  return;
}
