#include "dhcp.h"
#include "../../include/utils.h"

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

  // check magic cookie

  dhcp_status_t ret_status = parse_options(buf, len, out);
  if (ret_status != DHCP_OK) {
    return ret_status;
  }

  return DHCP_OK;
}

dhcp_status_t dhcp_build_offer(const uint8_t *req_raw, const dhcp_msg_t *req,
                               uint8_t *out, uint16_t cap, uint16_t *out_len) {
  memset(out, 0, cap);
  memcpy(out, req_raw, 236);
  // op = BOOTREPLY
  out[0] = BOOTREPLY;

  // hops = 0
  out[3] = 0;

  // secs = 0
  out[8] = 0;
  out[9] = 0;

  // magic cookie
  out[236] = 0x63;
  out[237] = 0x82;
  out[238] = 0x53;
  out[239] = 0x63;

  return DHCP_OK;
}
