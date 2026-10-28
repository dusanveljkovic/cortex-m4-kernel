#include "dhcp.h"
#include "../../include/net.h"
#include "../../include/syscall.h"
#include "../../include/utils.h"
#include "uprintf.h"

#define DHCP_DEFAULT_LENGTH 240
#define DHCP_OPTIONS_START 240

static dhcp_status_t parse_options(const uint8_t *buf, uint16_t len,
                                   dhcp_msg_t *out) {
  uint16_t pos = DHCP_OPTIONS_START;
  out->has_requested_ip = false;
  out->has_message_type = false;
  out->has_server_id = false;
  out->has_client_id = false;
  memset(out->requested_bitmap, 0, 32);
  while (pos + 1 < len) {
    uint8_t curr_byte = buf[pos];
    dhcp_option_code_t curr_code = (dhcp_option_code_t)curr_byte;
    if (curr_code == DHCP_OPT_PAD) {
      pos += 1;
      continue;
    }
    if (curr_code == DHCP_OPT_EXIT) {
      break;
    }

    pos++;
    uint8_t data_len = buf[pos];
    pos++;

    if (pos + data_len > len)
      return DHCP_ERR_PARSING;

    switch (curr_code) {
    case DHCP_OPT_REQUESTED_IP:
      if (data_len != 4)
        return DHCP_ERR_MALFORMED;
      out->has_requested_ip = true;
      memcpy(out->requested_ip, (buf + pos), data_len);
      break;
    case DHCP_OPT_MESSAGE_TYPE:
      if (data_len != 1)
        return DHCP_ERR_MALFORMED;
      out->has_message_type = true;
      out->message_type = (dhcp_msg_type_t)buf[pos];
      break;
    case DHCP_OPT_SERVER_ID:
      if (data_len != 4)
        return DHCP_ERR_MALFORMED;
      out->has_server_id = true;
      memcpy(out->server_id, (buf + pos), data_len);
      break;
    case DHCP_OPT_PARAMETER_REQUEST_LIST:
      for (int i = 0; i < data_len; i++) {
        uint8_t tag = buf[pos + i];
        out->requested_bitmap[tag >> 3] |= 1u << (tag & 7);
      }
      break;
    case DHCP_OPT_CLIENT_ID:
      if (data_len > CID_CAP)
        return DHCP_ERR_MALFORMED;
      out->has_client_id = true;
      memcpy(out->client_id, (buf + pos), data_len);
      out->client_id_len = data_len;
      break;
    default:
      break;
    }

    pos += data_len;
  }

  return DHCP_OK;
}

dhcp_status_t dhcp_parse(const uint8_t *buf, uint16_t len, dhcp_msg_t *out) {
  if (out == 0 || buf == 0)
    return DHCP_ERR_ARG;
  if (len < DHCP_DEFAULT_LENGTH)
    return DHCP_ERR_SHORT;

  if (buf[236] != 0x63 || buf[237] != 0x82 || buf[238] != 0x53 ||
      buf[239] != 0x63)
    return DHCP_ERR_NOT_DHCP;

  // htype must be ETHERNET and hlen must be 6
  if (buf[1] != 1 || buf[2] != 6)
    return DHCP_ERR_MALFORMED;

  out->op = (dhcp_op_t)buf[0];
  out->htype = buf[1];
  out->hlen = buf[2];
  out->hops = buf[3];

  memcpy(&out->xid, (buf + 4), 4);
  out->secs = buf[9] | (buf[8] << 8);
  out->flags = buf[11] | (buf[10] << 8);

  memcpy(out->ciaddr, (buf + 12), 4);
  memcpy(out->yiaddr, (buf + 16), 4);
  memcpy(out->siaddr, (buf + 20), 4);
  memcpy(out->giaddr, (buf + 24), 4);

  memcpy(out->chaddr, (buf + 28), 16);

  dhcp_status_t ret_status = parse_options(buf, len, out);
  if (ret_status != DHCP_OK) {
    return ret_status;
  }

  if (out->has_client_id) {
    memcpy(out->lease_key, out->client_id, out->client_id_len);
    out->lease_key_length = out->client_id_len;
  } else {
    memcpy(out->lease_key, out->chaddr, out->hlen);
    out->lease_key_length = out->hlen;
  }

  return DHCP_OK;
}

typedef struct {
  dhcp_msg_type_t type;
  uint8_t len;
  uint8_t val;
  const uint8_t *ptr;
  bool include;
} option_t;

static dhcp_status_t embedd_options(uint8_t *out, uint16_t cap,
                                    const option_t *options, uint8_t opt_len,
                                    uint16_t *out_len) {
  uint16_t curr_pos = DHCP_OPTIONS_START;
  uint16_t opt_size = 0;
  for (int i = 0; i < opt_len; i++) {
    if (!(options + i)->include)
      continue;
    opt_size +=
        1 + 1 +
        (options + i)->len; // 1 byte for code, 1 for length and length bytes
  }

  if (curr_pos + opt_size > cap)
    return DHCP_ERR_ARG;

  for (int i = 0; i < opt_len; i++) {
    const option_t *o = (options + i);
    if (!o->include)
      continue;
    out[curr_pos++] = o->type;
    out[curr_pos++] = o->len;
    if (o->ptr != 0) {
      memcpy((out + curr_pos), o->ptr, o->len);
      curr_pos += o->len;
    } else {
      out[curr_pos++] = o->val;
    }
  }

  // pad to 300
  if (curr_pos < 300) {
    memset((out + curr_pos), 0, 300 - curr_pos);
    curr_pos = 299;
  }

  // last byte is 255
  out[curr_pos] = 0xff;
  *out_len = (curr_pos + 1);

  return DHCP_OK;
}

static dhcp_status_t
dhcp_build_reply(const uint8_t *req_raw, const dhcp_msg_t *req,
                 dhcp_msg_type_t type, const dhcp_cfg_t *cfg, uint8_t yiaddr[4],
                 uint8_t *out, uint16_t cap, uint16_t *out_len) {
  if (out_len == 0) {
    return DHCP_ERR_ARG;
  }
  if (cfg == 0 || cap < 300) {
    *out_len = 0;
    return DHCP_ERR_ARG;
  }

  memset(out, 0, cap);
  memcpy(out, req_raw, 236);
  // op = BOOTREPLY
  out[0] = BOOTREPLY;

  // hops = 0
  out[3] = 0;

  // secs = 0
  out[8] = 0;
  out[9] = 0;

  // yiaddr
  out[16] = yiaddr[0];
  out[17] = yiaddr[1];
  out[18] = yiaddr[2];
  out[19] = yiaddr[3];
  uprintf("yiaddr %d.%d.%d.%d\r\n", out[0], out[1], out[2], out[3]);

  // magic cookie
  out[236] = 0x63;
  out[237] = 0x82;
  out[238] = 0x53;
  out[239] = 0x63;

  uint8_t serialized_lease_time[4] = {
      (cfg->lease_time >> 24), (cfg->lease_time >> 16), (cfg->lease_time >> 8),
      (cfg->lease_time)};
  option_t options[] = {
      {DHCP_OPT_MESSAGE_TYPE, 1, type, 0, true},
      {DHCP_OPT_SERVER_ID, 4, 0, cfg->server_id, true},
      {DHCP_OPT_LEASE_TIME, 4, 0, serialized_lease_time, true},
      {DHCP_OPT_SUBNET_MASK, 4, 0, cfg->subnet_mask, true},
      {DHCP_OPT_ROUTER, 4, 0, cfg->router, true},
      {DHCP_OPT_DNS, 4, 0, cfg->dns, true},
      {DHCP_OPT_BROADCAST_ADDR, 4, 0, cfg->broadcast_addr, true},
      {DHCP_OPT_CLIENT_ID, req->client_id_len, 0, req->client_id,
       req->has_client_id}};

  dhcp_status_t ret_status = embedd_options(
      out, cap, options, sizeof(options) / sizeof(option_t), out_len);
  if (ret_status != DHCP_OK) {
    *out_len = 0;
    return ret_status;
  }

  return DHCP_OK;
}

void print_net_error(net_reply_t *reply) {
  uprintf("![net] NET ERROR: ");
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
    uprintf("SHORT\r\n");
    break;
  case DHCP_ERR_ARG:
    uprintf("ARG\r\n");
    break;
  default:
    break;
  }
}

void dhcp_msg_print(dhcp_msg_t *msg, uint16_t len, uint8_t *buffer) {
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
  if (msg->has_message_type)
    uprintf("OPT[message type]: %d\r\n", msg->message_type);

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

#define DHCP_SERVER_PORT 6767
#define DHCP_CLIENT_PORT 6868

typedef enum {
  LEASE_ST_FREE = 0,
  LEASE_ST_OFFERED,
  LEASE_ST_BOUND
} lease_state_t;

typedef struct {
  lease_state_t state;
  uint8_t key[LEASE_KEY_CAP];
  uint8_t key_len;
  uint32_t expires_at;
} lease_slot_t;

typedef struct {
  lease_slot_t *slots;
  uint8_t size;
  uint8_t start_addr[4];
} lease_table_t;

static dhcp_status_t chose_address(lease_table_t *table, uint8_t *key,
                                   uint8_t key_len, uint8_t *chosen_address,
                                   bool *has_chosen) {

  if (table == 0 || key == 0 || key_len == 0 || chosen_address == 0 ||
      key_len > LEASE_KEY_CAP || has_chosen == 0)
    return DHCP_ERR_ARG;
  lease_slot_t *found = 0;
  uint8_t found_idx;
  lease_slot_t *first_free = 0;
  uint8_t first_free_idx;
  for (uint8_t i = 0; i < table->size; i++) {
    lease_slot_t *slot = (table->slots + i);
    if (first_free == 0 && slot->state == LEASE_ST_FREE) {
      first_free = slot;
      first_free_idx = i;
    }
    if (key_len != slot->key_len)
      continue;
    if (memcmp(slot->key, key, key_len) != 0)
      continue;
    found = slot;
    found_idx = i;
    break;
  }
  if (found != 0) {
    memcpy(chosen_address, table->start_addr, 4);
    chosen_address[3] += found_idx;
    *has_chosen = true;
  } else if (first_free != 0) {
    first_free->state = LEASE_ST_OFFERED;
    memcpy(first_free->key, key, key_len);
    first_free->key_len = key_len;

    memcpy(chosen_address, table->start_addr, 4);
    chosen_address[3] += first_free_idx;
    *has_chosen = true;
  } else {
    *has_chosen = false;
  }
  return DHCP_OK;
}

void dhcp_task(void *args) {
  uprintf("[dhcp] Starting server...\r\n");
  dhcp_cfg_t *cfg = (dhcp_cfg_t *)args;
  socket_t *socket = sys_socket_get(0);
  net_reply_t reply_slot = {0, 0, 0, 0, 0};

  socket->protocol = UDP;
  socket->port = DHCP_SERVER_PORT;

  sys_socket_open(socket, &reply_slot);
  if (reply_slot.status != NET_STATUS_OK) {
    print_net_error(&reply_slot);
    return;
  }

  uint8_t lease_table_size = cfg->end_addr[3] - cfg->start_addr[3] + 1;
  lease_slot_t *all_slots =
      (lease_slot_t *)sys_malloc(sizeof(lease_slot_t) * lease_table_size);
  memset(all_slots, 0, lease_table_size * sizeof(lease_slot_t));
  lease_table_t lease_table = {.slots = all_slots, .size = lease_table_size};
  memcpy(lease_table.start_addr, cfg->start_addr, 4);

  uint16_t net_buffer_size = 1500;
  uint8_t *net_buffer =
      (uint8_t *)sys_malloc(net_buffer_size * sizeof(uint8_t));
  dhcp_msg_t *received_msg = (dhcp_msg_t *)sys_malloc(sizeof(dhcp_msg_t));
  uint16_t dhcp_reply_buffer_size = 1000;
  uint8_t *dhcp_reply_buffer =
      (uint8_t *)sys_malloc(dhcp_reply_buffer_size * sizeof(uint8_t));

  uprintf("[dhcp] Listening on %d\r\n", DHCP_SERVER_PORT);

  while (true) {
    sys_net_recv(socket, net_buffer, net_buffer_size, &reply_slot);
    if (reply_slot.status == NET_STATUS_OK && reply_slot.got != 0) {
      dhcp_status_t dhcp_status =
          dhcp_parse(net_buffer, reply_slot.got, received_msg);
      if (dhcp_status != DHCP_OK) {
        print_dhcp_error(dhcp_status);
        continue;
      }

      uprintf("[dhcp] Received message:\r\n");
      dhcp_msg_print(received_msg, reply_slot.got, net_buffer);

      if (received_msg->op == BOOTREQUEST && received_msg->has_message_type &&
          received_msg->message_type == DHCP_MSG_DISCOVER) {
        uint16_t offer_length = 0;
        uint8_t chosen_address[4];
        bool has_chosen = false;
        chose_address(&lease_table, received_msg->lease_key,
                      received_msg->lease_key_length, chosen_address,
                      &has_chosen);
        if (!has_chosen) {
          uprintf("[dhcp] Didn't chose an address\r\n");
          continue;
        } else {
          uprintf("[dhcp] Chose address %d.%d.%d.%d\r\n", chosen_address[0],
                  chosen_address[1], chosen_address[2], chosen_address[3]);
        }
        dhcp_status = dhcp_build_reply(net_buffer, received_msg, DHCP_MSG_OFFER,
                                       cfg, chosen_address, dhcp_reply_buffer,
                                       dhcp_reply_buffer_size, &offer_length);
        if (dhcp_status != DHCP_OK) {
          uprintf("[dhcp][!] Couldn't build an offer\r\n");
          print_dhcp_error(dhcp_status);
          continue;
        }
        net_send_args_t offer_args = {.tx_data = dhcp_reply_buffer,
                                      .tx_length = offer_length,
                                      .dst_ip = {255, 255, 255, 255},
                                      .dst_port = DHCP_CLIENT_PORT};
        net_reply_t offer_reply_slot = {0, 0, 0, 0, 0};
        uprintf("[dhcp] Received a discover\r\n");
        uprintf("[dhcp] Sending an offer\r\n");

        sys_net_send(socket, &offer_args, &offer_reply_slot);

        if (offer_reply_slot.status != NET_STATUS_OK) {
          uprintf("[dhcp][!] Couldn't send an offer\r\n");
          print_net_error(&reply_slot);
          continue;
        }
      }
      if (received_msg->op == BOOTREQUEST && received_msg->has_message_type &&
          received_msg->message_type == DHCP_MSG_REQUEST) {
        uprintf("[dhcp] Received a request\r\n");

        if (received_msg->has_server_id &&
            memcmp(received_msg->server_id, cfg->server_id, 4) != 0)
          continue;

        uint8_t requested_addr[4];
        if (received_msg->has_requested_ip) {
          memcpy(requested_addr, received_msg->requested_ip, 4);
        } else {
          memcpy(requested_addr, received_msg->ciaddr, 4);
        }

        int slot_idx = requested_addr[3] - lease_table.start_addr[3];
        if (slot_idx < 0 || slot_idx >= lease_table.size)
          continue;
        lease_slot_t *slot = (lease_table.slots + slot_idx);
        if (slot->state != LEASE_ST_FREE &&
            slot->key_len == received_msg->lease_key_length &&
            memcmp(slot->key, received_msg->lease_key, slot->key_len) == 0) {
          slot->state = LEASE_ST_BOUND;

          uint16_t ack_length = 0;
          dhcp_status = dhcp_build_reply(net_buffer, received_msg, DHCP_MSG_ACK,
                                         cfg, requested_addr, dhcp_reply_buffer,
                                         dhcp_reply_buffer_size, &ack_length);
          if (dhcp_status != DHCP_OK) {
            uprintf("[dhcp][!] Couldn't build a ACK message\r\n");
            print_dhcp_error(dhcp_status);
            continue;
          }
          net_send_args_t offer_args = {.tx_data = dhcp_reply_buffer,
                                        .tx_length = ack_length,
                                        .dst_ip = {255, 255, 255, 255},
                                        .dst_port = DHCP_CLIENT_PORT};
          net_reply_t offer_reply_slot = {0, 0, 0, 0, 0};
          uprintf("[dhcp] Sending an ACK\r\n");

          sys_net_send(socket, &offer_args, &offer_reply_slot);

          if (offer_reply_slot.status != NET_STATUS_OK) {
            uprintf("[dhcp][!] Couldn't send an ACK\r\n");
            print_net_error(&reply_slot);
            continue;
          }
        }
      }
    }
    if (reply_slot.status != NET_STATUS_OK)
      print_net_error(&reply_slot);

    sys_sleep(20);
  }

  sys_free(net_buffer);
  sys_free(dhcp_reply_buffer);
  sys_free(received_msg);
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
