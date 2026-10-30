#ifndef _FLASH_H
#define _FLASH_H

#include "stdint.h"
typedef struct {
  uint32_t ACR;
  uint32_t KEYR;
  uint32_t OPTKEYR;
  uint32_t SR;
  uint32_t CR;
  uint32_t OPTCR;
} FLASH_RegisterMap;

#define FLASH ((volatile FLASH_RegisterMap *)0x40023C00)

#define FLASH_ACR_LATENCY_2WS 0x2

#define FLASH_ACR_ICEN (1 << 9)
#define FLASH_ACR_DCEN (1 << 10)

#define FLASH_SR_BUSY (1 << 16)

#define FLASH_CR_PSIZE_Pos (8)
#define FLASH_CR_SNB_Pos (3)
#define FLASH_CR_SER (1 << 1)
#define FLASH_CR_STRT (1 << 16)
#define FLASH_CR_PG (1 << 0)
#define FLASH_CR_LOCK (1 << 31)

#define PSIZE_64 (0b11)
#define PSIZE_32 (0b10)

#define FLASH_JOURNAL_SECTOR (3)

void flash_append_journal(uint32_t *data, uint32_t len);
void flash_program_journal(uint32_t *data, uint32_t len);
void flash_read_journal(uint32_t *data, uint32_t len);
void flash_journal_reset_read_pointer(void);
void flash_journal_reset_write_pointer(void);
void flash_journal_set_write_pointer(int pom);

#endif // !_FLASH_H
