#include "../include/syscall.h"
#include "../include/heap.h"
#include "../include/scb.h"
#include "../include/scheduler.h"
#include "../include/semaphore.h"
#include "../include/static_memory.h"
#include "../include/tcb.h"
#include "stdint.h"

void *syscall(uint32_t number, void *arg2, void *arg3, void *arg4) {
  register uint32_t r0 asm("r0") = number;
  asm volatile("svc #0" ::"r"(r0) : "memory");
  return (void *)r0;
}

inline void sys_yield(void) { syscall(SYS_YIELD, 0, 0, 0); }

inline void *sys_malloc(uint32_t size) {
  return syscall(SYS_MALLOC, (void *)size, 0, 0);
}
inline void *sys_calloc(uint32_t count, uint32_t size) {
  return syscall(SYS_CALLOC, (void *)count, (void *)size, 0);
}
inline void sys_free(void *ptr) { syscall(SYS_FREE, ptr, 0, 0); }

inline tcb_t *sys_task_create(uint8_t priority, void (*func)(void *),
                              void *args) {
  return (tcb_t *)syscall(SYS_TASK_CREATE, (void *)priority, func, args);
}

inline semaphore_t *sys_sem_create(uint32_t count) {
  return (semaphore_t *)syscall(SYS_SEM_CREATE, (void *)count, 0, 0);
}

inline void sys_sem_wait(semaphore_t *sem) { syscall(SYS_SEM_WAIT, sem, 0, 0); }
inline void sys_sem_post(semaphore_t *sem) { syscall(SYS_SEM_POST, sem, 0, 0); }

inline mutex_t *sys_mutex_create() {
  return (mutex_t *)syscall(SYS_MUTEX_CREATE, 0, 0, 0);
}
inline void sys_mutex_lock(mutex_t *m) { syscall(SYS_MUTEX_LOCK, m, 0, 0); }
inline void sys_mutex_unlock(mutex_t *m) { syscall(SYS_MUTEX_UNLOCK, m, 0, 0); }

inline void sys_task_exit(void) { syscall(SYS_TASK_EXIT, 0, 0, 0); }

void svc_dispatch(uint32_t *arg1, uint32_t *arg2, uint32_t *arg3,
                  uint32_t *arg4) {
  uint32_t syscall_number = *arg1;
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
    kfree(arg2);
    break;
  }
  case SYS_SEM_CREATE: {
    uint32_t count = (uint32_t)arg2;
    ret = (uint32_t *)alloc_semaphore();
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
    break;
  }

  case SYS_MUTEX_CREATE: {
    ret = (uint32_t *)alloc_mutex();
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
    void *args = arg4;
    ret = (uint32_t *)create_task(priority, func, args);
    break;
  }
  case SYS_TASK_EXIT:
    current_task->state = TASK_FINISHED;
    should_yield = 1;
    break;
  }

  if (should_yield == 1) {
    scheduler_put_task(current_task);
    SCB->ICSR |= ICSR_PENDSV_SET;
    asm volatile("dsb");
    asm volatile("isb");
  }

  *arg1 = (uint32_t)ret;

  return;
}
