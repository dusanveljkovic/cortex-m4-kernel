#include "../../include/flash.h"
#include "../../include/kprintf.h"

#define FLASH_KEYR_1 0x45670123
#define FLASH_KEYR_2 0xCDEF89AB

static void flash_erase_sector(uint8_t sector_number) {
  if (sector_number > 7)
    return;

  FLASH->KEYR = FLASH_KEYR_1;
  FLASH->KEYR = FLASH_KEYR_2;

  while (1) {
    uint8_t bsy = (FLASH->SR & FLASH_SR_BUSY);
    if (bsy == 0)
      break;
  }

  // x32 paralellism size
  FLASH->CR |= (PSIZE_32 << FLASH_CR_PSIZE_Pos);

  FLASH->CR |= (sector_number << FLASH_CR_SNB_Pos);
  FLASH->CR |= FLASH_CR_SER;

  FLASH->CR |= FLASH_CR_STRT;

  while (1) {
    uint8_t bsy = (FLASH->SR & FLASH_SR_BUSY);
    if (bsy == 0)
      break;
  }

  FLASH->CR &= ~(0b000 << FLASH_CR_SNB_Pos);
  FLASH->CR &= ~FLASH_CR_SER;
  FLASH->CR &= ~FLASH_CR_STRT;

  return;
}

static uintptr_t flash_sectors[8] = {0x08000000, 0x08004000, 0x08008000,
                                     0x0800C000, 0x08010000, 0x08020000,
                                     0x08040000, 0x08060000};
static uint32_t flash_sector_read_pointer[8] = {0, 0, 0, 0, 0, 0, 0, 0};
static uint32_t flash_sector_write_pointer[8] = {0, 0, 0, 0, 0, 0, 0, 0};

static void flash_program_word(uint8_t sector_number, uint32_t word) {
  if (sector_number > 7)
    return;
  volatile uint32_t *sector = (uint32_t *)flash_sectors[sector_number];
  uint32_t curr_pos = flash_sector_write_pointer[sector_number];
  while (1) {
    uint8_t bsy = (FLASH->SR & FLASH_SR_BUSY);
    if (bsy == 0)
      break;
  }

  FLASH->CR |= (PSIZE_32 << FLASH_CR_PSIZE_Pos);

  FLASH->CR |= FLASH_CR_PG;

  sector[curr_pos] = word;

  while (1) {
    uint8_t bsy = (FLASH->SR & FLASH_SR_BUSY);
    if (bsy == 0)
      break;
  }

  FLASH->CR &= ~FLASH_CR_PG;

  flash_sector_write_pointer[sector_number] += 1;
}

void flash_append_journal(uint32_t *data, uint32_t len) {
  FLASH->KEYR = FLASH_KEYR_1;
  FLASH->KEYR = FLASH_KEYR_2;
  for (uint32_t i = 0; i < len; i++) {
    flash_program_word(FLASH_JOURNAL_SECTOR, data[i]);
  }
  FLASH->CR |= FLASH_CR_LOCK;
}

void flash_program_journal(uint32_t *data, uint32_t len) {
  flash_erase_sector(FLASH_JOURNAL_SECTOR);
  flash_append_journal(data, len);
}

void flash_read_journal(uint32_t *data, uint32_t n_words) {
  uint32_t *journal_sector = (uint32_t *)flash_sectors[FLASH_JOURNAL_SECTOR];
  uint32_t curr_pos = flash_sector_read_pointer[FLASH_JOURNAL_SECTOR];
  for (uint32_t i = 0; i < n_words; i++) {
    data[i] = journal_sector[curr_pos + i];
  }
  flash_sector_read_pointer[FLASH_JOURNAL_SECTOR] += n_words;
}

void flash_journal_reset_read_pointer(void) {
  flash_sector_read_pointer[FLASH_JOURNAL_SECTOR] = 0;
}

void flash_journal_reset_write_pointer(void) {
  flash_sector_write_pointer[FLASH_JOURNAL_SECTOR] = 0;
}

void flash_journal_set_write_pointer(int pom) {
  flash_sector_write_pointer[FLASH_JOURNAL_SECTOR] =
      flash_sector_read_pointer[FLASH_JOURNAL_SECTOR] + pom;
}
