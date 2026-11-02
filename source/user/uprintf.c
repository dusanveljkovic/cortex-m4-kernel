#include "../../include/user/uprintf.h"
#include "../../include/kernel/kprintf.h"
#include "../../include/kernel/syscall.h"

static uint32_t clamp_len(int n, uint32_t cap) {
  if (n < 0)
    return 0;
  return (uint32_t)n >= cap ? cap - 1 : (uint32_t)n;
}

int uprintf(const char *fmt, ...) {
  char buf[128]; // lives on the task's own (small) stack
  va_list ap;
  va_start(ap, fmt);
  int n = kvsnprintf(buf, sizeof buf, fmt, ap);
  va_end(ap);

  sys_console_write(buf, clamp_len(n, sizeof buf));
  return n;
}

void print_net_error(net_reply_t *reply) {
  uprintf("![net] NET ERROR: ");
  switch (reply->status) {
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
  default:
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
    uprintf("SHORT\r\n");
    break;
  case DHCP_ERR_ARG:
    uprintf("ARG\r\n");
    break;
  default:
    break;
  }
}
void dhcp_print_msg_type(dhcp_msg_type_t msg_type) {
  switch (msg_type) {
  case DHCP_MSG_DISCOVER:
    uprintf("DISCOVER");
    break;
  case DHCP_MSG_OFFER:
    uprintf("OFFER");
    break;
  case DHCP_MSG_REQUEST:
    uprintf("REQUEST");
    break;
  case DHCP_MSG_ACK:
    uprintf("ACK");
    break;
  case DHCP_MSG_NAK:
    uprintf("NAK");
    break;
  case DHCP_MSG_DECLINE:
    uprintf("DECLINE");
    break;
  case DHCP_MSG_RELEASE:
    uprintf("RELEASE");
    break;
  case DHCP_MSG_INFORM:
    uprintf("INFORM");
    break;
  }
}

void dhcp_msg_print(dhcp_msg_t *msg) {
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
    uprintf("OPT[requested ip]: %d.%d.%d.%d\r\n", msg->requested_ip[0],
            msg->requested_ip[1], msg->requested_ip[2], msg->requested_ip[3]);
  if (msg->has_message_type) {
    uprintf("OPT[message type]: ");
    dhcp_print_msg_type(msg->message_type);
    uprintf("\r\n");
  }

  if (msg->has_server_id)
    uprintf("OPT[server id]: %d.%d.%d.%d\r\n", msg->server_id[0],
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
  // uprintf("RAW\r\n");
  // for (uint16_t i = 0; i < len; i++) {
  //   uprintf("%02x", buffer[i]);
  // }
  // uprintf("\r\n");
}
