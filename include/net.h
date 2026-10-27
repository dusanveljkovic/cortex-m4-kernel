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
  NET_OP_SOCKET_CLOSE,
  NET_OP_RECV,
  NET_OP_SEND
} net_op_t;

typedef enum {
  NET_STATUS_OK,
  NET_STATUS_FAULT,
  NET_STATUS_RETRY,
  NET_STATUS_BAD_REQUEST,
  NET_STATUS_UNREACHABLE,
  NET_STATUS_DEAD
} net_status_t;

typedef struct {
  uint16_t got;
  uint8_t src_ip[4];
  uint16_t port;
  bool truncated;

  net_status_t status;
} net_reply_t;

typedef struct {
  net_op_t op;
  volatile tcb_t *caller;
  socket_t *socket;
  int32_t *result;
  net_reply_t *reply_slot;

  uint8_t *buffer;
  uint16_t buf_len;

  uint8_t dst_ip[4];
  uint16_t dst_port;
} net_request_t;

void net_send_request(net_request_t *req);
bool net_will_wait(void);

void net_task(void *_arg);

#endif
