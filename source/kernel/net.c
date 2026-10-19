#include "../../include/net.h"
#include "../../include/syscall.h"
#include "../drivers/w5500.h"
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

  const uint8_t mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  const uint8_t ip[4] = {192, 168, 0, 105};
  const uint8_t gateway[4] = {192, 168, 0, 1};
  const uint8_t subnet[4] = {255, 255, 255, 0};
  w5500_init(mac, ip, gateway, subnet);

  while (1) {
    sys_sem_wait(net_posted);

    net_request_t my_req = current_req;

    switch (my_req.op) {
    case NET_OP_LINK: {
      int up = 0;
      w5500_link_up(&up);
      *my_req.result = up;
      break;
    }
    case NET_OP_SOCKET_OPEN: {
      w5500_socket_open(my_req.socket);
      break;
    }
    case NET_OP_RECV: {
      break;
    }
    case NET_OP_SEND: {
      break;
    }
    }

    sys_sem_post(net_done);
  }
}
