#include "../../include/syscall.h"
#include "../../include/arm.h"
#include "../../include/console.h"
#include "../../include/heap.h"
#include "../../include/net.h"
#include "../../include/scheduler.h"
#include "../../include/semaphore.h"
#include "../../include/sleep.h"
#include "../../include/static_memory.h"
#include "../../include/systick.h"
#include "../../include/tcb.h"
#include "../../include/utils.h"
#include "../drivers/w5500.h"
#include <stdint.h>

static uintptr_t syscall(uint32_t number, uintptr_t a0, uintptr_t a1,
                         uintptr_t a2, uintptr_t a3) {
  register uint32_t r12 asm("r12") = number;
  register uint32_t r0 asm("r0") = a0;
  register uint32_t r1 asm("r1") = a1;
  register uint32_t r2 asm("r2") = a2;
  register uint32_t r3 asm("r3") = a3;
  asm volatile("svc #0"
               : "+r"(r0)
               : "r"(r12), "r"(r1), "r"(r2), "r"(r3)
               : "memory");
  return r0;
}

inline void sys_yield(void) { syscall(SYS_YIELD, 0, 0, 0, 0); }

inline uint32_t sys_uptime(void) {
  return (uint32_t)syscall(SYS_UPTIME, 0, 0, 0, 0);
};

inline uintptr_t sys_malloc(uint32_t size) {
  return syscall(SYS_MALLOC, (uintptr_t)size, 0, 0, 0);
}
inline uintptr_t sys_calloc(uint32_t count, uint32_t size) {
  return syscall(SYS_CALLOC, (uintptr_t)count, (uintptr_t)size, 0, 0);
}
inline void sys_free(void *ptr) { syscall(SYS_FREE, (uintptr_t)ptr, 0, 0, 0); }

inline tcb_t *sys_task_create(uint8_t priority, void (*func)(void *),
                              void *args) {
  return (tcb_t *)syscall(SYS_TASK_CREATE, (uintptr_t)priority, (uintptr_t)func,
                          (uintptr_t)args, 0);
}

inline semaphore_t *sys_sem_create(uint32_t count) {
  return (semaphore_t *)syscall(SYS_SEM_CREATE, (uintptr_t)count, 0, 0, 0);
}

inline void sys_sem_wait(semaphore_t *sem) {
  syscall(SYS_SEM_WAIT, (uintptr_t)sem, 0, 0, 0);
}
inline void sys_sem_post(semaphore_t *sem) {
  syscall(SYS_SEM_POST, (uintptr_t)sem, 0, 0, 0);
}

inline mutex_t *sys_mutex_create() {
  return (mutex_t *)syscall(SYS_MUTEX_CREATE, 0, 0, 0, 0);
}
inline void sys_mutex_lock(mutex_t *m) {
  syscall(SYS_MUTEX_LOCK, (uintptr_t)m, 0, 0, 0);
}
inline void sys_mutex_unlock(mutex_t *m) {
  syscall(SYS_MUTEX_UNLOCK, (uintptr_t)m, 0, 0, 0);
}

inline void sys_task_exit(void) { syscall(SYS_TASK_EXIT, 0, 0, 0, 0); }
inline void sys_task_set_name(tcb_t *task, const char *name) {
  syscall(SYS_TASK_SET_NAME, (uintptr_t)task, (uintptr_t)name, 0, 0);
}

inline void sys_sleep(uint32_t ticks) { syscall(SYS_SLEEP, ticks, 0, 0, 0); }

inline void sys_putc(char c) { syscall(SYS_PUTC, (uintptr_t)c, 0, 0, 0); }
inline bool sys_getc(char *c) {
  return syscall(SYS_GETC, (uintptr_t)c, 0, 0, 0);
}
inline int sys_console_write(const char *buf, uint32_t len) {
  return (int)(uintptr_t)syscall(SYS_CONSOLE_WRITE, (uintptr_t)buf,
                                 (uintptr_t)len, 0, 0);
}

inline void sys_net_link(int32_t *up, net_reply_t *reply_slot) {
  syscall(SYS_NET_LINK, (uintptr_t)up, (uintptr_t)reply_slot, 0, 0);
}

inline socket_t *sys_socket_get(uint8_t n) {
  return (socket_t *)syscall(SYS_SOCKET_GET, n, 0, 0, 0);
}
inline socket_t *sys_socket_open(socket_t *socket, net_reply_t *reply_slot) {
  return (socket_t *)syscall(SYS_SOCKET_OPEN, (uintptr_t)socket,
                             (uintptr_t)reply_slot, 0, 0);
}
inline void sys_socket_close(socket_t *socket, net_reply_t *reply_slot) {
  syscall(SYS_SOCKET_CLOSE, (uintptr_t)socket, (uintptr_t)reply_slot, 0, 0);
}
inline void sys_net_recv(socket_t *socket, uint8_t *buffer, uint16_t buf_len,
                         net_reply_t *reply_slot) {
  syscall(SYS_NET_RECV, (uintptr_t)socket, (uintptr_t)buffer,
          (uintptr_t)buf_len, (uintptr_t)reply_slot);
}
inline void sys_net_send(socket_t *socket, net_send_args_t *args,
                         net_reply_t *reply_slot) {
  syscall(SYS_NET_SEND, (uintptr_t)socket, (uintptr_t)args,
          (uintptr_t)reply_slot, 0);
}

extern uint8_t _heap_start;
extern uint8_t _heap_end;

bool ptr_on_heap(uintptr_t ptr, uint32_t len, bool write) {
  if (ptr == 0 || ptr < (uintptr_t)&_heap_start ||
      (ptr + len) >= (uintptr_t)&_heap_end)
    return false;
  // overflow
  if (ptr + len < ptr)
    return false;
  return true;
}

bool ptr_on_stack(uintptr_t ptr, uint32_t len, bool write,
                  volatile tcb_t *task) {
  if (ptr == 0 || ptr < (uintptr_t)task->stack_base ||
      (ptr + len) >= (uintptr_t)(task->stack_base + task->stack_size))
    return false;
  // overflow
  if (ptr + len < ptr)
    return false;
  return true;
}

// frame contains pointer to start of whole hardware exception frame
// frame[0] = r0 [1]=r1 [2]=r2 [3]=r3 [4]=r4 [5]=lr [6]=pc [7]xPSR
void svc_dispatch(uintptr_t *frame) {
  uintptr_t arg0 = *frame;
  uintptr_t arg1 = *(frame + 1);
  uintptr_t arg2 = *(frame + 2);
  uintptr_t arg3 = *(frame + 3);
  uint32_t syscall_number = *(uint32_t *)(frame + 4);

  uint8_t should_yield = 0;
  uintptr_t ret = 0;

  switch (syscall_number) {
  case SYS_YIELD:
    should_yield = 1;
    break;
  case SYS_UPTIME:
    ret = system_ticks;
    break;
  case SYS_MALLOC: {
    ret = (uintptr_t)kmalloc((uint32_t)arg0);
    break;
  }
  case SYS_CALLOC: {
    ret = (uintptr_t)kcalloc((uint32_t)arg0, (uint32_t)arg1);
    break;
  }
  case SYS_FREE: {
    if (ptr_on_heap(arg0, 0, 0))
      kfree((void *)arg0);
    break;
  }
  case SYS_SEM_CREATE: {
    uint32_t count = (uint32_t)arg0;
    ret = (uintptr_t)alloc_sem();
    if (ret != 0)
      semaphore_init((semaphore_t *)ret, count);
    break;
  }
  case SYS_SEM_WAIT: {
    if (!validate_sem(arg0))
      break;
    semaphore_t *sem = (semaphore_t *)arg0;
    sem_result_t result = semaphore_wait(sem);
    if (result == SEM_OK_YIELD)
      should_yield = 1;
    ret = (uintptr_t)result;
    break;
  }
  case SYS_SEM_POST: {
    if (!validate_sem(arg0))
      break;
    semaphore_t *sem = (semaphore_t *)arg0;
    sem_result_t result = semaphore_post(sem);
    if (result == SEM_OK_YIELD)
      should_yield = 1;
    ret = (uintptr_t)result;
    break;
  }
  case SYS_SEM_CLOSE: {
    if (!validate_sem(arg0))
      break;
    semaphore_t *sem = (semaphore_t *)arg0;
    semaphore_close(sem);
    free_sem(sem);
    break;
  }

  case SYS_MUTEX_CREATE: {
    ret = (uintptr_t)kmalloc(sizeof(mutex_t));
    if (ret != 0)
      mutex_init((mutex_t *)ret);
    break;
  }
  case SYS_MUTEX_LOCK: {
    if (!validate_mutex(arg0))
      break;
    mutex_t *m = (mutex_t *)arg0;
    sem_result_t result = mutex_lock(m);
    if (result == SEM_OK_YIELD)
      should_yield = 1;
    ret = (uintptr_t)result;
    break;
  }
  case SYS_MUTEX_UNLOCK: {
    if (!validate_mutex(arg0))
      break;
    mutex_t *m = (mutex_t *)arg0;
    sem_result_t result = mutex_unlock(m);
    if (result == SEM_OK_YIELD)
      should_yield = 1;
    ret = (uintptr_t)result;
    break;
  }
  case SYS_TASK_CREATE: {
    uint8_t priority = (uint8_t)arg0;
    void (*func)(void *) = (void (*)(void *))arg1;
    void *args = (void *)arg2;
    ret = (uintptr_t)create_task(priority, func, args);
    if (ret != 0)
      scheduler_put_task((tcb_t *)ret);
    break;
  }
  case SYS_TASK_EXIT:
    current_task->state = TASK_FINISHED;
    mutex_unlock_all_owned(current_task->owned_mutexes);
    should_yield = 1;
    break;
  case SYS_TASK_SET_NAME: {
    if (!validate_task(arg0))
      break;
    tcb_t *task = (tcb_t *)arg0;
    if (task != 0)
      task->name = (char *)arg1;
    break;
  }
  case SYS_SLEEP:
    sleep_for_ticks((uint32_t)arg0);
    should_yield = 1;
    break;

  case SYS_PUTC: {
    char c = (char)arg0;
    console_write(&c, 1);
    break;
  }
  case SYS_GETC: {
    ret = (uintptr_t)(console_read((uint8_t *)arg0, 1));
    break;
  }
  case SYS_CONSOLE_WRITE: {
    uint32_t len = (uint32_t)arg1;
    if (!ptr_on_heap(arg0, len, false) &&
        !ptr_on_stack(arg0, len, false, current_task))
      break;
    console_write((const char *)arg0, len);
    ret = (uintptr_t)len;
    break;
  }

  case SYS_SOCKET_GET: {
    socket_t *socket = get_socket((uint8_t)arg0);
    ret = (uintptr_t)socket;
    break;
  }
  case SYS_SOCKET_OPEN: {
    if (!validate_socket(arg0))
      break;
    socket_t *socket = (socket_t *)arg0;
    net_request_t req = {.op = NET_OP_SOCKET_OPEN,
                         .caller = current_task,
                         .socket = socket,
                         .result = 0,
                         .dst_ip = 0,
                         .dst_port = 0,
                         .reply_slot = (net_reply_t *)arg1};
    net_send_request(&req);
    if (net_will_wait())
      should_yield = 1;
    break;
  }
  case SYS_SOCKET_CLOSE: {
    if (!validate_socket(arg0))
      break;
    socket_t *socket = (socket_t *)arg0;
    net_request_t req = {.op = NET_OP_SOCKET_CLOSE,
                         .caller = current_task,
                         .socket = socket,
                         .dst_ip = 0,
                         .dst_port = 0,
                         .reply_slot = (net_reply_t *)arg1};
    net_send_request(&req);
    if (net_will_wait())
      should_yield = 1;
    break;
  }
  case SYS_NET_LINK: {
    net_request_t req = {.op = NET_OP_LINK,
                         .caller = current_task,
                         .socket = 0,
                         .dst_ip = 0,
                         .dst_port = 0,
                         .result = (int32_t *)arg0,
                         .reply_slot = (net_reply_t *)arg1};
    net_send_request(&req);
    if (net_will_wait())
      should_yield = 1;
    break;
  }
  case SYS_NET_RECV: {
    if (!validate_socket(arg0))
      break;
    socket_t *socket = (socket_t *)arg0;
    net_request_t req = {.op = NET_OP_RECV,
                         .caller = current_task,
                         .socket = socket,
                         .buffer = (uint8_t *)arg1,
                         .buf_len = (uint16_t)arg2,
                         .dst_ip = 0,
                         .dst_port = 0,
                         .reply_slot = (net_reply_t *)arg3};
    net_send_request(&req);
    if (net_will_wait())
      should_yield = 1;
    break;
  }
  case SYS_NET_SEND: {
    if (!validate_socket(arg0))
      break;
    socket_t *socket = (socket_t *)arg0;
    net_send_args_t *send_args = (net_send_args_t *)arg1;
    net_request_t req = {.op = NET_OP_SEND,
                         .caller = current_task,
                         .socket = socket,
                         .buffer = send_args->tx_data,
                         .buf_len = send_args->tx_length,
                         .dst_port = send_args->dst_port,
                         .use_mac = send_args->use_mac,
                         .reply_slot = (net_reply_t *)arg2};
    memcpy(req.dst_mac, send_args->mac, 6);
    for (int i = 0; i < 4; i++) {
      req.dst_ip[i] = send_args->dst_ip[i];
    }
    net_send_request(&req);
    if (net_will_wait())
      should_yield = 1;
    break;
  }
  }

  if (should_yield == 1) {
    request_context_switch();
  }

  *frame = ret;

  return;
}
