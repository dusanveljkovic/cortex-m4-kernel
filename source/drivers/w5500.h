#ifndef _W5500_H_
#define _W5500_H_

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

typedef struct {
  uint8_t number;
} w5500_socket_t;

void w5500_init(const uint8_t mac[6], const uint8_t ip[4],
                const uint8_t gateway[4], const uint8_t subnet[4]);
uint8_t w5500_get_version(void);
bool w5500_link_up(void);

#endif
