#ifndef _SYSCALL_H
#define _SYSCALL_H

#include "stdint.h"
#include "tcb.h"

enum {
  SYS_YIELD = 0,
  SYS_SEM_WAIT,
  SYS_SEM_POST,
  SYS_TASK_CREATE,
  SYS_TASK_EXIT
};

void sys_yield(void);
tcb_t *sys_task_create(uint8_t priority, void (*func)(void *), void *args);
void sys_task_exit(void);
void svc_dispatch(uint32_t *arg1, uint32_t *arg2, uint32_t *arg3,
                  uint32_t *arg4);

#endif // !_SYSCALL_H
