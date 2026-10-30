#ifndef _SYSCALL_H
#define _SYSCALL_H

#include "../source/drivers/w5500.h"
#include "net.h"
#include "semaphore.h"
#include "stdbool.h"
#include "stdint.h"
#include "tcb.h"

enum {
  SYS_YIELD = 0,

  SYS_UPTIME,

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
  SYS_TASK_SET_NAME,

  SYS_SLEEP,

  SYS_PUTC,
  SYS_GETC,
  SYS_CONSOLE_WRITE,

  SYS_SOCKET_GET,
  SYS_SOCKET_OPEN,
  SYS_SOCKET_CLOSE,

  SYS_NET_LINK,
  SYS_NET_RECV,
  SYS_NET_SEND,
};

void sys_yield(void);

uint32_t sys_uptime(void);

uintptr_t sys_malloc(uint32_t size);
uintptr_t sys_calloc(uint32_t count, uint32_t size);
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
void sys_task_set_name(tcb_t *task, const char *name);

void sys_sleep(uint32_t ticks);

void sys_putc(char c);
bool sys_getc(char *c);
int sys_console_write(const char *buf, uint32_t len);

socket_t *sys_socket_get(uint8_t n);
socket_t *sys_socket_open(socket_t *socket, net_reply_t *reply_slot);
void sys_socket_close(socket_t *socket, net_reply_t *reply_slot);
void sys_net_recv(socket_t *socket, uint8_t *buffer, uint16_t buf_len,
                  net_reply_t *reply_slot);
typedef struct {
  uint8_t *tx_data;
  uint16_t tx_length;
  uint8_t dst_ip[4];
  uint16_t dst_port;
  bool use_mac;
  uint8_t mac[6];
} net_send_args_t;
void sys_net_send(socket_t *socket, net_send_args_t *args,
                  net_reply_t *reply_slot);
void sys_net_link(int32_t *up, net_reply_t *reply_slot);

void svc_dispatch(uintptr_t *frame);

#endif // !_SYSCALL_H
