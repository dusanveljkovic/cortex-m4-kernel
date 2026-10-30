#include "../../include/net.h"
#include "../../include/semaphore.h"
#include "../../include/syscall.h"
#include "../../include/utils.h"
#include "dhcp.h"
#include "discover_raw.h"
#include "fpu_test.h"
#include "uprintf.h"
#include <stdint.h>

// void parse_test() {
//   dhcp_cfg_t config = {.start_addr = {192, 168, 0, 110},
//                        .end_addr = {192, 168, 0, 200},
//                        .to_give = 90,
//                        .server_id = {192, 168, 0, 105},
//                        .lease_time = 3600,
//                        .subnet_mask = {192, 168, 0, 255},
//                        .router = {192, 168, 0, 1},
//                        .dns = {8, 8, 8, 8},
//                        .broadcast_addr = {255, 255, 255, 255}};
//   uint8_t *out = (uint8_t *)sys_malloc(1500 * sizeof(uint8_t));
//   dhcp_msg_t *msg = (dhcp_msg_t *)sys_malloc(sizeof(dhcp_msg_t));
//   dhcp_parse(dhcp_discover_raw, DHCP_DISCOVER_RAW_LEN, msg);
//   dhcp_msg_print(msg, DHCP_DISCOVER_RAW_LEN, (uint8_t *)dhcp_discover_raw);
//   uint16_t out_len = 0;
//   dhcp_build_offer(dhcp_discover_raw, msg, &config, out, 1500, &out_len);
//   dhcp_parse(out, out_len, msg);
//   dhcp_msg_print(msg, out_len, out);
// }

void user_main(void *arg) {
  // semaphore_t *sem1 = sys_sem_create(0);
  // semaphore_t *sem2 = sys_sem_create(0);
  // mutex_t *m1 = sys_mutex_create();
  // wrapper_t *parg1 = sys_malloc(sizeof(wrapper_t));
  // parg1->number = 1;
  // parg1->sem1 = sem1;
  // parg1->sem2 = sem2;
  // parg1->m = m1;
  uprintf("[user] user task\r\n");
  // tcb_t *t3 = sys_task_create(1, low_task, parg1);
  // sys_task_set_name(t3, "low task");
  // float x1 = 0.222;
  // float x2 = 0.333;
  // float x3 = x1 + x2;

  // *((uint32_t *)0x20000c00) = 1;
  // *((uint32_t *)0x20001000) = 1;
  // *((uint32_t *)0x20001004) = 1;
  // *((uint32_t *)0x20001008) = 1;
  // *((uint32_t *)0x2000100C) = 1;
  // uprintf("[user] testing FPU");
  // fpu_test_start();
  // net_test();
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
