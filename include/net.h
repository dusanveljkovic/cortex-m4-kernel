#ifndef _NET_H
#define _NET_H

#include "../source/drivers/w5500.h"
#include "semaphore.h"
#include "tcb.h"
#include <stdbool.h>
#include <stdint.h>

typedef enum {
  NET_OP_LINK,
  NET_OP_SOCKET_OPEN,
  NET_OP_RECV,
  NET_OP_SEND
} net_op_t;

typedef struct {
  net_op_t op;
  volatile tcb_t *caller;
  socket_t *socket;
  int32_t *result;
} net_request_t;

void net_send_request(net_request_t *req);
bool net_will_wait(void);

void net_task(void *_arg);

#endif
