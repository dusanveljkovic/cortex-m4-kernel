#include "../../include/net.h"
#include "../../include/semaphore.h"
#include "../../include/syscall.h"
#include "../../include/utils.h"
#include "dhcp.h"
#include "fpu_test.h"
#include "uprintf.h"
#include <stdint.h>

void print_net_error(net_reply_t *reply) {
  uprintf("![user] NET ERROR: ");
  switch (reply->status) {
  case NET_STATUS_OK:
    uprintf("OK\r\n");
    break;
  case NET_STATUS_RETRY:
    uprintf("RETRY\r\n");
    break;
  case NET_STATUS_BAD_REQUEST:
    uprintf("BAD REQUEST\r\n");
    break;
  case NET_STATUS_FAULT:
    uprintf("FAULT\r\n");
    break;
  case NET_STATUS_DEAD:
    uprintf("DEAD\r\n");
    break;
  case NET_STATUS_UNREACHABLE:
    uprintf("UNREACHABLE\r\n");
    break;
  }
}

void print_dhcp_error(dhcp_status_t status) {
  uprintf("![dhcp] ERROR: ");
  switch (status) {
  case DHCP_ERR_MALFORMED:
    uprintf("MALFORMED\r\n");
    break;
  case DHCP_ERR_NOT_DHCP:
    uprintf("NOT DHCP\r\n");
    break;
  case DHCP_ERR_SHORT:
    uprintf("SHORD");
    break;
  case DHCP_ERR_ARG:
    uprintf("ARG");
    break;
  default:
    break;
  }
}

void dhcp_msg_print(dhcp_msg_t *msg, uint16_t len, uint8_t *buffer) {
  uprintf("[user] parsed DHCP:\r\n");
  uprintf("OP: %d HTYPE: %d HLEN: %d HOPS: %d\r\n", msg->op, msg->htype,
          msg->hlen, msg->hops);
  uprintf("XID: %x%x%x%x\r\n", msg->xid[0], msg->xid[1], msg->xid[2],
          msg->xid[3]);
  uprintf("SECS: %d\tFLAGS: %x\r\n", msg->secs, msg->flags);
  uprintf("CIADDR: %d.%d.%d.%d\r\n", msg->ciaddr[0], msg->ciaddr[1],
          msg->ciaddr[2], msg->ciaddr[3]);
  uprintf("YIADDR: %d.%d.%d.%d\r\n", msg->yiaddr[0], msg->yiaddr[1],
          msg->yiaddr[2], msg->yiaddr[3]);
  uprintf("SIADDR: %d.%d.%d.%d\r\n", msg->siaddr[0], msg->siaddr[1],
          msg->siaddr[2], msg->siaddr[3]);
  uprintf("GIADDR: %d.%d.%d.%d\r\n", msg->giaddr[0], msg->giaddr[1],
          msg->giaddr[2], msg->giaddr[3]);
  uprintf("CHADDR: %02x:%02x:%02x:%02x:%02x:%02x\r\n", msg->chaddr[0],
          msg->chaddr[1], msg->chaddr[2], msg->chaddr[3], msg->chaddr[4],
          msg->chaddr[5]);

  if (msg->has_requested_ip)
    uprintf("OPT[requested ip]: %d%d%d%d\r\n", msg->requested_ip[0],
            msg->requested_ip[1], msg->requested_ip[2], msg->requested_ip[3]);
  if (msg->has_message_type)
    uprintf("OPT[message type]: %d\r\n", msg->message_type);

  if (msg->has_server_id)
    uprintf("OPT[server id]: %x%x%x%x\r\n", msg->server_id[0],
            msg->server_id[1], msg->server_id[2], msg->server_id[3]);

  uprintf("OPT[requested]: [");
  for (uint16_t i = 0; i < 32; i++) {
    for (uint8_t j = 0; j < 8; j++) {
      if (msg->requested_bitmap[i] & (1u << j))
        uprintf("%d, ", (i * 8 + j));
    }
  }
  uprintf("]\r\n");

  if (msg->has_client_id)
    uprintf("OPT[client_id]: %x%x%x%x\r\n", msg->client_id[0],
            msg->client_id[1], msg->client_id[2], msg->client_id[3]);
  uprintf("RAW\r\n");
  for (uint16_t i = 0; i < len; i++) {
    uprintf("%02x", buffer[i]);
  }
  uprintf("\r\n");
}

void net_test(void) {
  uprintf("[user] net test\r\n");
  int32_t link = -1;
  net_reply_t reply_slot = {0, 0, 0, 0, 0};

  while (true) {
    sys_net_link(&link, &reply_slot);
    if (reply_slot.status != NET_STATUS_OK) {
      print_net_error(&reply_slot);
      return;
    }
    uprintf("[user] link status %d\r\n", link);
    sys_sleep(1000);
    if (link == 1)
      break;
  }

  socket_t *socket = sys_socket_get(0);
  socket->protocol = UDP;
  socket->port = 6767;
  sys_socket_open(socket, &reply_slot);
  if (reply_slot.status != NET_STATUS_OK) {
    print_net_error(&reply_slot);
    return;
  }
  uint8_t *buffer = (uint8_t *)sys_malloc(1500 * sizeof(uint8_t));
  dhcp_msg_t *out = (dhcp_msg_t *)sys_malloc(sizeof(dhcp_msg_t));

  while (true) {
    sys_net_recv(socket, buffer, 1500, &reply_slot);
    if (reply_slot.status == NET_STATUS_OK && reply_slot.got != 0) {

      dhcp_status_t dhcp_status = dhcp_parse(buffer, reply_slot.got, out);
      if (dhcp_status != DHCP_OK)
        print_dhcp_error(dhcp_status);
      else {
        dhcp_msg_print(out, reply_slot.got, buffer);
      }
    }
    if (reply_slot.status != NET_STATUS_OK)
      print_net_error(&reply_slot);

    sys_sleep(2000);
  }
  // uint8_t *send_buffer = (uint8_t *)sys_malloc(300 * sizeof(uint8_t));
  // uint8_t count = 0;
  //
  // // while (true) {
  // memset(send_buffer, count++, 300);
  // uprintf("[user]sent buffer: \r\n");
  // for (int i = 0; i < 300; i++) {
  //   uprintf("%x ", send_buffer[i]);
  // }
  // uprintf("\r\n");
  // net_send_args_t args = {.tx_data = send_buffer,
  //                         .tx_length = 300,
  //                         .dst_ip = {192, 168, 0, 200},
  //                         .dst_port = 6767};
  // sys_net_send(socket, &args, &reply_slot);
  // print_net_error(&reply_slot);
  // sys_sleep(2000);
  // net_send_args_t args2 = {.tx_data = send_buffer,
  //                          .tx_length = 300,
  //                          .dst_ip = {255, 255, 255, 255},
  //                          .dst_port = 6767};
  // sys_net_send(socket, &args2, &reply_slot);
  // print_net_error(&reply_slot);
  // sys_sleep(2000);
  // }
}

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
  net_test();
}
