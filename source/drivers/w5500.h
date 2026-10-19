#ifndef _W5500_H_
#define _W5500_H_

#include "spi.h"
#include "stdbool.h"
#include "stdint.h"

#define W5500_MR 0x0000       // Mode (1 byte)
#define W5500_GAR 0x0001      // Gateway address (4 bytes)
#define W5500_SUBR 0x0005     // Subnet mask address (4 bytes)
#define W5500_SHAR 0x0009     // Source hardware address (6 bytes)
#define W5500_SIPR 0x000F     // Source IP address (4 bytes)
#define W5500_IR 0x0015       // Interrupt
#define W5500_IMR 0x0016      // Interrupt mask
#define W5500_PHYCFGR 0x002E  // PHY configuration
#define W5500_VERSIONR 0x0039 // Chip version

#define W5500_BSB_SOCK_REG(n) (1 + (n << 2))
#define _TX(n) (2 + (n << 2))
#define _RX(n) (3 + (n << 2))

#define SOCKET_MR 0x00
#define SOCKET_CR 0x01
#define SOCKET_IR 0x02
#define SOCKET_SR 0x03
#define SOCKET_PORT 0x04
#define SOCKET_DHAR 0x06
#define SOCKET_DIPR 0x0C
#define SOCKET_DORT 0x10
#define SOCKET_MSSR 0x12
#define SOCKET_TOS 0x15
#define SOCKET_TTL 0x16
#define SOCKET_RXBUF_SIZE 0x1E
#define SOCKET_TXBUF_SIZE 0x1F
#define SOCKET_TX_FSR 0x20
#define SOCKET_TX_RD 0x22
#define SOCKET_TX_WR 0x24
#define SOCKET_RX_RSR 0x26
#define SOCKET_RX_RD 0x28
#define SOCKET_RX_WR 0x2A
#define SOCKET_IMR 0x2C
#define SOCKET_FRAG 0x2D
#define SOCKET_KPALVTR 0x2F

#define SOCKET_PROTOCOL_CLOSED 0b0000
#define SOCKET_PROTOCOL_TCP 0b0001
#define SOCKET_PROTOCOL_UDP 0b0010
#define SOCKET_PROTOCOL_MACRAW 0b0100

#define SOCKET_CMD_OPEN 0x01
#define SOCKET_CMD_LISTEN 0x02
#define SOCKET_CMD_CONNECT 0x04
#define SOCKET_CMD_DISCON 0x08
#define SOCKET_CMD_CLOSE 0x10
#define SOCKET_CMD_SEND 0x20
#define SOCKET_CMD_SEND_MAC 0x21
#define SOCKET_CMD_RECV 0x40

#define SOCKET_SR_INIT 0x13
#define SOCKET_SR_UDP 0x22

typedef enum {
  CLOSED = 0b0000,
  TCP = 0b0001,
  UDP = 0b0010,
  MACRAW = 0b0100
} socket_protocol_t;

typedef enum { CREATED, OPEN, LISTENING, ERROR } socket_status_t;

typedef struct {
  uint8_t number;
  uint16_t port;
  socket_protocol_t protocol;
  socket_status_t status;
} socket_t;

void w5500_init(const uint8_t mac[6], const uint8_t ip[4],
                const uint8_t gateway[4], const uint8_t subnet[4]);
spi_return_t w5500_get_version(uint8_t *data);
spi_return_t w5500_link_up(int *up);
void w5500_socket_open(socket_t *socket);

#endif
