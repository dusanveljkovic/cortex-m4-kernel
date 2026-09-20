#ifndef _FLASH_H
#define _FLASH_H

#include "stdint.h"
typedef struct {
  uint32_t ACR;
} FLASH_RegisterMap;

#define FLASH ((FLASH_RegisterMap *)0x40023C00)

#define FLASH_ACR_LATENCY_2WS 0x2

#define FLASH_ACR_ICEN (1 << 9)
#define FLASH_ACR_DCEN (1 << 10)

#endif // !_FLASH_H
