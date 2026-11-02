#include "../../include/kernel/net.h"
#include "../../include/kernel/semaphore.h"
#include "../../include/kernel/syscall.h"
#include "../../include/kernel/utils.h"
#include "../../include/user/dhcp.h"
#include "../../include/user/uprintf.h"
#include <stdint.h>

void user_main(void *arg) {
  (void)arg;
  uprintf("[user] user task\r\n");

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
  net_reply_t reply_slot;
  memset(&reply_slot, 0, sizeof(net_reply_t));

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
}
