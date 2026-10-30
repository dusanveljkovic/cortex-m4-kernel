#include "../../include/kernel/net.h"
#include "../../include/kernel/arm.h"
#include "../../include/kernel/kprintf.h"
#include "../../include/kernel/static_memory.h"
#include "../../include/kernel/syscall.h"
#include <stdbool.h>

static net_request_t current_req = {0, 0, 0, 0, 0, 0, 0};
static semaphore_t *net_posted, *net_done;

void net_send_request(net_request_t *req) {
  current_req = *req;
  semaphore_post(net_posted);
}

bool net_will_wait(void) { return (semaphore_wait(net_done) == SEM_OK_YIELD); }

void net_task(void *_a) {
  net_posted = sys_sem_create(0);
  net_done = sys_sem_create(0);

  const uint8_t mac[6] = {0xEE, 0xFF, 0xEE, 0xFF, 0xEE, 0xFF};
  const uint8_t ip[4] = {192, 168, 0, 105};
  const uint8_t gateway[4] = {192, 168, 0, 1};
  const uint8_t subnet[4] = {255, 255, 255, 0};
  w5500_status_t init_status = w5500_init(mac, ip, gateway, subnet);

  while (1) {
    sys_sem_wait(net_posted);

    uint32_t irq_state = irq_save();
    net_request_t my_req = current_req;
    irq_restore(irq_state);
    w5500_status_t ret_status = W5500_OK;

    if (init_status != W5500_OK) {
      my_req.reply_slot->status = NET_STATUS_DEAD;
      sys_sem_post(net_done);
      continue;
    }

    switch (my_req.op) {
    case NET_OP_LINK: {
      int up = 0;
      ret_status = w5500_link_up(&up);
      if (ret_status == W5500_OK)
        *my_req.result = up;
      break;
    }
    case NET_OP_SOCKET_OPEN: {
      ret_status = w5500_socket_open(my_req.socket);
      break;
    }
    case NET_OP_RECV: {

      ret_status = w5500_socket_recv(
          my_req.socket, my_req.buffer, my_req.buf_len, &my_req.reply_slot->got,
          my_req.reply_slot->src_ip, &my_req.reply_slot->port,
          &my_req.reply_slot->truncated);

      break;
    }
    case NET_OP_SEND: {
      uint8_t *mac = 0;
      if (my_req.use_mac) {
        mac = my_req.dst_mac;
        kprintf("Sendind to mac %x:%x...\r\n", mac[0], mac[1]);
      } else {
        kprintf("Sending only to ip\r\n");
      }
      ret_status = w5500_socket_send(my_req.socket, my_req.buffer,
                                     my_req.buf_len, &my_req.reply_slot->got,
                                     my_req.dst_ip, my_req.dst_port, mac);
      break;
    }
    case NET_OP_SOCKET_CLOSE: {
      ret_status = w5500_socket_close(my_req.socket);
      break;
    }
    }

    switch (ret_status) {
    case W5500_OK: {
      my_req.reply_slot->status = NET_STATUS_OK;
      break;
    }
    case W5500_ERR_UNREACHABLE: {
      my_req.reply_slot->status = NET_STATUS_UNREACHABLE;
      break;
    }
    case W5500_ERR_UNSTABLE:
    case W5500_ERR_TRANSIENT: {
      my_req.reply_slot->status = NET_STATUS_RETRY;
      break;
    }
    case W5500_ERR_ARG: {
      my_req.reply_slot->status = NET_STATUS_BAD_REQUEST;
      break;
    }
    case W5500_ERR_STATE: {
      init_status = w5500_init(mac, ip, gateway, subnet);
      socket_memory_init();
      if (init_status != W5500_OK)
        my_req.reply_slot->status = NET_STATUS_DEAD;
      else
        my_req.reply_slot->status = NET_STATUS_FAULT;
      break;
    }
    case W5500_ERR_FAULT: {
      spi_return_t spi_ret = spi1_recover();
      my_req.reply_slot->status = NET_STATUS_FAULT;
      break;
    }
    }

    sys_sem_post(net_done);
  }
}
