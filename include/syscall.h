#ifndef _SYSCALL_H
#define _SYSCALL_H

#include "semaphore.h"
#include "stdbool.h"
#include "stdint.h"
#include "tcb.h"

enum {
  SYS_YIELD = 0,

  SYS_MALLOC,
  SYS_CALLOC,
  SYS_FREE,

  SYS_SEM_CREATE,
  SYS_SEM_WAIT,
  SYS_SEM_POST,
  SYS_SEM_CLOSE,

  SYS_MUTEX_CREATE,
  SYS_MUTEX_LOCK,
  SYS_MUTEX_UNLOCK,

  SYS_TASK_CREATE,
  SYS_TASK_EXIT,

  SYS_PUTC,
  SYS_GETC,
};

void sys_yield(void);

void *sys_malloc(uint32_t size);
void *sys_calloc(uint32_t count, uint32_t size);
void sys_free(void *ptr);

semaphore_t *sys_sem_create(uint32_t count);
void sys_sem_wait(semaphore_t *sem);
void sys_sem_post(semaphore_t *sem);
void sys_sem_close(semaphore_t *sem);

mutex_t *sys_mutex_create();
void sys_mutex_lock(mutex_t *m);
void sys_mutex_unlock(mutex_t *m);
void sys_mutex_close(mutex_t *m);

tcb_t *sys_task_create(uint8_t priority, void (*func)(void *), void *args);
void sys_task_exit(void);

void sys_putc(char c);
bool sys_getc(char *c);

void svc_dispatch(uintptr_t arg1, uintptr_t arg2, uintptr_t arg3,
                  uintptr_t arg4);

#endif // !_SYSCALL_H
