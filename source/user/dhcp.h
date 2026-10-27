#ifndef _DHCP_H
#define _DHCP_H

#include <stdbool.h>
#include <stdint.h>

enum { BOOTREQUEST = 1, BOOTREPLY = 2 };

typedef uint8_t dhcp_op_t;

enum {
  DHCP_OPT_PAD = 0,
  DHCP_OPT_SUBNET_MASK = 1,
  DHCP_OPT_TIME_OFFSET = 2,
  DHCP_OPT_ROUTER = 3,
  DHCP_OPT_TIME_SERVER = 4,
  DHCP_OPT_REQUESTED_IP = 50,
  DHCP_OPT_MESSAGE_TYPE = 53,
  DHCP_OPT_SERVER_ID = 54,
  DHCP_OPT_PARAMETER_REQUEST_LIST = 55,
  DHCP_OPT_CLIENT_ID = 61,
  DHCP_OPT_EXIT = 255
};

typedef uint8_t dhcp_option_code_t;

enum {
  DHCP_MSG_DISCOVER = 1,
  DHCP_MSG_OFFER = 2,
  DHCP_MSG_REQUEST = 3,
  DHCP_MSG_DECLINE = 4,
  DHCP_MSG_ACK = 5,
  DHCP_MSG_NAK = 6,
  DHCP_MSG_RELEASE = 7,
  DHCP_MSG_INFORM = 8,
};

typedef uint8_t dhcp_msg_type_t;

#define CID_CAP 16

typedef struct {
  dhcp_op_t op;
  uint8_t htype;
  uint8_t hlen;
  uint8_t hops;

  uint8_t xid[4];

  uint16_t secs;
  uint16_t flags;

  uint8_t ciaddr[4]; // client IPv4 address
  uint8_t yiaddr[4]; // IPv4 address that a server assigns
  uint8_t siaddr[4]; // IPv4 address of the server to be used in the next phase
  uint8_t giaddr[4]; // IPv4 address of hte first DHCP reply agent

  uint8_t chaddr[16]; // MAC address

  // uint8_t sname[64];
  // uint8_t file[128];

  uint8_t requested_ip[4];
  bool has_requested_ip;

  dhcp_msg_type_t message_type;
  bool has_message_type;

  uint8_t server_id[4];
  bool has_server_id;

  uint8_t client_id[CID_CAP];
  uint8_t client_id_len;
  bool has_client_id;

  uint8_t requested_bitmap[32];
} dhcp_msg_t;

typedef enum {
  DHCP_OK,
  DHCP_ERR_NOT_DHCP,
  DHCP_ERR_PARSING,
  DHCP_ERR_ARG,
  DHCP_ERR_SHORT,
  DHCP_ERR_MALFORMED,
} dhcp_status_t;

dhcp_status_t dhcp_parse(const uint8_t *buf, uint16_t len, dhcp_msg_t *out);

#endif // !_DHCP
