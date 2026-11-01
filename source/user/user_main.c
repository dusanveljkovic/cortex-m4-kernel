#include "../../include/kernel/net.h"
#include "../../include/kernel/semaphore.h"
#include "../../include/kernel/syscall.h"
#include "../../include/kernel/utils.h"
#include "../../include/user/dhcp.h"
#include "../../include/user/uprintf.h"
#include <stdint.h>

typedef struct {
  semaphore_t *sem1;
  semaphore_t *sem2;
} arg_wrap_t;

void ping_task(void *arg) {
  arg_wrap_t *a = (arg_wrap_t *)arg;
  while (true) {
    sys_sem_post(a->sem1);
    sys_sem_wait(a->sem2);
  }
}

void pong_task(void *arg) {
  arg_wrap_t *a = (arg_wrap_t *)arg;
  while (true) {
    sys_sem_post(a->sem2);
    sys_sem_wait(a->sem1);
  }
}

void fpu_task(void *arg) {
  float x = 1.0, y = 3.2, z = 0.0;
  while (true) {
    z += x * y;
  }
}

void user_main(void *arg) {
  // mutex_t *m1 = sys_mutex_create();
  // wrapper_t *parg1 = sys_malloc(sizeof(wrapper_t));
  // parg1->number = 1;
  // parg1->sem1 = sem1;
  // parg1->sem2 = sem2;
  // parg1->m = m1;
  uprintf("[user] user task\r\n");
#if BENCH
  semaphore_t *sem1 = sys_sem_create(0);
  semaphore_t *sem2 = sys_sem_create(0);
  arg_wrap_t *arg1 = (arg_wrap_t *)sys_malloc(sizeof(arg_wrap_t));
  arg1->sem1 = sem1;
  arg1->sem2 = sem2;
  tcb_t *t1 = sys_task_create(3, ping_task, arg1);
  tcb_t *t2 = sys_task_create(3, pong_task, arg1);
  tcb_t *t3 = sys_task_create(3, fpu_task, 0);
#endif

  dhcp_cfg_t *config = (dhcp_cfg_t *)sys_malloc(sizeof(dhcp_cfg_t));
  dhcp_cfg_t local_cfg = {.start_addr = {192, 168, 0, 110},
                          .end_addr = {192, 168, 0, 200},
                          .server_id = {192, 168, 0, 105},
                          .lease_time = 3600,
                          .subnet_mask = {192, 168, 0, 255},
                          .router = {192, 168, 0, 1},
                          .dns = {8, 8, 8, 8},
                          .broadcast_addr = {255, 255, 255, 255}};
  memcpy(config, &local_cfg, sizeof(dhcp_cfg_t));
  int32_t link = -1;
  net_reply_t reply_slot = {0, 0, 0, 0, 0};

  while (true) {
    sys_net_link(&link, &reply_slot);
    if (reply_slot.status != NET_STATUS_OK) {
      // print_net_error(&reply_slot);
      return;
    }
    uprintf("[user] link status %d\r\n", link);
    sys_sleep(1000);
    if (link == 1)
      break;
  }
  uprintf("[user] Starting DHCP server\r\n");
  tcb_t *dhcp_thread = sys_task_create(1, dhcp_task, (void *)(config));
  sys_task_set_name(dhcp_thread, "dhcp server");
  // parse_test();
}
